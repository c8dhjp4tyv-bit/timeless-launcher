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

#include <launch/LogModel.h>

class LogModelTest : public QObject {
    Q_OBJECT

   private slots:
    void previousLevel()
    {
        LogModel model;
        model.setMaxLines(3);
        QCOMPARE(model.previousLevel(), MessageLevel::Unknown);

        model.append(MessageLevel::Info, "one");
        QCOMPARE(model.previousLevel(), MessageLevel::Info);
        model.append(MessageLevel::Warning, "two");
        model.append(MessageLevel::Info, "three");
        QCOMPARE(model.previousLevel(), MessageLevel::Info);

        // once the log is full, each new line takes the place of the oldest, and the last one is no longer at the end of the buffer
        for (const auto level : { MessageLevel::Error, MessageLevel::Debug, MessageLevel::Fatal, MessageLevel::Warning }) {
            model.append(level, "more");
            QCOMPARE(model.rowCount(), 3);
            QCOMPARE(model.previousLevel(), level);
        }
    }

    void limitOfLessThanALine_data()
    {
        QTest::addColumn<int>("limit");

        // A settings file can hold any number, and a log that holds no lines has nothing to wrap around: appending to one divided by zero.
        QTest::newRow("nothing") << 0;
        QTest::newRow("less than nothing") << -1;
        QTest::newRow("far less than nothing") << -100000;
    }

    void limitOfLessThanALine()
    {
        QFETCH(const int, limit);

        LogModel model;
        model.setMaxLines(limit);
        QVERIFY(model.getMaxLines() >= 1);

        model.append(MessageLevel::Info, "one");
        model.append(MessageLevel::Warning, "two");
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), Qt::DisplayRole).toString(), "two");
        QCOMPARE(model.toPlainText(), "two\n");
    }
};

QTEST_GUILESS_MAIN(LogModelTest)

#include "LogModel_test.moc"
