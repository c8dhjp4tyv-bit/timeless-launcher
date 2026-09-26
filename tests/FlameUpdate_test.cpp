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
#include <modplatform/flame/FlameInstanceCreationTask.h>

class FlameUpdateTest : public QObject {
    Q_OBJECT

    static Flame::File packFile(int projectId, const QString& fileName, bool required)
    {
        Flame::File file;
        file.projectId = projectId;
        file.required = required;
        file.version.fileName = fileName;
        return file;
    }

   private slots:
    void installedFileEnabled()
    {
        const QTemporaryDir gameRoot;
        QVERIFY(gameRoot.isValid());
        for (const auto* file : { "mods/on.jar", "mods/off.jar.disabled", "resourcepacks/pack.zip.disabled" }) {
            FS::write(FS::PathCombine(gameRoot.path(), file), "");
        }
        const QDir dir(gameRoot.path());

        QCOMPARE(FlameCreationTask::installedFileEnabled(dir, "on.jar"), std::optional<bool>(true));
        QCOMPARE(FlameCreationTask::installedFileEnabled(dir, "off.jar"), std::optional<bool>(false));
        // files other than mods are looked for in the folders they may have been moved to
        QCOMPARE(FlameCreationTask::installedFileEnabled(dir, "pack.zip"), std::optional<bool>(false));
        QCOMPARE(FlameCreationTask::installedFileEnabled(dir, "gone.jar"), std::nullopt);
    }

    void fileEnabled()
    {
        // the player turned off a mod the pack requires and turned on an optional one
        const QHash<int, bool> enabledByProject{ { 1, false }, { 2, true } };
        const QStringList selected{ "mods/picked.jar" };

        QVERIFY(!FlameCreationTask::fileEnabled(packFile(1, "required-2.0.jar", true), enabledByProject, selected));
        QVERIFY(FlameCreationTask::fileEnabled(packFile(2, "optional-2.0.jar", false), enabledByProject, selected));
        // without the player's choice, as the pack and the optional mods picked have it
        QVERIFY(FlameCreationTask::fileEnabled(packFile(3, "new.jar", true), enabledByProject, selected));
        QVERIFY(FlameCreationTask::fileEnabled(packFile(4, "picked.jar", false), enabledByProject, selected));
        QVERIFY(!FlameCreationTask::fileEnabled(packFile(5, "not-picked.jar", false), enabledByProject, selected));
    }
};

QTEST_GUILESS_MAIN(FlameUpdateTest)

#include "FlameUpdate_test.moc"
