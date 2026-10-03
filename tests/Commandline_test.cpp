#include <QTest>

#include <Commandline.h>

class CommandlineTest : public QObject {
    Q_OBJECT
   private slots:
    void test_splitArgs_data()
    {
        QTest::addColumn<QString>("args");
        QTest::addColumn<QStringList>("expected");

        QTest::newRow("plain arguments") << "a b c" << QStringList{ "a", "b", "c" };
        QTest::newRow("quoted argument with spaces") << "a \"b c\" d" << QStringList{ "a", "b c", "d" };
        QTest::newRow("windows path without quotes") << "-Dorg.lwjgl.glfw.libname=C:\\Users\\user\\glfw3.dll"
                                                     << QStringList{ "-Dorg.lwjgl.glfw.libname=C:\\Users\\user\\glfw3.dll" };
        QTest::newRow("windows path in quotes keeps backslashes")
            << "-Dorg.lwjgl.glfw.libname=\"C:\\Users\\test user\\glfw3.dll\""
            << QStringList{ "-Dorg.lwjgl.glfw.libname=C:\\Users\\test user\\glfw3.dll" };
        QTest::newRow("fully quoted argument keeps backslashes")
            << "\"-Dorg.lwjgl.glfw.libname=C:\\Users\\test user\\glfw3.dll\""
            << QStringList{ "-Dorg.lwjgl.glfw.libname=C:\\Users\\test user\\glfw3.dll" };
        QTest::newRow("windows path in single quotes keeps backslashes")
            << "-Dfoo='C:\\Users\\test user\\glfw3.dll'" << QStringList{ "-Dfoo=C:\\Users\\test user\\glfw3.dll" };
        QTest::newRow("escaped quotes") << "-Dfoo=\"say \\\"hi\\\" now\"" << QStringList{ "-Dfoo=say \"hi\" now" };
        QTest::newRow("double backslash in quotes collapses")
            << "-Dfoo=\"C:\\\\path\\\\file.dll\"" << QStringList{ "-Dfoo=C:\\path\\file.dll" };
    }
    void test_splitArgs()
    {
        QFETCH(QString, args);
        QFETCH(QStringList, expected);

        QCOMPARE(Commandline::splitArgs(args), expected);
    }

    void test_quoteForSplitCommand_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("quoted");

        QTest::newRow("path") << "/usr/bin/nbtexplorer" << "/usr/bin/nbtexplorer";
        QTest::newRow("windows path") << "C:\\Tools\\NBTExplorer\\NBTExplorer.exe" << "C:\\Tools\\NBTExplorer\\NBTExplorer.exe";
        QTest::newRow("windows path with spaces") << "C:\\Program Files\\MCEdit\\mcedit.exe" << "\"C:\\Program Files\\MCEdit\\mcedit.exe\"";
        QTest::newRow("apostrophe") << "C:\\Users\\O'Brien\\mcedit.exe" << "\"C:\\Users\\O'Brien\\mcedit.exe\"";
        QTest::newRow("network share with spaces") << "\\\\NAS\\my tools\\nbt.exe" << "\"\\\\\\NAS\\my tools\\nbt.exe\"";
        QTest::newRow("quotes") << "/home/me/my \"tool\"" << "\"/home/me/my \\\"tool\\\"\"";
        QTest::newRow("tab") << "a\tb" << "\"a\tb\"";
        QTest::newRow("ends with a backslash") << "C:\\Some Dir\\" << "\"C:\\Some Dir\\\\\"";
        QTest::newRow("backslash before a quote") << "a\\\"b" << "\"a\\\\\\\"b\"";
    }
    void test_quoteForSplitCommand()
    {
        QFETCH(QString, input);
        QFETCH(QString, quoted);

        QCOMPARE(Commandline::quoteForSplitCommand(input), quoted);
        QCOMPARE(Commandline::splitArgs(quoted), QStringList{ input });
    }

    /// Every short run of what a path may have in it comes back out of splitArgs() as it went in
    void test_quoteForSplitCommand_roundTrip()
    {
        const QString alphabet = "a \t'\"\\";
        QStringList failed;
        QStringList shorter{ "" };
        for (int length = 1; length <= 5; length++) {
            QStringList inputs;
            for (const auto& input : shorter) {
                for (const auto c : alphabet) {
                    inputs << input + c;
                }
            }
            for (const auto& input : inputs) {
                if (Commandline::splitArgs(Commandline::quoteForSplitCommand(input)) != QStringList{ input }) {
                    failed << input;
                }
            }
            shorter = inputs;
        }
        // the first few that fail show in the report
        QCOMPARE(failed.mid(0, 5), QStringList());
    }

    /// The way a world tool is started: its path as the settings page writes it, then the world to open
    void test_worldTool()
    {
        QProcessEnvironment vars;
        vars.insert("WORLD_PATH", "/home/me/.local/share/saves/New World");

        for (const QString program : { "/usr/bin/nbtexplorer", "C:\\Users\\O'Brien\\NBTExplorer\\nbt.exe", "\\\\NAS\\my tools\\nbt.exe" }) {
            const auto command = Commandline::quoteForSplitCommand(program) + " ${WORLD_PATH}";
            QCOMPARE(Commandline::process(command, vars), (QStringList{ program, "/home/me/.local/share/saves/New World" }));
        }
    }
};

QTEST_GUILESS_MAIN(CommandlineTest)
#include "Commandline_test.moc"
