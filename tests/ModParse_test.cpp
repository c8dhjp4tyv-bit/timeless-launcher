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

#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

#include <archive/ArchiveWriter.h>
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

    /// A mod's jar as a download brings it, with more after its metadata for a cut to fall into
    static QByteArray wholeJar()
    {
        const QTemporaryDir dir;
        const auto path = dir.filePath("whole.jar");
        QByteArray data;
        for (int i = 0; i < 20000; i++) {
            data.append(static_cast<char>((i * 7919 + i / 13) % 256));
        }
        MMCZip::ArchiveWriter jar(path);
        if (!jar.open() || !jar.addFile("fabric.mod.json", fabricMod({})) || !jar.addFile("assets/example/data.bin", data) ||
            !jar.close()) {
            return {};
        }
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    /// The jar with a comment of that many bytes, which zips may have after their end record
    static QByteArray withComment(QByteArray jar, int length)
    {
        // the comment's length is the record's last field, and the record comes last in a jar without a comment
        jar[jar.size() - 2] = static_cast<char>(length % 256);
        jar[jar.size() - 1] = static_cast<char>(length / 256);
        return jar + QByteArray(length, 'x');
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

    void minecraftRequirement_data()
    {
        QTest::addColumn<QString>("metadataFile");
        QTest::addColumn<QByteArray>("metadata");
        // the requirement read, or nothing when the mod is held to no version
        QTest::addColumn<QString>("expected");

        QTest::newRow("Fabric, one predicate") << "fabric.mod.json" << fabricMod(R"("depends": {"minecraft": "~1.20.1"})") << "~1.20.1";
        QTest::newRow("Fabric, any of two") << "fabric.mod.json" << fabricMod(R"("depends": {"minecraft": ["1.20.1", "1.20.2"]})")
                                            << "1.20.1, 1.20.2";
        QTest::newRow("Fabric, recommended only") << "fabric.mod.json" << fabricMod(R"("recommends": {"minecraft": "1.20.1"})") << "";
        QTest::newRow("Fabric, not strings") << "fabric.mod.json" << fabricMod(R"("depends": {"minecraft": ["1.20.1", {"a": 1}]})") << "";
        QTest::newRow("Quilt") << "quilt.mod.json" << quiltMod(R"("depends": [{"id": "minecraft", "versions": ">=1.20"}])") << ">=1.20";
        QTest::newRow("Quilt, any and all") << "quilt.mod.json"
                                            << quiltMod(R"("depends": [{"id": "minecraft", "versions": {"any": ["1.20"]}}])") << "";
        QTest::newRow("Quilt, only in some case")
            << "quilt.mod.json" << quiltMod(R"("depends": [{"id": "minecraft", "versions": "1.20", "unless": "other"}])") << "";
        QTest::newRow("Quilt, optional") << "quilt.mod.json"
                                         << quiltMod(R"("depends": [{"id": "minecraft", "versions": "1.20", "optional": true}])") << "";
        const QString forge =
            "modLoader=\"javafml\"\nloaderVersion=\"[47,)\"\nlicense=\"MIT\"\n[[mods]]\nmodId=\"example\"\nversion=\"1\"\n"
            "[[dependencies.example]]\nmodId=\"minecraft\"\n%1\nversionRange=\"[1.20.1,1.21)\"\n";
        QTest::newRow("Forge") << "META-INF/mods.toml" << forge.arg("mandatory=true").toUtf8() << "[1.20.1,1.21)";
        QTest::newRow("Forge, not mandatory") << "META-INF/mods.toml" << forge.arg("mandatory=false").toUtf8() << "";
        QTest::newRow("NeoForge") << "META-INF/neoforge.mods.toml" << forge.arg("type=\"required\"").toUtf8() << "[1.20.1,1.21)";
        QTest::newRow("NeoForge, optional") << "META-INF/neoforge.mods.toml" << forge.arg("type=\"optional\"").toUtf8() << "";
    }

    void minecraftRequirement()
    {
        QFETCH(const QString, metadataFile);
        QFETCH(const QByteArray, metadata);
        QFETCH(const QString, expected);

        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("mod.jar");
        {
            MMCZip::ArchiveWriter jar(path);
            QVERIFY(jar.open());
            QVERIFY(jar.addFile(metadataFile, metadata));
            QVERIFY(jar.close());
        }
        Mod mod{ QFileInfo(path) };
        QVERIFY(ModUtils::process(mod));
        QCOMPARE(mod.details().minecraft.isSet(), !expected.isEmpty());
        QCOMPARE(mod.details().minecraft.text(), expected);
    }

    void missingZipEnd_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::addColumn<bool>("missing");

        const auto whole = wholeJar();
        QVERIFY(!whole.isEmpty());
        QTest::newRow("whole jar") << whole << false;
        QTest::newRow("cut short") << whole.first(whole.size() * 6 / 10) << true;
        QTest::newRow("cut in its end record") << whole.first(whole.size() - 10) << true;
        QTest::newRow("with a long comment") << withComment(whole, 60000) << false;
        QTest::newRow("web page") << QByteArray("<!DOCTYPE html><html><body>Your download will start shortly.</body></html>") << true;
        QTest::newRow("empty") << QByteArray() << true;
    }

    void missingZipEnd()
    {
        // Java refuses every one of these but the whole jar and the one with a comment, with "zip END header not found"
        QFETCH(const QByteArray, contents);
        QFETCH(const bool, missing);

        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("mod.jar");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), contents.size());
        file.close();

        QCOMPARE(ModUtils::isMissingZipEnd(path), missing);
    }

    void metadataOfACutShortJar()
    {
        // A Forge mod reads on after its mods.toml for the version in its manifest, and the cut comes first. The mod is still
        // named by what was read before it, as the Mods page shows the damaged file.
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("mod.jar");
        const QByteArray modsToml(
            "modLoader=\"javafml\"\n"
            "loaderVersion=\"[47,)\"\n"
            "license=\"MIT\"\n"
            "[[mods]]\n"
            "modId=\"example\"\n"
            "version=\"${file.jarVersion}\"\n"
            "displayName=\"Example Mod\"\n");
        QByteArray data;
        for (int i = 0; i < 20000; i++) {
            data.append(static_cast<char>((i * 7919 + i / 13) % 256));
        }
        {
            MMCZip::ArchiveWriter jar(path);
            QVERIFY(jar.open());
            QVERIFY(jar.addFile("META-INF/mods.toml", modsToml));
            QVERIFY(jar.addFile("assets/example/data.bin", data));
            QVERIFY(jar.addFile("META-INF/MANIFEST.MF", QByteArray("Manifest-Version: 1.0\nImplementation-Version: 1.2.3\n")));
            QVERIFY(jar.close());
        }
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.resize(file.size() / 2));
        file.close();

        Mod mod{ QFileInfo(path) };
        QVERIFY(ModUtils::process(mod));
        QCOMPARE(mod.mod_id(), "example");
        QCOMPARE(mod.name(), "Example Mod");
    }

    void missingZipEndOfAFileThatIsGone()
    {
        // nothing can be told of a file that can't be read, so it doesn't count as damaged
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!ModUtils::isMissingZipEnd(dir.filePath("gone.jar")));
    }
};

QTEST_GUILESS_MAIN(ModParseTest)

#include "ModParse_test.moc"
