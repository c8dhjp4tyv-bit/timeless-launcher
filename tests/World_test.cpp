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

#include <QEventLoop>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QThreadPool>
#include <QTimer>

#include <sstream>

#include <io/stream_writer.h>
#include <nbt_tags.h>

#include <FileSystem.h>
#include <GZip.h>
#include <archive/ArchiveReader.h>
#include <archive/ArchiveWriter.h>
#include <archive/ExportToZipTask.h>
#include <minecraft/World.h>
#include <minecraft/WorldList.h>

class WorldTest : public QObject {
    Q_OBJECT

    /// A gzipped level.dat that only holds the world's name, which is all a World needs to be valid.
    static QByteArray levelDat(const QString& levelName)
    {
        nbt::tag_compound data;
        data.put("LevelName", nbt::tag_string(levelName.toStdString()));
        nbt::tag_compound root;
        root.put("Data", std::move(data));

        std::ostringstream stream;
        nbt::io::write_tag("", root, stream);
        const auto raw = QByteArray::fromStdString(stream.str());

        QByteArray compressed;
        GZip::zip(raw, compressed);
        return compressed;
    }

    static QString nameOf(const QString& worldFolder)
    {
        World world{ QFileInfo(worldFolder) };
        world.loadMetadata();
        return world.name();
    }

    /// Runs the task to its end, and tells whether it succeeded
    static bool run(Task& task)
    {
        QEventLoop loop;
        connect(&task, &Task::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        task.start();
        if (!task.isFinished()) {
            loop.exec();
        }
        return task.wasSuccessful();
    }

   private slots:
    void installFromZip()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        // a world folder zipped up, the usual way worlds are shared
        const auto zipPath = temp.filePath("download.zip");
        MMCZip::ArchiveWriter zip(zipPath);
        QVERIFY(zip.open());
        QVERIFY(zip.addFile("MyWorld/level.dat", levelDat("My World")));
        QVERIFY(zip.addFile("MyWorld/region/r.0.0.mca", QByteArray("region")));
        QVERIFY(zip.close());

        const auto saves = temp.filePath("saves");
        QVERIFY(FS::ensureFolderPathExists(saves));

        World world{ QFileInfo(zipPath) };
        QVERIFY(world.isValid());
        QVERIFY(world.install(saves));

        // named after the world, with its level.dat right inside rather than one folder further down
        QVERIFY(QFileInfo::exists(FS::PathCombine(saves, "My World", "level.dat")));
        QVERIFY(QFileInfo::exists(FS::PathCombine(saves, "My World", "region", "r.0.0.mca")));
        QCOMPARE(QDir(saves).entryList(QDir::Dirs | QDir::NoDotAndDotDot), QStringList{ "My World" });
    }

    void installZipWithoutWorld()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const auto zipPath = temp.filePath("not-a-world.zip");
        MMCZip::ArchiveWriter zip(zipPath);
        QVERIFY(zip.open());
        QVERIFY(zip.addFile("readme.txt", QByteArray("hello")));
        QVERIFY(zip.close());

        const auto saves = temp.filePath("saves");
        QVERIFY(FS::ensureFolderPathExists(saves));

        World world{ QFileInfo(zipPath) };
        QVERIFY(!world.install(saves));
        QVERIFY(QDir(saves).isEmpty());
    }

    void copyWithNewName()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const auto saves = temp.filePath("saves");
        const auto original = FS::PathCombine(saves, "MyWorld");
        FS::write(FS::PathCombine(original, "level.dat"), levelDat("My World"));

        World world{ QFileInfo(original) };
        QVERIFY(world.install(saves, "Backup"));

        QCOMPARE(nameOf(FS::PathCombine(saves, "Backup")), "Backup");
        // the original is left as it was
        QCOMPARE(nameOf(original), "My World");
    }

    void backup()
    {
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const auto saves = temp.filePath("saves");
        const auto folder = FS::PathCombine(saves, "MyWorld");
        FS::write(FS::PathCombine(folder, "level.dat"), levelDat("My World"));
        FS::write(FS::PathCombine(folder, "region", "r.0.0.mca"), "region");
        // held by the game while the world is open
        FS::write(FS::PathCombine(folder, "session.lock"), "lock");

        WorldList worlds(saves, nullptr);
        QVERIFY(worlds.update());
        QCOMPARE(worlds.size(), 1);

        auto task = worlds.createBackupWorldTask(0);
        QVERIFY(task);
        // where the game puts its own backups, and named the same way
        const QFileInfo zip(task->outputPath());
        QCOMPARE(zip.absolutePath(), QDir(temp.filePath("backups")).absolutePath());
        static const QRegularExpression s_backupName(R"(^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}_MyWorld\.zip$)");
        QVERIFY2(s_backupName.match(zip.fileName()).hasMatch(), qPrintable(zip.fileName()));

        QVERIFY(run(*task));
        MMCZip::ArchiveReader reader(zip.absoluteFilePath());
        QVERIFY(reader.collectFiles());
        auto entries = reader.getFiles();
        entries.sort();
        QCOMPARE(entries, (QStringList{ "MyWorld/level.dat", "MyWorld/region/r.0.0.mca" }));

        // adding it restores a copy next to the world
        World backup{ zip };
        QVERIFY(backup.isValid());
        QVERIFY(backup.install(saves));
        QCOMPARE(nameOf(FS::PathCombine(saves, "My World")), "My World");
        QCOMPARE(FS::read(FS::PathCombine(saves, "My World", "region", "r.0.0.mca")), QByteArray("region"));

        // another one right after doesn't replace it, even within the same second
        auto again = worlds.createBackupWorldTask(0);
        QVERIFY(again);
        QVERIFY(!QFileInfo::exists(again->outputPath()));

        // the list reads the worlds' details on other threads, which have to be done before it goes away
        QThreadPool::globalInstance()->waitForDone();
    }
};

QTEST_GUILESS_MAIN(WorldTest)

#include "World_test.moc"
