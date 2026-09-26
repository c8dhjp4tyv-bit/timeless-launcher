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

#include "BackupWorlds.h"

#include <QDir>

#include "archive/ExportToZipTask.h"
#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/WorldBackups.h"
#include "settings/SettingsObject.h"

void BackupWorlds::executeTask()
{
    auto* instance = m_parent->instance();
    if (!instance->settings()->get("BackUpWorldsBeforeLaunch").toBool()) {
        emitSucceeded();
        return;
    }

    const auto saves = instance->worldDir();
    m_backupDir = WorldBackups::automaticBackupDir(saves);
    bool anyWorld = false;
    // the folders the Worlds page lists: the ones with a level.dat
    for (const auto& folder : QDir(saves).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (QFileInfo::exists(QDir(folder.absoluteFilePath()).filePath("level.dat"))) {
            anyWorld = true;
            if (WorldBackups::changedSinceBackup(folder, m_backupDir)) {
                m_pending << folder;
            }
        }
    }
    if (m_pending.isEmpty()) {
        if (anyWorld) {
            emit logLine(tr("No world was played since its last automatic backup."), MessageLevel::Launcher);
        }
        emitSucceeded();
        return;
    }

    // a large world takes a while, so this shows how far along it is
    emit progressReportingRequest();
}

void BackupWorlds::proceed()
{
    backUpNext();
}

void BackupWorlds::backUpNext()
{
    if (m_pending.isEmpty()) {
        emitSucceeded();
        return;
    }

    const auto world = m_pending.takeFirst();
    auto task = WorldBackups::createTask(world, m_backupDir);
    if (!task) {
        emit logLine(tr("Couldn't back up the world %1: its files or %2 can't be read.").arg(world.fileName(), m_backupDir),
                     MessageLevel::Warning);
        backUpNext();
        return;
    }

    emit logLine(tr("Backing up the world %1...").arg(world.fileName()), MessageLevel::Launcher);
    const auto zipPath = task->outputPath();
    m_current.reset(task.release());
    connect(m_current.get(), &Task::finished, this, [this, world, zipPath] {
        switch (m_current->getState()) {
            case State::Succeeded: {
                emit logLine(tr("Backed it up to %1").arg(zipPath), MessageLevel::Launcher);
                const auto keep = m_parent->instance()->settings()->get("WorldBackupsToKeep").toInt();
                if (const auto removed = WorldBackups::removeOldBackups(m_backupDir, world.fileName(), keep); !removed.isEmpty()) {
                    emit logLine(tr("Deleted its oldest automatic backups: %1").arg(removed.join(", ")), MessageLevel::Launcher);
                }
                break;
            }
            case State::AbortedByUser:
                m_pending.clear();
                emitAborted();
                return;
            default:
                // the game still starts: the worlds are no worse off than without the setting
                emit logLine(tr("Couldn't back up the world %1: %2").arg(world.fileName(), m_current->failReason()), MessageLevel::Warning);
                break;
        }
        backUpNext();
    });
    propagateFromOther(m_current.get());
    m_current->start();
}

bool BackupWorlds::abort()
{
    m_pending.clear();
    if (m_current && m_current->isRunning()) {
        // this step ends once the backup has stopped
        return m_current->abort();
    }
    return Task::abort();
}
