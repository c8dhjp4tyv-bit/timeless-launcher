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

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include <ui/pages/instance/ServerPingTask.h>

class ServerStatusTest : public QObject {
    Q_OBJECT

    /// The message of the day parsed from a description, with & standing in for § to keep the expected values readable
    static QString motd(const QJsonValue& description)
    {
        return ServerStatus::fromJson({ { "description", description } }).motd.replace(QChar(0x00A7), '&');
    }

   private slots:
    void vanillaServer()
    {
        const auto status = ServerStatus::fromJson({
            { "version", QJsonObject{ { "name", "1.21.1" }, { "protocol", 767 } } },
            { "players", QJsonObject{ { "max", 20 }, { "online", 3 }, { "sample", QJsonArray{} } } },
            { "description", QJsonObject{ { "text", "A Minecraft Server" } } },
        });
        QCOMPARE(status.onlinePlayers, 3);
        QCOMPARE(status.maxPlayers, 20);
        QCOMPARE(status.version, "1.21.1");
        QCOMPARE(status.motd.mid(2), "A Minecraft Server");
        QVERIFY(status.icon.isEmpty());
        QCOMPARE(status.latency, -1);
    }

    void nothingReported()
    {
        const auto status = ServerStatus::fromJson({});
        QCOMPARE(status.onlinePlayers, 0);
        QCOMPARE(status.maxPlayers, 0);
        QVERIFY(status.version.isEmpty());
        QVERIFY(status.motd.isEmpty());
    }

    void motd_data()
    {
        QTest::addColumn<QJsonValue>("description");
        QTest::addColumn<QString>("expected");

        const QString section(QChar(0x00A7));
        QTest::newRow("legacy text keeps its codes") << QJsonValue(section + "aGreen " + section + "lbold") << "&r&aGreen &lbold";
        QTest::newRow("styles are passed down, not across")
            << QJsonValue(QJsonObject{
                   { "text", "" },
                   { "extra", QJsonArray{ QJsonObject{ { "text", "Hyp" },
                                                       { "color", "gold" },
                                                       { "bold", true },
                                                       { "extra", QJsonArray{ QJsonObject{ { "text", "i" } } } } },
                                          QJsonObject{ { "text", "xel" }, { "color", "aqua" } }, " end" } },
               })
            << "&r&r&6&lHyp&r&6&li&r&bxel&r end";
        QTest::newRow("a child can switch a format off")
            << QJsonValue(QJsonObject{
                   { "text", "a" }, { "bold", true }, { "extra", QJsonArray{ QJsonObject{ { "text", "b" }, { "bold", false } } } } })
            << "&r&la&rb";
        QTest::newRow("hex colors have no code") << QJsonValue(QJsonObject{ { "text", "x" }, { "color", "#123456" } }) << "&rx";
        QTest::newRow("a list of components") << QJsonValue(QJsonArray{ "a", QJsonObject{ { "text", "b" }, { "italic", true } } })
                                              << "&ra&r&ob";
        QTest::newRow("every format") << QJsonValue(QJsonObject{ { "text", "x" },
                                                                 { "color", "red" },
                                                                 { "bold", true },
                                                                 { "italic", true },
                                                                 { "underlined", true },
                                                                 { "strikethrough", true },
                                                                 { "obfuscated", true } })
                                      << "&r&c&l&o&n&m&kx";
    }

    void motd()
    {
        QFETCH(QJsonValue, description);
        QFETCH(QString, expected);
        QCOMPARE(motd(description), expected);
    }

    void icon()
    {
        const QByteArray png("\x89PNG\r\n\x1a\n not really, but any bytes will do");
        auto base64 = png.toBase64();
        // some servers break the base64 into lines
        base64.insert(20, '\n');
        const auto status = ServerStatus::fromJson({ { "favicon", "data:image/png;base64," + QString::fromLatin1(base64) } });
        QCOMPARE(status.icon, png);

        QVERIFY(ServerStatus::fromJson({ { "favicon", "http://example.com/icon.png" } }).icon.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ServerStatusTest)

#include "ServerStatus_test.moc"
