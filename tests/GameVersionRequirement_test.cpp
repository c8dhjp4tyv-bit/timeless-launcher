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

#include <minecraft/mod/GameVersionRequirement.h>

class GameVersionRequirementTest : public QObject {
    Q_OBJECT

   private slots:
    void fabric_data()
    {
        // any one of the alternatives will do; in each, all of the conditions separated by spaces have to hold
        QTest::addColumn<QStringList>("anyOf");
        QTest::addColumn<QString>("minecraft");
        QTest::addColumn<bool>("accepted");

        QTest::newRow("exact") << QStringList{ "1.20.1" } << "1.20.1" << true;
        QTest::newRow("exact, another version") << QStringList{ "1.20.1" } << "1.20.2" << false;
        QTest::newRow("exact with =") << QStringList{ "=1.21" } << "1.21" << true;
        QTest::newRow("missing numbers are 0") << QStringList{ "1.20" } << "1.20.0" << true;
        QTest::newRow("1.20 isn't 1.20.1") << QStringList{ "1.20" } << "1.20.1" << false;
        QTest::newRow("at least") << QStringList{ ">=1.20.1" } << "1.21.4" << true;
        QTest::newRow("at least, older") << QStringList{ ">=1.20.1" } << "1.20" << false;
        QTest::newRow("more than") << QStringList{ ">1.20.1" } << "1.20.1" << false;
        QTest::newRow("at most") << QStringList{ "<=1.20.4" } << "1.20.4" << true;
        QTest::newRow("at most, newer") << QStringList{ "<=1.20.4" } << "1.20.5" << false;
        QTest::newRow("between") << QStringList{ ">=1.20.1 <1.21" } << "1.20.4" << true;
        QTest::newRow("between, the upper end") << QStringList{ ">=1.20.1 <1.21" } << "1.21" << false;
        // "1.21-" is the first pre-release of 1.21, which comes before 1.21 itself
        QTest::newRow("between pre-releases") << QStringList{ ">=1.20.1- <1.21-" } << "1.20.6" << true;
        QTest::newRow("between pre-releases, the release") << QStringList{ ">=1.20.1- <1.21-" } << "1.21" << false;
        QTest::newRow("from a pre-release on") << QStringList{ ">=1.21-" } << "1.21" << true;
        QTest::newRow("same minor version") << QStringList{ "~1.20.1" } << "1.20.6" << true;
        QTest::newRow("same minor version, older") << QStringList{ "~1.20.1" } << "1.20" << false;
        QTest::newRow("same minor version, the next one") << QStringList{ "~1.20.1" } << "1.21" << false;
        QTest::newRow("same major version") << QStringList{ "^1.20" } << "1.21.4" << true;
        QTest::newRow("wildcard") << QStringList{ "1.20.x" } << "1.20.4" << true;
        QTest::newRow("wildcard, 1.20 itself") << QStringList{ "1.20.x" } << "1.20" << true;
        QTest::newRow("wildcard, another minor version") << QStringList{ "1.20.x" } << "1.21" << false;
        QTest::newRow("anything") << QStringList{ "*" } << "1.8.9" << true;
        QTest::newRow("either of two") << QStringList{ "1.20", "1.20.1" } << "1.20.1" << true;
        QTest::newRow("neither of two") << QStringList{ "1.20", "1.20.1" } << "1.20.2" << false;
        QTest::newRow("build metadata doesn't count") << QStringList{ "1.20.1+build.5" } << "1.20.1" << true;

        // what can't be read holds the mod to nothing
        QTest::newRow("snapshot") << QStringList{ "1.20.1" } << "24w14a" << true;
        QTest::newRow("pre-release of the game") << QStringList{ "1.20.1" } << "1.21-pre1" << true;
        QTest::newRow("unknown operator") << QStringList{ "~>1.20" } << "1.21" << true;
        QTest::newRow("numbers after a wildcard") << QStringList{ "1.x.1" } << "1.21" << true;
        QTest::newRow("one of two unreadable") << QStringList{ "1.20.1", "latest" } << "1.21" << true;
        QTest::newRow("nothing given") << QStringList{} << "1.21" << true;
    }

    void fabric()
    {
        QFETCH(const QStringList, anyOf);
        QFETCH(const QString, minecraft);
        QFETCH(const bool, accepted);

        QCOMPARE(GameVersionRequirement::fromFabric(anyOf).accepts(minecraft), accepted);
    }

    void maven_data()
    {
        QTest::addColumn<QString>("range");
        QTest::addColumn<QString>("minecraft");
        QTest::addColumn<bool>("accepted");

        QTest::newRow("from, up to") << "[1.20.1,1.21)" << "1.20.6" << true;
        QTest::newRow("from, up to: the lower end") << "[1.20.1,1.21)" << "1.20.1" << true;
        QTest::newRow("from, up to: the upper end") << "[1.20.1,1.21)" << "1.21" << false;
        QTest::newRow("from, up to: older") << "[1.20.1,1.21)" << "1.20" << false;
        QTest::newRow("after, up to and with") << "(1.20,1.21]" << "1.21" << true;
        QTest::newRow("after, up to and with: the lower end") << "(1.20,1.21]" << "1.20.0" << false;
        QTest::newRow("one version") << "[1.20.1]" << "1.20.1" << true;
        QTest::newRow("one version, another") << "[1.20.1]" << "1.20.2" << false;
        QTest::newRow("from on") << "[1.20,)" << "1.21.4" << true;
        QTest::newRow("from on, older") << "[1.20,)" << "1.19.4" << false;
        QTest::newRow("before") << "(,1.21)" << "1.20.6" << true;
        QTest::newRow("either of two ranges") << "[1.19.2],[1.20.1]" << "1.20.1" << true;
        QTest::newRow("neither of two ranges") << "[1.19.2],[1.20.1]" << "1.19.4" << false;
        QTest::newRow("spaces") << " [1.20.1 , 1.21) " << "1.21" << false;
        // a version on its own only recommends it
        QTest::newRow("recommended version") << "1.20.1" << "1.21" << true;

        // what can't be read holds the mod to nothing
        QTest::newRow("placeholder") << "${minecraft_version_range}" << "1.21" << true;
        QTest::newRow("qualifier") << "[1.20.1-pre1,1.21)" << "1.21" << true;
        QTest::newRow("unclosed") << "[1.20.1,1.21" << "1.21" << true;
        QTest::newRow("snapshot") << "[1.20.1,1.21)" << "24w14a" << true;
        QTest::newRow("nothing given") << "" << "1.21" << true;
    }

    void maven()
    {
        QFETCH(const QString, range);
        QFETCH(const QString, minecraft);
        QFETCH(const bool, accepted);

        QCOMPARE(GameVersionRequirement::fromMaven(range).accepts(minecraft), accepted);
    }

    void modVersion_data()
    {
        QTest::addColumn<QStringList>("anyOf");
        QTest::addColumn<QString>("version");
        QTest::addColumn<bool>("accepted");

        // Fabric leaves build metadata after a + out of comparisons, on either side
        QTest::newRow("same, with build metadata") << QStringList{ "0.5.11" } << "0.5.11+mc1.20.1" << true;
        QTest::newRow("another") << QStringList{ "0.5.8" } << "0.5.11+mc1.20.1" << false;
        QTest::newRow("both with build metadata") << QStringList{ "0.5.11+mc1.20.1" } << "0.5.11+mc1.20.2" << true;
        QTest::newRow("at least") << QStringList{ ">=0.90.0" } << "0.92.2+1.20.1" << true;
        QTest::newRow("too old") << QStringList{ ">=0.95" } << "0.92.2+1.20.1" << false;
        QTest::newRow("same major") << QStringList{ "^0.5.8" } << "0.9.0" << true;
        QTest::newRow("any") << QStringList{ "*" } << "0.1" << true;
        // what can't be read may be what is needed
        QTest::newRow("pre-release") << QStringList{ "0.5.8" } << "0.6.0-beta.2" << true;
        QTest::newRow("placeholder") << QStringList{ ">=1" } << "${version}" << true;
    }

    void modVersion()
    {
        QFETCH(const QStringList, anyOf);
        QFETCH(const QString, version);
        QFETCH(const bool, accepted);

        QCOMPARE(GameVersionRequirement::fromFabric(anyOf).acceptsModVersion(version), accepted);
    }

    void surelyAcceptedModVersion_data()
    {
        QTest::addColumn<QString>("syntax");
        QTest::addColumn<QStringList>("requirement");
        QTest::addColumn<QString>("version");
        QTest::addColumn<bool>("accepted");

        // as Sodium says of the versions of Sodium Extra and Iris it mustn't be loaded with
        QTest::newRow("older") << "fabric" << QStringList{ "<0.5.4" } << "0.5.3" << true;
        QTest::newRow("older, with build metadata") << "fabric" << QStringList{ "<0.5.4" } << "0.5.3+mc1.20.1" << true;
        QTest::newRow("the version given") << "fabric" << QStringList{ "<0.5.4" } << "0.5.4" << false;
        QTest::newRow("up to and with") << "fabric" << QStringList{ "<=1.6.14" } << "1.6.14+1.20.1" << true;
        QTest::newRow("newer") << "fabric" << QStringList{ "<=1.6.14" } << "1.7.0+mc1.20.1" << false;
        QTest::newRow("one of two") << "fabric" << QStringList{ "<1.0", ">=2.0 <3.0" } << "2.5" << true;
        QTest::newRow("neither of two") << "fabric" << QStringList{ "<1.0", ">=2.0 <3.0" } << "1.5" << false;
        // * and nothing at all are any version, whatever it looks like
        QTest::newRow("any") << "fabric" << QStringList{ "*" } << "${version}" << true;
        QTest::newRow("empty") << "fabric" << QStringList{ "" } << "1.0-beta" << true;
        QTest::newRow("nothing given") << "fabric" << QStringList{} << "1.0" << true;
        // what can't be read can't be told to be one of them
        QTest::newRow("pre-release") << "fabric" << QStringList{ "<0.5.4" } << "0.5.3-beta.1" << false;
        QTest::newRow("placeholder") << "fabric" << QStringList{ "<2" } << "${version}" << false;
        QTest::newRow("unreadable condition") << "fabric" << QStringList{ "<latest" } << "1.0" << false;
        QTest::newRow("readable alternative") << "fabric" << QStringList{ "latest", "<2" } << "1.0" << true;
        // NeoForge's ranges
        QTest::newRow("maven range") << "maven" << QStringList{ "[1.0,2.0)" } << "1.5" << true;
        QTest::newRow("maven range, outside") << "maven" << QStringList{ "[1.0,2.0)" } << "2.0" << false;
        QTest::newRow("maven version alone") << "maven" << QStringList{ "1.0" } << "5.0" << true;
        QTest::newRow("maven placeholder") << "maven" << QStringList{ "${range}" } << "1.0" << false;
        QTest::newRow("maven qualifier") << "maven" << QStringList{ "[1.0,2.0)" } << "1.5-beta" << false;
    }

    void surelyAcceptedModVersion()
    {
        QFETCH(const QString, syntax);
        QFETCH(const QStringList, requirement);
        QFETCH(const QString, version);
        QFETCH(const bool, accepted);

        const auto versions =
            syntax == "maven" ? GameVersionRequirement::fromMaven(requirement.value(0)) : GameVersionRequirement::fromFabric(requirement);
        QCOMPARE(versions.surelyAcceptsModVersion(version), accepted);
    }

    void text()
    {
        QCOMPARE(GameVersionRequirement::fromFabric({ ">=1.20.1  <1.21", "1.19.2" }).text(), ">=1.20.1 <1.21, 1.19.2");
        QCOMPARE(GameVersionRequirement::fromMaven("[1.20.1,1.21)").text(), "[1.20.1,1.21)");
        QVERIFY(!GameVersionRequirement().isSet());
        QVERIFY(!GameVersionRequirement::fromFabric({}).isSet());
        QVERIFY(!GameVersionRequirement::fromMaven(" ").isSet());
    }
};

QTEST_GUILESS_MAIN(GameVersionRequirementTest)

#include "GameVersionRequirement_test.moc"
