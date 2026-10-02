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
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <FileSystem.h>
#include <minecraft/auth/AccountList.h>
#include <minecraft/auth/MinecraftAccount.h>

class AccountListTest : public QObject {
    Q_OBJECT

    /// A list of accounts as the launcher saves it, with the version given
    static QByteArray list(int formatVersion)
    {
        const QJsonObject account{ { "type", "MSA" }, { "ygg", QJsonObject{ { "token", "abc" } } } };
        return QJsonDocument(QJsonObject{ { "formatVersion", formatVersion }, { "accounts", QJsonArray{ account } } }).toJson();
    }

   private slots:
    void unreadableList_data()
    {
        QTest::addColumn<QByteArray>("contents");

        QTest::newRow("cut short") << list(3).left(60);
        QTest::newRow("not json") << QByteArray("<<<<<<< HEAD\n{}\n=======\n{}\n>>>>>>> theirs");
        QTest::newRow("an array") << QJsonDocument(QJsonArray{ QJsonObject{ { "formatVersion", 3 } } }).toJson();
        QTest::newRow("another version") << list(99);
    }

    void unreadableList()
    {
        // A list that can't be read is set aside, as saving the empty list over it would take the accounts, and the sign-ins they
        // hold, from the user for good.
        QFETCH(const QByteArray, contents);

        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("accounts.json");
        FS::write(path, contents);

        AccountList list;
        list.setListFilePath(path, true);
        QVERIFY(!list.loadList());
        QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(FS::read(path + ".broken"), contents);

        // and whatever is saved next goes to a list of its own
        QVERIFY(list.saveList());
        QVERIFY(QFileInfo::exists(path));
        QCOMPARE(FS::read(path + ".broken"), contents);
    }

    void readableList()
    {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("accounts.json");
        FS::write(path, list(3));

        AccountList list;
        list.setListFilePath(path, true);
        QVERIFY(list.loadList());
        QVERIFY(QFileInfo::exists(path));
        QVERIFY(!QFileInfo::exists(path + ".broken"));
    }

    void changingTheDefault()
    {
        // The list tells its views exactly what changed, also when the first account becomes the default, which had no default
        // before it.
        AccountList list;
        const QAbstractItemModelTester tester(&list, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const auto first = MinecraftAccount::createOffline("First");
        const auto second = MinecraftAccount::createOffline("Second");

        list.addAccount(first);
        list.addAccount(second);
        list.setDefaultAccount(first);
        QCOMPARE(list.defaultAccount(), first);
        list.setDefaultAccount(second);
        QCOMPARE(list.defaultAccount(), second);
        list.setDefaultAccount(nullptr);
        QVERIFY(!list.defaultAccount());
        list.removeAccount(list.index(0));
        QCOMPARE(list.count(), 1);
    }

    void noList()
    {
        // the first run has no list yet, which is nothing to set aside
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("accounts.json");

        AccountList list;
        list.setListFilePath(path, true);
        QVERIFY(!list.loadList());
        QVERIFY(!QFileInfo::exists(path + ".broken"));
    }
};

QTEST_GUILESS_MAIN(AccountListTest)

#include "AccountList_test.moc"
