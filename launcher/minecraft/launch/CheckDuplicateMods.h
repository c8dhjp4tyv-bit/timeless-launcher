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

/// Mod loaders refuse to start with two copies of a mod, so rather than let the game fail after loading for a while, this
/// names them and asks whether to launch anyway
class CheckDuplicateMods : public LaunchStep {
    Q_OBJECT

   public:
    explicit CheckDuplicateMods(LaunchTask* parent) : LaunchStep(parent) {}
    ~CheckDuplicateMods() override = default;

    void executeTask() override;
    bool canAbort() const override { return false; }

   private:
    void check();

    QMetaObject::Connection m_waitForParsing;
};
