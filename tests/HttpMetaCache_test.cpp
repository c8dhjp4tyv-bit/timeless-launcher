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

#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <FileSystem.h>
#include <net/HttpMetaCache.h>

class HttpMetaCacheTest : public QObject {
    Q_OBJECT

   private slots:
    void resolveEntry_data()
    {
        QTest::addColumn<QString>("resourcePath");
        QTest::addColumn<QString>("expectedInBase");

        QTest::newRow("a file") << "logo.png" << "logo.png";
        QTest::newRow("in a folder") << "logos/a.png" << "logos/a.png";
        // names that only look like it
        QTest::newRow("dots in a name") << "logos/..a.png" << "logos/..a.png";
        QTest::newRow("dots after a name") << "logos/a..png" << "logos/a..png";
        QTest::newRow("a version") << "logos/1.2..3" << "logos/1.2..3";
        // a path that leads out of the folder of the cache
        QTest::newRow("up and out") << "../escaped.png" << "-/escaped.png";
        QTest::newRow("up from a folder") << "logos/../../escaped.png" << "logos/-/-/escaped.png";
        QTest::newRow("up many times") << "../../../../escaped" << "-/-/-/-/escaped";
        QTest::newRow("backslashes") << "logos\\..\\..\\escaped.png" << "logos\\-\\-\\escaped.png";
        QTest::newRow("only up") << ".." << "-";
        // the way the pack installs make it of an address: host and path
        const QUrl encoded("https://pack.example.com/%2e%2e%2f%2e%2e%2fescaped.zip");
        QTest::newRow("the path of an address") << encoded.host() + '/' + encoded.path() << "pack.example.com/-/-/escaped.zip";
        const QUrl plain("https://pack.example.com/a/../../../escaped.zip");
        QTest::newRow("the path of another address") << plain.host() + '/' + plain.path() << "pack.example.com/a/-/-/-/escaped.zip";
        // a backslash stays in a file name from an address on Linux, and is a separator on Windows
        QTest::newRow("a file name with backslashes") << QUrl("https://cdn.example.com/a%5c..%5c..%5cx.png").fileName() << "a\\-\\-\\x.png";
    }

    void resolveEntry()
    {
        QFETCH(QString, resourcePath);
        QFETCH(QString, expectedInBase);

        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto base = temp.filePath("cache/logos-base");
        HttpMetaCache cache(temp.filePath("index.json"));
        cache.addBase("test", base);

        auto entry = cache.resolveEntry("test", resourcePath);
        QVERIFY(entry);
        const auto full = entry->getFullPath();
        // whatever is stored for it is in the folder of its base, and where a download would put it
        QVERIFY2(FS::isInside(full, base), qPrintable(full));
        QCOMPARE(full, FS::PathCombine(base, expectedInBase));
    }
};

QTEST_GUILESS_MAIN(HttpMetaCacheTest)

#include "HttpMetaCache_test.moc"
