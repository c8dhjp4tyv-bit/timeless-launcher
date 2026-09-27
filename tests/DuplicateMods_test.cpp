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

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <archive/ArchiveWriter.h>
#include <minecraft/mod/ModFolderModel.h>

class DuplicateModsTest : public QObject {
    Q_OBJECT

    static bool writeJar(const QString& path, const QString& metadataFile, const QByteArray& metadata)
    {
        MMCZip::ArchiveWriter jar(path);
        return jar.open() && jar.addFile(metadataFile, metadata) && jar.close();
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

    static QByteArray fabricMetadata(const QString& id, const QString& fields = {})
    {
        return QString(R"({"schemaVersion": 1, "id": "%1", "version": "1.0.0"%2})")
            .arg(id, fields.isEmpty() ? QString() : ", " + fields)
            .toUtf8();
    }

    /// Each enabled mod that needs mods that aren't there, or not in versions it takes, with their IDs
    static QStringList missingOf(ModFolderModel& model, ModPlatform::ModLoaderTypes loaders, const QSet<Mod*>& turnedOff = {})
    {
        QStringList lines;
        for (const auto& found : model.modsMissingDependencies(loaders, turnedOff)) {
            auto needs = found.ids;
            for (const auto& id : found.otherVersions) {
                needs << id + " (another version)";
            }
            lines << found.mod->fileinfo().fileName() + ": " + needs.join(", ");
        }
        return lines;
    }

    static bool writeFabricMod(const QString& path, const QString& id, const QString& version = "1.0.0")
    {
        return writeJar(path, "fabric.mod.json", QString(R"({"schemaVersion": 1, "id": "%1", "version": "%2"})").arg(id, version).toUtf8());
    }

    static bool writeForgeMod(const QString& path, const QString& id)
    {
        return writeJar(
            path, "META-INF/mods.toml",
            QString("modLoader=\"javafml\"\nloaderVersion=\"[47,)\"\nlicense=\"MIT\"\n[[mods]]\nmodId=\"%1\"\nversion=\"1.0.0\"\n")
                .arg(id)
                .toUtf8());
    }

    /// Scans the folder and waits until every mod in it has been read.
    static bool load(ModFolderModel& model)
    {
        QEventLoop loop;
        bool updated = false;
        connect(&model, &ModFolderModel::updateFinished, &loop, [&] {
            updated = true;
            if (!model.hasPendingParseTasks()) {
                loop.quit();
            }
        });
        connect(&model, &ModFolderModel::parseFinished, &loop, [&] {
            if (updated && !model.hasPendingParseTasks()) {
                loop.quit();
            }
        });

        QTimer timeout;
        timeout.setSingleShot(true);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(10000);

        if (!model.update()) {
            return false;
        }
        loop.exec();
        return timeout.isActive();
    }

    /// A Fabric mod's jar that ends before its end record, as when its download was cut short, while the metadata at its start
    /// is still whole
    static bool writeCutFabricMod(const QString& path, const QString& id)
    {
        QByteArray data;
        for (int i = 0; i < 20000; i++) {
            data.append(static_cast<char>((i * 7919 + i / 13) % 256));
        }
        MMCZip::ArchiveWriter jar(path);
        if (!jar.open() ||
            !jar.addFile("fabric.mod.json", QString(R"({"schemaVersion": 1, "id": "%1", "version": "1.0.0"})").arg(id).toUtf8()) ||
            !jar.addFile("assets/data.bin", data) || !jar.close()) {
            return false;
        }
        QFile file(path);
        return file.resize(file.size() * 6 / 10);
    }

    /// A jar holding only this Fabric metadata, to nest in another
    static QByteArray jarOf(const QTemporaryDir& dir, const QByteArray& metadata)
    {
        const auto path = dir.filePath("nested.jar.tmp");
        QFile::remove(path);
        if (!writeJar(path, "fabric.mod.json", metadata)) {
            return {};
        }
        QFile file(path);
        const auto bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        file.close();
        QFile::remove(path);
        return bytes;
    }

    static QModelIndex indexOf(ModFolderModel& model, const QString& fileName)
    {
        for (int row = 0; row < model.rowCount(); row++) {
            if (model.at(row).fileinfo().fileName() == fileName) {
                return model.index(row, 0);
            }
        }
        return {};
    }

   private slots:
    void otherGameVersion()
    {
        const QTemporaryDir mods;
        QVERIFY(mods.isValid());

        // as each loader's metadata gives the versions of Minecraft a mod works with
        QVERIFY(writeJar(mods.filePath("fabric-old.jar"), "fabric.mod.json",
                         R"({"schemaVersion": 1, "id": "old", "version": "1", "depends": {"minecraft": ">=1.20.1 <1.21"}})"));
        QVERIFY(writeJar(mods.filePath("fabric-new.jar"), "fabric.mod.json",
                         R"({"schemaVersion": 1, "id": "new", "version": "1", "depends": {"minecraft": ["1.20.6", "~1.21"]}})"));
        QVERIFY(writeJar(mods.filePath("fabric-any.jar"), "fabric.mod.json", R"({"schemaVersion": 1, "id": "any", "version": "1"})"));
        QVERIFY(writeJar(mods.filePath("quilt-old.jar"), "quilt.mod.json",
                         R"({"schema_version": 1, "quilt_loader": {"id": "quiltold", "version": "1",
                             "depends": [{"id": "minecraft", "versions": "1.20.1"}]}})"));
        QVERIFY(writeJar(mods.filePath("forge-old.jar"), "META-INF/mods.toml",
                         "modLoader=\"javafml\"\nloaderVersion=\"[47,)\"\nlicense=\"MIT\"\n[[mods]]\nmodId=\"forgeold\"\nversion=\"1\"\n"
                         "[[dependencies.forgeold]]\nmodId=\"minecraft\"\nmandatory=true\nversionRange=\"[1.20.1,1.20.2)\"\n"));
        QVERIFY(writeJar(mods.filePath("neoforge-new.jar"), "META-INF/neoforge.mods.toml",
                         "modLoader=\"javafml\"\nloaderVersion=\"[4,)\"\nlicense=\"MIT\"\n[[mods]]\nmodId=\"neonew\"\nversion=\"1\"\n"
                         "[[dependencies.neonew]]\nmodId=\"minecraft\"\ntype=\"required\"\nversionRange=\"[1.21,1.21.2)\"\n"));
        // one the loader doesn't hold to a version, and one turned off, which isn't loaded
        QVERIFY(writeJar(mods.filePath("forge-optional.jar"), "META-INF/mods.toml",
                         "modLoader=\"javafml\"\nloaderVersion=\"[47,)\"\nlicense=\"MIT\"\n[[mods]]\nmodId=\"optional\"\nversion=\"1\"\n"
                         "[[dependencies.optional]]\nmodId=\"minecraft\"\nmandatory=false\nversionRange=\"[1.20.1]\"\n"));
        QVERIFY(QFile::copy(mods.filePath("fabric-old.jar"), mods.filePath("fabric-off.jar.disabled")));

        ModFolderModel model(mods.path(), nullptr, false, false);
        QVERIFY(load(model));
        QCOMPARE(model.rowCount(), 8);

        const auto fileNames = [&model](const QString& minecraft) {
            QStringList names;
            for (const auto* mod : model.otherGameVersionMods(minecraft)) {
                names << mod->fileinfo().fileName();
            }
            return names;
        };
        QCOMPARE(fileNames("1.21"), QStringList({ "fabric-old.jar", "forge-old.jar", "quilt-old.jar" }));
        QCOMPARE(fileNames("1.20.1"), QStringList({ "fabric-new.jar", "neoforge-new.jar" }));
        // a snapshot isn't compared
        QCOMPARE(fileNames("24w14a"), QStringList());

        const auto& old = static_cast<const Mod&>(model.at(indexOf(model, "fabric-old.jar").row()));
        QCOMPARE(old.details().minecraft.text(), ">=1.20.1 <1.21");
    }

    void missingDependencies()
    {
        const QTemporaryDir mods;
        QVERIFY(mods.isValid());
        const auto fabric = [&mods](const QString& fileName, const QString& id, const QString& fields) {
            return writeJar(mods.filePath(fileName), "fabric.mod.json", fabricMetadata(id, fields));
        };

        // what the loader provides itself
        QVERIFY(fabric("builtin.jar", "builtin",
                       R"("depends": {"minecraft": "1.20.1", "fabricloader": ">=0.15", "java": ">=17", "mixinextras": "*"})"));
        // a mod that isn't here, and one that is turned off
        QVERIFY(fabric("iris.jar", "iris", R"("depends": {"sodium": "0.5.8", "fabric-api": "*"})"));
        QVERIFY(fabric("sodium.jar.disabled", "sodium", ""));
        // a mod provided under an older name, and one nested in the mod that needs it, as Mod Menu nests the parts of Fabric API it
        // uses
        QVERIFY(fabric("renamed.jar", "new-name", R"("provides": ["old-name"])"));
        QVERIFY(fabric("uses-old-name.jar", "usesold", R"("depends": {"old-name": "*"})"));
        QVERIFY(writeJar(mods.filePath("modmenu.jar"),
                         { { "fabric.mod.json", fabricMetadata("modmenu", R"("depends": {"fabric-screen-api-v1": ">=1.0.4"},
                                                                             "jars": [{"file": "META-INF/jars/screen.jar"}])") },
                           { "META-INF/jars/screen.jar",
                             jarOf(mods, R"({"schemaVersion": 1, "id": "fabric-screen-api-v1", "version": "2.0.6+b3afc78b77"})") } }));
        // a mod only for servers, which the loader leaves out, so it provides nothing and needs nothing
        QVERIFY(fabric("server.jar", "serverlib", R"("environment": "server", "depends": {"nothing": "*"})"));
        QVERIFY(fabric("needs-server.jar", "needsserver", R"("depends": {"serverlib": "*"})"));
        // and a Forge mod, which Fabric doesn't read
        QVERIFY(writeJar(mods.filePath("forge.jar"), "META-INF/mods.toml",
                         "modLoader=\"javafml\"\nloaderVersion=\"[47,)\"\nlicense=\"MIT\"\n[[mods]]\nmodId=\"forgemod\"\nversion=\"1\"\n"
                         "[[dependencies.forgemod]]\nmodId=\"jei\"\nmandatory=true\n"));

        ModFolderModel model(mods.path(), nullptr, false, false);
        QVERIFY(load(model));

        QCOMPARE(missingOf(model, ModPlatform::Fabric), (QStringList{ "iris.jar: fabric-api, sodium", "needs-server.jar: serverlib" }));
        QCOMPARE(missingOf(model, ModPlatform::Forge), QStringList{ "forge.jar: jei" });
        const auto providers = model.disabledProviders("sodium", ModPlatform::Fabric);
        QCOMPARE(providers.size(), 1);
        QCOMPARE(providers.first()->fileinfo().fileName(), "sodium.jar.disabled");
        QVERIFY(model.disabledProviders("fabric-api", ModPlatform::Fabric).isEmpty());

        // Quilt loads the mods in folders in the mods folder too, which aren't read, so it can't be told what is missing
        QVERIFY(QDir(mods.path()).mkdir(".index"));
        QVERIFY(QDir(mods.path()).mkdir("old.disabled"));
        QCOMPARE(missingOf(model, ModPlatform::Quilt).size(), 2);
        QVERIFY(QDir(mods.path()).mkdir("more"));
        QVERIFY(missingOf(model, ModPlatform::Quilt).isEmpty());
        QCOMPARE(missingOf(model, ModPlatform::Fabric).size(), 2);
    }

    void missingDependencyVersions()
    {
        // Fabric holds a mod to the versions of the mods it needs, as Iris to the one version of Sodium it was made with, and
        // leaves out build metadata after a +
        const QTemporaryDir mods;
        QVERIFY(mods.isValid());
        const auto fabric = [&mods](const QString& fileName, const QString& fields) {
            return writeJar(mods.filePath(fileName), "fabric.mod.json", QString("{\"schemaVersion\": 1, %1}").arg(fields).toUtf8());
        };
        QVERIFY(fabric("sodium.jar", R"("id": "sodium", "version": "0.5.11+mc1.20.1")"));
        QVERIFY(fabric("fabric-api.jar", R"("id": "fabric-api", "version": "0.92.2+1.20.1", "provides": ["fabric"])"));
        QVERIFY(fabric("iris-old.jar", R"("id": "irisold", "version": "1.7.0", "depends": {"sodium": "0.5.8"})"));
        QVERIFY(fabric("iris-new.jar", R"("id": "irisnew", "version": "1.7.2", "depends": {"sodium": "0.5.11"})"));
        QVERIFY(fabric("needs-newer-api.jar", R"("id": "newerapi", "version": "1", "depends": {"fabric": ">=0.95", "sodium": "~0.5"})"));
        QVERIFY(fabric("needs-api.jar", R"("id": "api", "version": "1", "depends": {"fabric-api": [">=0.90.0 <0.93", "1.0.x"]})"));
        // a version that can't be read may be the one needed, and a mod the loader comes with, as MixinExtras, takes any version even
        // when a mod nests an older copy of it
        QVERIFY(fabric("placeholder.jar", R"("id": "placeholder", "version": "${version}")"));
        QVERIFY(fabric("needs-placeholder.jar", R"("id": "needsplaceholder", "version": "1", "depends": {"placeholder": ">=2"})"));
        QVERIFY(writeJar(
            mods.filePath("nests-mixinextras.jar"),
            { { "fabric.mod.json", fabricMetadata("nester", R"("jars": [{"file": "META-INF/jars/mixinextras.jar"}])") },
              { "META-INF/jars/mixinextras.jar", jarOf(mods, R"({"schemaVersion": 1, "id": "mixinextras", "version": "0.2.0"})") } }));
        QVERIFY(fabric("needs-mixinextras.jar", R"("id": "needsmixin", "version": "1", "depends": {"mixinextras": ">=0.3.2"})"));
        // and a Forge mod, going by Maven's ranges
        QVERIFY(writeJar(mods.filePath("jei.jar"), "META-INF/mods.toml",
                         "modLoader=\"javafml\"\nloaderVersion=\"[47,)\"\nlicense=\"MIT\"\n[[mods]]\nmodId=\"jei\"\nversion=\"11.6.0\"\n"));
        QVERIFY(writeJar(mods.filePath("needs-jei.jar"), "META-INF/mods.toml",
                         "modLoader=\"javafml\"\nloaderVersion=\"[47,)\"\nlicense=\"MIT\"\n[[mods]]\nmodId=\"jeiaddon\"\nversion=\"1\"\n"
                         "[[dependencies.jeiaddon]]\nmodId=\"jei\"\nmandatory=true\nversionRange=\"[15.0,)\"\n"));

        ModFolderModel model(mods.path(), nullptr, false, false);
        QVERIFY(load(model));

        QCOMPARE(missingOf(model, ModPlatform::Fabric),
                 (QStringList{ "iris-old.jar: sodium (another version)", "needs-newer-api.jar: fabric (another version)" }));
        QCOMPARE(missingOf(model, ModPlatform::Forge), QStringList{ "needs-jei.jar: jei (another version)" });
        QCOMPARE(model.providedVersions("sodium", ModPlatform::Fabric), QStringList{ "0.5.11+mc1.20.1" });
        const auto found = model.modsMissingDependencies(ModPlatform::Fabric).first();
        QCOMPARE(model.describeNeeds(found, ModPlatform::Fabric), QStringList{ "sodium 0.5.8 (0.5.11+mc1.20.1 is here)" });
    }

    void missingDependenciesInTurn()
    {
        // turning off a mod that needs a mod that isn't there leaves out what needs that mod in turn
        const QTemporaryDir mods;
        QVERIFY(mods.isValid());
        QVERIFY(writeJar(mods.filePath("create.jar"), "fabric.mod.json", fabricMetadata("create", R"("depends": {"flywheel": "*"})")));
        QVERIFY(writeJar(mods.filePath("addon.jar"), "fabric.mod.json", fabricMetadata("addon", R"("depends": {"create": "*"})")));

        ModFolderModel model(mods.path(), nullptr, false, false);
        QVERIFY(load(model));

        QCOMPARE(missingOf(model, ModPlatform::Fabric), QStringList{ "create.jar: flywheel" });
        const QSet<Mod*> create{ &model.at(indexOf(model, "create.jar").row()) };
        QCOMPARE(missingOf(model, ModPlatform::Fabric, create), QStringList{ "addon.jar: create" });
    }

    void missingDependenciesUnknown_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<QByteArray>("metadata");

        QTest::newRow("metadata that can't be read") << "broken.jar" << QByteArray(R"({"schemaVersion": 1, "id": )");
        QTest::newRow("a nested mod that isn't there")
            << "nesting.jar" << fabricMetadata("nesting", R"("jars": [{"file": "META-INF/jars/gone.jar"}])");
    }

    void missingDependenciesUnknown()
    {
        // a mod that couldn't be read may be the one that provides what another needs
        QFETCH(const QString, fileName);
        QFETCH(const QByteArray, metadata);

        const QTemporaryDir mods;
        QVERIFY(mods.isValid());
        QVERIFY(writeJar(mods.filePath("iris.jar"), "fabric.mod.json", fabricMetadata("iris", R"("depends": {"sodium": "*"})")));
        QVERIFY(writeJar(mods.filePath(fileName), "fabric.mod.json", metadata));

        ModFolderModel model(mods.path(), nullptr, false, false);
        QVERIFY(load(model));
        QVERIFY(missingOf(model, ModPlatform::Fabric).isEmpty());

        // unless it is turned off
        QVERIFY(model.setModsEnabled({ indexOf(model, fileName) }, EnableAction::DISABLE));
        QCOMPARE(missingOf(model, ModPlatform::Fabric), QStringList{ "iris.jar: sodium" });
    }

    void damaged()
    {
        const QTemporaryDir mods;
        QVERIFY(mods.isValid());

        QVERIFY(writeFabricMod(mods.filePath("whole.jar"), "whole"));
        QVERIFY(writeCutFabricMod(mods.filePath("cut.jar"), "cut"));
        // a web page saved in place of a mod
        QFile page(mods.filePath("page.jar"));
        QVERIFY(page.open(QIODevice::WriteOnly));
        QVERIFY(page.write("<!DOCTYPE html><html><body>Your download will start shortly.</body></html>") > 0);
        page.close();
        // one that is turned off isn't loaded
        QVERIFY(QFile::copy(page.fileName(), mods.filePath("other-page.jar.disabled")));

        ModFolderModel model(mods.path(), nullptr, false, false);
        QVERIFY(load(model));
        QCOMPARE(model.rowCount(), 4);

        QCOMPARE(model.damagedMods(), QStringList({ "cut.jar", "page.jar" }));
        // the archive library reads a jar that was cut short as far as it goes, which is how it passed for a whole mod
        const auto cut = indexOf(model, "cut.jar");
        QVERIFY(cut.isValid());
        QCOMPARE(static_cast<const Mod&>(model.at(cut.row())).mod_id(), "cut");
        QVERIFY(!static_cast<const Mod&>(model.at(indexOf(model, "whole.jar").row())).details().damaged);
    }

    void duplicates()
    {
        QTemporaryDir mods;
        QVERIFY(mods.isValid());

        // two versions of one mod, left behind by an update
        QVERIFY(writeFabricMod(mods.filePath("sodium-1.0.jar"), "sodium"));
        QVERIFY(writeFabricMod(mods.filePath("sodium-1.1.jar"), "sodium"));
        // the same for a Forge mod
        QVERIFY(writeForgeMod(mods.filePath("create-0.5.jar"), "create"));
        QVERIFY(writeForgeMod(mods.filePath("create-0.6.jar"), "create"));
        // each loader only reads its own build, so these can sit side by side
        QVERIFY(writeFabricMod(mods.filePath("jei-fabric.jar"), "jei"));
        QVERIFY(writeForgeMod(mods.filePath("jei-forge.jar"), "jei"));
        // a disabled copy isn't loaded
        QVERIFY(writeFabricMod(mods.filePath("lithium-1.1.jar"), "lithium"));
        QVERIFY(writeFabricMod(mods.filePath("lithium-1.0.jar.disabled"), "lithium"));

        ModFolderModel model(mods.path(), nullptr, false, false);
        QVERIFY2(load(model), "The mods were never all read.");
        QCOMPARE(model.rowCount(), 8);

        QCOMPARE(model.duplicatesOf("sodium-1.0.jar"), QStringList{ "sodium-1.1.jar" });
        QCOMPARE(model.duplicatesOf("sodium-1.1.jar"), QStringList{ "sodium-1.0.jar" });
        QCOMPARE(model.duplicatesOf("create-0.5.jar"), QStringList{ "create-0.6.jar" });
        QVERIFY(model.duplicatesOf("jei-fabric.jar").isEmpty());
        QVERIFY(model.duplicatesOf("jei-forge.jar").isEmpty());
        QVERIFY(model.duplicatesOf("lithium-1.1.jar").isEmpty());
        QVERIFY(model.duplicatesOf("lithium-1.0.jar.disabled").isEmpty());

        // a loader only trips over the copies it reads
        const QList<QStringList> sodium{ QStringList{ "sodium-1.0.jar", "sodium-1.1.jar" } };
        const QList<QStringList> create{ QStringList{ "create-0.5.jar", "create-0.6.jar" } };
        QCOMPARE(model.duplicateGroups(ModPlatform::Fabric), sodium);
        QCOMPARE(model.duplicateGroups(ModPlatform::Quilt), sodium);
        QCOMPARE(model.duplicateGroups(ModPlatform::Forge), create);
        QCOMPARE(model.duplicateGroups(ModPlatform::NeoForge), create);

        // disabling one of the pair settles it right away
        auto oldSodium = indexOf(model, "sodium-1.0.jar");
        QVERIFY(oldSodium.isValid());
        QVERIFY(model.setResourceEnabled({ oldSodium }, EnableAction::DISABLE));
        QVERIFY(model.duplicatesOf("sodium-1.1.jar").isEmpty());
        QVERIFY(model.duplicatesOf("sodium-1.0.jar.disabled").isEmpty());
        QVERIFY(model.duplicateGroups(ModPlatform::Fabric).isEmpty());
        QCOMPARE(model.duplicateGroups(ModPlatform::Forge), create);
    }

    void olderDuplicates()
    {
        const QTemporaryDir mods;
        QVERIFY(mods.isValid());

        // the latest version stays, going by the numbers rather than the text
        QVERIFY(writeFabricMod(mods.filePath("sodium-0.5.3.jar"), "sodium", "0.5.3"));
        QVERIFY(writeFabricMod(mods.filePath("sodium-0.5.10.jar"), "sodium", "0.5.10"));
        QVERIFY(writeFabricMod(mods.filePath("sodium-0.5.8.jar"), "sodium", "0.5.8"));
        // with the same version, the file changed last stays
        QVERIFY(writeFabricMod(mods.filePath("lithium-a.jar"), "lithium", "0.11.2"));
        QVERIFY(writeFabricMod(mods.filePath("lithium-b.jar"), "lithium", "0.11.2"));
        for (const auto& [fileName, modified] : { std::pair{ "lithium-a.jar", QDateTime(QDate(2026, 9, 1), QTime(12, 0)) },
                                                  std::pair{ "lithium-b.jar", QDateTime(QDate(2026, 9, 20), QTime(12, 0)) } }) {
            QFile file(mods.filePath(fileName));
            QVERIFY(file.open(QIODevice::ReadWrite));
            QVERIFY(file.setFileTime(modified, QFileDevice::FileModificationTime));
        }
        // not duplicates for Fabric
        QVERIFY(writeFabricMod(mods.filePath("iris.jar"), "iris", "1.7.0"));
        QVERIFY(writeForgeMod(mods.filePath("create-0.5.jar"), "create"));
        QVERIFY(writeForgeMod(mods.filePath("create-0.6.jar"), "create"));

        ModFolderModel model(mods.path(), nullptr, false, false);
        QVERIFY2(load(model), "The mods were never all read.");
        QCOMPARE(model.rowCount(), 8);

        auto older = model.olderDuplicates(ModPlatform::Fabric);
        older.sort();
        QCOMPARE(older, (QStringList{ "lithium-a.jar", "sodium-0.5.3.jar", "sodium-0.5.8.jar" }));

        QModelIndexList indexes;
        for (const auto& fileName : older) {
            indexes << indexOf(model, fileName);
        }
        QVERIFY(model.setModsEnabled(indexes, EnableAction::DISABLE));
        QVERIFY(model.duplicateGroups(ModPlatform::Fabric).isEmpty());
        QVERIFY(model.olderDuplicates(ModPlatform::Fabric).isEmpty());
        QVERIFY(QFileInfo::exists(mods.filePath("sodium-0.5.10.jar")));
        QVERIFY(QFileInfo::exists(mods.filePath("lithium-b.jar")));
        QVERIFY(QFileInfo::exists(mods.filePath("sodium-0.5.3.jar.disabled")));
    }
};

QTEST_GUILESS_MAIN(DuplicateModsTest)

#include "DuplicateMods_test.moc"
