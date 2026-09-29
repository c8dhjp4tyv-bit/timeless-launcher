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

#include <QDirIterator>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

#include <MMCZip.h>
#include <archive/ArchiveWriter.h>
#include <archive/ExtractZipTask.h>

class ExtractZipTest : public QObject {
    Q_OBJECT

    /// Every file under the folder, relative to it
    static QStringList filesIn(const QString& folder)
    {
        QStringList files;
        QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            files << QDir(folder).relativeFilePath(it.next());
        }
        files.sort();
        return files;
    }

    /// Writes an instance zip whose last file is damaged in a way that shows only once it is extracted: the headers of
    /// its files, which are all that is read before extracting, are fine.
    static void writeZipDamagedAtTheEnd(const QString& zipPath)
    {
        MMCZip::ArchiveWriter zip(zipPath);
        QVERIFY(zip.open());
        QVERIFY(zip.addFile("instance.cfg", QByteArray("[General]\n")));
        QVERIFY(zip.addFile("minecraft/options.txt", QByteArray("fov:70\n")));
        QVERIFY(zip.addFile("minecraft/servers.dat", QByteArray(4096, 'S')));
        QVERIFY(zip.close());

        QFile file(zipPath);
        QVERIFY(file.open(QIODevice::ReadWrite));
        const auto data = file.readAll();
        const auto uint16At = [&data](qsizetype offset) { return qFromLittleEndian<quint16>(data.mid(offset, 2).constData()); };
        // A file's header takes 30 bytes, then come its name, its extra fields and its data
        const auto header = data.indexOf("minecraft/servers.dat") - 30;
        QVERIFY(header > 0);
        QCOMPARE(uint16At(header + 8), quint16(8));  // deflated
        const auto dataStart = header + 30 + uint16At(header + 26) + uint16At(header + 28);
        // the data starts with a block of the reserved type, which can't be inflated
        QVERIFY(file.seek(dataStart));
        QCOMPARE(file.write("\xFF", 1), qint64(1));
        file.close();
    }

   private slots:
    void extractSubdirectory_data()
    {
        QTest::addColumn<QString>("subdirectory");
        QTest::newRow("folder name") << "Survival";
        QTest::newRow("folder name with slash") << "Survival/";
    }

    void extractSubdirectory()
    {
        QFETCH(QString, subdirectory);

        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        // an instance next to another folder whose name starts the same, as in a zipped instances folder
        const auto zipPath = temp.filePath("instances.zip");
        MMCZip::ArchiveWriter zip(zipPath);
        QVERIFY(zip.open());
        QVERIFY(zip.addFile("Survival/instance.cfg", QByteArray("[General]\n")));
        QVERIFY(zip.addFile("Survival/minecraft/options.txt", QByteArray("fov:70\n")));
        QVERIFY(zip.addFile("Survival 2/instance.cfg", QByteArray("[General]\n")));
        QVERIFY(zip.addFile("Survival 2/minecraft/other.txt", QByteArray("other\n")));
        QVERIFY(zip.close());

        const auto target = temp.filePath("extracted");
        MMCZip::ExtractZipTask task(zipPath, QDir(target), subdirectory);
        QSignalSpy finished(&task, &Task::finished);
        task.start();
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(task.wasSuccessful());

        QCOMPARE(filesIn(target), (QStringList{ "instance.cfg", "minecraft/options.txt" }));
    }

    void cutShort()
    {
        // An instance zip whose download stopped where its last file starts. None of it is extracted, rather than the files
        // before the cut as if they were all of it.
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const auto zipPath = temp.filePath("instance.zip");
        MMCZip::ArchiveWriter zip(zipPath);
        QVERIFY(zip.open());
        QVERIFY(zip.addFile("instance.cfg", QByteArray("[General]\n")));
        QVERIFY(zip.addFile("minecraft/options.txt", QByteArray("fov:70\n")));
        QVERIFY(zip.addFile("minecraft/servers.dat", QByteArray("servers\n")));
        QVERIFY(zip.close());

        QFile file(zipPath);
        QVERIFY(file.open(QIODevice::ReadWrite));
        // the header of a file starts 30 bytes before its name
        const auto lastHeader = file.readAll().indexOf("minecraft/servers.dat") - 30;
        QVERIFY(lastHeader > 0);
        QVERIFY(file.resize(lastHeader));
        file.close();

        const auto target = temp.filePath("extracted");
        MMCZip::ExtractZipTask task(zipPath, QDir(target));
        const QSignalSpy finished(&task, &Task::finished);
        task.start();
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!task.wasSuccessful());
        QVERIFY2(task.failReason().contains("instance.zip"), qPrintable(task.failReason()));

        QCOMPARE(filesIn(target), QStringList());
    }

    void damagedPartWay()
    {
        // The files extracted before the damaged one are removed again, and so is what got written of that one, rather
        // than leaving the start of an instance behind.
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto zipPath = temp.filePath("instance.zip");
        writeZipDamagedAtTheEnd(zipPath);
        if (QTest::currentTestFailed()) {
            return;
        }

        const auto target = temp.filePath("extracted");
        MMCZip::ExtractZipTask task(zipPath, QDir(target));
        const QSignalSpy finished(&task, &Task::finished);
        task.start();
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!task.wasSuccessful());
        QVERIFY2(task.failReason().contains("servers.dat"), qPrintable(task.failReason()));

        QCOMPARE(filesIn(target), QStringList());
    }

    void extractDirDamagedPartWay()
    {
        // the same for extracting a zip right away, as the updater does with a release
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto zipPath = temp.filePath("instance.zip");
        writeZipDamagedAtTheEnd(zipPath);
        if (QTest::currentTestFailed()) {
            return;
        }

        const auto target = temp.filePath("extracted");
        QVERIFY(!MMCZip::extractDir(zipPath, target).has_value());
        QCOMPARE(filesIn(target), QStringList());
    }
};

QTEST_GUILESS_MAIN(ExtractZipTest)

#include "ExtractZip_test.moc"
