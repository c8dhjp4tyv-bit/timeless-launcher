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

#include <QStringList>

#include <minecraft/mod/tasks/LocalModParseTask.h>

class ModParseTest : public QObject {
    Q_OBJECT

    static QByteArray quiltMod(const QString& loaderFields, const QString& metadataFields = {})
    {
        return QString(R"({"schema_version": 1, "quilt_loader": {"group": "com.example", "id": "example", "version": "1.0.0",
                           "metadata": {%1} %2}})")
            .arg(metadataFields, loaderFields.isEmpty() ? QString() : ", " + loaderFields)
            .toUtf8();
    }

    static QByteArray fabricMod(const QString& fields)
    {
        return QString(R"({"schemaVersion": 1, "id": "example", "version": "1.0.0", %1})").arg(fields).toUtf8();
    }

    static void addIconRows()
    {
        QTest::addColumn<QString>("icon");
        QTest::addColumn<QString>("expected");

        QTest::newRow("single path") << R"("assets/example/icon.png")" << "assets/example/icon.png";
        // Both specs key an icon map by the width of each image, as a plain number.
        QTest::newRow("largest of several sizes") << R"({"32": "icon32.png", "128": "icon128.png", "64": "icon64.png"})" << "icon128.png";
        QTest::newRow("unparsable size") << R"({"small": "icon.png"})" << "icon.png";
    }

   private slots:
    void quiltDependencies_data()
    {
        QTest::addColumn<QString>("depends");
        QTest::addColumn<QStringList>("expected");

        QTest::newRow("loader, api and game") << R"([{"id": "quilt_loader", "versions": ">=0.19.1"},
                                                    {"id": "quilted_fabric_api", "versions": ">=7.0.2"},
                                                    {"id": "minecraft", "versions": ">=1.20"}])"
                                              << QStringList{ "quilted_fabric_api" };
        QTest::newRow("plain id") << R"(["owo"])" << QStringList{ "owo" };
        QTest::newRow("maven group") << R"(["io.wispforest:owo", {"id": "com.terraformersmc:modmenu"}])" << QStringList{ "owo", "modmenu" };
        QTest::newRow("optional") << R"([{"id": "modmenu", "optional": true}, {"id": "owo", "optional": false}])" << QStringList{ "owo" };
        // An array is satisfied by any one of its entries, so none of them is required on its own.
        QTest::newRow("any of") << R"([[{"id": "sodium"}, {"id": "embeddium"}], "owo"])" << QStringList{ "owo" };
    }

    void quiltDependencies()
    {
        QFETCH(QString, depends);
        QFETCH(QStringList, expected);

        // depends belongs to quilt_loader, not to the root of quilt.mod.json
        auto details = ModUtils::ReadQuiltModInfo(quiltMod(R"("depends": )" + depends));

        QCOMPARE(details.mod_id, "example");
        QCOMPARE(details.dependencies, expected);
    }

    void quiltIcon_data() { addIconRows(); }

    void quiltIcon()
    {
        QFETCH(QString, icon);
        QFETCH(QString, expected);

        auto details = ModUtils::ReadQuiltModInfo(quiltMod({}, R"("icon": )" + icon));

        QCOMPARE(details.icon_file, expected);
    }

    void fabricIcon_data() { addIconRows(); }

    void fabricIcon()
    {
        QFETCH(QString, icon);
        QFETCH(QString, expected);

        auto details = ModUtils::ReadFabricModInfo(fabricMod(R"("icon": )" + icon));

        QCOMPARE(details.icon_file, expected);
    }
};

QTEST_GUILESS_MAIN(ModParseTest)

#include "ModParse_test.moc"
