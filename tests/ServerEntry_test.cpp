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

#include <tag_list.h>

#include "ui/pages/instance/ServerEntry.h"

class ServerEntryTest : public QObject {
    Q_OBJECT

    /// The entry of a server as the game writes it
    static nbt::tag_compound gameEntry()
    {
        nbt::tag_compound entry;
        entry.insert("name", std::string("Home"));
        entry.insert("ip", std::string("mc.example.com:25566"));
        entry.insert("icon", std::string("aWNvbg=="));
        entry.insert("acceptTextures", nbt::tag_byte(1));
        return entry;
    }

   private slots:
    void readAndWrite()
    {
        auto entry = gameEntry();

        Server server(entry);
        QCOMPARE(server.m_name, QString("Home"));
        QCOMPARE(server.m_address, QString("mc.example.com:25566"));
        QCOMPARE(server.m_icon, QByteArray("icon"));
        QVERIFY(server.m_acceptsTextures == Server::AcceptsTextures::ALWAYS);

        nbt::tag_compound written;
        server.serialize(written);
        QVERIFY(written == entry);
    }

    void tagsTheLauncherDoesNotEditSurvive()
    {
        // "hidden" marks a server the game does not list (one that was joined with Direct Connection), and other tags may come with other
        // versions
        auto entry = gameEntry();
        entry.insert("hidden", nbt::tag_byte(1));
        entry.insert("somethingNew", std::string("a value"));
        nbt::tag_compound nested;
        nested.insert("count", nbt::tag_int(3));
        entry.insert("moreNew", std::move(nested));
        nbt::tag_list numbers(nbt::tag_type::Int);
        numbers.push_back(nbt::tag_int(1));
        numbers.push_back(nbt::tag_int(2));
        entry.insert("numbers", std::move(numbers));

        Server server(entry);
        nbt::tag_compound written;
        server.serialize(written);

        QVERIFY(written.has_key("hidden", nbt::tag_type::Byte));
        QCOMPARE(written.at("hidden").as<nbt::tag_byte>().get(), 1);
        QCOMPARE(written.at("somethingNew").as<nbt::tag_string>().get(), std::string("a value"));
        QVERIFY(written == entry);
    }

    void editsAreWrittenAndTheRestStays()
    {
        auto entry = gameEntry();
        entry.insert("hidden", nbt::tag_byte(1));

        Server server(entry);
        server.m_name = "  New name ";
        server.m_address = "other.example.com";
        server.m_acceptsTextures = Server::AcceptsTextures::NEVER;

        nbt::tag_compound written;
        server.serialize(written);

        QCOMPARE(written.at("name").as<nbt::tag_string>().get(), std::string("New name"));
        QCOMPARE(written.at("ip").as<nbt::tag_string>().get(), std::string("other.example.com"));
        QCOMPARE(written.at("acceptTextures").as<nbt::tag_byte>().get(), 0);
        QVERIFY(written.has_key("hidden"));
        QCOMPARE(written.at("icon").as<nbt::tag_string>().get(), std::string("aWNvbg=="));
    }

    void anEntryIsNotWrittenTwice()
    {
        // an entry that is read, copied around by the list and written again is the same entry
        auto entry = gameEntry();
        entry.insert("hidden", nbt::tag_byte(1));
        const Server server(entry);

        Server copy = server;
        nbt::tag_compound first;
        copy.serialize(first);
        nbt::tag_compound second;
        copy.serialize(second);
        QVERIFY(first == second);
        QCOMPARE(int(second.size()), 5);
    }
};

QTEST_GUILESS_MAIN(ServerEntryTest)

#include "ServerEntry_test.moc"
