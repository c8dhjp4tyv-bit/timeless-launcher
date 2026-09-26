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

#include "CheckDuplicateMods.h"

#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModFolderModel.h"
#include "ui/dialogs/CustomMessageBox.h"

void CheckDuplicateMods::executeTask()
{
    auto* mods = m_parent->instance()->loaderModList();
    // the mods were just scanned, and a mod's ID is only known once it has been read
    if (mods->hasPendingParseTasks()) {
        m_waitForParsing = connect(mods, &ModFolderModel::parseFinished, this, [this, mods] {
            if (!mods->hasPendingParseTasks()) {
                check();
            }
        });
        return;
    }
    check();
}

void CheckDuplicateMods::check()
{
    disconnect(m_waitForParsing);

    // without a mod loader nothing reads the mods, and these are the loaders known to refuse to start with two copies of one
    const auto loaders = m_parent->instance()->getPackProfile()->getModLoaders().value_or(ModPlatform::ModLoaderTypes()) &
                         (ModPlatform::Fabric | ModPlatform::Quilt | ModPlatform::Forge | ModPlatform::NeoForge);
    const auto groups = !loaders ? QList<QStringList>() : m_parent->instance()->loaderModList()->duplicateGroups(loaders);
    if (groups.isEmpty()) {
        emitSucceeded();
        return;
    }

    QStringList lines;
    for (const auto& group : groups) {
        lines << group.join(", ");
    }
    emit logLine(
        tr("These mods are enabled more than once, and the mod loader will refuse to start with them:\n  %1").arg(lines.join("\n  ")),
        MessageLevel::Warning);

    auto* dialog = CustomMessageBox::selectable(
        nullptr, tr("Duplicate mods"),
        tr("Some mods are enabled more than once, and the game won't start like this. Each line holds copies of one mod:\n\n"
           "%1\n\n"
           "Disable or delete all copies but one on the Mods page. Launch anyway?")
            .arg(lines.join("\n")),
        QMessageBox::Icon::Warning, QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No, QMessageBox::StandardButton::No);
    const bool launchAnyway = dialog->exec() == QMessageBox::Yes;
    dialog->deleteLater();

    if (!launchAnyway) {
        const auto reason = tr("Some mods are enabled more than once");
        emit logLine(reason, MessageLevel::Fatal);
        emitFailed(reason);
        return;
    }
    emitSucceeded();
}
