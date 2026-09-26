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

#include <QEventLoop>
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

    static bool writeFabricMod(const QString& path, const QString& id)
    {
        return writeJar(path, "fabric.mod.json", QString(R"({"schemaVersion": 1, "id": "%1", "version": "1.0.0"})").arg(id).toUtf8());
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
    void duplicates()
    {
        QTemporaryDir mods;
        QVERIFY(mods.isValid());

        // two versions of one mod, left behind by an update
        QVERIFY(writeFabricMod(mods.filePath("sodium-1.0.jar"), "sodium"));
        QVERIFY(writeFabricMod(mods.filePath("sodium-1.1.jar"), "sodium"));
        // each loader only reads its own build, so these can sit side by side
        QVERIFY(writeFabricMod(mods.filePath("jei-fabric.jar"), "jei"));
        QVERIFY(writeForgeMod(mods.filePath("jei-forge.jar"), "jei"));
        // a disabled copy isn't loaded
        QVERIFY(writeFabricMod(mods.filePath("lithium-1.1.jar"), "lithium"));
        QVERIFY(writeFabricMod(mods.filePath("lithium-1.0.jar.disabled"), "lithium"));

        ModFolderModel model(mods.path(), nullptr, false, false);
        QVERIFY2(load(model), "The mods were never all read.");
        QCOMPARE(model.rowCount(), 6);

        QCOMPARE(model.duplicatesOf("sodium-1.0.jar"), QStringList{ "sodium-1.1.jar" });
        QCOMPARE(model.duplicatesOf("sodium-1.1.jar"), QStringList{ "sodium-1.0.jar" });
        QVERIFY(model.duplicatesOf("jei-fabric.jar").isEmpty());
        QVERIFY(model.duplicatesOf("jei-forge.jar").isEmpty());
        QVERIFY(model.duplicatesOf("lithium-1.1.jar").isEmpty());
        QVERIFY(model.duplicatesOf("lithium-1.0.jar.disabled").isEmpty());

        // disabling one of the pair settles it right away
        auto oldSodium = indexOf(model, "sodium-1.0.jar");
        QVERIFY(oldSodium.isValid());
        QVERIFY(model.setResourceEnabled({ oldSodium }, EnableAction::DISABLE));
        QVERIFY(model.duplicatesOf("sodium-1.1.jar").isEmpty());
        QVERIFY(model.duplicatesOf("sodium-1.0.jar.disabled").isEmpty());
    }
};

QTEST_GUILESS_MAIN(DuplicateModsTest)

#include "DuplicateMods_test.moc"
