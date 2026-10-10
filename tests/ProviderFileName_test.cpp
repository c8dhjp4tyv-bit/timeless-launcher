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

#include <modplatform/flame/FlameModIndex.h>
#include <modplatform/modrinth/ModrinthPackIndex.h>

class ProviderFileNameTest : public QObject {
    Q_OBJECT

    static QJsonObject flameFile(const QString& fileName)
    {
        return { { "gameVersions", QJsonArray{ "1.20.1", "Fabric" } },
                 { "modId", 1 },
                 { "id", 2 },
                 { "fileDate", "2025-01-01T00:00:00Z" },
                 { "displayName", "A mod" },
                 { "downloadUrl", "https://example.com/a.jar" },
                 { "fileName", fileName },
                 { "releaseType", 1 } };
    }

    static QJsonObject modrinthVersion(const QString& fileName)
    {
        return { { "project_id", "AABBCCDD" },
                 { "id", "EEFFGGHH" },
                 { "date_published", "2025-01-01T00:00:00Z" },
                 { "game_versions", QJsonArray{ "1.20.1" } },
                 { "loaders", QJsonArray{ "fabric" } },
                 { "name", "A mod" },
                 { "version_number", "1.0" },
                 { "version_type", "release" },
                 { "files", QJsonArray{ QJsonObject{ { "url", "https://example.com/a.jar" },
                                                     { "filename", fileName },
                                                     { "primary", true },
                                                     { "hashes", QJsonObject{ { "sha512", "00" } } } } } } };
    }

   private slots:
    void fileName_data()
    {
        QTest::addColumn<QString>("fromTheSite");
        QTest::addColumn<QString>("used");

        QTest::newRow("a name") << "sodium-fabric-0.5.jar" << "sodium-fabric-0.5.jar";
        QTest::newRow("spaces and brackets") << "My Mod (1.20) [fabric].jar" << "My Mod (1.20) [fabric].jar";
        // a name is not a path: with a separator in it the file would be put in another folder, or outside of the instance
        QTest::newRow("up and out") << "../../escaped.jar" << "..-..-escaped.jar";
        QTest::newRow("a folder") << "config/options.jar" << "config-options.jar";
        QTest::newRow("backslashes") << "..\\..\\escaped.jar" << "..-..-escaped.jar";
        QTest::newRow("an absolute path") << "/etc/escaped.jar" << "-etc-escaped.jar";
        QTest::newRow("characters Windows refuses") << "a:b*c?.jar" << "a-b-c-.jar";
    }

    void fileName()
    {
        QFETCH(QString, fromTheSite);
        QFETCH(QString, used);

        auto flame = flameFile(fromTheSite);
        QCOMPARE(FlameMod::loadIndexedPackVersion(flame).fileName, used);

        auto modrinth = modrinthVersion(fromTheSite);
        QCOMPARE(Modrinth::loadIndexedPackVersion(modrinth).fileName, used);
    }
};

QTEST_GUILESS_MAIN(ProviderFileNameTest)

#include "ProviderFileName_test.moc"
