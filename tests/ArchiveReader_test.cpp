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

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QTemporaryDir>

#include <archive/ArchiveReader.h>
#include <archive/ArchiveWriter.h>

class ArchiveReaderTest : public QObject {
    Q_OBJECT

   private slots:
    void readWhole()
    {
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto path = temp.filePath("pack.zip");
        QVERIFY(writeFile(path, packZip()));

        MMCZip::ArchiveReader reader(path);
        QVERIFY(reader.collectFiles());
        QCOMPARE(reader.getFiles(),
                 (QStringList{ "modrinth.index.json", "overrides/config/a.dat", "overrides/config/b.dat", "overrides/config/c.dat" }));
        QVERIFY(reader.errorString().isEmpty());
    }

    void cutShort_data()
    {
        QTest::addColumn<QString>("where");
        QTest::newRow("in a file's header") << "header";
        QTest::newRow("between two files") << "between";
        QTest::newRow("in a file's data") << "data";
    }

    void cutShort()
    {
        // Only the files before the cut can be read, which used to pass for all of them unless the cut fell in a file's data
        QFETCH(const QString, where);

        const auto whole = packZip();
        QVERIFY(!whole.isEmpty());
        // the third file's header starts where the second one's data, and its size and checksum after that, end
        const auto second = headerOf(whole, "overrides/config/a.dat");
        const auto third = headerOf(whole, "overrides/config/b.dat");
        QVERIFY(second > 0 && third > second + 4096);
        auto cutAt = (second + third) / 2;
        if (where == "header") {
            cutAt = third + 10;
        } else if (where == "between") {
            cutAt = third;
        }

        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto path = temp.filePath("pack.zip");
        QVERIFY(writeFile(path, whole.first(cutAt)));

        MMCZip::ArchiveReader reader(path);
        QVERIFY(!reader.collectFiles());
        QVERIFY(reader.endedEarly());
        QVERIFY(!reader.errorString().isEmpty());
        // what a pack's list reads from, as the files before the cut are all there is to name it by
        QCOMPARE(reader.getFiles(), (QStringList{ "modrinth.index.json", "overrides/config/a.dat" }));
    }

    void stopBeforeTheCut()
    {
        // reading only the start, as for a mod's metadata, doesn't need the rest
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto whole = packZip();
        const auto path = temp.filePath("pack.zip");
        QVERIFY(writeFile(path, whole.first(whole.size() / 2)));

        MMCZip::ArchiveReader reader(path);
        QStringList read;
        QVERIFY(reader.parse([&read](MMCZip::ArchiveReader::File* f, bool& stop) {
            read << f->filename();
            stop = true;
            return f->skip();
        }));
        QCOMPARE(read, QStringList{ "modrinth.index.json" });
    }

    void notAnArchive()
    {
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto path = temp.filePath("pack.zip");
        QVERIFY(writeFile(path, QByteArray("<!DOCTYPE html><html><body>Your download will start shortly.</body></html>")));

        MMCZip::ArchiveReader reader(path);
        QVERIFY(!reader.collectFiles());
        QVERIFY(reader.getFiles().isEmpty());
        QVERIFY(!reader.errorString().isEmpty());
        // there's nothing it was cut from
        QVERIFY(!reader.endedEarly());
    }

    void readPastAHeaderWithAWarning()
    {
        // The zip's list of its files at the end gives the first one a checksum that its own header doesn't. libarchive warns of
        // that and reads it all the same, and reading used to stop right there, as if the zip held nothing.
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto path = temp.filePath("mod.jar");
        QVERIFY(writeFile(path, storedZip({ { "fabric.mod.json", "{}" }, { "assets/example/icon.png", "icon" } }, 0)));

        MMCZip::ArchiveReader reader(path);
        QMap<QString, QByteArray> read;
        QVERIFY(reader.parse([&read](MMCZip::ArchiveReader::File* f) {
            int status = 0;
            read.insert(f->filename(), f->readAll(&status));
            return status >= 0;
        }));
        QCOMPARE(read, (QMap<QString, QByteArray>{ { "fabric.mod.json", "{}" }, { "assets/example/icon.png", "icon" } }));

        QVERIFY(reader.goToFile("assets/example/icon.png"));
    }

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
    static bool writeFile(const QString& path, const QByteArray& contents)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
    }

    /// Where the header of the file with that name starts in the zip, which is 30 bytes before its name
    static qsizetype headerOf(const QByteArray& zip, const QByteArray& name)
    {
        const auto at = zip.indexOf(name);
        return at < 30 ? -1 : at - 30;
    }

    /// A modpack as the launcher writes zips, deflated with each file's size and checksum after its data, and with files big
    /// enough for a cut to fall well inside them
    static QByteArray packZip()
    {
        const QTemporaryDir temp;
        const auto path = temp.filePath("pack.zip");
        MMCZip::ArchiveWriter zip(path);
        if (!zip.open() || !zip.addFile("modrinth.index.json", QByteArray(R"({"formatVersion": 1})"))) {
            return {};
        }
        // random bytes don't compress, and a fixed seed makes them the same every time
        QRandomGenerator random(42);
        for (const auto* name : { "overrides/config/a.dat", "overrides/config/b.dat", "overrides/config/c.dat" }) {
            QByteArray data(4096, Qt::Uninitialized);
            for (auto& byte : data) {
                byte = static_cast<char>(random.bounded(256));
            }
            if (!zip.addFile(name, data)) {
                return {};
            }
        }
        if (!zip.close()) {
            return {};
        }
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    /// The checksum zip gives each file, CRC-32
    static quint32 crc32Of(const QByteArray& data)
    {
        quint32 crc = 0xFFFFFFFFU;
        for (const auto byte : data) {
            crc ^= static_cast<quint8>(byte);
            for (int bit = 0; bit < 8; bit++) {
                crc = (crc & 1U) != 0 ? (crc >> 1U) ^ 0xEDB88320U : crc >> 1U;
            }
        }
        return ~crc;
    }

    /// A zip of stored files as many tools write them, with each file's size and checksum in its own header as well as in the
    /// list of files at the end, where the file at `inconsistent` gets a checksum its header disagrees with
    static QByteArray storedZip(const QList<std::pair<QByteArray, QByteArray>>& files, qsizetype inconsistent = -1)
    {
        QByteArray zip;
        QByteArray list;
        QDataStream out(&zip, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        QDataStream listOut(&list, QIODevice::WriteOnly);
        listOut.setByteOrder(QDataStream::LittleEndian);
        for (qsizetype i = 0; i < files.size(); i++) {
            const auto& [name, data] = files.at(i);
            const auto crc = crc32Of(data);
            const auto size = static_cast<quint32>(data.size());
            const auto offset = static_cast<quint32>(zip.size());
            // version, flags, stored, time and date (1 January 1980)
            out << quint32{ 0x04034b50 } << quint16{ 20 } << quint16{ 0 } << quint16{ 0 } << quint16{ 0 } << quint16{ 0x21 };
            out << crc << size << size << static_cast<quint16>(name.size()) << quint16{ 0 };
            out.writeRawData(name.constData(), static_cast<int>(name.size()));
            out.writeRawData(data.constData(), static_cast<int>(data.size()));

            listOut << quint32{ 0x02014b50 } << quint16{ 20 } << quint16{ 20 } << quint16{ 0 } << quint16{ 0 } << quint16{ 0 }
                    << quint16{ 0x21 };
            listOut << (i == inconsistent ? ~crc : crc) << size << size << static_cast<quint16>(name.size()) << quint16{ 0 }
                    << quint16{ 0 };
            // disk, attributes, and where the file's header is
            listOut << quint16{ 0 } << quint16{ 0 } << quint32{ 0 } << offset;
            listOut.writeRawData(name.constData(), static_cast<int>(name.size()));
        }
        const auto listOffset = static_cast<quint32>(zip.size());
        out.writeRawData(list.constData(), static_cast<int>(list.size()));
        const auto count = static_cast<quint16>(files.size());
        out << quint32{ 0x06054b50 } << quint16{ 0 } << quint16{ 0 } << count << count << static_cast<quint32>(list.size()) << listOffset
            << quint16{ 0 };
        return zip;
    }

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
