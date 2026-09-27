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

    static QByteArray fabricMod(const QString& id, const QString& fields)
    {
        return QString(R"({"schemaVersion": 1, "id": "%1", "version": "1.0.0"%2})")
            .arg(id, fields.isEmpty() ? QString() : ", " + fields)
            .toUtf8();
    }

    static bool writeJar(const QString& path, const QList<std::pair<QString, QByteArray>>& files)
    {
        MMCZip::ArchiveWriter jar(path);
        if (!jar.open()) {
            return false;
        }
        for (const auto& [name, contents] : files) {
            if (!jar.addFile(name, contents)) {
                return false;
            }
        }
        return jar.close();
    }

    /// A jar holding these files, to nest in another
    static QByteArray jarBytes(const QTemporaryDir& dir, const QList<std::pair<QString, QByteArray>>& files)
    {
        const auto path = dir.filePath("nested.jar");
        QFile::remove(path);
        if (!writeJar(path, files)) {
            return {};
        }
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    /// Bytes that don't compress, for a cut to fall into
    static QByteArray noise()
    {
        QByteArray data;
        for (int i = 0; i < 20000; i++) {
            data.append(static_cast<char>((i * 7919 + i / 13) % 256));
        }
        return data;
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

    void fabricNeedsAndProvides()
    {
        // what the loader holds the mod to, and what it gives the loader besides itself, as Sodium lists them
        const auto details = ModUtils::ReadFabricModInfo(fabricMod(R"(
            "environment": "client", "provides": ["example-old"],
            "depends": {"minecraft": "1.20.1", "fabricloader": ">=0.12.0", "fabric-rendering-fluids-v1": ">=0.1"},
            "recommends": {"modmenu": "*"},
            "jars": [{"file": "META-INF/jars/fabric-api-base.jar"}, {"file": "META-INF/jars/fabric-rendering-fluids-v1.jar"}])"));
        QCOMPARE(details.requiredMods, (QStringList{ "fabric-rendering-fluids-v1", "fabricloader", "minecraft" }));
        QCOMPARE(details.providedMods, (QStringList{ "example", "example-old" }));
        // and in which versions, the ones it provides being in its own
        QCOMPARE(details.requiredVersions.value("fabric-rendering-fluids-v1").text(), ">=0.1");
        QCOMPARE(details.requiredVersions.value("minecraft").text(), "1.20.1");
        QCOMPARE(details.providedVersions.value("example-old"), QStringList{ "1.0.0" });
        QCOMPARE(details.nestedJars, (QStringList{ "META-INF/jars/fabric-api-base.jar", "META-INF/jars/fabric-rendering-fluids-v1.jar" }));
        QVERIFY(!details.serverOnly);

        QVERIFY(ModUtils::ReadFabricModInfo(fabricMod(R"("environment": "server")")).serverOnly);
        // metadata that can't be read provides nothing, not a mod without an ID
        QVERIFY(ModUtils::ReadFabricModInfo("{\"schemaVersion\": 1, \"id\": \"example\"").providedMods.isEmpty());
    }

    void quiltNeedsAndProvides()
    {
        const auto details = ModUtils::ReadQuiltModInfo(
            QByteArray(R"({"schema_version": 1, "minecraft": {"environment": "*"}, "quilt_loader": {"id": "example", "version": "1",
                           "provides": ["old_example", {"id": "com.example:older_example", "version": "1"}],
                           "jars": ["META-INF/jars/library.jar"],
                           "depends": ["quilt_loader", {"id": "org.quiltmc:qsl", "versions": ">=6"}, {"id": "modmenu", "optional": true},
                                       {"id": "sodium", "unless": "embeddium"}, [{"id": "a"}, {"id": "b"}]]}})"));
        // one needed unless another mod is there, and one of several, may not be needed at all
        QCOMPARE(details.requiredMods, (QStringList{ "quilt_loader", "qsl" }));
        QCOMPARE(details.providedMods, (QStringList{ "example", "old_example", "older_example" }));
        QCOMPARE(details.requiredVersions.value("qsl").text(), ">=6");
        QVERIFY(!details.requiredVersions.contains("quilt_loader"));
        // an object may give the version it provides the ID in, which is otherwise the mod's own
        QCOMPARE(details.providedVersions.value("old_example"), QStringList{ "1" });
        QCOMPARE(details.providedVersions.value("older_example"), QStringList{ "1" });
        QCOMPARE(details.nestedJars, QStringList{ "META-INF/jars/library.jar" });
        QVERIFY(!details.serverOnly);

        QVERIFY(
            ModUtils::ReadQuiltModInfo(
                R"({"schema_version": 1, "minecraft": {"environment": "dedicated_server"}, "quilt_loader": {"id": "a", "version": "1"}})")
                .serverOnly);
    }

    void forgeNeedsAndProvides_data()
    {
        QTest::addColumn<QString>("metadataFile");
        QTest::addColumn<QString>("dependency");
        QTest::addColumn<QStringList>("required");

        QTest::newRow("Forge, mandatory") << "META-INF/mods.toml" << "mandatory=true" << QStringList{ "forge", "jei" };
        QTest::newRow("Forge, not mandatory") << "META-INF/mods.toml" << "mandatory=false" << QStringList{ "forge" };
        QTest::newRow("NeoForge, required") << "META-INF/neoforge.mods.toml" << "type=\"required\"" << QStringList{ "forge", "jei" };
        QTest::newRow("NeoForge, optional") << "META-INF/neoforge.mods.toml" << "type=\"optional\"" << QStringList{ "forge" };
        // a server needs it, and the client doesn't
        QTest::newRow("only on servers") << "META-INF/mods.toml" << "mandatory=true\nside=\"SERVER\"" << QStringList{ "forge" };
    }

    void forgeNeedsAndProvides()
    {
        QFETCH(const QString, metadataFile);
        QFETCH(const QString, dependency);
        QFETCH(const QStringList, required);

        // two mods in one file, each with its own dependencies
        const auto modsToml = QString(
                                  "modLoader=\"javafml\"\nloaderVersion=\"[47,)\"\nlicense=\"MIT\"\n"
                                  "[[mods]]\nmodId=\"example\"\nversion=\"1\"\n"
                                  "[[mods]]\nmodId=\"example_addon\"\nversion=\"1\"\n"
                                  "[[dependencies.example]]\nmodId=\"forge\"\nmandatory=true\ntype=\"required\"\nversionRange=\"[47,)\"\n"
                                  "[[dependencies.example_addon]]\nmodId=\"jei\"\n%1\nversionRange=\"[15,)\"\n")
                                  .arg(dependency)
                                  .toUtf8();
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("mod.jar");
        QVERIFY(writeJar(path, { { metadataFile, modsToml } }));

        Mod mod{ QFileInfo(path) };
        QVERIFY(ModUtils::process(mod));
        QCOMPARE(mod.details().providedMods, (QStringList{ "example", "example_addon" }));
        QCOMPARE(mod.details().providedVersions.value("example_addon"), QStringList{ "1" });
        QCOMPARE(mod.details().requiredMods, required);
        QCOMPARE(mod.details().requiredVersions.value("jei").text(), required.contains("jei") ? "[15,)" : "");
    }

    void nestedMods()
    {
        // Fabric loads the jars a mod's metadata lists as mods of their own, and those can nest more, as Fabric API's modules
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto inner = jarBytes(dir, { { "fabric.mod.json", fabricMod("inner", R"("provides": ["inner-old"])") } });
        const auto middle = jarBytes(dir, { { "fabric.mod.json", fabricMod("middle", R"("jars": [{"file": "META-INF/jars/inner.jar"}])") },
                                            { "META-INF/jars/inner.jar", inner } });
        // a library Loom gave metadata of its own, and one only for servers, which the loader leaves out
        const auto library = jarBytes(dir, { { "fabric.mod.json", fabricMod("org_example_library", "") } });
        const auto server = jarBytes(dir, { { "fabric.mod.json", fabricMod("serverside", R"("environment": "server")") } });
        const auto path = dir.filePath("mod.jar");
        QVERIFY(writeJar(path, { { "fabric.mod.json", fabricMod("example", R"("jars": [{"file": "META-INF/jars/middle.jar"},
                                                                                       {"file": "META-INF/jars/library.jar"},
                                                                                       {"file": "META-INF/jars/server.jar"}])") },
                                 { "META-INF/jars/middle.jar", middle },
                                 { "META-INF/jars/library.jar", library },
                                 { "META-INF/jars/server.jar", server },
                                 { "META-INF/jars/unlisted.jar", library } }));

        Mod mod{ QFileInfo(path) };
        QVERIFY(ModUtils::process(mod));
        QCOMPARE(mod.details().providedMods, (QStringList{ "example", "middle", "inner", "inner-old", "org_example_library" }));
        QCOMPARE(mod.details().providedVersions.value("inner-old"), QStringList{ "1.0.0" });
        QVERIFY(!mod.details().unreadNestedMods);

        // what the basic information of a mod needs doesn't include what is nested in it
        Mod basic{ QFileInfo(path) };
        QVERIFY(ModUtils::process(basic, ModUtils::ProcessingLevel::BasicInfoOnly));
        QCOMPARE(basic.details().providedMods, QStringList{ "example" });
    }

    void nestedModsThatCantBeRead_data()
    {
        QTest::addColumn<QString>("nestedJar");

        QTest::newRow("listed and not there") << "absent";
        QTest::newRow("with metadata that can't be read") << "unreadable";
        QTest::newRow("in a file cut short") << "cut";
    }

    void nestedModsThatCantBeRead()
    {
        // whatever they would provide isn't known then
        QFETCH(const QString, nestedJar);

        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QList<std::pair<QString, QByteArray>> files{ { "fabric.mod.json",
                                                       fabricMod("example", R"("jars": [{"file": "META-INF/jars/nested.jar"}])") } };
        if (nestedJar == "unreadable") {
            files.append({ "META-INF/jars/nested.jar", jarBytes(dir, { { "fabric.mod.json", "{\"id\": " } }) });
        } else if (nestedJar == "cut") {
            files.append(
                { "META-INF/jars/nested.jar", jarBytes(dir, { { "fabric.mod.json", fabricMod("nested", "") }, { "data.bin", noise() } }) });
        }
        const auto path = dir.filePath("mod.jar");
        QVERIFY(writeJar(path, files));
        if (nestedJar == "cut") {
            // as a download cut short in the nested jar, after the mod's own metadata
            QFile file(path);
            QVERIFY(file.resize(file.size() * 6 / 10));
        }

        Mod mod{ QFileInfo(path) };
        QVERIFY(ModUtils::process(mod));
        QCOMPARE(mod.mod_id(), "example");
        QVERIFY(mod.details().unreadNestedMods);
    }

    void jarJar()
    {
        // Forge and NeoForge load the jars in META-INF/jarjar, mods and libraries alike
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto nestedMod = jarBytes(
            dir, { { "META-INF/neoforge.mods.toml",
                     "modLoader=\"javafml\"\nloaderVersion=\"[4,)\"\nlicense=\"MIT\"\n[[mods]]\nmodId=\"flywheel\"\nversion=\"1\"\n" } });
        const auto library = jarBytes(dir, { { "org/example/Library.class", "\xCA\xFE\xBA\xBE" } });
        const auto path = dir.filePath("mod.jar");
        QVERIFY(writeJar(
            path, { { "META-INF/neoforge.mods.toml",
                      "modLoader=\"javafml\"\nloaderVersion=\"[4,)\"\nlicense=\"MIT\"\n[[mods]]\nmodId=\"create\"\nversion=\"6\"\n" },
                    { "META-INF/jarjar/metadata.json", R"({"jars": []})" },
                    { "META-INF/jarjar/flywheel.jar", nestedMod },
                    { "META-INF/jarjar/library.jar", library } }));

        Mod mod{ QFileInfo(path) };
        QVERIFY(ModUtils::process(mod));
        QCOMPARE(mod.details().providedMods, (QStringList{ "create", "flywheel" }));
        QVERIFY(!mod.details().unreadNestedMods);
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

    void versionFromTheManifest()
    {
        // a Forge mod can take its version from the jar's manifest, whose lines may end in \r\n
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("mod.jar");
        QVERIFY(writeJar(path, { { "META-INF/MANIFEST.MF", "Manifest-Version: 1.0\r\nimplementation-version: 1.2.3 \r\nBuilt-By: x\r\n" },
                                 { "META-INF/mods.toml",
                                   "modLoader=\"javafml\"\nloaderVersion=\"[47,)\"\nlicense=\"MIT\"\n[[mods]]\n"
                                   "modId=\"example\"\nversion=\"${file.jarVersion}\"\n" } }));
        Mod mod{ QFileInfo(path) };
        QVERIFY(ModUtils::process(mod));
        QCOMPARE(mod.version(), "1.2.3");
        QCOMPARE(mod.details().providedVersions.value("example"), QStringList{ "1.2.3" });
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
