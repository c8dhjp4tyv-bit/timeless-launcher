#pragma once

#include <optional>
#include <vector>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QQueue>
#include <QString>
#include <QUrl>
#include <QVector>

#include "BaseInstance.h"
#include "InstanceTask.h"

class Resource;

class ModrinthCreationTask final : public InstanceTask {
    Q_OBJECT
   public:
    struct File {
        QString path;

        QCryptographicHash::Algorithm hashAlgorithm;
        QByteArray hash;
        QQueue<QUrl> downloads;
        bool required = true;
    };

    /** Turns each file of an update of a pack on or off as the player has the installed file it replaces.
     *
     *  `installed` are the files of the installed version that the update replaces or drops, and `update` the ones it
     *  downloads; `gameRoot` is the game folder of the instance. A file replaces one from the same Modrinth project, going by
     *  their download addresses, or else one with the same path.
     */
    static void keepEnabledStates(const std::vector<File>& installed, std::vector<File>& update, const QDir& gameRoot);

    ModrinthCreationTask(const QString& stagingPath,
                         bool trustedSource,
                         SettingsObject* globalSettings,
                         QWidget* parent,
                         QString id,
                         QString versionId = {},
                         QString originalInstanceId = {});
    ~ModrinthCreationTask() override;

    bool abort() override;

    void createInstance();
    void executeTask() override;

   private slots:
    void finishInstall();

   private:
    bool parseManifest(const QString&, std::vector<File>&, bool setInternalData = true, bool showOptionalDialog = true);

    void ensureMetaLoop();
    void setManagedPack(BaseInstance* instance);

    [[nodiscard]] bool promptForUntrustedMods();

   private:
    QWidget* m_parent = nullptr;
    bool m_trustedSource;

    QString m_minecraftVersion, m_fabricVersion, m_quiltVersion, m_forgeVersion, m_neoForgeVersion;
    QString m_managedId, m_managedVersionId, m_managedName;

    std::vector<File> m_files;
    Task::Ptr m_task;

    std::optional<BaseInstance*> m_oldInstance;
    std::unique_ptr<MinecraftInstance> m_newInstance;

    QString m_rootPath = "minecraft";

    QHash<QString, Resource*> m_resources;
};
