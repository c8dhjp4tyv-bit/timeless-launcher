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
};

QTEST_GUILESS_MAIN(ExtractZipTest)

#include "ExtractZip_test.moc"
