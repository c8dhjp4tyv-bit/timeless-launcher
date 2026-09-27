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

#include <QHash>
#include <QString>
#include <QStringList>

#include <cstdint>

/// Finds the mod behind a problem the way a person would by hand: it turns off half of the mods that may be causing it, and the
/// person checks whether the problem is still there. A mod that others need is only ever turned off together with them, so that
/// the mod loader can still start the game. Once it is down to one mod, or to mods that need each other and so can't be told
/// apart, a last test turns off just those, to make sure the problem goes away with them.
class ModBisect {
   public:
    enum class State : std::uint8_t { Searching, Confirming, Found, NotFound };

    /// mods: the mods to search among, which are all on to begin with; needs: for each of them, the mods among them it needs
    ModBisect(const QStringList& mods, const QHash<QString, QStringList>& needs);

    State state() const { return m_state; }
    /// The mods that may still be the cause, or the ones found: one, or a few that need each other
    QStringList suspects() const { return m_suspects; }
    /// The mods to turn off for the next test, while the others stay on
    QStringList modsToTurnOff() const { return m_off; }
    /// At most how many more tests it takes
    int testsLeft() const;

    /// Tells how the test with modsToTurnOff() off went
    void report(bool problemStillHappens);

   private:
    void planNextTest();
    /// The mods together with every mod that needs one of them, directly or not, in the order of m_mods
    QStringList withModsThatNeedThem(const QStringList& mods) const;

    /// every mod, the ones others need before the ones that need them
    QStringList m_mods;
    QHash<QString, QStringList> m_requiredBy;
    State m_state = State::Searching;
    QStringList m_suspects;
    QStringList m_off;
    /// the mods that were off in the last test, if the problem was gone in it
    QStringList m_offWhenGone;
};
