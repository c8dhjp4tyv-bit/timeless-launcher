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

#include "ui/pages/instance/ManagedPackPage.h"

class ManagedPackNameTest : public QObject {
    Q_OBJECT

   private slots:
    void nameForUpdate_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("oldVersion");
        QTest::addColumn<QString>("newVersion");
        QTest::addColumn<QString>("expected");

        QTest::newRow("the version is in the name") << "Create 1.2.0" << "1.2.0" << "1.3.0" << "Create 1.3.0";
        QTest::newRow("in the middle") << "Create 1.2.0 (mine)" << "1.2.0" << "1.3.0" << "Create 1.3.0 (mine)";
        // a name the player chose has no version in it, and stays
        QTest::newRow("a name of its own") << "My modpack" << "1.2.0" << "1.3.0" << "My modpack";
        // an instance that doesn't know its version used to get the new one between all the letters of its name
        QTest::newRow("no version known") << "Create" << "" << "1.3.0" << "Create";
        QTest::newRow("no version known, empty name") << "" << "" << "1.3.0" << "";
        QTest::newRow("same version") << "Create 1.2.0" << "1.2.0" << "1.2.0" << "Create 1.2.0";
    }

    void nameForUpdate()
    {
        QFETCH(QString, name);
        QFETCH(QString, oldVersion);
        QFETCH(QString, newVersion);
        QFETCH(QString, expected);

        QCOMPARE(ManagedPackPage::nameForUpdate(name, oldVersion, newVersion), expected);
    }
};

QTEST_GUILESS_MAIN(ManagedPackNameTest)

#include "ManagedPackName_test.moc"
