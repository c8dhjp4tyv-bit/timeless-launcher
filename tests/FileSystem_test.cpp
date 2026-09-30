#include <QDir>
#include <QProcess>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

#include <tasks/Task.h>

#include <FileSystem.h>
#include <StringUtils.h>

#include <filesystem>
namespace fs = std::filesystem;

class LinkTask : public Task {
    Q_OBJECT

    friend class FileSystemTest;

    LinkTask(QString src, QString dst)
    {
        m_lnk = new FS::create_link(src, dst, this);
        m_lnk->debug(true);
    }

    ~LinkTask() { delete m_lnk; }

    void matcher(Filter filter) { m_lnk->matcher(filter); }

    void linkRecursively(bool recursive)
    {
        m_lnk->linkRecursively(recursive);
        m_linkRecursive = recursive;
    }

    void whitelist(bool b) { m_lnk->whitelist(b); }

    void setMaxDepth(int depth) { m_lnk->setMaxDepth(depth); }

   private:
    void executeTask() override
    {
        if (!(*m_lnk)()) {
#if defined Q_OS_WIN32
            if (!m_useHard) {
                qDebug() << "EXPECTED: Link failure, Windows requires permissions for symlinks";

                qDebug() << "atempting to run with privelage";
                connect(m_lnk, &FS::create_link::finishedPrivileged, this, [this](bool gotResults) {
                    if (gotResults) {
                        emitSucceeded();
                    } else {
                        qDebug() << "Privileged run exited without results!";
                        emitFailed();
                    }
                });
                m_lnk->runPrivileged();
            } else {
                qDebug() << "Link Failed!" << m_lnk->getOSError().value() << m_lnk->getOSError().message().c_str();
            }
#else
            qDebug() << "Link Failed!" << m_lnk->getOSError().value() << m_lnk->getOSError().message().c_str();
#endif
        } else {
            emitSucceeded();
        }
    }

    FS::create_link* m_lnk;
#if defined Q_OS_WIN32
    bool m_useHard = false;
#endif
    bool m_linkRecursive = true;
};

#ifdef Q_OS_LINUX
// A data folder of its own for the trash to be in, so that a test doesn't put anything in the user's.
class TrashHome {
   public:
    TrashHome() : m_before(qgetenv("XDG_DATA_HOME")) { qputenv("XDG_DATA_HOME", m_folder.path().toUtf8()); }
    ~TrashHome()
    {
        if (m_before.isNull()) {
            qunsetenv("XDG_DATA_HOME");
        } else {
            qputenv("XDG_DATA_HOME", m_before);
        }
    }
    TrashHome(const TrashHome&) = delete;
    TrashHome& operator=(const TrashHome&) = delete;

    bool isValid() const { return m_folder.isValid(); }
    QString filePath(const QString& name) const { return m_folder.filePath(name); }

   private:
    QTemporaryDir m_folder;
    QByteArray m_before;
};
#endif

class FileSystemTest : public QObject {
    Q_OBJECT

    const QString bothSlash = "/foo/";
    const QString trailingSlash = "foo/";
    const QString leadingSlash = "/foo";

   private slots:
    void test_pathCombine()
    {
        QCOMPARE(QString("/foo/foo"), FS::PathCombine(bothSlash, bothSlash));
        QCOMPARE(QString("foo/foo"), FS::PathCombine(trailingSlash, trailingSlash));
        QCOMPARE(QString("/foo/foo"), FS::PathCombine(leadingSlash, leadingSlash));

        QCOMPARE(QString("/foo/foo/foo"), FS::PathCombine(bothSlash, bothSlash, bothSlash));
        QCOMPARE(QString("foo/foo/foo"), FS::PathCombine(trailingSlash, trailingSlash, trailingSlash));
        QCOMPARE(QString("/foo/foo/foo"), FS::PathCombine(leadingSlash, leadingSlash, leadingSlash));
    }

    void test_PathCombine1_data()
    {
        QTest::addColumn<QString>("result");
        QTest::addColumn<QString>("path1");
        QTest::addColumn<QString>("path2");

        QTest::newRow("qt 1") << "/abc/def/ghi/jkl" << "/abc/def" << "ghi/jkl";
        QTest::newRow("qt 2") << "/abc/def/ghi/jkl" << "/abc/def/" << "ghi/jkl";
#if defined(Q_OS_WIN)
        QTest::newRow("win native, from C:") << "C:/abc" << "C:" << "abc";
        QTest::newRow("win native 1") << "C:/abc/def/ghi/jkl" << "C:\\abc\\def" << "ghi\\jkl";
        QTest::newRow("win native 2") << "C:/abc/def/ghi/jkl" << "C:\\abc\\def\\" << "ghi\\jkl";
#endif
    }

    void test_PathCombine1()
    {
        QFETCH(QString, result);
        QFETCH(QString, path1);
        QFETCH(QString, path2);

        QCOMPARE(FS::PathCombine(path1, path2), result);
    }

    void test_PathCombine2_data()
    {
        QTest::addColumn<QString>("result");
        QTest::addColumn<QString>("path1");
        QTest::addColumn<QString>("path2");
        QTest::addColumn<QString>("path3");

        QTest::newRow("qt 1") << "/abc/def/ghi/jkl" << "/abc" << "def" << "ghi/jkl";
        QTest::newRow("qt 2") << "/abc/def/ghi/jkl" << "/abc/" << "def" << "ghi/jkl";
        QTest::newRow("qt 3") << "/abc/def/ghi/jkl" << "/abc" << "def/" << "ghi/jkl";
        QTest::newRow("qt 4") << "/abc/def/ghi/jkl" << "/abc/" << "def/" << "ghi/jkl";
#if defined(Q_OS_WIN)
        QTest::newRow("win 1") << "C:/abc/def/ghi/jkl" << "C:\\abc" << "def" << "ghi\\jkl";
        QTest::newRow("win 2") << "C:/abc/def/ghi/jkl" << "C:\\abc\\" << "def" << "ghi\\jkl";
        QTest::newRow("win 3") << "C:/abc/def/ghi/jkl" << "C:\\abc" << "def\\" << "ghi\\jkl";
        QTest::newRow("win 4") << "C:/abc/def/ghi/jkl" << "C:\\abc\\" << "def" << "ghi\\jkl";
#endif
    }

    void test_PathCombine2()
    {
        QFETCH(QString, result);
        QFETCH(QString, path1);
        QFETCH(QString, path2);
        QFETCH(QString, path3);

        QCOMPARE(FS::PathCombine(path1, path2, path3), result);
    }

    void test_copy()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::copy c(folder, target_dir.path());
            c();

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
            }
            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_copy_with_blacklist()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::copy c(folder, target_dir.path());
            auto re = Filters::regexp(QRegularExpression("[.]?mcmeta"));
            c.matcher(re);
            c();

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
            }
            QVERIFY(!target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_copy_with_whitelist()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::copy c(folder, target_dir.path());
            auto re = Filters::regexp(QRegularExpression("[.]?mcmeta"));
            c.matcher(re);
            c.whitelist(true);
            c();

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
            }
            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(!target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_copy_with_dot_hidden()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::copy c(folder, target_dir.path());
            c();

            auto filter = QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::Hidden;

            for (auto entry : target_dir.entryList(filter)) {
                qDebug() << entry;
            }

            QVERIFY(target_dir.entryList(filter).contains(".secret_folder"));
            target_dir.cd(".secret_folder");
            QVERIFY(target_dir.entryList(filter).contains(".secret_file.txt"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_copy_single_file()
    {
        QTemporaryDir tempDir;
        tempDir.setAutoRemove(true);

        {
            QString file = QFINDTESTDATA("testdata/FileSystem/test_folder/pack.mcmeta");

            qDebug() << "From:" << file << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "pack.mcmeta"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::copy c(file, target_dir.filePath("pack.mcmeta"));
            c();

            auto filter = QDir::Filter::Files;

            for (auto entry : target_dir.entryList(filter)) {
                qDebug() << entry;
            }

            QVERIFY(target_dir.entryList(filter).contains("pack.mcmeta"));
        }
    }

    void test_copy_reports_every_failure_data()
    {
        QTest::addColumn<QString>("conflicting");

        // The directory is not listed in any particular order, so each file takes a turn at failing: whichever order
        // the files come in, some row has the failure before a file that is copied fine.
        QTest::newRow("a.txt") << "a.txt";
        QTest::newRow("b.txt") << "b.txt";
        QTest::newRow("c.txt") << "c.txt";
    }

    void test_copy_reports_every_failure()
    {
        QFETCH(QString, conflicting);

        QTemporaryDir source;
        QTemporaryDir target;
        QVERIFY(source.isValid() && target.isValid());
        for (const auto* name : { "a.txt", "b.txt", "c.txt" }) {
            FS::write(source.filePath(name), "new");
        }
        // without overwrite, a file already at the destination can't be copied
        FS::write(target.filePath(conflicting), "old");

        FS::copy c(source.path(), target.path());

        QVERIFY(!c());
        QCOMPARE(c.totalFailed(), qsizetype(1));
        QCOMPARE(QFileInfo(c.failed().first()).fileName(), conflicting);
        QCOMPARE(c.totalCopied(), qsizetype(2));
        QCOMPARE(FS::read(target.filePath(conflicting)), QByteArray("old"));
        for (const auto* name : { "a.txt", "b.txt", "c.txt" }) {
            if (name != conflicting) {
                QCOMPARE(FS::read(target.filePath(name)), QByteArray("new"));
            }
        }
    }

    void test_move_keeps_source_when_copy_fails_data() { test_copy_reports_every_failure_data(); }

    void test_move_keeps_source_when_copy_fails()
    {
        QFETCH(QString, conflicting);

        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto source = temp.filePath("source");
        const auto target = temp.filePath("target");
        for (const auto* name : { "a.txt", "b.txt", "c.txt" }) {
            FS::write(FS::PathCombine(source, name), "new");
        }
        // A non-empty target can't be renamed over, so the move falls back to copying and deleting the source.
        FS::write(FS::PathCombine(target, conflicting), "old");

        QVERIFY(!FS::move(source, target));
        // the file that could not be copied must still be where it was
        QCOMPARE(FS::read(FS::PathCombine(source, conflicting)), QByteArray("new"));
    }

    void test_removeFiles()
    {
        // What an extraction that failed part way removes again: the files it extracted, listed in the archive's order
        // with its folders, which come first in most archives.
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        FS::write(temp.filePath("config/sodium.json"), "{}");
        FS::write(temp.filePath("options.txt"), "fov:70");
        FS::write(temp.filePath("kept.txt"), "not extracted");

        const QStringList extracted{ temp.filePath("config") + '/', temp.filePath("config/sodium.json"), temp.filePath("gone.txt"),
                                     temp.filePath("options.txt") };

        // gone.txt is no longer there to be removed
        QVERIFY(!FS::removeFiles(extracted));
        QVERIFY(!QFileInfo::exists(temp.filePath("config/sodium.json")));
        QVERIFY(!QFileInfo::exists(temp.filePath("options.txt")));
        QVERIFY(QFileInfo(temp.filePath("config")).isDir());
        QVERIFY(QFileInfo::exists(temp.filePath("kept.txt")));
    }

    void test_setAsideBroken()
    {
        // the second file the launcher can't read under a name doesn't take the place of the first
        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto path = temp.filePath("accounts.json");

        QVERIFY(FS::setAsideBroken(path).isEmpty());  // nothing there

        FS::write(path, "{\"accounts\": [");
        QCOMPARE(FS::setAsideBroken(path), path + ".broken");
        QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(FS::read(path + ".broken"), QByteArray("{\"accounts\": ["));

        FS::write(path, "not json");
        QCOMPARE(FS::setAsideBroken(path), path + ".broken2");
        QCOMPARE(FS::read(path + ".broken"), QByteArray("{\"accounts\": ["));
        QCOMPARE(FS::read(path + ".broken2"), QByteArray("not json"));

        // a folder is not a file to set aside
        QVERIFY(QDir(temp.path()).mkdir("folder"));
        QVERIFY(FS::setAsideBroken(temp.filePath("folder")).isEmpty());
    }

#ifdef Q_OS_LINUX
    void test_trash()
    {
        // the trash is in a data folder of its own here, not the user's
        const TrashHome data;
        QVERIFY(data.isValid());

        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto path = temp.filePath("file.txt");
        FS::write(path, "content");

        QString inTrash;
        QVERIFY(FS::trash(path, &inTrash));
        QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(FS::read(inTrash), QByteArray("content"));
    }

    void test_trashOfAFileThatStays()
    {
        // A file that can't be removed from its folder isn't trashed. Qt says it was, having linked it into the trash and failed to
        // unlink it from where it was: the file has to stay where it is, with nothing in the trash to show for it.
        const TrashHome data;
        QVERIFY(data.isValid());

        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto path = temp.filePath("file.txt");
        const auto probe = temp.filePath("probe.txt");
        FS::write(path, "content");
        FS::write(probe, "probe");

        // a folder that can't be written to holds files that can't be removed
        QVERIFY(QFile::setPermissions(temp.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        const auto restorePermissions = qScopeGuard(
            [&] { QFile::setPermissions(temp.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner); });
        if (QFile::remove(probe)) {
            QSKIP("Files in a folder can be removed here whatever its permissions, as by the administrator.");
        }

        QString inTrash;
        QVERIFY(!FS::trash(path, &inTrash));
        QVERIFY(inTrash.isEmpty());
        QVERIFY(QFileInfo::exists(path));
        QVERIFY(QDir(data.filePath("Trash/files")).isEmpty());
        QVERIFY(QDir(data.filePath("Trash/info")).isEmpty());
    }
#endif

    void test_getDesktop() { QCOMPARE(FS::getDesktopDir(), QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)); }

    // In the expected results, ~ stands for a double quote, which moc can't take in a raw string literal.

    void test_desktopEntryExec()
    {
        // world names, which shortcuts to a world pass along, can hold any of these
        const QStringList args{ "--world", "Steve's World", "100% Survival", "Cash $5", "a\"b", R"(back\slash)", "`id`" };
        QString expected =
            R"(~/opt/Timeless Launcher/launcher~ ~--world~ ~Steve's World~ ~100%% Survival~ ~Cash \\$5~ ~a\\~b~ ~back\\\\slash~ ~\\`id\\`~)";
        QCOMPARE(FS::desktopEntryExec("/opt/Timeless Launcher/launcher", args), expected.replace('~', '"'));
    }

    void test_shellCommand()
    {
        const QStringList args{ "Steve's World", "100% Survival", "Cash $5", "a\"b", R"(back\slash)", "`id`", "" };
        QString expected = R"('/opt/Timeless Launcher/launcher' 'Steve'\''s World' '100% Survival' 'Cash $5' 'a~b' 'back\slash' '`id`' '')";
        QCOMPARE(FS::shellCommand("/opt/Timeless Launcher/launcher", args), expected.replace('~', '"'));
#if defined(Q_OS_UNIX)
        // and the shell gives them back as they were
        QProcess shell;
        shell.start("/bin/sh", { "-c", FS::shellCommand("printf", QStringList{ "[%s]" } + args) });
        QVERIFY(shell.waitForFinished());
        QString printed = R"([Steve's World][100% Survival][Cash $5][a~b][back\slash][`id`][])";
        QCOMPARE(QString::fromUtf8(shell.readAllStandardOutput()), printed.replace('~', '"'));
#endif
    }

    void test_link()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            lnk_tsk.linkRecursively(false);
            connect(&lnk_tsk, &Task::finished, &lnk_tsk,
                    [&lnk_tsk] { QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been."); });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                if (!entry_lnk_info.isDir())
                    QVERIFY(!entry_lnk_info.isSymLink());
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());
            QVERIFY(lnk_info.isSymLink());

            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_hard_link()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            // use working dir to prevent makeing a hard link to a tmpfs or across devices
            QTemporaryDir tempDir("./tmp");
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::create_link lnk(folder, target_dir.path());
            lnk.useHardLinks(true);
            lnk.debug(true);
            if (!lnk()) {
                qDebug() << "Link Failed!" << lnk.getOSError().value() << lnk.getOSError().message().c_str();
            }

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                QVERIFY(!entry_lnk_info.isSymLink());
                QFileInfo entry_orig_info(QDir(folder).filePath(entry));
                if (!entry_lnk_info.isDir()) {
                    qDebug() << "hard link equivalency?" << entry_lnk_info.absoluteFilePath() << "vs" << entry_orig_info.absoluteFilePath();
                    QVERIFY(fs::equivalent(fs::path(StringUtils::toStdString(entry_lnk_info.absoluteFilePath())),
                                           fs::path(StringUtils::toStdString(entry_orig_info.absoluteFilePath()))));
                }
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());
            QVERIFY(!lnk_info.isSymLink());

            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_link_with_blacklist()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            auto re = Filters::regexp(QRegularExpression("[.]?mcmeta"));
            lnk_tsk.matcher(re);
            lnk_tsk.linkRecursively(true);
            connect(&lnk_tsk, &Task::finished, &lnk_tsk,
                    [&lnk_tsk] { QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been."); });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                if (!entry_lnk_info.isDir())
                    QVERIFY(entry_lnk_info.isSymLink());
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());

            QVERIFY(!target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_link_with_whitelist()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            auto re = Filters::regexp(QRegularExpression("[.]?mcmeta"));
            lnk_tsk.matcher(re);
            lnk_tsk.linkRecursively(true);
            lnk_tsk.whitelist(true);
            connect(&lnk_tsk, &Task::finished, &lnk_tsk,
                    [&lnk_tsk] { QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been."); });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                if (!entry_lnk_info.isDir())
                    QVERIFY(entry_lnk_info.isSymLink());
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());

            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(!target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_link_with_dot_hidden()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            lnk_tsk.linkRecursively(true);
            connect(&lnk_tsk, &Task::finished, &lnk_tsk,
                    [&lnk_tsk] { QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been."); });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            auto filter = QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::Hidden;

            for (auto entry : target_dir.entryList(filter)) {
                qDebug() << entry;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                if (!entry_lnk_info.isDir())
                    QVERIFY(entry_lnk_info.isSymLink());
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());

            QVERIFY(target_dir.entryList(filter).contains(".secret_folder"));
            target_dir.cd(".secret_folder");
            QVERIFY(target_dir.entryList(filter).contains(".secret_file.txt"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_link_single_file()
    {
        QTemporaryDir tempDir;
        tempDir.setAutoRemove(true);

        {
            QString file = QFINDTESTDATA("testdata/FileSystem/test_folder/pack.mcmeta");

            qDebug() << "From:" << file << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "pack.mcmeta"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(file, target_dir.filePath("pack.mcmeta"));
            connect(&lnk_tsk, &Task::finished, &lnk_tsk,
                    [&lnk_tsk] { QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been."); });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            auto filter = QDir::Filter::Files;

            for (auto entry : target_dir.entryList(filter)) {
                qDebug() << entry;
            }

            QFileInfo lnk_info(target_dir.filePath("pack.mcmeta"));
            QVERIFY(lnk_info.exists());
            QVERIFY(lnk_info.isSymLink());

            QVERIFY(target_dir.entryList(filter).contains("pack.mcmeta"));
        }
    }

    void test_link_with_max_depth()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            lnk_tsk.linkRecursively(true);
            lnk_tsk.setMaxDepth(0);
            connect(&lnk_tsk, &Task::finished, &lnk_tsk,
                    [&lnk_tsk] { QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been."); });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            QVERIFY(!QFileInfo(target_dir.path()).isSymLink());

            auto filter = QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::Hidden;
            for (auto entry : target_dir.entryList(filter)) {
                qDebug() << entry;
                if (entry == "." || entry == "..")
                    continue;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                QVERIFY(entry_lnk_info.isSymLink());
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());
            QVERIFY(!lnk_info.isSymLink());

            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_link_with_no_max_depth()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            lnk_tsk.linkRecursively(true);
            lnk_tsk.setMaxDepth(-1);
            connect(&lnk_tsk, &Task::finished, &lnk_tsk,
                    [&lnk_tsk] { QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been."); });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            std::function<void(QString)> verify_check = [&verify_check](QString check_path) {
                QDir check_dir(check_path);
                auto filter = QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::Hidden;
                for (auto entry : check_dir.entryList(filter)) {
                    QFileInfo entry_lnk_info(check_dir.filePath(entry));
                    qDebug() << entry << check_dir.filePath(entry);
                    if (!entry_lnk_info.isDir()) {
                        QVERIFY(entry_lnk_info.isSymLink());
                    } else if (entry != "." && entry != "..") {
                        qDebug() << "Decending tree to verify symlinks:" << check_dir.filePath(entry);
                        verify_check(entry_lnk_info.filePath());
                    }
                }
            };

            verify_check(target_dir.path());

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());

            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_path_depth()
    {
        QCOMPARE(FS::pathDepth(""), 0);
        QCOMPARE(FS::pathDepth("."), 0);
        QCOMPARE(FS::pathDepth("foo.txt"), 0);
        QCOMPARE(FS::pathDepth("./foo.txt"), 0);
        QCOMPARE(FS::pathDepth("./bar/foo.txt"), 1);
        QCOMPARE(FS::pathDepth("../bar/foo.txt"), 0);
        QCOMPARE(FS::pathDepth("/bar/foo.txt"), 1);
        QCOMPARE(FS::pathDepth("baz/bar/foo.txt"), 2);
        QCOMPARE(FS::pathDepth("/baz/bar/foo.txt"), 2);
        QCOMPARE(FS::pathDepth("./baz/bar/foo.txt"), 2);
        QCOMPARE(FS::pathDepth("/baz/../bar/foo.txt"), 1);
    }

    void test_path_trunc()
    {
        QCOMPARE(FS::pathTruncate("", 0), QDir::toNativeSeparators(""));
        QCOMPARE(FS::pathTruncate("foo.txt", 0), QDir::toNativeSeparators(""));
        QCOMPARE(FS::pathTruncate("foo.txt", 1), QDir::toNativeSeparators(""));
        QCOMPARE(FS::pathTruncate("./bar/foo.txt", 0), QDir::toNativeSeparators("./bar"));
        QCOMPARE(FS::pathTruncate("./bar/foo.txt", 1), QDir::toNativeSeparators("./bar"));
        QCOMPARE(FS::pathTruncate("/bar/foo.txt", 1), QDir::toNativeSeparators("/bar"));
        QCOMPARE(FS::pathTruncate("bar/foo.txt", 1), QDir::toNativeSeparators("bar"));
        QCOMPARE(FS::pathTruncate("baz/bar/foo.txt", 2), QDir::toNativeSeparators("baz/bar"));
#if defined(Q_OS_WIN)
        QCOMPARE(FS::pathTruncate("C:\\bar\\foo.txt", 1), QDir::toNativeSeparators("C:\\bar"));
#endif
    }
};

QTEST_GUILESS_MAIN(FileSystemTest)

#include "FileSystem_test.moc"
