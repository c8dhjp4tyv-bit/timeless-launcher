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

#include "net/PasteUpload.h"

class PasteUploadTest : public QObject {
    Q_OBJECT

    /// What a server that reads application/x-www-form-urlencoded makes of a body: the fields, with a plus sign as a space
    static QMap<QString, QString> readForm(const QByteArray& body)
    {
        QMap<QString, QString> fields;
        for (const auto& pair : body.split('&')) {
            const auto separator = pair.indexOf('=');
            const auto name = QUrl::fromPercentEncoding(pair.left(separator).replace('+', ' '));
            const auto value = QUrl::fromPercentEncoding(pair.mid(separator + 1).replace('+', ' '));
            fields.insert(name, value);
        }
        return fields;
    }

   private slots:
    void mclogsFormBody_data()
    {
        QTest::addColumn<QString>("log");

        QTest::newRow("empty") << "";
        QTest::newRow("plain") << "[12:00:00] [main/INFO]: Loading Minecraft 1.20.1";
        QTest::newRow("jvm flags") << "Java Arguments: -XX:+UnlockExperimentalVMOptions -XX:+UseG1GC -Xmx4096m";
        QTest::newRow("version with a plus") << "Java is version 17.0.5+8, using 64 Bit";
        QTest::newRow("loader version") << "fabric-loader 0.15.0+mc1.20.1";
        QTest::newRow("only plus signs") << "+++";
        QTest::newRow("a sum") << "1 + 2 = 3";
        QTest::newRow("form delimiters") << "a=b&c=d;e=f";
        QTest::newRow("percent") << "100% of %20 and %2B and %";
        QTest::newRow("hash and question mark") << "https://example.com/a?b=c#d";
        QTest::newRow("lines") << "one\ntwo\r\nthree\n";
        QTest::newRow("non-ASCII") << "Sihirli yazı: çağrı, öğrenci, şişe, ığdır, İstanbul, \xe2\x82\xac, \xf0\x9f\x98\x80";
        QTest::newRow("tabs and spaces") << "a\tb  c ";
    }

    void mclogsFormBody()
    {
        QFETCH(const QString, log);

        const auto body = PasteUpload::mclogsFormBody(log);

        // one field, which the server reads back as the log it was given
        const auto fields = readForm(body);
        QCOMPARE(fields.size(), 1);
        QVERIFY(fields.contains("content"));
        QCOMPARE(fields.value("content"), log);

        // and nothing in it that a form could read as something else
        QVERIFY(!body.contains(' '));
        QVERIFY(!body.contains('\n'));
        QCOMPARE(body.count('&'), 0);
        QCOMPARE(body.count('='), 1);
        QCOMPARE(body.count('+'), 0);
    }
};

QTEST_GUILESS_MAIN(PasteUploadTest)

#include "PasteUpload_test.moc"
