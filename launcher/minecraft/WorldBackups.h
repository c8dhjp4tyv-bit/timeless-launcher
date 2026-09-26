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

#pragma once

#include <QFileInfo>
#include <QFileInfoList>
#include <QString>
#include <QStringList>

#include <memory>

namespace MMCZip {
class ExportToZipTask;
}

/// Backups of worlds made the way the game's own Make Backup makes them: a zip named after the date, the time and the world's
/// folder, holding that folder. Adding such a zip on the Worlds page restores the world.
namespace WorldBackups {

/// The folder the game puts its backups in, next to the saves folder
QString backupDir(const QString& savesDir);

/// The folder backups made before launching go to. Only those are ever deleted, to keep a limited number of them.
QString automaticBackupDir(const QString& savesDir);

/// A task that backs up the world in the given folder to a new zip in backupDir, leaving out the session.lock the game holds while
/// the world is open. Returns nothing if the world's files or backupDir can't be read.
std::unique_ptr<MMCZip::ExportToZipTask> createTask(const QFileInfo& world, const QString& backupDir);

/// The backups of the world with that folder name in backupDir, oldest first. A second one made in the same second, which gets a
/// number added to its name, isn't among them.
QFileInfoList backupsOf(const QString& backupDir, const QString& worldFolderName);

/// Whether the game saved the world in the given folder after its newest backup in backupDir was made, or it has none there
bool changedSinceBackup(const QFileInfo& world, const QString& backupDir);

/// Deletes all but the newest `keep` backups of the world with that folder name in backupDir, and returns the names of the ones
/// it deleted
QStringList removeOldBackups(const QString& backupDir, const QString& worldFolderName, int keep);

}  // namespace WorldBackups
