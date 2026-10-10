// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Timeless Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "WorldBackups.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <algorithm>
#include <functional>
#include <utility>

#include "FileSystem.h"
#include "MMCZip.h"
#include "archive/ExportToZipTask.h"

namespace WorldBackups {

QString backupDir(const QString& savesDir)
{
    return QDir::cleanPath(FS::PathCombine(savesDir, "..", "backups"));
}

QString automaticBackupDir(const QString& savesDir)
{
    return FS::PathCombine(backupDir(savesDir), "automatic");
}

std::unique_ptr<MMCZip::ExportToZipTask> createTask(const QFileInfo& world, const QString& backupDir)
{
    QFileInfoList files;
    if (!FS::ensureFolderPathExists(backupDir) || !MMCZip::collectFileListRecursively(world.absoluteFilePath(), nullptr, &files, nullptr)) {
        return nullptr;
    }

    // the name the game gives its own backups, and the way it keeps a second one in the same second from replacing the first
    const auto name = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss") + '_' + world.fileName();
    auto zipPath = FS::PathCombine(backupDir, name + ".zip");
    for (int copy = 1; QFileInfo::exists(zipPath); copy++) {
        // the name goes in with the number in one go: a name with "%2" in it would take the number in its place otherwise
        zipPath = FS::PathCombine(backupDir, QString("%1 (%2).zip").arg(name, QString::number(copy)));
    }

    auto task = std::make_unique<MMCZip::ExportToZipTask>(zipPath, world.absoluteFilePath(), files, world.fileName() + '/');
    // the game holds this lock while the world is open, and leaves it out of its backups too
    task->setExcludeFiles({ "session.lock" });
    return task;
}

QFileInfoList backupsOf(const QString& backupDir, const QString& worldFolderName)
{
    // Only the name itself: the game names a second world with the same name "New World (1)", so "..._New World (1).zip" may
    // well be a backup of another world rather than a second one of "New World" made in the same second
    const QRegularExpression backupName(R"(^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}_)" + QRegularExpression::escape(worldFolderName) +
                                        R"(\.zip$)");
    QFileInfoList backups;
    for (const auto& file : QDir(backupDir).entryInfoList(QDir::Files)) {
        if (backupName.match(file.fileName()).hasMatch()) {
            backups << file;
        }
    }
    // the names start with when they were made
    std::ranges::sort(backups, {}, &QFileInfo::fileName);
    return backups;
}

bool changedSinceBackup(const QFileInfo& world, const QString& backupDir)
{
    const auto backups = backupsOf(backupDir, world.fileName());
    if (backups.isEmpty()) {
        return true;
    }
    // the game writes level.dat whenever it saves the world
    return QFileInfo(QDir(world.absoluteFilePath()).filePath("level.dat")).lastModified() > backups.last().lastModified();
}

QStringList removeOldBackups(const QString& backupDir, const QString& worldFolderName, int keep)
{
    const auto backups = backupsOf(backupDir, worldFolderName);
    QStringList removed;
    // the newest one always stays, whatever the setting says
    for (qsizetype i = 0; i + std::max(keep, 1) < backups.size(); i++) {
        if (QFile::remove(backups.at(i).absoluteFilePath())) {
            removed << backups.at(i).fileName();
        }
    }
    return removed;
}

QList<Backup> allBackupsOf(const QString& savesDir, const QString& worldFolderName)
{
    QList<Backup> backups;
    for (const auto& [dir, automatic] : { std::pair{ backupDir(savesDir), false }, std::pair{ automaticBackupDir(savesDir), true } }) {
        for (const auto& file : backupsOf(dir, worldFolderName)) {
            // the name starts with the local date and time it was made at
            backups.append({ file, QDateTime::fromString(file.fileName().left(19), "yyyy-MM-dd_HH-mm-ss"), automatic });
        }
    }
    std::ranges::stable_sort(backups, std::greater{}, &Backup::made);
    return backups;
}

}  // namespace WorldBackups
