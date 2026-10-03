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

#include <StringUtils.h>

class StringUtilsTest : public QObject {
    Q_OBJECT

   private slots:
    void replaceTokens_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("expected");

        QTest::newRow("nothing to replace") << "--demo" << "--demo";
        QTest::newRow("no text") << "" << "";
        QTest::newRow("a token on its own") << "${game_directory}" << "/home/me/.minecraft";
        QTest::newRow("a token in a word") << "-DgameVersion=${version_name}.jar" << "-DgameVersion=1.20.1.jar";
        // each token is replaced where it is, and the text between them stays as it is
        QTest::newRow("two tokens") << "${game_directory}/versions/${version_name}/client.jar"
                                    << "/home/me/.minecraft/versions/1.20.1/client.jar";
        QTest::newRow("the same token twice") << "${version_name}-${version_name}" << "1.20.1-1.20.1";
        QTest::newRow("tokens next to each other") << "${version_name}${version_name}" << "1.20.11.20.1";
        QTest::newRow("three tokens with text around them")
            << "a${game_directory}b${version_name}c${version_name}d" << "a/home/me/.minecraftb1.20.1c1.20.1d";
        QTest::newRow("a token that has no value") << "a${nothing}b" << "ab";
        QTest::newRow("a token with a value after one that has none") << "${nothing}${version_name}" << "1.20.1";
        QTest::newRow("a token that is not closed") << "a${version_name" << "a${version_name";
        QTest::newRow("a token with nothing in it") << "a${}b" << "a${}b";
        QTest::newRow("a dollar sign that starts no token") << "$HOME and ${version_name}" << "$HOME and 1.20.1";
        // what a value contains is text and not looked at for tokens
        QTest::newRow("a value that looks like a token") << "${looks_like_a_token}" << "${version_name}";
    }

    void replaceTokens()
    {
        QFETCH(const QString, text);
        QFETCH(const QString, expected);

        const QMap<QString, QString> values{ { "game_directory", "/home/me/.minecraft" },
                                             { "version_name", "1.20.1" },
                                             { "looks_like_a_token", "${version_name}" } };

        QCOMPARE(StringUtils::replaceTokens(text, values), expected);
    }
};

QTEST_GUILESS_MAIN(StringUtilsTest)

#include "StringUtils_test.moc"
