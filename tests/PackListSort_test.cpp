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

#include <QStandardItemModel>
#include <QTest>

#include <modplatform/atlauncher/ATLPackIndex.h>
#include <modplatform/ftb/FTBPackManifest.h>
#include <modplatform/import_ftb/PackHelpers.h>
#include <modplatform/legacy_ftb/PackHelpers.h>
#include <ui/pages/modplatform/atlauncher/AtlFilterModel.h>
#include <ui/pages/modplatform/ftb/FtbFilterModel.h>
#include <ui/pages/modplatform/import_ftb/ListModel.h>
#include <ui/pages/modplatform/legacy_ftb/ListModel.h>

namespace {

/// A pack as far as the order of a list goes: its name, and a tag to tell packs of the same name apart
struct Pack {
    QString name;
    QString tag;
};

/// The order of a list is what its `lessThan()` says. That is not public, and it is what the test asks.
template <typename Filter>
struct Open : Filter {
    using Filter::Filter;
    using Filter::lessThan;
};

FTB::Modpack ftbPack(const QString& name)
{
    FTB::Modpack pack{};
    pack.name = name;
    return pack;
}

ATLauncher::IndexedPack atlPack(const QString& name)
{
    ATLauncher::IndexedPack pack{};
    pack.name = name;
    return pack;
}

LegacyFTB::Modpack legacyFtbPack(const QString& name)
{
    LegacyFTB::Modpack pack{};
    pack.name = name;
    return pack;
}

FTBImportAPP::Modpack importPack(const QString& name)
{
    FTBImportAPP::Modpack pack{};
    pack.name = name;
    return pack;
}

/// Fills the list with the packs and has it sort them by name. The source model belongs to the list.
template <typename Filter, typename Make>
QStandardItemModel* fill(Filter& filter, const QList<Pack>& packs, Make make)
{
    auto* source = new QStandardItemModel(&filter);
    for (const auto& pack : packs) {
        auto* item = new QStandardItem(pack.tag);
        item->setData(QVariant::fromValue(make(pack.name)), Qt::UserRole);
        source->appendRow(item);
    }
    filter.setSourceModel(source);
    filter.setSorting(Filter::ByName);
    return source;
}

/// The tags of the packs in the order a list sorted the way given shows them
template <typename Filter, typename Make>
QStringList shown(const QList<Pack>& packs, Make make, Qt::SortOrder order)
{
    Filter filter;
    fill(filter, packs, make);
    filter.sort(0, order);

    QStringList tags;
    for (int row = 0; row < filter.rowCount(); ++row) {
        tags << filter.index(row, 0).data().toString();
    }
    return tags;
}

/// The pairs of packs for which a list says that each goes before the other. A pack alone is one of those pairs as well.
template <typename Filter, typename Make>
QStringList eachBeforeTheOther(const QList<Pack>& packs, Make make)
{
    Open<Filter> filter;
    auto* source = fill(filter, packs, make);

    QStringList pairs;
    for (int a = 0; a < source->rowCount(); ++a) {
        for (int b = a; b < source->rowCount(); ++b) {
            const auto first = source->index(a, 0);
            const auto second = source->index(b, 0);
            if (filter.lessThan(first, second) && filter.lessThan(second, first)) {
                pairs << first.data().toString() + " and " + second.data().toString();
            }
        }
    }
    return pairs;
}

}  // namespace

class PackListSortTest : public QObject {
    Q_OBJECT

    template <typename Filter, typename Make>
    static void byName(Make make)
    {
        const QList<Pack> packs{
            { "Gamma", "gamma" }, { "Alpha", "alpha" }, { "Pack 10", "ten" }, { "Beta", "beta" }, { "Pack 2", "two" }
        };

        // A list starts with the first order that a header offers, which is "descending", and that shows the names from A to Z.
        // Numbers count by their value.
        QCOMPARE(shown<Filter>(packs, make, Qt::DescendingOrder), QStringList({ "alpha", "beta", "gamma", "two", "ten" }));
        QCOMPARE(shown<Filter>(packs, make, Qt::AscendingOrder), QStringList({ "ten", "two", "gamma", "beta", "alpha" }));
    }

    /// Sorting is only defined when no pack goes before itself or before another with the same name while that one goes before it
    /// too. Packs with the same name do exist, for example when a pack comes in more than one edition.
    template <typename Filter, typename Make>
    static void sameName(Make make)
    {
        const QList<Pack> packs{ { "Alpha", "first alpha" }, { "Beta", "beta" }, { "Alpha", "second alpha" }, { "Alpha", "third alpha" } };

        QCOMPARE(eachBeforeTheOther<Filter>(packs, make), QStringList());
    }

   private slots:
    void ftbPacks()
    {
        byName<Ftb::FilterModel>(ftbPack);
        sameName<Ftb::FilterModel>(ftbPack);
    }

    void atlauncherPacks()
    {
        byName<Atl::FilterModel>(atlPack);
        sameName<Atl::FilterModel>(atlPack);
    }

    void legacyFtbPacks()
    {
        byName<LegacyFTB::FilterModel>(legacyFtbPack);
        sameName<LegacyFTB::FilterModel>(legacyFtbPack);
    }

    void importedFtbPacks()
    {
        byName<FTBImportAPP::FilterModel>(importPack);
        sameName<FTBImportAPP::FilterModel>(importPack);
    }
};

QTEST_GUILESS_MAIN(PackListSortTest)

#include "PackListSort_test.moc"
