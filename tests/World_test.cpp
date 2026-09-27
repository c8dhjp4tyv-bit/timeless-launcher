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

#include <QCoreApplication>
#include <QDataStream>
#include <QEventLoop>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QThreadPool>
#include <QTimer>

#include <limits>
#include <sstream>

#include <io/stream_writer.h>
#include <nbt_tags.h>

#include <FileSystem.h>
#include <GZip.h>
#include <archive/ArchiveReader.h>
#include <archive/ArchiveWriter.h>
#include <archive/ExportToZipTask.h>
#include <minecraft/World.h>
#include <minecraft/WorldBackups.h>
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

    /// The values one after the other, big-endian as NBT has them
    template <typename... Values>
    static QByteArray bigEndian(Values... values)
    {
        QByteArray bytes;
        QDataStream out(&bytes, QIODevice::WriteOnly);
        (out << ... << values);
        return bytes;
    }

    static QString nameOf(const QString& worldFolder)
    {
        World world{ QFileInfo(worldFolder) };
        world.loadMetadata();
        return world.name();
    }

    static QStringList fileNames(const QFileInfoList& files)
    {
        QStringList names;
        for (const auto& file : files) {
            names << file.fileName();
        }
        return names;
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

    void installCutShortZip()
    {
        // A world zip whose download stopped where its last region file starts. Nothing is installed, rather than a world that
        // is missing part of its map.
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const auto zipPath = temp.filePath("download.zip");
        MMCZip::ArchiveWriter zip(zipPath);
        QVERIFY(zip.open());
        QVERIFY(zip.addFile("MyWorld/level.dat", levelDat("My World")));
        QVERIFY(zip.addFile("MyWorld/region/r.0.0.mca", QByteArray("region")));
        QVERIFY(zip.addFile("MyWorld/region/r.0.1.mca", QByteArray("region")));
        QVERIFY(zip.close());

        QFile file(zipPath);
        QVERIFY(file.open(QIODevice::ReadWrite));
        // the header of a file starts 30 bytes before its name
        const auto lastHeader = file.readAll().indexOf("MyWorld/region/r.0.1.mca") - 30;
        QVERIFY(lastHeader > 0);
        QVERIFY(file.resize(lastHeader));
        file.close();

        const auto saves = temp.filePath("saves");
        QVERIFY(FS::ensureFolderPathExists(saves));

        World world{ QFileInfo(zipPath) };
        QVERIFY(!world.install(saves));
        QVERIFY(QDir(saves).isEmpty());
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

        // or under another name, as restoring it from the Worlds page does
        const auto restored = temp.filePath("restored");
        QVERIFY(World{ zip }.install(restored, "My World (backup)"));
        QCOMPARE(nameOf(FS::PathCombine(restored, "My World (backup)")), "My World (backup)");
        QCOMPARE(nameOf(folder), "My World");

        // another one right after doesn't replace it, even within the same second
        auto again = worlds.createBackupWorldTask(0);
        QVERIFY(again);
        QVERIFY(!QFileInfo::exists(again->outputPath()));

        // the list reads the worlds' details on other threads, which have to be done before it goes away
        QThreadPool::globalInstance()->waitForDone();
    }

    void copyToAnotherInstance()
    {
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const auto ours = temp.filePath("ours/saves");
        const auto theirs = temp.filePath("theirs/saves");
        FS::write(FS::PathCombine(ours, "MyWorld", "level.dat"), levelDat("My World"));
        FS::write(FS::PathCombine(ours, "MyWorld", "region", "r.0.0.mca"), "region");
        // the other instance already has a world in a folder with that name
        FS::write(FS::PathCombine(theirs, "My World", "level.dat"), levelDat("Their World"));

        WorldList source(ours, nullptr);
        WorldList target(theirs, nullptr);
        QVERIFY(source.update());
        QVERIFY(target.update());

        auto task = source.createCopyWorldTask(0, "My World", &target);
        QVERIFY(task);
        QVERIFY(run(*task));

        // the copy is in a folder of its own among the other instance's saves, which the list already shows
        QCOMPARE(target.size(), 2);
        auto folders = QDir(theirs).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        folders.removeOne("My World");
        QCOMPARE(folders.size(), 1);
        QCOMPARE(nameOf(FS::PathCombine(theirs, folders.first())), "My World");
        QCOMPARE(FS::read(FS::PathCombine(theirs, folders.first(), "region", "r.0.0.mca")), QByteArray("region"));

        // and nothing else changed
        QCOMPARE(nameOf(FS::PathCombine(theirs, "My World")), "Their World");
        QCOMPARE(source.size(), 1);
        QCOMPARE(nameOf(FS::PathCombine(ours, "MyWorld")), "My World");

        QThreadPool::globalInstance()->waitForDone();
    }

    void detailsLoadInTheBackground()
    {
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto saves = temp.filePath("saves");
        const auto level = levelDat("My World");
        FS::write(FS::PathCombine(saves, "MyWorld", "level.dat"), level);
        FS::write(FS::PathCombine(saves, "MyWorld", "region", "r.0.0.mca"), "region");

        WorldList worlds(saves, nullptr);
        QVERIFY(worlds.update());
        // the size is worked out on another thread, and filled in once it's done
        QTRY_COMPARE(worlds.allWorlds().first().bytes(), level.size() + 6);
        QThreadPool::globalInstance()->waitForDone();
    }

    void listGoneBeforeItsWorldsAreRead()
    {
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto saves = temp.filePath("saves");
        for (int i = 0; i < 20; i++) {
            FS::write(FS::PathCombine(saves, QString("World%1").arg(i), "level.dat"), levelDat(QString("World %1").arg(i)));
        }
        {
            WorldList worlds(saves, nullptr);
            QVERIFY(worlds.update());
        }
        // what the threads found out for the list goes nowhere
        QThreadPool::globalInstance()->waitForDone();
        QCoreApplication::processEvents();
    }

    void backupWhenChanged()
    {
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const auto saves = temp.filePath("saves");
        const QFileInfo world(FS::PathCombine(saves, "MyWorld"));
        const auto levelDatPath = FS::PathCombine(world.absoluteFilePath(), "level.dat");
        FS::write(levelDatPath, levelDat("My World"));
        const auto automatic = WorldBackups::automaticBackupDir(saves);
        QCOMPARE(automatic, QDir::cleanPath(temp.filePath("backups/automatic")));

        // a world without a backup needs one
        QVERIFY(WorldBackups::changedSinceBackup(world, automatic));
        auto task = WorldBackups::createTask(world, automatic);
        QVERIFY(task);
        QVERIFY(run(*task));

        // and doesn't once it has one, until the game saves it again
        QVERIFY(!WorldBackups::changedSinceBackup(world, automatic));
        QFile levelDatFile(levelDatPath);
        QVERIFY(levelDatFile.open(QIODevice::ReadWrite));
        QVERIFY(levelDatFile.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
        levelDatFile.close();
        QVERIFY(WorldBackups::changedSinceBackup(world, automatic));
    }

    void removeOldBackups()
    {
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const QStringList backups = {
            "2025-12-30_23-59-59_New World.zip", "2026-01-01_10-00-00_New World.zip", "2026-01-02_10-00-00_New World.zip",
            "2026-01-03_10-00-00_New World.zip", "2026-01-04_10-00-00_New World.zip", "2026-01-05_10-00-00_New World.zip",
            "2026-01-06_10-00-00_New World.zip",
        };
        // the game names a second world called New World "New World (1)", and those are its backups, not the first one's
        const QStringList others = {
            "2025-01-01_10-00-00_New World (1).zip", "2026-01-02_10-00-00_New World (1).zip",
            "2026-01-01_10-00-00_New World 2.zip",   "New World.zip",
            "2026-01-01_10-00-00_New World.zip.txt",
        };
        for (const auto& name : backups + others) {
            FS::write(FS::PathCombine(temp.path(), name), "zip");
        }

        QCOMPARE(fileNames(WorldBackups::backupsOf(temp.path(), "New World")), backups);
        QCOMPARE(WorldBackups::removeOldBackups(temp.path(), "New World", 5), backups.mid(0, 2));
        QCOMPARE(fileNames(WorldBackups::backupsOf(temp.path(), "New World")), backups.mid(2));
        for (const auto& name : others) {
            QVERIFY2(QFileInfo::exists(FS::PathCombine(temp.path(), name)), qPrintable(name));
        }
        QCOMPARE(fileNames(WorldBackups::backupsOf(temp.path(), "New World (1)")), others.mid(0, 2));

        // the newest one always stays
        QCOMPARE(WorldBackups::removeOldBackups(temp.path(), "New World", 0).size(), 4);
        QCOMPARE(fileNames(WorldBackups::backupsOf(temp.path(), "New World")), QStringList{ backups.last() });
    }

    void listBackups()
    {
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const auto saves = temp.filePath("saves");
        const auto manual = WorldBackups::backupDir(saves);
        const auto automatic = WorldBackups::automaticBackupDir(saves);
        FS::write(FS::PathCombine(manual, "2026-03-01_12-00-00_MyWorld.zip"), "zip");
        FS::write(FS::PathCombine(automatic, "2026-02-01_12-00-00_MyWorld.zip"), "zip");
        FS::write(FS::PathCombine(automatic, "2026-04-01_08-30-00_MyWorld.zip"), "zip");
        FS::write(FS::PathCombine(manual, "2026-05-01_12-00-00_Other.zip"), "zip");

        // newest first, whichever folder they are in
        const auto backups = WorldBackups::allBackupsOf(saves, "MyWorld");
        QCOMPARE(backups.size(), 3);
        QCOMPARE(backups.at(0).file.fileName(), "2026-04-01_08-30-00_MyWorld.zip");
        QCOMPARE(backups.at(0).made, QDateTime(QDate(2026, 4, 1), QTime(8, 30)));
        QVERIFY(backups.at(0).automatic);
        QCOMPARE(backups.at(1).file.fileName(), "2026-03-01_12-00-00_MyWorld.zip");
        QVERIFY(!backups.at(1).automatic);
        QCOMPARE(backups.at(2).file.fileName(), "2026-02-01_12-00-00_MyWorld.zip");
        QVERIFY(backups.at(2).automatic);

        QVERIFY(WorldBackups::allBackupsOf(saves, "Nothing").isEmpty());
    }

    void levelDatClaimingMoreThanItHolds_data()
    {
        // an entry of the world's Data, as its type and what follows its name
        QTest::addColumn<quint8>("type");
        QTest::addColumn<QByteArray>("payload");

        const auto twoBillion = std::numeric_limits<qint32>::max();
        QTest::newRow("list of ints") << quint8(9) << bigEndian(quint8(3), twoBillion);
        QTest::newRow("byte array") << quint8(7) << bigEndian(twoBillion);
        QTest::newRow("int array") << quint8(11) << bigEndian(twoBillion);
        QTest::newRow("long array") << quint8(12) << bigEndian(twoBillion);
        QTest::newRow("list in a list") << quint8(9) << bigEndian(quint8(9), qint32(1), quint8(3), twoBillion);
    }

    void levelDatClaimingMoreThanItHolds()
    {
        // A level.dat with an entry that claims two billion values and holds none, as a damaged or made-up one can. The NBT
        // library would set aside room for all of them before reading any, and read an int array's on past the end of the
        // data. Written by hand, since a list or array can't be made to claim more than it holds.
        QFETCH(const quint8, type);
        QFETCH(const QByteArray, payload);
        QByteArray raw = bigEndian(quint8(10), quint16(0));  // the compound around everything, without a name
        raw += bigEndian(quint8(10), quint16(4)) + "Data";
        raw += bigEndian(type, quint16(4)) + "Mods" + payload;

        const QTemporaryDir saves;
        QVERIFY(saves.isValid());
        const auto folder = saves.filePath("World");
        QByteArray compressed;
        QVERIFY(GZip::zip(raw, compressed));
        FS::write(FS::PathCombine(folder, "level.dat"), compressed);

        World world{ QFileInfo(folder) };
        world.loadMetadata();
        QVERIFY(!world.isValid());
    }

    void levelDatWithArraysAndLists()
    {
        // arrays and lists of all kinds, as the game and mods write them, which go on being read
        nbt::tag_compound data;
        data.put("LevelName", nbt::tag_string("Big World"));
        data.put("LastPlayed", nbt::tag_long(1700000000000));
        data.put("GameType", nbt::tag_int(1));
        data.put("Difficulty", nbt::tag_byte(2));
        data.put("SpawnAngle", nbt::tag_float(90.0F));
        data.put("BorderSize", nbt::tag_double(59999968.0));
        data.put("Version", nbt::tag_short(19133));
        data.put("Bytes", nbt::tag_byte_array{ 1, 2, 3 });
        data.put("Ints", nbt::tag_int_array{ 1, 2, 3 });
        data.put("Longs", nbt::tag_long_array{ 1, 2 });
        nbt::tag_compound player;
        player.put("Name", nbt::tag_string("Steve"));
        player.put("Pos", nbt::tag_list{ 1.5, 64.0, -3.5 });
        data.put("Players", nbt::tag_list{ player, player });
        data.put("Lists", nbt::tag_list{ nbt::tag_list{ int32_t(1), int32_t(2) }, nbt::tag_list() });
        data.put("Nothing", nbt::tag_list());  // written as a list of End
        nbt::tag_compound root;
        root.put("Data", std::move(data));

        std::ostringstream stream;
        nbt::io::write_tag("", root, stream);
        QByteArray compressed;
        QVERIFY(GZip::zip(QByteArray::fromStdString(stream.str()), compressed));

        const QTemporaryDir saves;
        QVERIFY(saves.isValid());
        const auto folder = saves.filePath("World");
        FS::write(FS::PathCombine(folder, "level.dat"), compressed);

        World world{ QFileInfo(folder) };
        world.loadMetadata();
        QVERIFY(world.isValid());
        QCOMPARE(world.name(), "Big World");
        QCOMPARE(world.lastPlayed(), QDateTime::fromMSecsSinceEpoch(1700000000000));
        QCOMPARE(world.gameType().type, GameType::Creative);
    }

    void renameWithoutLevelData()
    {
        // a level.dat that can be read, but doesn't have the Data the world's name is in
        nbt::tag_compound root;
        root.put("Other", nbt::tag_int(1));
        std::ostringstream stream;
        nbt::io::write_tag("", root, stream);
        QByteArray compressed;
        QVERIFY(GZip::zip(QByteArray::fromStdString(stream.str()), compressed));

        const QTemporaryDir saves;
        QVERIFY(saves.isValid());
        const auto folder = saves.filePath("World");
        FS::write(FS::PathCombine(folder, "level.dat"), compressed);

        World world{ QFileInfo(folder) };
        QVERIFY(!world.rename("Renamed"));
        QCOMPARE(FS::read(FS::PathCombine(folder, "level.dat")), compressed);
    }
};

QTEST_GUILESS_MAIN(WorldTest)

#include "World_test.moc"
