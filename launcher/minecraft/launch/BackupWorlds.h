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

#include <QFileInfoList>

#include "launch/LaunchStep.h"

/// When the instance is set to, backs up each world played since its last automatic backup before the game starts, and keeps a
/// limited number of those backups per world. A backup that fails is reported in the log, but doesn't stop the launch.
class BackupWorlds : public LaunchStep {
    Q_OBJECT

   public:
    explicit BackupWorlds(LaunchTask* parent) : LaunchStep(parent) {}
    ~BackupWorlds() override = default;

    void executeTask() override;
    void proceed() override;
    bool canAbort() const override { return true; }

   public slots:
    bool abort() override;

   private:
    void backUpNext();

    QString m_backupDir;
    QFileInfoList m_pending;
    Task::Ptr m_current;
};
