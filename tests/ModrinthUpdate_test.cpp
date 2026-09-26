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

#include <QTemporaryDir>
#include <QTest>

#include <FileSystem.h>
#include <modplatform/modrinth/ModrinthInstanceCreationTask.h>

using File = ModrinthCreationTask::File;

class ModrinthUpdateTest : public QObject {
    Q_OBJECT

    static File packFile(const QString& path, const QString& url)
    {
        File file;
        file.path = path;
        file.downloads.enqueue(QUrl(url));
        return file;
    }

   private slots:
    void keepEnabledStates()
    {
        const QTemporaryDir gameRoot;
        QVERIFY(gameRoot.isValid());
        // what the player has: a mod of the pack turned off, an optional one turned on, one left on, and one taken out
        for (const auto* file : { "mods/alpha-1.0.jar.disabled", "mods/beta-1.0.jar", "mods/delta-1.0.jar", "mods/gamma.jar.disabled" }) {
            FS::write(FS::PathCombine(gameRoot.path(), file), "");
        }

        const std::vector<File> installed{
            packFile("mods/alpha-1.0.jar", "https://cdn.modrinth.com/data/AAAA1111/versions/v1/alpha-1.0.jar"),
            // optional files the pack comes with start out off
            packFile("mods/beta-1.0.jar.disabled", "https://cdn.modrinth.com/data/BBBB2222/versions/v1/beta-1.0.jar"),
            packFile("mods/delta-1.0.jar", "https://cdn.modrinth.com/data/DDDD4444/versions/v1/delta-1.0.jar"),
            packFile("mods/gamma.jar", "https://github.com/example/gamma/releases/download/1.0/gamma.jar"),
            packFile("mods/epsilon-1.0.jar", "https://cdn.modrinth.com/data/EEEE5555/versions/v1/epsilon-1.0.jar"),
        };
        std::vector<File> update{
            packFile("mods/alpha-1.1.jar", "https://cdn.modrinth.com/data/AAAA1111/versions/v2/alpha-1.1.jar"),
            packFile("mods/beta-1.1.jar.disabled", "https://cdn.modrinth.com/data/BBBB2222/versions/v2/beta-1.1.jar"),
            packFile("mods/delta-1.1.jar", "https://cdn.modrinth.com/data/DDDD4444/versions/v2/delta-1.1.jar"),
            // not from Modrinth, so matched by its path
            packFile("mods/gamma.jar", "https://github.com/example/gamma/releases/download/1.1/gamma.jar"),
            packFile("mods/epsilon-1.1.jar", "https://cdn.modrinth.com/data/EEEE5555/versions/v2/epsilon-1.1.jar"),
            packFile("mods/zeta-1.0.jar", "https://cdn.modrinth.com/data/ZZZZ9999/versions/v1/zeta-1.0.jar"),
            packFile("mods/eta-1.0.jar.disabled", "https://cdn.modrinth.com/data/HHHH8888/versions/v1/eta-1.0.jar"),
        };

        ModrinthCreationTask::keepEnabledStates(installed, update, QDir(gameRoot.path()));

        QStringList paths;
        for (const auto& file : update) {
            paths << file.path;
        }
        QCOMPARE(paths, QStringList({
                            "mods/alpha-1.1.jar.disabled",
                            "mods/beta-1.1.jar",
                            "mods/delta-1.1.jar",
                            "mods/gamma.jar.disabled",
                            // the player took it out, so there's nothing to go by
                            "mods/epsilon-1.1.jar",
                            // new in the update, so as the pack has them
                            "mods/zeta-1.0.jar",
                            "mods/eta-1.0.jar.disabled",
                        }));
    }
};

QTEST_GUILESS_MAIN(ModrinthUpdateTest)

#include "ModrinthUpdate_test.moc"
