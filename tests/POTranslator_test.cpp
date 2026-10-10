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

#include <translations/POTranslator.h>

namespace {

/// A PO file of the lines given, and the translator that reads it
struct Po {
    QTemporaryDir dir;
    std::unique_ptr<POTranslator> translator;

    explicit Po(const QByteArray& content)
    {
        const auto path = dir.filePath("test.po");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(content);
        }
        file.close();
        translator = std::make_unique<POTranslator>(path);
    }

    QString operator()(const char* source, int n = -1) const { return translator->translate("", source, nullptr, n); }
};

const QByteArray g_header = "msgid \"\"\nmsgstr \"\"\n\"Content-Type: text/plain; charset=UTF-8\\n\"\n\n";

}  // namespace

class POTranslatorTest : public QObject {
    Q_OBJECT

   private slots:
    void plainEntries()
    {
        const Po po(g_header +
                    "msgid \"Hello\"\nmsgstr \"Merhaba\"\n\n"
                    "msgid \"Two\\nlines\"\nmsgstr \"Iki\\nsatir \\\"quoted\\\" \\\\ \\t.\"\n\n"
                    "msgid \"Split\"\nmsgstr \"\"\n\"one \"\n\"two\"\n");
        QVERIFY(!po.translator->isEmpty());
        QCOMPARE(po("Hello"), QString("Merhaba"));
        QCOMPARE(po("Two\nlines"), QString("Iki\nsatir \"quoted\" \\ \t."));
        QCOMPARE(po("Split"), QString("one two"));
        QCOMPARE(po("Unknown"), QString());
    }

    void escapesWithNumbers_data()
    {
        QTest::addColumn<QByteArray>("written");
        QTest::addColumn<QString>("expected");

        QTest::newRow("octal in the middle") << QByteArray("a\\101bc") << "aAbc";
        QTest::newRow("octal first") << QByteArray("\\101bc") << "Abc";
        QTest::newRow("octal at the end") << QByteArray("a\\101") << "aA";
        QTest::newRow("only an octal") << QByteArray("\\101") << "A";
        QTest::newRow("octal then an escape") << QByteArray("\\101\\n") << "A\n";
        QTest::newRow("short octal") << QByteArray("\\7x") << "\ax";
        QTest::newRow("hex in the middle") << QByteArray("a\\x41zz") << "aAzz";
        QTest::newRow("hex at the end") << QByteArray("a\\x41") << "aA";
        QTest::newRow("hex then an escape") << QByteArray("\\x41\\t") << "A\t";
        QTest::newRow("two octals") << QByteArray("\\101\\102") << "AB";
    }

    void escapesWithNumbers()
    {
        // The letter after an escape of a number was dropped, and a string that ended with one was refused (the whole file with it)
        QFETCH(QByteArray, written);
        QFETCH(QString, expected);
        const Po po(g_header + "msgid \"key\"\nmsgstr \"" + written + "\"\n\nmsgid \"after\"\nmsgstr \"still here\"\n");
        QVERIFY(!po.translator->isEmpty());
        QCOMPARE(po("key"), expected);
        QCOMPARE(po("after"), QString("still here"));
    }

    void pluralEntriesDontStopTheFile()
    {
        // lconvert writes the strings with %n like this. The lines of the forms were not understood, and the next msgid ended the
        // reading: nothing after the first plural was in the translator, and when it came first nothing at all.
        const Po po(g_header +
                    "msgid \"%n mod\"\nmsgid_plural \"%n mods\"\nmsgstr[0] \"%n mod\"\nmsgstr[1] \"%n mod (cok)\"\n\n"
                    "msgid \"Hello\"\nmsgstr \"Merhaba\"\n\n"
                    "msgid \"%n file\"\nmsgid_plural \"%n files\"\nmsgstr[0] \"\"\n\"%n dosya\"\nmsgstr[1] \"%n dosya (cok)\"\n\n"
                    "msgid \"Bye\"\nmsgstr \"Hosca kal\"\n");
        QVERIFY(!po.translator->isEmpty());
        QCOMPARE(po("Hello"), QString("Merhaba"));
        QCOMPARE(po("Bye"), QString("Hosca kal"));

        QCOMPARE(po("%n mod", 1), QString("%n mod"));
        QCOMPARE(po("%n mod", 5), QString("%n mod (cok)"));
        // without a count the first form is the one
        QCOMPARE(po("%n mod"), QString("%n mod"));
        QCOMPARE(po("%n file", 1), QString("%n dosya"));
        QCOMPARE(po("%n file", 2), QString("%n dosya (cok)"));
    }

    void aPluralWithOneForm()
    {
        // languages with one form write just one
        const Po po(g_header +
                    "msgid \"%n mod\"\nmsgid_plural \"%n mods\"\nmsgstr[0] \"%n modu\"\n\nmsgid \"Hello\"\nmsgstr \"Merhaba\"\n");
        QVERIFY(!po.translator->isEmpty());
        QCOMPARE(po("%n mod", 1), QString("%n modu"));
        QCOMPARE(po("%n mod", 7), QString("%n modu"));
        QCOMPARE(po("Hello"), QString("Merhaba"));
    }
};

QTEST_GUILESS_MAIN(POTranslatorTest)

#include "POTranslator_test.moc"
