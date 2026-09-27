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
#include <QRegularExpression>

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
    if (!loaders || (checkDamaged(mods) && checkGameVersion(mods) && checkDuplicates(mods, loaders) && checkDependencies(mods, loaders))) {
        emitSucceeded();
    }
}

CheckModFiles::Answer CheckModFiles::ask(ModFolderModel* mods, const Question& question)
{
    auto* dialog = CustomMessageBox::selectable(nullptr, question.title, question.text, QMessageBox::Icon::Warning,
                                                QMessageBox::StandardButton::Cancel, QMessageBox::StandardButton::NoButton);
    auto* button = dialog->addButton(question.button, QMessageBox::AcceptRole);
    auto* launchAnyway = dialog->addButton(tr("Launch Anyway"), QMessageBox::ActionRole);
    dialog->setDefaultButton(button);
    dialog->setEscapeButton(QMessageBox::StandardButton::Cancel);
    dialog->exec();
    auto* choice = dialog->clickedButton();
    dialog->deleteLater();

    if (choice == launchAnyway) {
        return Answer::LaunchAnyway;
    }
    if (choice != button) {
        emit logLine(question.canceled, MessageLevel::Fatal);
        emitFailed(question.canceled);
        return Answer::Stop;
    }
    if (!mods->setModsEnabled(indexesOf(mods, question.fileNames), question.action)) {
        emit logLine(question.failed, MessageLevel::Fatal);
        emitFailed(question.failed);
        return Answer::Stop;
    }
    // named as they are now
    auto fileNames = question.fileNames;
    if (question.action == EnableAction::ENABLE) {
        for (auto& fileName : fileNames) {
            fileName.remove(QRegularExpression("\\.disabled$"));
        }
    }
    emit logLine(question.done + "\n  " + fileNames.join("\n  "), MessageLevel::Launcher);
    return Answer::Done;
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
                         .button = tr("Turn Them Off"),
                         .fileNames = damaged,
                         .done = tr("Turned off the damaged mod files:"),
                         .failed = tr("Couldn't turn off the damaged mod files"),
                         .canceled = tr("Some mod files are damaged"),
                     }) != Answer::Stop;
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
                         .button = tr("Turn Them Off"),
                         .fileNames = fileNames,
                         .done = tr("Turned off the mods made for another version of Minecraft:"),
                         .failed = tr("Couldn't turn off the mods made for another version of Minecraft"),
                         .canceled = tr("Some mods are made for another version of Minecraft"),
                     }) != Answer::Stop;
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
                         .button = tr("Turn Off Older Copies"),
                         .fileNames = older,
                         .done = tr("Turned off the older copies:"),
                         .failed = tr("Couldn't turn off the older copies of the mods"),
                         .canceled = tr("Some mods are enabled more than once"),
                     }) != Answer::Stop;
}

bool CheckModFiles::checkDependencies(ModFolderModel* mods, ModPlatform::ModLoaderTypes loaders)
{
    auto missing = mods->modsMissingDependencies(loaders);
    if (missing.isEmpty()) {
        return true;
    }

    // a mod another needs may only be turned off, and can be turned back on, unless the loader would stop at it: at one that is
    // damaged or made for another version of Minecraft, or at a second copy of an enabled mod, as when an older version of it is
    // the one enabled
    const auto minecraft = m_parent->instance()->getPackProfile()->getComponentVersion("net.minecraft");
    QSet<QString> enabledIds;
    for (const auto* mod : mods->allMods()) {
        if (mod->enabled() && mod->details().loaders.testAnyFlags(loaders)) {
            enabledIds << mod->mod_id();
        }
    }
    QStringList turnOn;
    QStringList lines;
    for (const auto& found : missing) {
        for (const auto& id : found.ids + found.otherVersions) {
            const auto requirement = found.mod->details().requiredVersions.value(id);
            for (const auto* provider : mods->disabledProviders(id, loaders)) {
                const auto fileName = provider->fileinfo().fileName();
                const auto& details = provider->details();
                const auto versions = details.providedVersions.value(id);
                const bool neededVersion = std::ranges::any_of(
                    versions, [&requirement](const QString& version) { return requirement.acceptsModVersion(version); });
                if (details.damaged || !details.minecraft.accepts(minecraft) || !neededVersion ||
                    (enabledIds.contains(provider->mod_id()) && !turnOn.contains(fileName))) {
                    continue;
                }
                const auto needed = requirement.isSet() ? tr("%1 %2").arg(id, requirement.text()) : id;
                lines << tr("%1 needs %2, which %3 provides").arg(found.mod->fileinfo().fileName(), needed, fileName);
                if (!turnOn.contains(fileName)) {
                    turnOn << fileName;
                    enabledIds << provider->mod_id();
                }
                break;
            }
        }
    }
    if (!turnOn.isEmpty()) {
        emit logLine(tr("These mods need mods that are turned off, and the mod loader won't start the game without them:\n  %1")
                         .arg(lines.join("\n  ")),
                     MessageLevel::Warning);
        const auto answer = ask(mods, {
                                          .title = tr("Mods that are turned off"),
                                          .text = tr("Some mods need mods that are turned off, and the game won't start without "
                                                     "them:\n\n"
                                                     "%1\n\n"
                                                     "Turn Them On turns these on:\n\n"
                                                     "%2")
                                                      .arg(lines.join("\n"), turnOn.join("\n")),
                                          .button = tr("Turn Them On"),
                                          .fileNames = turnOn,
                                          .action = EnableAction::ENABLE,
                                          .done = tr("Turned on the mods others need:"),
                                          .failed = tr("Couldn't turn on the mods others need"),
                                          .canceled = tr("Some mods need mods that are turned off"),
                                      });
        if (answer != Answer::Done) {
            return answer == Answer::LaunchAnyway;
        }
        missing = mods->modsMissingDependencies(loaders);
        if (missing.isEmpty()) {
            return true;
        }
    }

    // turning a mod off leaves out whatever needs it too, and so on
    QSet<Mod*> turnOff;
    for (auto found = missing; !found.isEmpty(); found = mods->modsMissingDependencies(loaders, turnOff)) {
        for (const auto& more : found) {
            turnOff << more.mod;
        }
    }
    lines.clear();
    bool loaderVersion = false;
    for (const auto& found : missing) {
        lines << tr("%1 needs %2").arg(found.mod->fileinfo().fileName(), mods->describeNeeds(found, loaders).join(", "));
        loaderVersion |= found.otherVersions.contains("fabricloader") || found.otherVersions.contains("quilt_loader");
    }
    QStringList fileNames;
    QStringList others;
    for (const auto* mod : turnOff) {
        fileNames << mod->fileinfo().fileName();
    }
    fileNames.sort();
    for (const auto& fileName : fileNames) {
        if (!std::ranges::any_of(missing, [&fileName](const auto& found) { return found.mod->fileinfo().fileName() == fileName; })) {
            others << fileName;
        }
    }

    emit logLine(tr("These mods need mods that aren't here, or other versions of them, and the mod loader won't start the game "
                    "without them:\n  %1")
                     .arg(lines.join("\n  ")),
                 MessageLevel::Warning);
    auto text = tr("Some mods need mods that aren't here, or other versions of them, and the game won't start without them:\n\n"
                   "%1\n\n"
                   "Turn Them Off takes these mods out of the game (the Mods page can turn them back on). Add the mods they need, "
                   "in the versions they need, to use them.")
                    .arg(lines.join("\n"));
    if (loaderVersion) {
        text += ' ' + tr("The version of the mod loader is picked on the Version page of the instance.");
    }
    if (!others.isEmpty()) {
        text += "\n\n" + tr("It turns these off too, as they need those in turn:\n\n%1").arg(others.join("\n"));
    }
    return ask(mods, {
                         .title = tr("Missing mods"),
                         .text = text,
                         .button = tr("Turn Them Off"),
                         .fileNames = fileNames,
                         .done = tr("Turned off the mods that need mods that aren't here:"),
                         .failed = tr("Couldn't turn off the mods that need mods that aren't here"),
                         .canceled = tr("Some mods need mods that aren't here, or other versions of them"),
                     }) != Answer::Stop;
}
