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

#include <NullInstance.h>
#include <settings/INISettingsObject.h>

class InstanceCommandsTest : public QObject {
    Q_OBJECT

    static void registerGlobalSettings(INISettingsObject& global)
    {
        for (const char* name :
             { "ShowGameTime", "RecordGameTime", "PreLoadCommand", "PreLaunchCommand", "WrapperCommand", "PostExitCommand", "ShowConsole",
               "AutoCloseConsole", "ShowConsoleOnError", "LogPrePostOutput", "ConsoleMaxLines", "ConsoleOverflowStop" }) {
            global.registerSetting(name, "");
        }
    }

   private slots:
    void commandsAreAsTyped_data()
    {
        QTest::addColumn<QString>("typed");
        QTest::addColumn<QString>("command");

        QTest::newRow("nothing") << "" << "";
        QTest::newRow("a command") << "notify-send hi" << "notify-send hi";
        QTest::newRow("spaces around it") << "  notify-send hi \t" << "notify-send hi";
        // the spaces inside it are part of it
        QTest::newRow("quoted spaces") << "echo \"  a  \"" << "echo \"  a  \"";
        QTest::newRow("one space") << " " << "";
        QTest::newRow("tabs and spaces") << " \t  \t" << "";
        QTest::newRow("a new line") << "\n" << "";
    }

    void commandsAreAsTyped()
    {
        // A pre-launch command of just a space was a command: the launch stopped with "command is empty, skipping", where a command
        // that isn't there at all is passed over. Every place that decides whether there is a command asks for it like this.
        QFETCH(QString, typed);
        QFETCH(QString, command);

        QTemporaryDir work;
        QVERIFY(work.isValid());
        INISettingsObject global(work.filePath("global.cfg"));
        registerGlobalSettings(global);
        NullInstance instance(&global, std::make_unique<INISettingsObject>(work.filePath("instance.cfg")), work.path());

        for (const char* setting : { "PreLoadCommand", "PreLaunchCommand", "WrapperCommand", "PostExitCommand" }) {
            global.set(setting, typed);
        }
        QCOMPARE(instance.getPreLoadCommand(), command);
        QCOMPARE(instance.getPreLaunchCommand(), command);
        QCOMPARE(instance.getWrapperCommand(), command);
        QCOMPARE(instance.getPostExitCommand(), command);
        QCOMPARE(instance.getPreLaunchCommand().isEmpty(), command.isEmpty());

        // and the same when the instance has commands of its own
        instance.settings()->set("OverrideCommands", true);
        for (const char* setting : { "PreLoadCommand", "PreLaunchCommand", "WrapperCommand", "PostExitCommand" }) {
            instance.settings()->set(setting, typed);
        }
        QCOMPARE(instance.getPreLoadCommand(), command);
        QCOMPARE(instance.getPreLaunchCommand(), command);
        QCOMPARE(instance.getWrapperCommand(), command);
        QCOMPARE(instance.getPostExitCommand(), command);
    }
};

QTEST_GUILESS_MAIN(InstanceCommandsTest)

#include "InstanceCommands_test.moc"
