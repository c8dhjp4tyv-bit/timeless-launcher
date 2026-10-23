#pragma once
#include <memory>
#include <optional>

#include <QProcess>
#include <QTimer>

#include "JavaVersion.h"
#include "QObjectPtr.h"
#include "tasks/Task.h"

class JavaChecker : public Task {
    Q_OBJECT
   public:
    using Ptr = shared_qobject_ptr<JavaChecker>;

    struct Result {
        QString path;
        int id;
        QString mojangPlatform;
        QString realPlatform;
        JavaVersion javaVersion;
        QString javaVendor;
        QString outLog;
        QString errorLog;
        bool is_64bit = false;
        enum class Validity { Errored, ReturnedInvalidData, Valid } validity = Validity::Errored;
    };

    explicit JavaChecker(QString path, QString args, int minMem = 0, int maxMem = 0, int permGen = 0, int id = 0);
    ~JavaChecker() override = default;

    /// The jar that does the checking. When it isn't set the one that comes with the launcher is used; an empty path is a jar that
    /// can't be found.
    void setCheckerJar(QString path) { m_checkerJar = std::move(path); }

   signals:
    void checkFinished(const Result& result);

   protected:
    virtual void executeTask() override;

   private:
    std::unique_ptr<QProcess> m_process;
    QTimer m_killTimer;
    QString m_stdout;
    QString m_stderr;

    std::optional<QString> m_checkerJar;
    QString m_path;
    QString m_args;
    int m_minMem = 0;
    int m_maxMem = 0;
    int m_permGen = 64;
    int m_id = 0;

   private slots:
    void timeout();
    void finished(int exitcode, QProcess::ExitStatus);
    void error(QProcess::ProcessError);
    void stdoutReady();
    void stderrReady();
};
