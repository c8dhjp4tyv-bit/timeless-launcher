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

#include "CheckModFiles.h"

#include <QPushButton>

#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModFolderModel.h"
#include "ui/dialogs/CustomMessageBox.h"

void CheckModFiles::executeTask()
{
    auto* mods = m_parent->instance()->loaderModList();
    // the mods were just scanned, and a mod's ID, like whether Java can open its file, is only known once it has been read
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

namespace {

QModelIndexList indexesOf(ModFolderModel* mods, const QStringList& fileNames)
{
    QModelIndexList indexes;
    for (int row = 0; row < mods->rowCount(); row++) {
        if (fileNames.contains(mods->at(row).fileinfo().fileName())) {
            indexes << mods->index(row, 0);
        }
    }
    return indexes;
}

}  // namespace

void CheckModFiles::check()
{
    disconnect(m_waitForParsing);

    // without a mod loader nothing reads the mods, and these are the loaders known to stop at such files
    const auto loaders = m_parent->instance()->getPackProfile()->getModLoaders().value_or(ModPlatform::ModLoaderTypes()) &
                         (ModPlatform::Fabric | ModPlatform::Quilt | ModPlatform::Forge | ModPlatform::NeoForge);
    auto* mods = m_parent->instance()->loaderModList();
    if (!loaders || (checkDamaged(mods) && checkGameVersion(mods) && checkDuplicates(mods, loaders))) {
        emitSucceeded();
    }
}

bool CheckModFiles::ask(ModFolderModel* mods, const Question& question)
{
    auto* dialog = CustomMessageBox::selectable(nullptr, question.title, question.text, QMessageBox::Icon::Warning,
                                                QMessageBox::StandardButton::Cancel, QMessageBox::StandardButton::NoButton);
    auto* turnOff = dialog->addButton(question.turnOff, QMessageBox::AcceptRole);
    auto* launchAnyway = dialog->addButton(tr("Launch Anyway"), QMessageBox::ActionRole);
    dialog->setDefaultButton(turnOff);
    dialog->setEscapeButton(QMessageBox::StandardButton::Cancel);
    dialog->exec();
    auto* choice = dialog->clickedButton();
    dialog->deleteLater();

    if (choice == turnOff) {
        if (!mods->setModsEnabled(indexesOf(mods, question.fileNames), EnableAction::DISABLE)) {
            emit logLine(question.couldNotTurnOff, MessageLevel::Fatal);
            emitFailed(question.couldNotTurnOff);
            return false;
        }
        emit logLine(question.turnedOff + "\n  " + question.fileNames.join("\n  "), MessageLevel::Launcher);
    } else if (choice != launchAnyway) {
        emit logLine(question.canceled, MessageLevel::Fatal);
        emitFailed(question.canceled);
        return false;
    }
    return true;
}

bool CheckModFiles::checkDamaged(ModFolderModel* mods)
{
    const auto damaged = mods->damagedMods();
    if (damaged.isEmpty()) {
        return true;
    }

    emit logLine(
        tr("These mod files are damaged or aren't mods at all, and the mod loader will stop at them:\n  %1").arg(damaged.join("\n  ")),
        MessageLevel::Warning);

    return ask(mods, {
                         .title = tr("Damaged mod files"),
                         .text = tr("These files in the mods folder are damaged or aren't mods at all, as when a download was cut "
                                    "short, and the mod loader will stop at them:\n\n"
                                    "%1\n\n"
                                    "Turn Them Off takes them out of the game (the Mods page can turn them back on). Download them "
                                    "again to use them.")
                                     .arg(damaged.join("\n")),
                         .turnOff = tr("Turn Them Off"),
                         .fileNames = damaged,
                         .turnedOff = tr("Turned off the damaged mod files:"),
                         .couldNotTurnOff = tr("Couldn't turn off the damaged mod files"),
                         .canceled = tr("Some mod files are damaged"),
                     });
}

bool CheckModFiles::checkGameVersion(ModFolderModel* mods)
{
    const auto minecraft = m_parent->instance()->getPackProfile()->getComponentVersion("net.minecraft");
    const auto others = mods->otherGameVersionMods(minecraft);
    if (others.isEmpty()) {
        return true;
    }

    QStringList fileNames;
    QStringList lines;
    for (const auto* mod : others) {
        fileNames << mod->fileinfo().fileName();
        lines << tr("%1 (made for Minecraft %2)").arg(mod->fileinfo().fileName(), mod->details().minecraft.text());
    }
    emit logLine(tr("These mods are made for another version of Minecraft than this instance's %1, and the mod loader will stop at "
                    "them:\n  %2")
                     .arg(minecraft, lines.join("\n  ")),
                 MessageLevel::Warning);

    return ask(mods, {
                         .title = tr("Mods for another version of Minecraft"),
                         .text = tr("These mods say they are made for another version of Minecraft than this instance's %1, and the "
                                    "mod loader will stop at them:\n\n"
                                    "%2\n\n"
                                    "Turn Them Off takes them out of the game (the Mods page can turn them back on). Get the versions "
                                    "of them made for Minecraft %1 to use them.")
                                     .arg(minecraft, lines.join("\n")),
                         .turnOff = tr("Turn Them Off"),
                         .fileNames = fileNames,
                         .turnedOff = tr("Turned off the mods made for another version of Minecraft:"),
                         .couldNotTurnOff = tr("Couldn't turn off the mods made for another version of Minecraft"),
                         .canceled = tr("Some mods are made for another version of Minecraft"),
                     });
}

bool CheckModFiles::checkDuplicates(ModFolderModel* mods, ModPlatform::ModLoaderTypes loaders)
{
    const auto groups = mods->duplicateGroups(loaders);
    if (groups.isEmpty()) {
        return true;
    }

    QStringList lines;
    for (const auto& group : groups) {
        lines << group.join(", ");
    }
    emit logLine(
        tr("These mods are enabled more than once, and the mod loader will refuse to start with them:\n  %1").arg(lines.join("\n  ")),
        MessageLevel::Warning);

    const auto older = mods->olderDuplicates(loaders);
    return ask(mods, {
                         .title = tr("Duplicate mods"),
                         .text = tr("Some mods are enabled more than once, and the game won't start like this. Each line holds "
                                    "copies of one mod:\n\n"
                                    "%1\n\n"
                                    "Turn Off Older Copies keeps the latest version of each and turns these off (the Mods page can "
                                    "turn them back on):\n\n"
                                    "%2")
                                     .arg(lines.join("\n"), older.join("\n")),
                         .turnOff = tr("Turn Off Older Copies"),
                         .fileNames = older,
                         .turnedOff = tr("Turned off the older copies:"),
                         .couldNotTurnOff = tr("Couldn't turn off the older copies of the mods"),
                         .canceled = tr("Some mods are enabled more than once"),
                     });
}
