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

#include <net/NetUtils.h>

#include <cstdlib>
#include <ctime>

class NetUtilsTest : public QObject {
    Q_OBJECT

    // the same moment as the dates in the rows below, 12:00:00 GMT
    static QDateTime noon() { return QDateTime(QDate(2026, 10, 12), QTime(12, 0, 0), QTimeZone::UTC); }

   private slots:
    void retryAfterSeconds_data()
    {
        QTest::addColumn<QByteArray>("header");
        QTest::addColumn<qint64>("expected");  // 0 for nothing

        QTest::newRow("seconds") << QByteArray("30") << qint64(30);
        QTest::newRow("seconds with spaces") << QByteArray("  30 ") << qint64(30);
        QTest::newRow("a long wait") << QByteArray("86400") << qint64(86400);
        QTest::newRow("zero") << QByteArray("0") << qint64(0);
        QTest::newRow("negative") << QByteArray("-5") << qint64(0);
        QTest::newRow("empty") << QByteArray("") << qint64(0);
        QTest::newRow("text") << QByteArray("soon") << qint64(0);
        QTest::newRow("seconds and text") << QByteArray("30 seconds") << qint64(0);

        QTest::newRow("date in the future") << QByteArray("Mon, 12 Oct 2026 12:00:45 GMT") << qint64(45);
        QTest::newRow("date with spaces") << QByteArray(" Mon, 12 Oct 2026 12:01:00 GMT ") << qint64(60);
        QTest::newRow("date a day on") << QByteArray("Tue, 13 Oct 2026 12:00:00 GMT") << qint64(86400);
        QTest::newRow("date in the past") << QByteArray("Mon, 12 Oct 2026 11:59:00 GMT") << qint64(0);
        QTest::newRow("date that is now") << QByteArray("Mon, 12 Oct 2026 12:00:00 GMT") << qint64(0);
        QTest::newRow("date that isn't one") << QByteArray("Someday, 99 Foo 2026 12:00:00 GMT") << qint64(0);
    }

    void retryAfterSeconds()
    {
        QFETCH(QByteArray, header);
        QFETCH(qint64, expected);

        const auto seconds = Net::retryAfterSeconds(header, noon());
        if (expected == 0) {
            QVERIFY(!seconds.has_value());
        } else {
            QVERIFY(seconds.has_value());
            QCOMPARE(*seconds, expected);
        }
    }

    // A date in GMT is a date in GMT, wherever the launcher runs: it must not be read as local time
    void retryAfterDateDoesNotDependOnTheTimeZone_data()
    {
        QTest::addColumn<QByteArray>("zone");

        QTest::newRow("UTC") << QByteArray("UTC");
        QTest::newRow("New York") << QByteArray("America/New_York");
        QTest::newRow("Istanbul") << QByteArray("Europe/Istanbul");
        QTest::newRow("Auckland") << QByteArray("Pacific/Auckland");
    }

    void retryAfterDateDoesNotDependOnTheTimeZone()
    {
#ifdef Q_OS_WIN
        QSKIP("the time zone is set through TZ");
#else
        QFETCH(QByteArray, zone);

        const auto old = qgetenv("TZ");
        qputenv("TZ", ":" + zone);
        tzset();

        const auto seconds = Net::retryAfterSeconds(QByteArray("Mon, 12 Oct 2026 12:00:45 GMT"), noon());

        if (old.isNull()) {
            qunsetenv("TZ");
        } else {
            qputenv("TZ", old);
        }
        tzset();

        QVERIFY(seconds.has_value());
        QCOMPARE(*seconds, qint64(45));
#endif
    }
};

QTEST_GUILESS_MAIN(NetUtilsTest)

#include "NetUtils_test.moc"
