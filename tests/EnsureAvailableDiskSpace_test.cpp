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

#include <minecraft/launch/EnsureAvailableDiskSpace.h>

class EnsureAvailableDiskSpaceTest : public QObject {
    Q_OBJECT

   private slots:
    void isLow_data()
    {
        QTest::addColumn<qint64>("bytesAvailable");
        QTest::addColumn<bool>("low");

        QTest::newRow("full") << qint64{ 0 } << true;
        QTest::newRow("a few megabytes") << qint64{ 12'000'000 } << true;
        QTest::newRow("just under the limit") << EnsureAvailableDiskSpace::LowSpace - 1 << true;
        QTest::newRow("at the limit") << EnsureAvailableDiskSpace::LowSpace << false;
        QTest::newRow("plenty") << qint64{ 200'000'000'000 } << false;
        // QStorageInfo gives -1 when it can't tell, which says nothing about the disk
        QTest::newRow("unknown") << qint64{ -1 } << false;
    }

    void isLow()
    {
        QFETCH(const qint64, bytesAvailable);
        QFETCH(const bool, low);

        QCOMPARE(EnsureAvailableDiskSpace::isLow(bytesAvailable), low);
    }
};

QTEST_GUILESS_MAIN(EnsureAvailableDiskSpaceTest)

#include "EnsureAvailableDiskSpace_test.moc"
