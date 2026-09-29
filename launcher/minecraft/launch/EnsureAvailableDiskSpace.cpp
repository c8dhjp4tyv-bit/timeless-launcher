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

#include "EnsureAvailableDiskSpace.h"

#include <QDir>
#include <QLocale>
#include <QStorageInfo>

#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"
#include "ui/dialogs/CustomMessageBox.h"

void EnsureAvailableDiskSpace::executeTask()
{
    const auto gameRoot = m_parent->instance()->gameRoot();
    const QStorageInfo storage(QDir(gameRoot).absolutePath());
    // a folder whose disk can't be told about says nothing
    const auto available = storage.isValid() && storage.isReady() ? storage.bytesAvailable() : -1;
    if (!isLow(available)) {
        emitSucceeded();
        return;
    }

    const auto freeSpace = QLocale().formattedDataSize(available, 1, QLocale::DataSizeSIFormat);
    auto* dialog = CustomMessageBox::selectable(
        nullptr, tr("Low disk space"),
        tr("Only %1 is free on the disk with this instance's game folder:\n%2\n\n"
           "The game can't save a world on a full disk, and whatever it was saving when the disk filled up is lost. "
           "Free up some space before playing.\n\n"
           "Launch anyway?")
            .arg(freeSpace, QDir::toNativeSeparators(gameRoot)),
        QMessageBox::Icon::Warning, QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No, QMessageBox::StandardButton::No);
    const bool launch = dialog->exec() == QMessageBox::Yes;
    dialog->deleteLater();

    const auto message = tr("Only %1 is free on the disk with the game folder").arg(freeSpace);
    if (!launch) {
        emit logLine(message, MessageLevel::Fatal);
        emitFailed(message);
        return;
    }
    emit logLine(message, MessageLevel::Warning);
    emitSucceeded();
}
