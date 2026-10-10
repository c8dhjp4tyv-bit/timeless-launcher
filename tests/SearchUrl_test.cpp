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
#include <QUrl>

#include <modplatform/flame/FlameAPI.h>
#include <modplatform/modrinth/ModrinthAPI.h>

/// What the search of a mod site is sent as, and what the site makes of it
class SearchUrlTest : public QObject {
    Q_OBJECT

    struct Query {
        QStringList names;              // in the order they were sent
        QMap<QString, QString> values;  // as the site reads them: a plus sign is a space
        bool hasFragment = false;
    };

    static Query readQuery(const QString& address)
    {
        // the way the request is sent: the address as the network layer encodes it
        const QUrl url(address);
        const auto encoded = QString::fromLatin1(url.toEncoded());

        Query query;
        query.hasFragment = url.hasFragment();
        const auto questionMark = encoded.indexOf('?');
        for (const auto& pair : encoded.mid(questionMark + 1).split('&', Qt::SkipEmptyParts)) {
            const auto separator = pair.indexOf('=');
            const auto name = separator < 0 ? pair : pair.left(separator);
            const auto value = separator < 0 ? QString() : pair.mid(separator + 1);
            query.names.append(name);
            query.values.insert(name, QUrl::fromPercentEncoding(QString(value).replace('+', ' ').toLatin1()));
        }
        return query;
    }

    static void addTexts()
    {
        QTest::addColumn<QString>("text");

        QTest::newRow("a name") << "sodium";
        QTest::newRow("words") << "create steam and rails";
        QTest::newRow("an ampersand") << "Iris & Sodium";
        QTest::newRow("a hash") << "C# bindings";
        QTest::newRow("a plus sign") << "Sodium+ extra";
        QTest::newRow("only plus signs") << "C++";
        QTest::newRow("an equals sign") << "a=b";
        QTest::newRow("a percent sign") << "100% and %41";
        QTest::newRow("a question mark") << "why?";
        QTest::newRow("quotes and brackets") << "\"quoted\" [bracket] {brace}";
        QTest::newRow("non-ASCII") << "Sihirli ığdır çağrı İstanbul";
    }

   private slots:
    void modrinth_data() { addTexts(); }

    void modrinth()
    {
        QFETCH(const QString, text);

        ResourceAPI::SearchArgs args{};
        args.type = ModPlatform::ResourceType::Mod;
        args.search = text;

        const auto address = ModrinthAPI().getSearchURL(args);
        QVERIFY(address.has_value());

        const auto query = readQuery(*address);
        QVERIFY(!query.hasFragment);
        // the arguments are the ones that were built, and the text is the one that was typed
        QCOMPARE(query.names, (QStringList{ "offset", "limit", "query", "facets" }));
        QCOMPARE(query.values.value("query"), text);
        QCOMPARE(query.values.value("facets"), QString(R"([["project_type:mod"]])"));
    }

    void flame_data() { addTexts(); }

    void flame()
    {
        QFETCH(const QString, text);

        ResourceAPI::SearchArgs args{};
        args.type = ModPlatform::ResourceType::Mod;
        args.search = text;

        const auto address = FlameAPI::get().getSearchURL(args);
        QVERIFY(address.has_value());

        const auto query = readQuery(*address);
        QVERIFY(!query.hasFragment);
        QCOMPARE(query.names, (QStringList{ "gameId", "classId", "index", "pageSize", "searchFilter", "sortOrder" }));
        QCOMPARE(query.values.value("searchFilter"), text);
        QCOMPARE(query.values.value("sortOrder"), QString("desc"));
    }
};

QTEST_GUILESS_MAIN(SearchUrlTest)

#include "SearchUrl_test.moc"
