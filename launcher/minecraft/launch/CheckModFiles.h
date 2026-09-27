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
#include "modplatform/ModIndex.h"

class ModFolderModel;

/// Mod loaders stop at a mod file Java can't open, and refuse to start with two copies of a mod, so rather than let the game
/// fail after loading for a while, this names such files and offers to turn them off
class CheckModFiles : public LaunchStep {
    Q_OBJECT

   public:
    explicit CheckModFiles(LaunchTask* parent) : LaunchStep(parent) {}
    ~CheckModFiles() override = default;

    void executeTask() override;
    bool canAbort() const override { return false; }

   private:
    void check();
    /// Each of these says what it found and asks what to do, and returns whether the launch goes on; it has failed if not
    bool checkDamaged(ModFolderModel* mods);
    bool checkDuplicates(ModFolderModel* mods, ModPlatform::ModLoaderTypes loaders);

    QMetaObject::Connection m_waitForParsing;
};
