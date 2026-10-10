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

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <atomic>

#include <InstanceCopyPrefs.h>
#include <InstanceCopyTask.h>
#include <NullInstance.h>
#include <settings/INISettingsObject.h>

namespace {

int filesIn(const QString& folder)
{
    int count = 0;
    QDirIterator it(folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}

}  // namespace

class InstanceCopyTaskTest : public QObject {
    Q_OBJECT

    /// The settings that an instance takes over from the global ones
    static void registerGlobalSettings(INISettingsObject& global)
    {
        for (const char* name :
             { "ShowGameTime", "RecordGameTime", "PreLoadCommand", "PreLaunchCommand", "WrapperCommand", "PostExitCommand", "ShowConsole",
               "AutoCloseConsole", "ShowConsoleOnError", "LogPrePostOutput", "ConsoleMaxLines", "ConsoleOverflowStop" }) {
            global.registerSetting(name, "");
        }
    }

   private slots:
    void copy()
    {
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const auto source = work.filePath("source");
        const auto staging = work.filePath("staging");
        QVERIFY(QDir().mkpath(source + "/minecraft/a"));
        for (int i = 0; i < 20; ++i) {
            QFile file(QString("%1/minecraft/a/%2.txt").arg(source).arg(i));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("text");
        }

        INISettingsObject global(work.filePath("global.cfg"));
        registerGlobalSettings(global);
        NullInstance instance(&global, std::make_unique<INISettingsObject>(source + "/instance.cfg"), source);

        InstanceCopyTask task(&instance, InstanceCopyPrefs());
        task.setParentSettings(&global);
        task.setStagingPath(staging);
        task.setName("copy");
        QSignalSpy succeeded(&task, &Task::succeeded);
        QSignalSpy failed(&task, &Task::failed);
        task.start();
        QTRY_VERIFY_WITH_TIMEOUT(succeeded.count() + failed.count() > 0, 10000);

        QCOMPARE(failed.count(), 0);
        QCOMPARE(filesIn(staging + "/minecraft"), 20);
    }

    void abortStopsTheCopy()
    {
        // A copy that is called off ends when the files stop being copied, and not before: the folder it copies into is taken away
        // as soon as the task is over, and the thread that copies would put files back into it, or use a task that has gone.
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const auto source = work.filePath("source");
        const auto staging = work.filePath("staging");
        constexpr int folders = 10;
        constexpr int filesPerFolder = 200;
        for (int folder = 0; folder < folders; ++folder) {
            const auto path = QString("%1/minecraft/f%2").arg(source).arg(folder);
            QVERIFY(QDir().mkpath(path));
            for (int i = 0; i < filesPerFolder; ++i) {
                QFile file(QString("%1/%2.txt").arg(path).arg(i));
                QVERIFY(file.open(QIODevice::WriteOnly));
                file.write("text");
            }
        }
        const int total = folders * filesPerFolder;

        INISettingsObject global(work.filePath("global.cfg"));
        registerGlobalSettings(global);
        NullInstance instance(&global, std::make_unique<INISettingsObject>(source + "/instance.cfg"), source);

        auto task = std::make_unique<InstanceCopyTask>(&instance, InstanceCopyPrefs());
        task->setParentSettings(&global);
        task->setStagingPath(staging);
        task->setName("copy");
        QSignalSpy failed(task.get(), &Task::failed);
        QSignalSpy finished(task.get(), &Task::finished);

        // It is called off from the thread that copies, after some of the files: how fast the copy is makes no difference to the test
        Task* asTask = task.get();  // the way the dialog asks for it
        std::atomic_bool calledOff{ false };
        std::atomic_bool wasRunning{ false };
        connect(
            task.get(), &Task::progress, task.get(),
            [&](qint64 current, qint64) {
                if (current >= 50 && !calledOff.exchange(true)) {
                    wasRunning = asTask->abort();
                }
            },
            Qt::DirectConnection);
        task->start();
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() > 0, 30000);
        QVERIFY(calledOff);
        QVERIFY(wasRunning);
        QCOMPARE(failed.count(), 1);

        // it has ended, so nothing is copied from now on
        const int atTheEnd = filesIn(staging);
        QTest::qWait(500);
        QCOMPARE(filesIn(staging), atTheEnd);
        QVERIFY2(atTheEnd < total, "the whole of it was copied before it could be called off");

        // and the task can go
        task.reset();
        QTest::qWait(100);
        QCOMPARE(filesIn(staging), atTheEnd);
    }
};

QTEST_GUILESS_MAIN(InstanceCopyTaskTest)

#include "InstanceCopyTask_test.moc"
