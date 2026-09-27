#pragma once

#include <QDebug>
#include <QObject>

#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModDetails.h"

#include "tasks/Task.h"

namespace ModUtils {

ModDetails ReadFabricModInfo(QByteArray contents);
ModDetails ReadQuiltModInfo(QByteArray contents);
ModDetails ReadForgeInfo(QByteArray contents);
ModDetails ReadLiteModInfo(QByteArray contents);

enum class ProcessingLevel { Full, BasicInfoOnly };

bool process(Mod& mod, ProcessingLevel level = ProcessingLevel::Full);

bool processZIP(Mod& mod, ProcessingLevel level = ProcessingLevel::Full);
bool processLitemod(Mod& mod, ProcessingLevel level = ProcessingLevel::Full);

/** Checks whether a file is valid as a mod or not. */
bool validate(QFileInfo file);

/**
 * Whether the file lacks the end record of a zip, which is the first thing Java looks for in a jar and fails to open it
 * without. The archive library reads a zip that was cut short up to where it ends, so this is what tells it from a whole one.
 * A file that can't be read at all doesn't count, since that says nothing about it.
 */
bool isMissingZipEnd(const QString& path);

bool processIconPNG(const Mod& mod, QByteArray&& raw_data, QPixmap* pixmap);
bool loadIconFile(const Mod& mod, QPixmap* pixmap);
}  // namespace ModUtils

class LocalModParseTask : public Task {
    Q_OBJECT
   public:
    struct Result {
        ModDetails details;
    };
    using ResultPtr = std::shared_ptr<Result>;
    ResultPtr result() const { return m_result; }

    bool canAbort() const override { return true; }
    bool abort() override;

    LocalModParseTask(int token, ResourceType type, const QFileInfo& modFile);
    void executeTask() override;

    int token() const { return m_token; }

   private:
    int m_token;
    ResourceType m_type;
    QFileInfo m_modFile;
    ResultPtr m_result;

    std::atomic<bool> m_aborted = false;
};
