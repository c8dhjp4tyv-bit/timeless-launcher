// SPDX-License-Identifier: GPL-3.0-only AND LicenseRef-PublicDomain
/*
 *  Timeless Launcher - Minecraft Launcher
 *  Copyright (c) 2025 Trial97 <alexandru.tripon97@gmail.com>
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
 *
 *  Additional note: Portions of this file are released into the public domain
 *  under LicenseRef-PublicDomain.
 */
#include "ArchiveReader.h"
#include <archive.h>
#include <archive_entry.h>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>

namespace MMCZip {
QStringList ArchiveReader::getFiles()
{
    return m_fileNames;
}

bool ArchiveReader::collectFiles(bool onlyFiles)
{
    return parse([this, onlyFiles](File* f) {
        if (!onlyFiles || f->isFile()) {
            m_fileNames << f->filename();
        }
        // a file that breaks off also ends the entries after it, which parse() tells from their end
        f->skip();
        return true;
    });
}

using getPathFunc = std::function<const char*(archive_entry*)>;
static QString decodeLibArchivePath(archive_entry* entry, const getPathFunc& getUtf8Path, const getPathFunc& getPath)
{
    auto fileName = QString::fromUtf8(getUtf8Path(entry));
    if (fileName.isEmpty()) {
        fileName = QString::fromLocal8Bit(getPath(entry));
    }
    return fileName;
}

QString ArchiveReader::File::filename()
{
    return decodeLibArchivePath(m_entry, archive_entry_pathname_utf8, archive_entry_pathname);
}

QByteArray ArchiveReader::File::readAll(int* outStatus)
{
    QByteArray data;
    const void* buff = nullptr;
    size_t size = 0;
    la_int64_t offset = 0;

    int status = 0;
    while ((status = archive_read_data_block(m_archive.get(), &buff, &size, &offset)) == ARCHIVE_OK) {
        data.append(static_cast<const char*>(buff), static_cast<qsizetype>(size));
    }
    if (status != ARCHIVE_EOF && status != ARCHIVE_OK) {
        qWarning() << "libarchive read error:" << archive_error_string(m_archive.get());
    }
    if (outStatus) {
        *outStatus = status;
    }
    return data;
}

QDateTime ArchiveReader::File::dateTime()
{
    auto mtime = archive_entry_mtime(m_entry);
    auto mtime_nsec = archive_entry_mtime_nsec(m_entry);
    auto dt = QDateTime::fromSecsSinceEpoch(mtime);
    return dt.addMSecs(mtime_nsec / 1e6);
}

int ArchiveReader::File::readNextHeader()
{
    return archive_read_next_header(m_archive.get(), &m_entry);
}

namespace {
/// libarchive reads a header it warns about all the same, as one whose sizes or checksum the zip's own list of its files
/// disagrees with, so only a worse status ends the entries
bool isReadable(int headerStatus)
{
    return headerStatus == ARCHIVE_OK || headerStatus == ARCHIVE_WARN;
}
}  // namespace

auto ArchiveReader::goToFile(const QString& filename) -> std::unique_ptr<File>
{
    auto f = std::make_unique<File>();
    auto* a = f->m_archive.get();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    auto fileName = m_archivePath.toStdWString();
    if (archive_read_open_filename_w(a, fileName.data(), m_blockSize) != ARCHIVE_OK) {
        qCritical() << "Failed to open archive file:" << m_archivePath << "-" << archive_error_string(a);
        return nullptr;
    }

    while (isReadable(f->readNextHeader())) {
        if (f->filename() == filename) {
            return f;
        }
        f->skip();
    }

    archive_read_close(a);
    return nullptr;
}

static int copy_data(struct archive* ar, struct archive* aw, bool notBlock = false)
{
    int r = 0;
    const void* buff = nullptr;
    size_t size = 0;
    la_int64_t offset = 0;

    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF) {
            return ARCHIVE_OK;
        }
        if (r < ARCHIVE_OK) {
            qCritical() << "Failed reading data block:" << archive_error_string(ar);
            return (r);
        }
        if (notBlock) {
            r = archive_write_data(aw, buff, size);
        } else {
            r = archive_write_data_block(aw, buff, size, offset);
        }
        if (r < ARCHIVE_OK) {
            qCritical() << "Failed writing data block:" << archive_error_string(aw);
            return (r);
        }
    }
}

static bool willEscapeRoot(const QDir& root, archive_entry* entry)
{
    auto entryPath = decodeLibArchivePath(entry, archive_entry_pathname_utf8, archive_entry_pathname);
    auto linkTarget = decodeLibArchivePath(entry, archive_entry_symlink_utf8, archive_entry_symlink);
    auto hardLink = decodeLibArchivePath(entry, archive_entry_hardlink_utf8, archive_entry_hardlink);

    if (entryPath.isEmpty() || (linkTarget.isEmpty() && hardLink.isEmpty())) {
        return false;
    }

    bool isHardLink = false;
    if (isHardLink = linkTarget.isEmpty(); isHardLink) {
        linkTarget = hardLink;
    }

    QString linkFullPath = root.filePath(entryPath);
    auto rootDir = QUrl::fromLocalFile(root.absolutePath());

    if (!rootDir.isParentOf(QUrl::fromLocalFile(linkFullPath))) {
        return true;
    }

    QDir linkDir = QFileInfo(linkFullPath).dir();
    if (!QDir::isAbsolutePath(linkTarget)) {
        linkTarget = (!isHardLink ? linkDir : root).filePath(linkTarget);
    }
    return !rootDir.isParentOf(QUrl::fromLocalFile(QDir::cleanPath(linkTarget)));
}

/// The path with the symbolic links along the part of it that already exists resolved.
/// libarchive refuses to write through any symbolic link, including the ones above the folder being extracted to, which
/// nothing in the archive can have made: a data or instances folder moved to another drive and linked back, or macOS's
/// /var, which holds the temporary folders. Only those are resolved; links inside the extraction folder are still refused.
static QString resolveSymlinks(const QString& path)
{
#ifdef Q_OS_WIN
    // canonical paths turn mapped drives into network paths there, so leave them be
    return path;
#else
    QString existing = QDir::cleanPath(path);
    QStringList missing;
    while (!QFileInfo::exists(existing)) {
        const QFileInfo info(existing);
        const auto parent = info.path();
        if (parent == existing || parent == ".") {
            return path;
        }
        missing.prepend(info.fileName());
        existing = parent;
    }
    const auto canonical = QFileInfo(existing).canonicalFilePath();
    if (canonical.isEmpty()) {
        return path;
    }
    return missing.isEmpty() ? canonical : QDir(canonical).filePath(missing.join('/'));
#endif
}

bool ArchiveReader::File::writeFile(archive* out, const QString& targetFileName, bool notBlock)
{
    return writeFile(out, targetFileName, {}, notBlock);
};

bool ArchiveReader::File::writeFile(archive* out, const QString& targetFileName, std::optional<QDir> root, bool notBlock)
{
    auto* entry = m_entry;
    std::unique_ptr<archive_entry, decltype(&archive_entry_free)> entryClone(nullptr, &archive_entry_free);
    if (!targetFileName.isEmpty()) {
        entryClone.reset(archive_entry_clone(m_entry));
        entry = entryClone.get();

        // Resolve the links above the extraction folder (the file's own folder, without one), keeping the file where it is in there
        auto targetPath = targetFileName;
        const auto base = root.has_value() ? root->absolutePath() : QFileInfo(targetFileName).absolutePath();
        if (const auto resolvedBase = resolveSymlinks(base); resolvedBase != base && targetFileName.startsWith(base + '/')) {
            targetPath = resolvedBase + targetFileName.mid(base.size());
            if (root.has_value()) {
                root = QDir(resolvedBase);
            }
        }
        auto nameUtf8 = targetPath.toUtf8();
        archive_entry_set_pathname_utf8(entry, nameUtf8.constData());

        // A hard link target names another entry of the archive, so it has to be rebased along with the
        // pathname; left alone, libarchive would resolve it against the working directory instead of the
        // extraction root. This also makes the check below inspect the target that is actually written.
        if (root.has_value()) {
            auto hardLink = decodeLibArchivePath(entry, archive_entry_hardlink_utf8, archive_entry_hardlink);
            if (!hardLink.isEmpty() && !QDir::isAbsolutePath(hardLink)) {
                auto targetUtf8 = QDir::cleanPath(root->filePath(hardLink)).toUtf8();
                archive_entry_set_hardlink_utf8(entry, targetUtf8.constData());
            }
        }
    }
    if (root.has_value() && willEscapeRoot(root.value(), entry)) {
        qCritical() << "Failed to write header to entry:" << filename() << "-" << "file outside root";
        return false;
    }
    if (archive_write_header(out, entry) < ARCHIVE_OK) {
        qCritical() << "Failed to write header to entry:" << filename() << "-" << archive_error_string(out) << targetFileName;
        return false;
    }
    if (archive_entry_size(m_entry) > 0) {
        auto r = copy_data(m_archive.get(), out, notBlock);
        if (r < ARCHIVE_OK) {
            qCritical() << "Failed reading data block:" << archive_error_string(out);
        }
        if (r < ARCHIVE_WARN) {
            return false;
        }
    }
    auto r = archive_write_finish_entry(out);
    if (r < ARCHIVE_OK) {
        qCritical() << "Failed to finish writing entry:" << archive_error_string(out);
    }
    return (r >= ARCHIVE_WARN);
}

bool ArchiveReader::parse(const std::function<bool(File*, bool&)>& doStuff)
{
    m_errorString.clear();
    m_endedEarly = false;
    auto f = std::make_unique<File>();
    auto* a = f->m_archive.get();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    auto fileName = m_archivePath.toStdWString();
    if (archive_read_open_filename_w(a, fileName.data(), m_blockSize) != ARCHIVE_OK) {
        m_errorString = QString::fromUtf8(f->error());
        qCritical() << "Failed to open archive file:" << m_archivePath << "-" << m_errorString;
        return false;
    }

    bool breakControl = false;
    int status = ARCHIVE_OK;
    while (isReadable(status = f->readNextHeader())) {
        if (status == ARCHIVE_WARN) {
            qWarning() << "Reading" << f->filename() << "from" << m_archivePath << "-" << f->error();
        }
        if (f && !doStuff(f.get(), breakControl)) {
            m_errorString = QString::fromUtf8(f->error());
            qCritical() << "Failed to parse file:" << f->filename() << "-" << m_errorString;
            return false;
        }
        if (breakControl) {
            break;
        }
    }
    // Anything but the end means the archive is damaged or was cut short, as a download can be, and only the files before
    // that were read
    if (!breakControl && status != ARCHIVE_EOF) {
        m_endedEarly = true;
        m_errorString = QString::fromUtf8(f->error());
        if (m_errorString.isEmpty()) {
            // what libarchive leaves it at when a zip ends right where the next file should start
            m_errorString = QCoreApplication::translate("MMCZip::ArchiveReader", "Unexpected end of archive");
        }
        qCritical() << "Failed to read archive file:" << m_archivePath << "-" << m_errorString;
        return false;
    }

    archive_read_close(a);
    return true;
}

bool ArchiveReader::parse(const std::function<bool(File*)>& doStuff)
{
    return parse([doStuff](File* f, bool&) { return doStuff(f); });
}

bool ArchiveReader::File::isFile()
{
    return (archive_entry_filetype(m_entry) & AE_IFMT) == AE_IFREG;
}
bool ArchiveReader::File::skip()
{
    return archive_read_data_skip(m_archive.get()) == ARCHIVE_OK;
}
const char* ArchiveReader::File::error()
{
    return archive_error_string(m_archive.get());
}
QString ArchiveReader::getZipName()
{
    return m_archivePath;
}

bool ArchiveReader::exists(const QString& filePath) const
{
    if (filePath == QLatin1String("/") || filePath.isEmpty()) {
        return true;
    }
    // Normalize input path (remove trailing slash, if any)
    QString normalizedPath = QDir::cleanPath(filePath);
    if (normalizedPath.startsWith('/')) {
        normalizedPath.remove(0, 1);
    }
    if (normalizedPath == QLatin1String(".")) {
        return true;
    }
    if (normalizedPath == QLatin1String("..")) {
        return false;  // root only
    }

    // Check for exact file match
    if (m_fileNames.contains(normalizedPath, Qt::CaseInsensitive)) {
        return true;
    }

    // Check for directory existence by seeing if any file starts with that path
    QString dirPath = normalizedPath + QLatin1Char('/');
    for (const QString& f : m_fileNames) {
        if (f.startsWith(dirPath, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

ArchiveReader::File::File() : m_archive(ArchivePtr(archive_read_new(), archive_read_free)) {}
}  // namespace MMCZip
