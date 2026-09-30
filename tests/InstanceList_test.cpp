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

#include <QAbstractItemModelTester>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include <FileSystem.h>
#include <InstanceList.h>
#include <settings/INISettingsObject.h>

class InstanceListTest : public QObject {
    Q_OBJECT

   private slots:
    void changingTheInstanceFolder()
    {
        // The list drops its rows when the folder changes, which is rows 0 to count() - 1, and no rows at all when there are none, as
        // when the first folder is picked. (Loading instances needs the application, so there are none here.)
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto first = dir.filePath("first");
        const auto second = dir.filePath("second");
        QVERIFY(QDir().mkpath(first));

        INISettingsObject settings(dir.filePath("launcher.cfg"));
        const auto instanceDir = settings.registerSetting("InstanceDir", first);
        settings.registerSetting("AdditionalInstanceDirs", QStringList());

        InstanceList list(&settings, { first });
        // a list that is watched loads instances when they change, which needs the application
        QObject::disconnect(&list, &InstanceList::instancesChanged, &list, nullptr);
        const QAbstractItemModelTester tester(&list, QAbstractItemModelTester::FailureReportingMode::QtTest);

        settings.set("InstanceDir", second);
        list.on_InstFolderChanged(*instanceDir, second);
        QCOMPARE(list.rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(InstanceListTest)

#include "InstanceList_test.moc"
