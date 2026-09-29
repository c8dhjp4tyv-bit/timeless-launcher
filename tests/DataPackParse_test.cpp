// SPDX-FileCopyrightText: 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Timeless Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
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

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QTest>
#include <QTimer>

#include <FileSystem.h>

#include <minecraft/mod/DataPack.h>
#include <minecraft/mod/tasks/LocalDataPackParseTask.h>

class DataPackParseTest : public QObject {
    Q_OBJECT

   private slots:
    void test_parseZIP()
    {
        QString source = QFINDTESTDATA("testdata/DataPackParse");

        QString zip_dp = FS::PathCombine(source, "test_data_pack_boogaloo.zip");
        DataPack pack{ QFileInfo(zip_dp) };

        bool valid = DataPackUtils::processZIP(&pack);

        QVERIFY(pack.packFormat() == 4);
        QVERIFY(pack.description() == "Some data pack 2 boobgaloo");
        QVERIFY(valid == true);
    }

    void test_parseFolder()
    {
        QString source = QFINDTESTDATA("testdata/DataPackParse");

        QString folder_dp = FS::PathCombine(source, "test_folder");
        DataPack pack{ QFileInfo(folder_dp) };

        bool valid = DataPackUtils::processFolder(&pack);

        QVERIFY(pack.packFormat() == 10);
        QVERIFY(pack.description() == "Some data pack, maybe");
        QVERIFY(valid == true);
    }

    void test_parseFolder2()
    {
        QString source = QFINDTESTDATA("testdata/DataPackParse");

        QString folder_dp = FS::PathCombine(source, "another_test_folder");
        DataPack pack{ QFileInfo(folder_dp) };

        bool valid = DataPackUtils::process(&pack);

        QVERIFY(pack.packFormat() == 6);
        QVERIFY(pack.description() == "Some data pack three, leaves on the tree");
        QVERIFY(valid == true);
    }

    void processComponent_data()
    {
        QTest::addColumn<QJsonValue>("description");
        QTest::addColumn<QString>("html");

        // The description is shown as rich text, where a < in the pack's text would start a tag that swallows the rest of it
        QTest::newRow("text") << QJsonValue("Faithful <3 x32, for 1.20 & up") << "Faithful &lt;3 x32, for 1.20 &amp; up";
        QTest::newRow("formatted text") << QJsonValue(
                                               QJsonObject{ { "text", "a < b" }, { "bold", true }, { "extra", QJsonArray{ "<c>" } } })
                                        << "<span style=\"font-weight: bold;\">a &lt; b&lt;c&gt;</span>";
        // and what the pack gives for the attributes of a tag can't end them
        QTest::newRow("color") << QJsonValue(QJsonObject{ { "text", "x" }, { "color", "red\" title=\"y" } })
                               << "<span style=\"color: red&quot; title=&quot;y;\">x</span>";
        QTest::newRow("link") << QJsonValue(QJsonObject{ { "text", "site" },
                                                         { "clickEvent", QJsonObject{ { "action", "open_url" },
                                                                                      { "value", "https://example.com/?a=1&b=\"2\"" } } } })
                              << "<a href=\"https://example.com/?a=1&amp;b=&quot;2&quot;\">site</a>";
        // the game opens only web links
        QTest::newRow("link to a file") << QJsonValue(QJsonObject{ { "text", "pack" },
                                                                   { "clickEvent", QJsonObject{ { "action", "open_url" },
                                                                                                { "value", "file:///C:/pack.bat" } } } })
                                        << "pack";
    }

    void searchDescription()
    {
        // searching goes by the text of the description, not by the tags and escapes that make it rich text
        DataPack pack{ QFileInfo("pack.zip") };
        pack.setDescription(DataPackUtils::processComponent(QJsonObject{ { "text", "Faithful <3 & more" }, { "color", "gold" } }));
        QVERIFY(pack.applyFilter(QRegularExpression("<3 & more")));
        QVERIFY(!pack.applyFilter(QRegularExpression("span|gold|amp")));
    }

    void processComponent()
    {
        QFETCH(const QJsonValue, description);
        QFETCH(const QString, html);

        QCOMPARE(DataPackUtils::processComponent(description), html);
    }
};

QTEST_GUILESS_MAIN(DataPackParseTest)

#include "DataPackParse_test.moc"
