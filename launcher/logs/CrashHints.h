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

#include <QCoreApplication>
#include <QString>
#include <QStringList>

/// Picks out, from the log of a game that didn't exit cleanly, the causes of a crash that are both common and easy to fix
/// once named: the wrong Java version, too little memory, a JVM argument Java doesn't accept, a missing graphics driver.
class CrashHints {
    Q_DECLARE_TR_FUNCTIONS(CrashHints)

   public:
    /// @param log the game's log as the launcher shows it
    /// @param javaMajorVersion the major version of the Java the game ran on, or 0 if it isn't known
    /// @return an explanation of each cause found, saying what to change
    static QStringList find(const QString& log, int javaMajorVersion = 0);
};
