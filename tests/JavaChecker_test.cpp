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

#include <QSignalSpy>
#include <QTest>

#include <java/JavaChecker.h>

class JavaCheckerTest : public QObject {
    Q_OBJECT

   private slots:
    void aMissingCheckerJarEndsTheTask()
    {
        // Without the jar nothing can be checked, and the task used to return without ending: the detection of Java installations
        // waits for all of its checks, and never got to show a list.
        JavaChecker checker("/nonexistent/java", "", 0, 0, 0, 7);
        checker.setCheckerJar("");
        QSignalSpy checked(&checker, &JavaChecker::checkFinished);
        QSignalSpy finished(&checker, &Task::finished);

        checker.start();
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 5000);

        QCOMPARE(checked.count(), 1);
        const auto result = checked.first().first().value<JavaChecker::Result>();
        QCOMPARE(result.validity, JavaChecker::Result::Validity::Errored);
        QCOMPARE(result.path, QString("/nonexistent/java"));
        QCOMPARE(result.id, 7);
        QVERIFY(!result.errorLog.isEmpty());
    }
};

QTEST_GUILESS_MAIN(JavaCheckerTest)

#include "JavaChecker_test.moc"
