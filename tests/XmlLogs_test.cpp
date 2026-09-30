// SPDX-FileCopyrightText: 2025 Rachel Powers <508861+Ryex@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Timeless Launcher - Minecraft Launcher
 *  Copyright (C) 2025 Rachel Powers <508861+Ryex@users.noreply.github.com>
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

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QRegularExpression>
#include <QString>

#include <algorithm>
#include <iterator>

#include <FileSystem.h>
#include <MessageLevel.h>
#include <logs/LogParser.h>

class XmlLogParseTest : public QObject {
    Q_OBJECT

   private slots:

    void guessLevel_timestampFormats()
    {
        QCOMPARE(LogParser::guessLevel("[21:16:07] [Server thread/WARN]: short timestamp", MessageLevel::Unknown), MessageLevel::Warning);
        QCOMPARE(LogParser::guessLevel("[23Jul2026 18:12:07.877] [main/WARN] [Sodium-Workarounds/]: date and millis timestamp",
                                       MessageLevel::Unknown),
                 MessageLevel::Warning);
        QCOMPARE(LogParser::guessLevel(
                     "[25Jul2026 14:10:58.723] [main/ERROR] [net.minecraftforge.fml.loading.moddiscovery.ModFileParser/LOADING]: error",
                     MessageLevel::Unknown),
                 MessageLevel::Error);
    }

    void parseEventTimestamp()
    {
        // Taken verbatim from testdata/TestLogs/vanilla-1.21.5.xml.log. These two events sit just under
        // two seconds apart, as they do in the plain text capture of the same startup sequence - read as
        // seconds they would be 33 minutes apart, in the year 57267.
        const QStringList lines = {
            R"(  <log4j:Event logger="com.mojang.datafixers.DataFixerBuilder" timestamp="1745005148589" level="INFO" thread="Datafixer Bootstrap">)",
            R"(    <log4j:Message><![CDATA[263 Datafixer optimizations took 906 milliseconds]]></log4j:Message>)",
            R"(  </log4j:Event>)",
            R"(  <log4j:Event logger="com.mojang.authlib.yggdrasil.YggdrasilAuthenticationService" timestamp="1745005150587" level="INFO" thread="Render thread">)",
            R"(    <log4j:Message><![CDATA[Environment: Environment[sessionHost=https://sessionserver.mojang.com, servicesHost=https://api.minecraftservices.com, name=PROD]]]></log4j:Message>)",
            R"(  </log4j:Event>)",
        };

        LogParser parser;
        QList<QDateTime> timestamps;

        for (const auto& line : lines) {
            parser.appendLine(line);

            for (const auto& item : parser.parseAvailable()) {
                QVERIFY(std::holds_alternative<LogParser::LogEntry>(item));
                timestamps.append(std::get<LogParser::LogEntry>(item).timestamp);
            }
        }

        QCOMPARE(timestamps.length(), 2);
        // Comparing instants rather than rendered clock times keeps this independent of the time zone.
        QCOMPARE(timestamps[0], QDateTime::fromMSecsSinceEpoch(1745005148589));
        QCOMPARE(timestamps[1], QDateTime::fromMSecsSinceEpoch(1745005150587));
        QCOMPARE(timestamps[0].toUTC().date(), QDate(2025, 4, 18));
        QCOMPARE(timestamps[0].msecsTo(timestamps[1]), 1998);
    }

    void parseThrowable()
    {
        // Shortened from testdata/TestLogs/TerraFirmaGreg-Modern-forge.xml.log: an event can carry the exception that was logged
        // with the message, stack trace and all.
        const QStringList trace = {
            "com.google.gson.JsonParseException: com.google.gson.stream.MalformedJsonException: Use JsonReader.setLenient(true)",
            "\tat TRANSFORMER/minecraft@1.20.1/net.minecraft.util.GsonHelper.m_13780_(GsonHelper.java:526)",
            "Caused by: com.google.gson.stream.MalformedJsonException: Use JsonReader.setLenient(true) to accept malformed JSON",
            "\tat MC-BOOTSTRAP/com.google.gson@2.10/com.google.gson.stream.JsonReader.syntaxError(JsonReader.java:1657)",
            "\t... 13 more",
        };
        QStringList lines = {
            R"(  <log4j:Event logger="net.minecraft.server.packs.resources.SimpleJsonResourceReloadListener" timestamp="1745005487560" level="ERROR" thread="Worker-ResourceReload-4">)",
            R"(    <log4j:Message><![CDATA[Couldn't parse data file tfc:field_guide/ru_ru/entries/tfg_ores/surface_copper]]></log4j:Message>)",
            "    <log4j:Throwable><![CDATA[" + trace.first(),
        };
        lines << trace.mid(1);
        lines << R"(]]></log4j:Throwable>)" << R"(  </log4j:Event>)";
        // the next event has none
        lines << R"(  <log4j:Event logger="de.keksuccino.fancymenu.util.window.WindowHandler" timestamp="1745005487588" level="INFO">)"
              << R"(    <log4j:Message><![CDATA[[FANCYMENU] Custom window icon successfully updated!]]></log4j:Message>)"
              << R"(  </log4j:Event>)";

        LogParser parser;
        QList<LogParser::LogEntry> entries;
        for (const auto& line : lines) {
            parser.appendLine(line);

            for (const auto& item : parser.parseAvailable()) {
                QVERIFY(std::holds_alternative<LogParser::LogEntry>(item));
                entries.append(std::get<LogParser::LogEntry>(item));
            }
        }

        QCOMPARE(entries.length(), 2);
        QCOMPARE(entries[0].level, MessageLevel::Error);
        QCOMPARE(entries[0].message, "Couldn't parse data file tfc:field_guide/ru_ru/entries/tfg_ores/surface_copper");
        QCOMPARE(entries[0].throwable, trace.join('\n'));
        QCOMPARE(entries[1].message, "[FANCYMENU] Custom window icon successfully updated!");
        QVERIFY(entries[1].throwable.isEmpty());
    }

    void parseXml_data()
    {
        QString source = QFINDTESTDATA("testdata/TestLogs");

        QString shortXml = QString::fromUtf8(FS::read(FS::PathCombine(source, "vanilla-1.21.5.xml.log")));
        QString shortText = QString::fromUtf8(FS::read(FS::PathCombine(source, "vanilla-1.21.5.text.log")));
        QStringList shortTextLevels_s = QString::fromUtf8(FS::read(FS::PathCombine(source, "vanilla-1.21.5-levels.txt")))
                                            .split(QRegularExpression("\n|\r\n|\r"), Qt::SkipEmptyParts);

        QList<MessageLevel> shortTextLevels;
        shortTextLevels.reserve(24);
        std::transform(shortTextLevels_s.cbegin(), shortTextLevels_s.cend(), std::back_inserter(shortTextLevels),
                       [](const QString& line) { return MessageLevel::fromName(line.trimmed()); });

        QString longXml = QString::fromUtf8(FS::read(FS::PathCombine(source, "TerraFirmaGreg-Modern-forge.xml.log")));
        QString longText = QString::fromUtf8(FS::read(FS::PathCombine(source, "TerraFirmaGreg-Modern-forge.text.log")));
        QStringList longTextLevels_s = QString::fromUtf8(FS::read(FS::PathCombine(source, "TerraFirmaGreg-Modern-levels.txt")))
                                           .split(QRegularExpression("\n|\r\n|\r"), Qt::SkipEmptyParts);
        QStringList longTextLevelsXml_s = QString::fromUtf8(FS::read(FS::PathCombine(source, "TerraFirmaGreg-Modern-xml-levels.txt")))
                                              .split(QRegularExpression("\n|\r\n|\r"), Qt::SkipEmptyParts);

        QList<MessageLevel> longTextLevelsPlain;
        longTextLevelsPlain.reserve(974);
        std::transform(longTextLevels_s.cbegin(), longTextLevels_s.cend(), std::back_inserter(longTextLevelsPlain),
                       [](const QString& line) { return MessageLevel::fromName(line.trimmed()); });
        QList<MessageLevel> longTextLevelsXml;
        longTextLevelsXml.reserve(896);
        std::transform(longTextLevelsXml_s.cbegin(), longTextLevelsXml_s.cend(), std::back_inserter(longTextLevelsXml),
                       [](const QString& line) { return MessageLevel::fromName(line.trimmed()); });

        QTest::addColumn<QString>("log");
        QTest::addColumn<int>("num_entries");
        QTest::addColumn<QList<MessageLevel>>("entry_levels");

        QTest::newRow("short-vanilla-plain") << shortText << 25 << shortTextLevels;
        QTest::newRow("short-vanilla-xml") << shortXml << 25 << shortTextLevels;
        QTest::newRow("long-forge-plain") << longText << 945 << longTextLevelsPlain;
        QTest::newRow("long-forge-xml") << longXml << 869 << longTextLevelsXml;
    }

    void parseXml()
    {
        QFETCH(QString, log);
        QFETCH(int, num_entries);
        QFETCH(QList<MessageLevel>, entry_levels);

        QList<std::pair<MessageLevel, QString>> entries = {};

        QBENCHMARK
        {
            entries = parseLines(log.split(QRegularExpression("\n|\r\n|\r")));
        }

        QCOMPARE(entries.length(), num_entries);

        QList<MessageLevel> levels = {};

        std::transform(entries.cbegin(), entries.cend(), std::back_inserter(levels),
                       [](std::pair<MessageLevel, QString> entry) { return entry.first; });

        QCOMPARE(levels, entry_levels);
    }

    void parseAngleBrackets_data()
    {
        QTest::addColumn<QStringList>("lines");
        QTest::addColumn<QStringList>("messages");

        // Text that merely begins to look like a log4j event must not be held back: lines reach the
        // parser whole, so the rest of `<log4j:Event` can never turn up later on. See #5825.
        QTest::newRow("trailing left angle bracket")
            << QStringList{ "[21:16:07] [Render thread/INFO]: happy >w<", "[21:16:08] [Render thread/INFO]: unrelated" }
            << QStringList{ "[21:16:07] [Render thread/INFO]: happy >w<", "[21:16:08] [Render thread/INFO]: unrelated" };
        QTest::newRow("trailing partial tag") << QStringList{ "generics are <log", "unrelated" }
                                              << QStringList{ "generics are <log", "unrelated" };
        QTest::newRow("lone left angle bracket") << QStringList{ "<", "unrelated" } << QStringList{ "<", "unrelated" };
        QTest::newRow("unrelated markup") << QStringList{ "<html>", "unrelated" } << QStringList{ "<html>", "unrelated" };
        QTest::newRow("longer element name") << QStringList{ "talking about <log4j:eventually", "unrelated" }
                                             << QStringList{ "talking about <log4j:eventually", "unrelated" };

        // ... while real events, spread over several lines or not, still have to be recognised.
        QTest::newRow("event over several lines")
            << QStringList{ R"(  <log4j:Event logger="fqq" timestamp="1745005150596" level="INFO" thread="Render thread">)",
                            R"(    <log4j:Message><![CDATA[Setting user: Ryexandrite]]></log4j:Message>)", R"(  </log4j:Event>)" }
            << QStringList{ "Setting user: Ryexandrite" };
        QTest::newRow("event with attributes on the next line")
            << QStringList{ R"(  <log4j:Event)", R"(      logger="fqq" timestamp="1745005150596" level="INFO" thread="Render thread">)",
                            R"(    <log4j:Message><![CDATA[Setting user: Ryexandrite]]></log4j:Message>)", R"(  </log4j:Event>)" }
            << QStringList{ "Setting user: Ryexandrite" };
        QTest::newRow("event preceded by text")
            << QStringList{ R"(stray output <log4j:Event logger="fqq" timestamp="1745005150596" level="INFO" thread="Render thread">)"
                            R"(<log4j:Message><![CDATA[Setting user: Ryexandrite]]></log4j:Message></log4j:Event>)" }
            << QStringList{ "stray output ", "Setting user: Ryexandrite" };
    }

    void parseAngleBrackets()
    {
        QFETCH(QStringList, lines);
        QFETCH(QStringList, messages);

        QCOMPARE(parseMessages(lines), messages);
    }

    void rootLogger()
    {
        // the root logger has no name, and what is logged to it is no less an event: an empty logger used to make the parser give up on
        // it and on every line after it
        LogParser parser;
        QList<LogParser::LogEntry> entries;
        for (const auto& line : goodEvent("main", "from the root logger", "")) {
            parser.appendLine(line);
            for (const auto& item : parser.parseAvailable()) {
                QVERIFY(std::holds_alternative<LogParser::LogEntry>(item));
                entries.append(std::get<LogParser::LogEntry>(item));
            }
        }
        for (const auto& line : goodEvent("main", "from a logger", "a")) {
            parser.appendLine(line);
            for (const auto& item : parser.parseAvailable()) {
                QVERIFY(std::holds_alternative<LogParser::LogEntry>(item));
                entries.append(std::get<LogParser::LogEntry>(item));
            }
        }

        QCOMPARE(entries.length(), 2);
        QCOMPARE(entries[0].message, "from the root logger");
        QVERIFY(entries[0].logger.isEmpty());
        QCOMPARE(entries[0].thread, "main");
        QCOMPARE(entries[1].message, "from a logger");
        QCOMPARE(entries[1].logger, "a");
    }

    void unreadableEvents_data()
    {
        QTest::addColumn<QStringList>("bad");
        QTest::addColumn<QStringList>("shown");

        // An event the parser can't read is shown as it came, and what comes after it is read as if it had not been there. Held back
        // instead, it stood in front of every line after it, so that the game log stopped for the rest of the session.
        const QString start = R"(  <log4j:Event logger="a" timestamp="1745005150597" level="INFO" thread="main">)";
        const QString end = "  </log4j:Event>";

        const QString withControl = "    <log4j:Message><![CDATA[colored \x1b[31mred\x1b[0m text]]></log4j:Message>";
        QTest::newRow("control character in the message")
            << QStringList{ start, withControl, end } << QStringList{ start.trimmed(), withControl, end };

        const QString withAmpersand = R"(  <log4j:Event logger="a&b" timestamp="1745005150597" level="INFO" thread="main">)";
        const QString message = "    <log4j:Message><![CDATA[the message]]></log4j:Message>";
        QTest::newRow("& in an attribute") << QStringList{ withAmpersand, message, end } << QStringList{ withAmpersand, message, end };

        // the time is the first thing the parser looks for, and what it puts the event in order by
        const QString withoutTime = R"(  <log4j:Event logger="a" timestamp="" level="INFO" thread="main">)";
        QTest::newRow("no time") << QStringList{ withoutTime, message, end }
                                 << QStringList{ withoutTime.trimmed() + "\n" + message + "\n" + end };

        QTest::newRow("no message") << QStringList{ start, end } << QStringList{ start.trimmed() + "\n" + end };

        QTest::newRow("text that mentions an event")
            << QStringList{ "see <log4j:Event for details" } << QStringList{ "see ", "<log4j:Event for details" };
    }

    void unreadableEvents()
    {
        QFETCH(const QStringList, bad);
        QFETCH(QStringList, shown);

        QStringList lines = bad;
        lines << goodEvent("main", "the next event", "b") << "plain after";
        shown << "the next event" << "plain after";

        QCOMPARE(parseMessages(lines), shown);
    }

    void eventThatNeverEnds()
    {
        // An event is waited for as long as it can still be one, but not for ever: what goes on for longer than any log message is
        // text, and shown as it came.
        const QString start = R"(  <log4j:Event logger="a" timestamp="1745005150597" level="INFO" thread="main">)";
        QStringList lines = { start };
        const QString filler(qsizetype(40) * 1024, 'x');
        for (int i = 0; i < 8; i++) {
            lines << filler;
        }
        lines << goodEvent("main", "the next event", "b") << "plain after";

        const auto messages = parseMessages(lines);
        QCOMPARE(messages.mid(messages.length() - 2), QStringList({ "the next event", "plain after" }));
        qsizetype length = 0;
        for (const auto& message : messages.mid(0, messages.length() - 2)) {
            length += message.length();
        }
        // all of it, less the line breaks
        QVERIFY(length >= start.trimmed().length() + 8 * filler.length());
    }

    void nonAsciiMessages()
    {
        // where an event ends is told in characters, whatever their size: none of what follows may be left in the buffer
        QStringList lines;
        const QStringList texts = { QString::fromUtf8("caf\xC3\xA9 \xC2\xA7"
                                                      "6gold"),
                                    QString::fromUtf8("emoji \xF0\x9F\x98\x80 inside"),
                                    QString::fromUtf8("\xF0\x9F\x98\x80\xF0\x9F\x98\x80"),
                                    QString::fromUtf8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E") };
        for (const auto& text : texts) {
            lines << goodEvent("main", text, "a") << "plain after";
        }

        QStringList expected;
        for (const auto& text : texts) {
            expected << text << "plain after";
        }
        QCOMPARE(parseMessages(lines), expected);
    }

   private:
    /// The lines of an event as the game writes them
    static QStringList goodEvent(const QString& thread, const QString& message, const QString& logger)
    {
        return { QString(R"(  <log4j:Event logger="%1" timestamp="1745005150597" level="INFO" thread="%2">)").arg(logger, thread),
                 "    <log4j:Message><![CDATA[" + message + "]]></log4j:Message>", "  </log4j:Event>" };
    }

    QList<std::pair<MessageLevel, QString>> parseLines(const QStringList& lines)
    {
        LogParser parser;
        QList<std::pair<MessageLevel, QString>> out;
        MessageLevel last = MessageLevel::Unknown;

        for (const auto& line : lines) {
            parser.appendLine(line);

            auto items = parser.parseAvailable();
            for (const auto& item : items) {
                if (std::holds_alternative<LogParser::LogEntry>(item)) {
                    auto entry = std::get<LogParser::LogEntry>(item);
                    auto msg = QString("[%1] [%2/%3] [%4]: %5")
                                   .arg(entry.timestamp.toString("HH:mm:ss"))
                                   .arg(entry.thread)
                                   .arg(entry.levelText)
                                   .arg(entry.logger)
                                   .arg(entry.message);
                    out.append(std::make_pair(entry.level, msg));
                    last = entry.level;
                } else if (std::holds_alternative<LogParser::PlainText>(item)) {
                    auto msg = std::get<LogParser::PlainText>(item).message;
                    auto level = LogParser::guessLevel(msg, last);

                    out.append(std::make_pair(level, msg));
                    last = level;
                }
            }
        }
        return out;
    }

    /// The messages the parser produces, without the timestamp formatting parseLines() applies - so that
    /// expectations do not depend on the time zone the test happens to run in.
    QStringList parseMessages(const QStringList& lines)
    {
        LogParser parser;
        QStringList out;

        for (const auto& line : lines) {
            parser.appendLine(line);

            for (const auto& item : parser.parseAvailable()) {
                if (std::holds_alternative<LogParser::LogEntry>(item)) {
                    out.append(std::get<LogParser::LogEntry>(item).message);
                } else if (std::holds_alternative<LogParser::PlainText>(item)) {
                    out.append(std::get<LogParser::PlainText>(item).message);
                }
            }
        }
        return out;
    }
};

QTEST_GUILESS_MAIN(XmlLogParseTest)

#include "XmlLogs_test.moc"
