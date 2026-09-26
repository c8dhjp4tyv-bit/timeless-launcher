#include "OverrideUtils.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include "FileSystem.h"

namespace Override {

void createOverrides(const QString& name, const QString& parent_folder, const QString& override_path)
{
    QString file_path(FS::PathCombine(parent_folder, name + ".txt"));
    if (QFile::exists(file_path))
        FS::deletePath(file_path);

    FS::ensureFilePathExists(file_path);

    QFile file(file_path);
    if (!file.open(QFile::WriteOnly)) {
        qWarning() << "Failed to open file" << file.fileName() << "for writing:" << file.errorString();
        return;
    }

    const QDir overrides(override_path);
    QDirIterator override_iterator(override_path, QDirIterator::Subdirectories);
    while (override_iterator.hasNext()) {
        auto override_file_path = override_iterator.next();
        QFileInfo info(override_file_path);
        if (info.isFile()) {
            // the path in the game folder, which the overrides go into
            file.write(overrides.relativeFilePath(override_file_path).toUtf8());
            file.write("\n");
        }
    }

    file.close();
}

QStringList readOverrides(const QString& name, const QString& parent_folder)
{
    QString file_path(FS::PathCombine(parent_folder, name + ".txt"));

    QFile file(file_path);
    if (!file.exists())
        return {};

    QStringList previous_overrides;

    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "Failed to open file" << file.fileName() << "for reading:" << file.errorString();
        return previous_overrides;
    }

    QString entry;
    do {
        entry = file.readLine();
        previous_overrides.append(entry.trimmed());
    } while (!entry.isEmpty());

    file.close();

    return previous_overrides;
}

namespace {

/// The SHA-1 of a file as a hexadecimal string, or nothing if it's missing, unreadable or too large to be worth comparing
QString sha1Of(const QString& path)
{
    constexpr qint64 largestCompared = qint64{ 16 } * 1024 * 1024;
    QFile file(path);
    if (file.size() > largestCompared || !file.open(QFile::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&file)) {
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

}  // namespace

QStringList keepPlayerChanges(const QString& game_root,
                              const QString& pack_folder,
                              QStringList overrides,
                              const QString& old_game_root,
                              const QString& old_pack_folder)
{
    constexpr auto hashesFile = "override-hashes.json";
    const QDir game(game_root);
    QJsonObject oldHashes;
    if (!old_pack_folder.isEmpty()) {
        QFile file(FS::PathCombine(old_pack_folder, hashesFile));
        if (file.open(QFile::ReadOnly)) {
            oldHashes = QJsonDocument::fromJson(file.readAll()).object();
        }
    }

    QJsonObject hashes;
    QStringList kept;
    overrides.removeDuplicates();
    for (const auto& path : overrides) {
        if (path.isEmpty() || path == "options.txt" || path.startsWith("saves/")) {
            continue;
        }
        const auto hash = sha1Of(game.filePath(path));
        if (hash.isEmpty()) {
            continue;
        }
        hashes.insert(path, hash);
        if (old_game_root.isEmpty() || oldHashes.value(path).toString() != hash) {
            continue;
        }
        const auto playerFile = QDir(old_game_root).filePath(path);
        if (const auto playerHash = sha1Of(playerFile); playerHash.isEmpty() || playerHash == hash) {
            continue;
        }
        try {
            FS::write(game.filePath(path), FS::read(playerFile));
            kept << path;
        } catch (const FS::FileSystemException& e) {
            qWarning() << "Could not keep the player's changes to" << path << ":" << e.cause();
        }
    }

    try {
        FS::write(FS::PathCombine(pack_folder, hashesFile), QJsonDocument(hashes).toJson());
    } catch (const FS::FileSystemException& e) {
        qWarning() << "Could not keep what the pack's files are:" << e.cause();
    }
    return kept;
}

void keepEnabledStates(const QString& game_root, const QStringList& overrides, const QString& old_game_root)
{
    const QDir game(game_root);
    const QDir oldGame(old_game_root);
    for (const auto& entry : overrides) {
        if (entry.isEmpty()) {
            continue;
        }
        const auto path = entry.endsWith(".disabled") ? entry.chopped(9) : entry;
        QString wanted;
        if (QFileInfo::exists(oldGame.filePath(path))) {
            wanted = path;
        } else if (QFileInfo::exists(oldGame.filePath(path + ".disabled"))) {
            wanted = path + ".disabled";
        }
        if (wanted.isEmpty() || wanted == entry || !QFileInfo::exists(game.filePath(entry)) || QFileInfo::exists(game.filePath(wanted))) {
            continue;
        }
        if (!QFile::rename(game.filePath(entry), game.filePath(wanted))) {
            qWarning() << "Could not turn" << entry << "on or off as the player had it";
        }
    }
}

namespace {

/// The lines of an options.txt, which the game writes with the line breaks of the system it runs on
QStringList optionLines(const QString& text)
{
    QStringList lines;
    for (auto line : text.split('\n')) {
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (!line.isEmpty()) {
            lines << line;
        }
    }
    return lines;
}

/// The key of a line of an options.txt, which the game takes up to the first colon, or nothing if the line has none
QString optionKey(const QString& line)
{
    const auto colon = line.indexOf(':');
    return colon > 0 ? line.left(colon) : QString();
}

QHash<QString, QString> optionValues(const QStringList& lines)
{
    QHash<QString, QString> values;
    for (const auto& line : lines) {
        if (const auto key = optionKey(line); !key.isEmpty()) {
            values.insert(key, line.mid(key.size() + 1));
        }
    }
    return values;
}

QString readText(const QString& path)
{
    return QFileInfo::exists(path) ? QString::fromUtf8(FS::read(path)) : QString();
}

}  // namespace

QString mergeGameOptions(const QString& player, const QString& oldPack, const QString& newPack)
{
    const auto oldValues = optionValues(optionLines(oldPack));
    const auto newLines = optionLines(newPack);
    const auto newValues = optionValues(newLines);

    QStringList merged;
    QSet<QString> keys;
    for (const auto& line : optionLines(player)) {
        const auto key = optionKey(line);
        keys.insert(key);
        // one the player left as the pack had it is the pack's to change
        if (!key.isEmpty() && newValues.contains(key) && oldValues.contains(key) && oldValues.value(key) == line.mid(key.size() + 1)) {
            merged << key + ':' + newValues.value(key);
        } else {
            merged << line;
        }
    }
    for (const auto& line : newLines) {
        if (const auto key = optionKey(line); !key.isEmpty() && !keys.contains(key)) {
            merged << line;
            keys.insert(key);
        }
    }
    return merged.join('\n') + '\n';
}

void keepGameOptions(const QString& game_root, const QString& pack_folder, const QString& old_game_root, const QString& old_pack_folder)
{
    const auto options = FS::PathCombine(game_root, "options.txt");
    const auto oldPackOptions = old_pack_folder.isEmpty() ? QString() : FS::PathCombine(old_pack_folder, "options.txt");
    try {
        const bool packHasOptions = QFileInfo::exists(options);
        const auto pack = readText(options);
        // what the installed version kept is replaced even when this one has no options, so that it isn't taken for this one's
        if (packHasOptions || QFileInfo::exists(oldPackOptions)) {
            FS::write(FS::PathCombine(pack_folder, "options.txt"), pack.toUtf8());
        }

        const auto playerOptions = old_game_root.isEmpty() ? QString() : FS::PathCombine(old_game_root, "options.txt");
        if (!QFileInfo::exists(playerOptions)) {
            return;
        }
        // this goes in place of the player's file once the files the installed version put there are taken out
        const auto player = FS::read(playerOptions);
        FS::write(options, packHasOptions ? mergeGameOptions(QString::fromUtf8(player), readText(oldPackOptions), pack).toUtf8() : player);
    } catch (const FS::FileSystemException& e) {
        qWarning() << "Could not keep the player's game options:" << e.cause();
    }
}

}  // namespace Override
