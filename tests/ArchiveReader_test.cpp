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

#include <QTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <archive/ArchiveReader.h>
#include <archive/ArchiveWriter.h>

class ArchiveReaderTest : public QObject {
    Q_OBJECT

   private slots:

    void extractHardLink()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        auto root = extractionRoot(temp);

        QVERIFY(extractTo(QFINDTESTDATA("testdata/ArchiveReader/hard-link.tar"), root));

        // bin/link is a hard link to bin/target, named relative to the archive root. It only resolves if
        // it was rebased onto the extraction directory - otherwise libarchive looks for it next to the
        // working directory, which is not where we just put bin/target.
        QFile link(root + "/bin/link");
        QVERIFY(link.exists());
        QVERIFY(link.open(QIODevice::ReadOnly));
        QCOMPARE(link.readAll(), QByteArrayLiteral("timeless\n"));
    }

    void refuseHardLinkOutsideRoot()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        auto root = extractionRoot(temp);

        QVERIFY(!extractTo(QFINDTESTDATA("testdata/ArchiveReader/escaping-hard-link.tar"), root));
        // bin/target sits before the link in the archive, so it proves extraction really got as far as
        // the link and stopped there, rather than failing earlier for some unrelated reason.
        QVERIFY(QFile::exists(root + "/bin/target"));
        QVERIFY(!QFile::exists(root + "/bin/link"));
    }

    void extractIntoLinkedFolder()
    {
#ifdef Q_OS_WIN
        QSKIP("Symbolic links need extra rights on Windows, and aren't resolved there");
#endif
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        // like a data folder moved to another drive and linked back
        QVERIFY(QDir(temp.path()).mkdir("elsewhere"));
        QVERIFY(QFile::link(temp.filePath("elsewhere"), temp.filePath("data")));

        QVERIFY(extractTo(QFINDTESTDATA("testdata/ArchiveReader/hard-link.tar"), temp.filePath("data/instance")));

        QFile link(temp.filePath("elsewhere/instance/bin/link"));
        QVERIFY(link.open(QIODevice::ReadOnly));
        QCOMPARE(link.readAll(), QByteArrayLiteral("timeless\n"));
    }

   private:
    /// The folder to extract to, as QTemporaryDir gives it: on macOS that is under /var, which is a symlink to /private/var
    static QString extractionRoot(const QTemporaryDir& temp) { return QDir(temp.path()).absolutePath(); }

    /// Unpacks every entry into `root` the way ExtractZipTask does, by handing each one an absolute
    /// target path inside the extraction directory.
    static bool extractTo(const QString& archivePath, const QString& root)
    {
        MMCZip::ArchiveReader reader(archivePath);
        auto writer = MMCZip::ArchiveWriter::createDiskWriter();
        QDir rootDir(root);

        return reader.parse([&](MMCZip::ArchiveReader::File* f) {
            auto target = rootDir.filePath(f->filename());
            QDir().mkpath(QFileInfo(target).absolutePath());
            return f->writeFile(writer.get(), target, rootDir);
        });
    }
};

QTEST_GUILESS_MAIN(ArchiveReaderTest)

#include "ArchiveReader_test.moc"
