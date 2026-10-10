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
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QUrl>

#include <QStringList>

#include <modplatform/modrinth/ModrinthAPI.h>

class ModrinthAPITest : public QObject {
    Q_OBJECT

   private slots:

    void searchFacets_data()
    {
        QTest::addColumn<QStringList>("categories");
        QTest::addColumn<ModPlatform::ModLoaderTypes>("loaders");
        QTest::addColumn<QString>("facets");

        // Modrinth ANDs separate arrays and ORs the entries of one array, so each category needs an
        // array of its own for the search to narrow down as more of them are ticked.
        QTest::newRow("no categories") << QStringList{} << ModPlatform::ModLoaderTypes{} << R"([["project_type:mod"]])";
        QTest::newRow("one category") << QStringList{ "adventure" } << ModPlatform::ModLoaderTypes{}
                                      << R"([["categories:adventure"],["project_type:mod"]])";
        QTest::newRow("two categories") << QStringList{ "adventure", "magic" } << ModPlatform::ModLoaderTypes{}
                                        << R"([["categories:adventure"],["categories:magic"],["project_type:mod"]])";

        // Loaders share one array on purpose: a mod is built for one of them, so they have to be ORed.
        QTest::newRow("two loaders") << QStringList{} << ModPlatform::ModLoaderTypes(ModPlatform::Fabric | ModPlatform::Quilt)
                                     << R"([["categories:fabric","categories:quilt"],["project_type:mod"]])";
    }

    void searchFacets()
    {
        QFETCH(QStringList, categories);
        QFETCH(ModPlatform::ModLoaderTypes, loaders);
        QFETCH(QString, facets);

        ResourceAPI::SearchArgs args{};
        args.type = ModPlatform::ResourceType::Mod;
        if (!categories.isEmpty()) {
            args.categoryIds = categories;
        }
        if (loaders != 0) {
            args.loaders = loaders;
        }

        ModrinthAPI api;
        auto url = api.getSearchURL(args);

        QVERIFY(url.has_value());
        QCOMPARE(url->section("facets=", 1), facets);
    }

    void splitProjectIds_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<int>("perRequest");
        QTest::addColumn<QList<int>>("groupSizes");

        QTest::newRow("none") << 0 << 100 << QList<int>{};
        QTest::newRow("one") << 1 << 100 << QList<int>{ 1 };
        QTest::newRow("a full group") << 100 << 100 << QList<int>{ 100 };
        QTest::newRow("one over") << 101 << 100 << QList<int>{ 100, 1 };
        QTest::newRow("hundreds of mods") << 650 << 100 << QList<int>{ 100, 100, 100, 100, 100, 100, 50 };
        QTest::newRow("small groups") << 5 << 2 << QList<int>{ 2, 2, 1 };
        // a group of nothing would be an endless loop, and is taken as one
        QTest::newRow("a group of nothing") << 3 << 0 << QList<int>{ 1, 1, 1 };
    }

    void splitProjectIds()
    {
        QFETCH(int, count);
        QFETCH(int, perRequest);
        QFETCH(QList<int>, groupSizes);

        QStringList ids;
        for (int i = 0; i < count; ++i) {
            ids << QString("id%1").arg(i);
        }
        const auto groups = ModrinthAPI::splitProjectIds(ids, perRequest);

        QList<int> sizes;
        QStringList again;
        for (const auto& group : groups) {
            sizes << group.size();
            again << group;
        }
        QCOMPARE(sizes, groupSizes);
        // nothing lost, nothing twice, in the order they came
        QCOMPARE(again, ids);
    }

    void aLongListIsAskedForInSeveralAddresses()
    {
        QStringList ids;
        for (int i = 0; i < 700; ++i) {
            ids << QString("AANobb%1").arg(i, 2, 10, QChar('0')).left(8);
        }
        for (const auto& group : ModrinthAPI::splitProjectIds(ids)) {
            // about 15 characters for each project, so a group stays far from what an address can hold
            QVERIFY(QUrl(ModrinthAPI::getMultipleModInfoURL(group)).toEncoded().size() < 3000);
        }
    }

    void mergeProjectReplies()
    {
        const auto merged = ModrinthAPI::mergeProjectReplies({ R"([{"id":"a"},{"id":"b"}])", R"([{"id":"c"}])", "[]", R"([{"id":"d"}])" });
        const auto doc = QJsonDocument::fromJson(merged);
        QVERIFY(doc.isArray());
        QStringList ids;
        for (const auto& project : doc.array()) {
            ids << project.toObject().value("id").toString();
        }
        QCOMPARE(ids, (QStringList{ "a", "b", "c", "d" }));

        // one reply is the reply, byte for byte
        QCOMPARE(ModrinthAPI::mergeProjectReplies({ R"( [ {"id":"a"} ] )" }), QByteArray(R"( [ {"id":"a"} ] )"));
        // a reply that isn't a list (an error of the site) is what the caller sees, as with a single request
        const QByteArray error = R"({"error":"invalid_input"})";
        QCOMPARE(ModrinthAPI::mergeProjectReplies({ R"([{"id":"a"}])", error, R"([{"id":"c"}])" }), error);
        QCOMPARE(ModrinthAPI::mergeProjectReplies({ R"([{"id":"a"}])", "not json" }), QByteArray("not json"));
    }
};

QTEST_GUILESS_MAIN(ModrinthAPITest)

#include "ModrinthAPI_test.moc"
