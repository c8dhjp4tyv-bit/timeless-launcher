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

#include "launch/LaunchStep.h"

/// The game can't save a world on a full disk, and a chunk it was writing when the disk filled up is lost, so this asks before
/// launching when the disk with the game's folder is nearly full
class EnsureAvailableDiskSpace : public LaunchStep {
    Q_OBJECT

   public:
    explicit EnsureAvailableDiskSpace(LaunchTask* parent) : LaunchStep(parent) {}
    ~EnsureAvailableDiskSpace() override = default;

    void executeTask() override;
    bool canAbort() const override { return false; }

    /// Below this, a session that explores a little can fill the disk: each region file of a world grows by megabytes as new
    /// chunks are saved in it
    static constexpr qint64 LowSpace = qint64{ 500 } * 1000 * 1000;
    /// Whether that much free space is too little, where a negative amount is one that couldn't be told
    static bool isLow(qint64 bytesAvailable) { return bytesAvailable >= 0 && bytesAvailable < LowSpace; }
};
