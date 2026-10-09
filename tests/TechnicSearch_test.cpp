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

#include "ui/pages/modplatform/technic/TechnicModel.h"

class TechnicSearchTest : public QObject {
    Q_OBJECT

    static constexpr auto s_base = "https://api.technicpack.net/";
    static constexpr auto s_build = "multimc";

    /// The arguments of an address as the site reads them, in the order they were sent: a plus sign is a space
    static QList<QPair<QString, QString>> readArguments(const QUrl& url)
    {
        QList<QPair<QString, QString>> arguments;
        for (const auto& pair : QString::fromLatin1(url.toEncoded()).section('?', 1).split('&', Qt::SkipEmptyParts)) {
            const auto separator = pair.indexOf('=');
            arguments.append(
                { pair.left(separator), QUrl::fromPercentEncoding(QString(pair.mid(separator + 1)).replace('+', ' ').toLatin1()) });
        }
        return arguments;
    }

    using Arguments = QList<QPair<QString, QString>>;

   private slots:
    void searchUrl_data()
    {
        QTest::addColumn<QString>("term");
        QTest::addColumn<QString>("clientId");
        QTest::addColumn<QString>("address");  // up to the arguments
        QTest::addColumn<Arguments>("arguments");
        QTest::addColumn<bool>("single");

        QTest::newRow("trending") << "" << "" << "https://api.technicpack.net/trending" << Arguments{ { "build", "multimc" } } << false;
        QTest::newRow("trending with a client id") << "" << "abc123" << "https://api.technicpack.net/trending"
                                                   << Arguments{ { "build", "multimc" }, { "cid", "abc123" } } << false;
        QTest::newRow("a search") << "tekkit" << "" << "https://api.technicpack.net/search"
                                  << Arguments{ { "build", "multimc" }, { "q", "tekkit" } } << false;
        QTest::newRow("a search with a client id") << "tekkit" << "abc123" << "https://api.technicpack.net/search"
                                                   << Arguments{ { "build", "multimc" }, { "q", "tekkit" }, { "cid", "abc123" } } << false;
        QTest::newRow("a search with an ampersand") << "Tom & Jerry" << "" << "https://api.technicpack.net/search"
                                                    << Arguments{ { "build", "multimc" }, { "q", "Tom & Jerry" } } << false;
        QTest::newRow("a search with a hash") << "C# pack" << "" << "https://api.technicpack.net/search"
                                              << Arguments{ { "build", "multimc" }, { "q", "C# pack" } } << false;
        QTest::newRow("a search with a plus sign") << "C++ pack" << "" << "https://api.technicpack.net/search"
                                                   << Arguments{ { "build", "multimc" }, { "q", "C++ pack" } } << false;
        QTest::newRow("a pack by its name") << "#tekkit-classic" << "" << "https://api.technicpack.net/modpack/tekkit-classic"
                                            << Arguments{ { "build", "multimc" } } << true;
        QTest::newRow("a pack by its name, with a client id")
            << "#tekkit-classic" << "abc123" << "https://api.technicpack.net/modpack/tekkit-classic"
            << Arguments{ { "build", "multimc" }, { "cid", "abc123" } } << true;
        QTest::newRow("a pasted address") << "https://api.technicpack.net/modpack/tekkit-classic" << ""
                                          << "https://api.technicpack.net/modpack/tekkit-classic" << Arguments{ { "build", "multimc" } }
                                          << true;
        QTest::newRow("a pasted address without https")
            << "http://api.technicpack.net/modpack/tekkit-classic" << "" << "https://api.technicpack.net/modpack/tekkit-classic"
            << Arguments{ { "build", "multimc" } } << true;
        // an address that has arguments of its own keeps them, and the build is not there twice
        QTest::newRow("a pasted address with arguments") << "https://api.technicpack.net/modpack/tekkit-classic?build=old&x=1" << "abc123"
                                                         << "https://api.technicpack.net/modpack/tekkit-classic"
                                                         << Arguments{ { "x", "1" }, { "build", "multimc" }, { "cid", "abc123" } } << true;
    }

    void searchUrl()
    {
        QFETCH(const QString, term);
        QFETCH(const QString, clientId);
        QFETCH(const QString, address);
        QFETCH(const Arguments, arguments);
        QFETCH(const bool, single);

        bool isSingle = !single;
        const auto url = Technic::searchUrl(term, s_base, s_build, clientId, &isSingle);

        QCOMPARE(isSingle, single);
        QVERIFY(!url.hasFragment());
        QCOMPARE(url.toString(QUrl::RemoveQuery), address);
        // one question mark, and the arguments in it
        QCOMPARE(QString::fromLatin1(url.toEncoded()).count('?'), 1);
        QCOMPARE(readArguments(url), arguments);
    }
};

QTEST_GUILESS_MAIN(TechnicSearchTest)

#include "TechnicSearch_test.moc"
