// SPDX-License-Identifier: GPL-3.0-only
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
 */
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QStringList>
#include <memory>
#include <optional>
#include <utility>

struct archive;
struct archive_entry;
namespace MMCZip {
class ArchiveReader {
   public:
    using ArchivePtr = std::unique_ptr<struct archive, int (*)(struct archive*)>;
    explicit ArchiveReader(QString fileName) : m_archivePath(std::move(fileName)) {}
    virtual ~ArchiveReader() = default;

    QStringList getFiles();
    QString getZipName();
    bool collectFiles(bool onlyFiles = true);
    bool exists(const QString& filePath) const;

    class File {
       public:
        File();
        virtual ~File() = default;

        QString filename();
        bool isFile();
        QDateTime dateTime();
        const char* error();

        QByteArray readAll(int* outStatus = nullptr);
        bool skip();
        bool writeFile(archive* out, const QString& targetFileName = "", bool notBlock = false);
        bool writeFile(archive* out, const QString& targetFileName, std::optional<QDir> root, bool notBlock = false);

       private:
        int readNextHeader();

       private:
        friend ArchiveReader;
        ArchivePtr m_archive;
        archive_entry* m_entry;
    };

    std::unique_ptr<File> goToFile(const QString& filename);
    /// Hands each entry to the function until it asks to stop. Fails when the function does, and when the archive can't be
    /// read to its end, as when it is damaged or was cut short.
    bool parse(const std::function<bool(File*)>&);
    bool parse(const std::function<bool(File*, bool&)>&);
    /// Why the last parse() or collectFiles() failed, as libarchive put it, when reading the archive is what failed
    QString errorString() const { return m_errorString; }
    /// Whether the last parse() or collectFiles() failed only because the archive broke off before its end, as a damaged or
    /// cut-short one does, after handing on the entries before that. What reads just a few of them, as a pack's metadata,
    /// can use those; what extracts it must not.
    bool endedEarly() const { return m_endedEarly; }

   private:
    QString m_archivePath;
    size_t m_blockSize = 10240;

    QStringList m_fileNames;
    QString m_errorString;
    bool m_endedEarly = false;
};
}  // namespace MMCZip
