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

#include <limits>

#include <MMCTime.h>

class MMCTimeTest : public QObject {
    Q_OBJECT

   private slots:
    void humanReadableDuration_data()
    {
        QTest::addColumn<double>("duration");
        QTest::addColumn<int>("precision");
        QTest::addColumn<QString>("expected");

        QTest::newRow("zero") << 0.0 << 0 << "0ms";
        QTest::newRow("under a second") << 0.25 << 0 << "250ms";
        QTest::newRow("single digit seconds") << 5.0 << 0 << "5s";
        QTest::newRow("seconds") << 45.0 << 0 << "45s";
        QTest::newRow("minutes") << 125.0 << 0 << "2m 5s";
        QTest::newRow("hours") << 3725.0 << 0 << "1h 2m 5s";
        QTest::newRow("days") << 90061.0 << 0 << "1d 1h 1m 1s";
        QTest::newRow("zero parts are left out") << 86460.0 << 0 << "1d 1m";
        QTest::newRow("milliseconds with precision") << 61.5 << 1 << "1m 1s 500ms";
        QTest::newRow("milliseconds without precision") << 61.5 << 0 << "1m 1s";
        QTest::newRow("negative") << -5.0 << 0 << "-5s";
        // The download ETA of a transfer that has not received anything yet
        QTest::newRow("infinite") << std::numeric_limits<double>::infinity() << 0 << "∞";
        QTest::newRow("not a number") << std::numeric_limits<double>::quiet_NaN() << 0 << "∞";
        QTest::newRow("too long for the day count") << 1e300 << 0 << "∞";
    }

    void humanReadableDuration()
    {
        QFETCH(double, duration);
        QFETCH(int, precision);
        QFETCH(QString, expected);

        QCOMPARE(Time::humanReadableDuration(duration, precision), expected);
    }
};

QTEST_GUILESS_MAIN(MMCTimeTest)

#include "MMCTime_test.moc"
