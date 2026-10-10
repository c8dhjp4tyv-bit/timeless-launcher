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

#include <QNetworkAccessManager>
#include <QTest>

#include <net/NetJob.h>

class NetJobTest : public QObject {
    Q_OBJECT

   private slots:
    void noRequestFailedMeansNoStatus()
    {
        // The callers that report a failed job to the user ask for the status of the first request that failed. A job that has none
        // (one that couldn't be aborted fails like that) used to be asked for the first of an empty list.
        QNetworkAccessManager network;
        NetJob job("nothing failed", &network);
        QVERIFY(job.getFailedActions().isEmpty());
        QCOMPARE(job.firstFailedStatusCode(), -1);
    }
};

QTEST_GUILESS_MAIN(NetJobTest)

#include "NetJob_test.moc"
