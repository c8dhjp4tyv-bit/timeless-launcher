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

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <FileSystem.h>
#include <modplatform/helpers/OverrideUtils.h>

class OverridesTest : public QObject {
    Q_OBJECT

   private slots:
    void createOverrides_data()
    {
        QTest::addColumn<QString>("folder");

        QTest::newRow("folder named overrides") << "overrides";
        // CurseForge packs name the folder in their manifest
        QTest::newRow("folder named otherwise") << "Overrides";
    }

    // The list has the path of each file in the game folder, which is where a later update removes it from
    void createOverrides()
    {
        QFETCH(QString, folder);

        const QTemporaryDir staging;
        QVERIFY(staging.isValid());
        const auto overrides = FS::PathCombine(staging.path(), folder);
        QStringList files{ "options.txt", "config/overrides.json", "config/moreoverrides/settings.toml" };
        for (const auto& file : files) {
            const auto path = FS::PathCombine(overrides, file);
            QVERIFY(FS::ensureFilePathExists(path));
            QFile out(path);
            QVERIFY(out.open(QFile::WriteOnly));
        }

        const auto packFolder = FS::PathCombine(staging.path(), "flame");
        Override::createOverrides("overrides", packFolder, overrides);

        auto listed = Override::readOverrides("overrides", packFolder);
        listed.removeAll(QString());
        listed.sort();
        files.sort();
        QCOMPARE(listed, files);
    }

    void mergeGameOptions_data()
    {
        QTest::addColumn<QString>("player");
        QTest::addColumn<QString>("oldPack");
        QTest::addColumn<QString>("newPack");
        QTest::addColumn<QString>("merged");

        QTest::newRow("player's changes and the pack's")
            << "version:3465\nfov:0.5\nrenderDistance:12\nguiScale:2\nkey_key.jump:key.keyboard.space\n"
            << "renderDistance:8\nguiScale:2\nfov:0.0\n"
            << "renderDistance:10\nguiScale:3\nfov:0.0\nkey_key.examplemod.open:key.keyboard.k\n"
            << "version:3465\nfov:0.5\nrenderDistance:12\nguiScale:3\nkey_key.jump:key.keyboard.space\n"
               "key_key.examplemod.open:key.keyboard.k\n";

        // as for instances installed before the pack's options were kept
        QTest::newRow("installed version's options unknown") << "renderDistance:8\nguiScale:2\n"
                                                             << "" << "renderDistance:10\nfov:0.0\n"
                                                             << "renderDistance:8\nguiScale:2\nfov:0.0\n";

        QTest::newRow("values with colons") << "lastServer:play.example.com:25565\nresourcePacks:[\"vanilla\",\"file/A.zip\"]\n"
                                            << "lastServer:\nresourcePacks:[\"vanilla\",\"file/A.zip\"]\n"
                                            << "lastServer:mc.example.net:25565\nresourcePacks:[\"vanilla\",\"file/B.zip\"]\n"
                                            << "lastServer:play.example.com:25565\nresourcePacks:[\"vanilla\",\"file/B.zip\"]\n";

        QTest::newRow("Windows line breaks") << "renderDistance:12\r\nfov:0.5\r\n"
                                             << "renderDistance:8\r\nfov:0.5\r\n"
                                             << "renderDistance:10\r\nfov:0.0\r\n"
                                             << "renderDistance:12\nfov:0.0\n";

        // the pack no longer setting an option doesn't take it from the player
        QTest::newRow("option the pack dropped") << "fov:0.0\nguiScale:2\n"
                                                 << "fov:0.0\nguiScale:2\n"
                                                 << "guiScale:3\n"
                                                 << "fov:0.0\nguiScale:3\n";

        QTest::newRow("empty options of the player") << "" << "" << "renderDistance:10\n"
                                                     << "renderDistance:10\n";
    }

    void mergeGameOptions()
    {
        QFETCH(QString, player);
        QFETCH(QString, oldPack);
        QFETCH(QString, newPack);
        QFETCH(QString, merged);

        QCOMPARE(Override::mergeGameOptions(player, oldPack, newPack), merged);
    }

    void keepGameOptions()
    {
        const QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto gameRoot = FS::PathCombine(root.path(), "staging", "minecraft");
        const auto packFolder = FS::PathCombine(root.path(), "staging", "mrpack");
        const auto oldGameRoot = FS::PathCombine(root.path(), "instance", "minecraft");
        const auto oldPackFolder = FS::PathCombine(root.path(), "instance", "mrpack");
        const auto options = FS::PathCombine(gameRoot, "options.txt");
        const auto packCopy = FS::PathCombine(packFolder, "options.txt");

        // installing keeps the pack's options aside, and leaves them as they are
        FS::write(options, "renderDistance:8\nguiScale:2\n");
        Override::keepGameOptions(gameRoot, packFolder);
        QCOMPARE(FS::read(options), "renderDistance:8\nguiScale:2\n");
        QCOMPARE(FS::read(packCopy), "renderDistance:8\nguiScale:2\n");

        // updating merges the player's options into the update's
        FS::write(FS::PathCombine(oldGameRoot, "options.txt"), "renderDistance:16\r\nguiScale:2\r\n");
        FS::write(FS::PathCombine(oldPackFolder, "options.txt"), "renderDistance:8\nguiScale:2\n");
        FS::write(options, "renderDistance:10\nguiScale:3\n");
        Override::keepGameOptions(gameRoot, packFolder, oldGameRoot, oldPackFolder);
        QCOMPARE(FS::read(options), "renderDistance:16\nguiScale:3\n");
        QCOMPARE(FS::read(packCopy), "renderDistance:10\nguiScale:3\n");

        // an update without options keeps the player's as they are, and what the installed version kept isn't kept for it
        QVERIFY(QFile::remove(options));
        Override::keepGameOptions(gameRoot, packFolder, oldGameRoot, oldPackFolder);
        QCOMPARE(FS::read(options), "renderDistance:16\r\nguiScale:2\r\n");
        QCOMPARE(FS::read(packCopy), "");

        // a player without options gets the update's
        QVERIFY(QFile::remove(FS::PathCombine(oldGameRoot, "options.txt")));
        FS::write(options, "renderDistance:10\n");
        Override::keepGameOptions(gameRoot, packFolder, oldGameRoot, oldPackFolder);
        QCOMPARE(FS::read(options), "renderDistance:10\n");
    }
};

QTEST_GUILESS_MAIN(OverridesTest)

#include "Overrides_test.moc"
