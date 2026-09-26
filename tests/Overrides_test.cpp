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
};

QTEST_GUILESS_MAIN(OverridesTest)

#include "Overrides_test.moc"
