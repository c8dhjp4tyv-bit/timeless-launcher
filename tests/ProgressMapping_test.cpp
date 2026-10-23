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

#include <ui/dialogs/ProgressMapping.h>

#include <cmath>
#include <limits>

class ProgressMappingTest : public QObject {
    Q_OBJECT

    static constexpr int s_max = std::numeric_limits<int>::max();

   private slots:
    void inTheRange_data()
    {
        QTest::addColumn<qint64>("current");
        QTest::addColumn<qint64>("max");
        QTest::addColumn<qint64>("min");
        QTest::addColumn<int>("expected");

        QTest::newRow("start") << qint64(0) << qint64(100) << qint64(0) << 0;
        QTest::newRow("end") << qint64(100) << qint64(100) << qint64(0) << s_max;
        QTest::newRow("half") << qint64(50) << qint64(100) << qint64(0) << s_max / 2;
        QTest::newRow("a quarter, off the start") << qint64(30) << qint64(50) << qint64(10) << s_max / 2;
        QTest::newRow("big sizes") << qint64(5'000'000'000) << qint64(10'000'000'000) << qint64(0) << s_max / 2;
    }

    void inTheRange()
    {
        QFETCH(qint64, current);
        QFETCH(qint64, max);
        QFETCH(qint64, min);
        QFETCH(int, expected);

        const auto [mapped, total] = map_int_zero_max<qint64>(current, max, min);
        QCOMPARE(total, s_max);
        // the half of an odd INT_MAX is not exact
        QVERIFY2(std::abs(mapped - expected) <= 1, qPrintable(QString::number(mapped)));
    }

    // a download can be longer than it said it was, and a task can say it has no size
    void outsideTheRange_data()
    {
        QTest::addColumn<qint64>("current");
        QTest::addColumn<qint64>("max");
        QTest::addColumn<qint64>("min");
        QTest::addColumn<int>("expected");

        QTest::newRow("a little past the end") << qint64(101) << qint64(100) << qint64(0) << s_max;
        QTest::newRow("twice the size") << qint64(200) << qint64(100) << qint64(0) << s_max;
        QTest::newRow("far past the end") << qint64(1'000'000'000'000) << qint64(100) << qint64(0) << s_max;
        QTest::newRow("before the start") << qint64(-5) << qint64(100) << qint64(0) << 0;
        QTest::newRow("no size") << qint64(0) << qint64(0) << qint64(0) << 0;
        QTest::newRow("no size, some progress") << qint64(1000) << qint64(0) << qint64(0) << 0;
        QTest::newRow("negative size") << qint64(5) << qint64(-1) << qint64(0) << 0;
    }

    void outsideTheRange()
    {
        QFETCH(qint64, current);
        QFETCH(qint64, max);
        QFETCH(qint64, min);
        QFETCH(int, expected);

        const auto [mapped, total] = map_int_zero_max<qint64>(current, max, min);
        QCOMPARE(total, s_max);
        QCOMPARE(mapped, expected);
    }

    void notANumber()
    {
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        QCOMPARE(std::get<0>(map_int_zero_max<double>(nan, 1.0, 0.0)), 0);
        QCOMPARE(std::get<0>(map_int_zero_max<double>(0.5, nan, 0.0)), 0);
        QCOMPARE(std::get<0>(map_int_zero_max<double>(0.5, std::numeric_limits<double>::infinity(), 0.0)), 0);
    }
};

QTEST_GUILESS_MAIN(ProgressMappingTest)

#include "ProgressMapping_test.moc"
