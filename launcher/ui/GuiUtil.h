#pragma once

#include <QFileInfo>
#include <QWidget>
#include <optional>

class BaseInstance;

namespace GuiUtil {
std::optional<QString> uploadPaste(const QString& name, const QFileInfo& filePath, QWidget* parentWidget);
std::optional<QString> uploadPaste(const QString& name, const QString& data, QWidget* parentWidget);
void setClipboardText(QString text);
QStringList browseForFiles(const QString& context,
                           const QString& caption,
                           const QString& filter,
                           const QString& defaultPath,
                           QWidget* parentWidget);
QString browseForFile(const QString& context,
                      const QString& caption,
                      const QString& filter,
                      const QString& defaultPath,
                      QWidget* parentWidget);

/** Backs up every world of an instance before its modpack is updated, if its settings say to, showing the progress over
 *  `parentWidget`. When a world can't be backed up, the player is asked whether to update anyway. Returns whether the update
 *  is to go on, and puts the folder the backups went to, if any were made, in `backedUpTo`.
 */
bool backUpWorldsBeforeUpdate(BaseInstance* instance, QWidget* parentWidget, QString* backedUpTo = nullptr);
}  // namespace GuiUtil
