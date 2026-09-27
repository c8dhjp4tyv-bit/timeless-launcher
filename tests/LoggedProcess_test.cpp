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

#include <LoggedProcess.h>
#include <MessageLevel.h>

class LoggedProcessTest : public QObject {
    Q_OBJECT

   private slots:
    void linesKeepToTheirStream()
    {
#ifdef Q_OS_WIN
        QSKIP("Runs its output through sh");
#endif
        LoggedProcess process;
        QStringList out;
        QStringList err;
        connect(&process, &LoggedProcess::log, this, [&out, &err](const QStringList& lines, MessageLevel level) {
            if (static_cast<MessageLevel::Enum>(level) == MessageLevel::StdErr) {
                err << lines;
            } else if (static_cast<MessageLevel::Enum>(level) == MessageLevel::StdOut) {
                out << lines;
            }
        });

        // half a line on stdout, a whole one on stderr while it waits for the rest, and a last line that never gets its line break
        process.start("sh", { "-c",
                              "printf 'Loading'; sleep 0.3; printf 'a warning\\n' >&2; sleep 0.3; printf ' done\\n'; sleep 0.3; "
                              "printf 'last words'" });

        QTRY_VERIFY_WITH_TIMEOUT(process.state() == LoggedProcess::Finished, 10000);
        QCOMPARE(out, (QStringList{ "Loading done", "last words" }));
        QCOMPARE(err, QStringList{ "a warning" });
    }
};

QTEST_GUILESS_MAIN(LoggedProcessTest)

#include "LoggedProcess_test.moc"
