#pragma once

#include <QFuture>
#include <QFutureWatcher>
#include <QUrl>
#include <atomic>
#include "BaseInstance.h"
#include "BaseVersion.h"
#include "Filter.h"
#include "InstanceCopyPrefs.h"
#include "InstanceTask.h"
#include "net/NetJob.h"
#include "settings/SettingsObject.h"
#include "tasks/Task.h"

class InstanceCopyTask : public InstanceTask {
    Q_OBJECT
   public:
    explicit InstanceCopyTask(BaseInstance* origInstance, const InstanceCopyPrefs& prefs);
    ~InstanceCopyTask() override;

   protected:
    //! Entry point for tasks.
    virtual void executeTask() override;
    bool abort() override;
    void copyFinished();

   private:
    /* data */
    BaseInstance* m_origInstance;
    QFuture<bool> m_copyFuture;
    QFutureWatcher<bool> m_copyFutureWatcher;
    // set when the copy is called off, and looked at by the thread that copies
    std::atomic_bool m_abortRequested{ false };
    // relative to the instance root, filled in by the copy thread before the future finishes
    QStringList m_failedCopies;
    Filter m_matcher;
    bool m_keepPlaytime;
    bool m_useLinks = false;
    bool m_useHardLinks = false;
    bool m_copySaves = false;
    bool m_linkRecursively = false;
    bool m_useClone = false;
};
