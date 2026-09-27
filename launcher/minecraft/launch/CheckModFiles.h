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

/// Mod loaders stop at a mod file Java can't open and at a mod made for another version of Minecraft, and refuse to start with
/// two copies of a mod, so rather than let the game fail after loading for a while, this names such files and offers to turn
/// them off
class CheckModFiles : public LaunchStep {
    Q_OBJECT

   public:
    explicit CheckModFiles(LaunchTask* parent) : LaunchStep(parent) {}
    ~CheckModFiles() override = default;

    void executeTask() override;
    bool canAbort() const override { return false; }

   private:
    /// What to ask about some mods the loader would stop at, and what to say about each answer
    struct Question {
        QString title;
        QString text;
        /// The button that turns the mods off, which is the default
        QString turnOff;
        QStringList fileNames;
        /// Logged, followed by the file names, once they are off
        QString turnedOff;
        /// What the launch fails with when turning them off does, and when the question is canceled
        QString couldNotTurnOff;
        QString canceled;
    };

    void check();
    /// Each of these says what it found and asks what to do, and returns whether the launch goes on; it has failed if not
    bool checkDamaged(ModFolderModel* mods);
    bool checkGameVersion(ModFolderModel* mods);
    bool checkDuplicates(ModFolderModel* mods, ModPlatform::ModLoaderTypes loaders);
    bool ask(ModFolderModel* mods, const Question& question);

    QMetaObject::Connection m_waitForParsing;
};
