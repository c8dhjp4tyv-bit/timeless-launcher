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

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <minecraft/mod/Mod.h>
#include <modplatform/helpers/ExportToModList.h>

class ExportToModListTest : public QObject {
    Q_OBJECT

    static ExportToModList::OptionalData everything()
    {
        return ExportToModList::OptionalData(ExportToModList::Authors) | ExportToModList::Url | ExportToModList::Version |
               ExportToModList::FileName;
    }

    /// A mod that only has what the export reads
    std::unique_ptr<Mod> mod(const QString& fileName,
                             const QString& name,
                             const QString& version,
                             const QString& homepage,
                             const QStringList& authors) const
    {
        const auto path = m_dir.filePath(fileName);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            qFatal("Can't create %s", qPrintable(path));
        }
        file.close();

        auto mod = std::make_unique<Mod>(QFileInfo(path));
        ModDetails details;
        details.name = name;
        details.version = version;
        details.homeurl = homepage;
        details.authors = authors;
        mod->setDetails(details);
        return mod;
    }

    QTemporaryDir m_dir;

   private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void csvOfPlainMods()
    {
        auto first = mod("lithium.jar", "Lithium", "0.12.1", "https://modrinth.com/mod/lithium", { "JellySquid" });
        auto second = mod("sodium.jar", "Sodium", "0.5.8", "https://modrinth.com/mod/sodium", { "JellySquid", "IMS" });

        QCOMPARE(ExportToModList::exportToModList({ first.get(), second.get() }, ExportToModList::CSV, everything()),
                 "Lithium,https://modrinth.com/mod/lithium,0.12.1,JellySquid,lithium.jar\n"
                 "Sodium,https://modrinth.com/mod/sodium,0.5.8,\"JellySquid,IMS\",sodium.jar");
        QCOMPARE(ExportToModList::exportToModList({ first.get() }, ExportToModList::CSV, ExportToModList::None), "Lithium");
    }

    /// A field with a comma, a quote or a line break in it is put in quotes, and the quotes in it are doubled, so that every line has
    /// the fields it should
    void csvOfFieldsThatHaveMoreInThem()
    {
        auto commas = mod("ae2.jar", "Applied Energistics 2, Unofficial", "1.0, build 5", "https://example.org/a,b", { "Smith, John" });
        auto quotes = mod("best.jar", "The \"Best\" Mod", "1.0", "", { "Alice", "Bob \"B\"" });
        auto lines = mod("lines.jar", "Two\nLines", "1.0", "", {});

        QCOMPARE(ExportToModList::exportToModList({ commas.get(), quotes.get(), lines.get() }, ExportToModList::CSV, everything()),
                 "\"Applied Energistics 2, Unofficial\",\"https://example.org/a,b\",\"1.0, build 5\",\"Smith, John\",ae2.jar\n"
                 "\"The \"\"Best\"\" Mod\",,1.0,\"Alice,Bob \"\"B\"\"\",best.jar\n"
                 "\"Two\nLines\",,1.0,,lines.jar");
    }
};

QTEST_GUILESS_MAIN(ExportToModListTest)

#include "ExportToModList_test.moc"
