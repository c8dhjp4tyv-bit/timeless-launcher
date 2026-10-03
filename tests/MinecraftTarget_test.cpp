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

#include <minecraft/launch/MinecraftTarget.h>

class MinecraftTargetTest : public QObject {
    Q_OBJECT

   private slots:
    void address_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("address");
        QTest::addColumn<int>("port");

        QTest::newRow("a host") << "mc.example.com" << "mc.example.com" << 25565;
        QTest::newRow("a host and a port") << "mc.example.com:25566" << "mc.example.com" << 25566;
        QTest::newRow("the largest port") << "mc.example.com:65535" << "mc.example.com" << 65535;
        QTest::newRow("an IPv6 address in brackets") << "[::1]" << "::1" << 25565;
        QTest::newRow("an IPv6 address in brackets and a port") << "[::1]:25566" << "::1" << 25566;
        QTest::newRow("an IPv6 address in no brackets") << "::1" << "::1" << 25565;
        // what is no port is not one, as the game takes it
        QTest::newRow("a port that is not a number") << "mc.example.com:abc" << "mc.example.com" << 25565;
        QTest::newRow("a port that is nothing") << "mc.example.com:" << "mc.example.com" << 25565;
        QTest::newRow("a port that is negative") << "mc.example.com:-1" << "mc.example.com" << 25565;
        QTest::newRow("a port past the largest") << "mc.example.com:65536" << "mc.example.com" << 25565;
        QTest::newRow("a port far past the largest") << "mc.example.com:70000" << "mc.example.com" << 25565;
        QTest::newRow("a port that wraps around to another") << "mc.example.com:4294967297" << "mc.example.com" << 25565;
    }

    void address()
    {
        QFETCH(const QString, text);
        QFETCH(const QString, address);
        QFETCH(const int, port);

        const auto target = MinecraftTarget::parse(text, false);
        QCOMPARE(target.address, address);
        QCOMPARE(int(target.port), port);
        QVERIFY(target.world.isEmpty());
    }

    void world()
    {
        const auto target = MinecraftTarget::parse("New World", true);
        QCOMPARE(target.world, "New World");
        QVERIFY(target.address.isEmpty());
    }
};

QTEST_GUILESS_MAIN(MinecraftTargetTest)

#include "MinecraftTarget_test.moc"
