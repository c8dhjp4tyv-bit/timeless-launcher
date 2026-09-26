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

#include "ModBisect.h"

#include <QSet>

#include <algorithm>
#include <cmath>

ModBisect::ModBisect(const QStringList& mods, const QHash<QString, QStringList>& needs)
{
    const QSet<QString> known(mods.begin(), mods.end());
    for (const auto& mod : mods) {
        for (const auto& needed : needs.value(mod)) {
            if (needed != mod && known.contains(needed)) {
                m_requiredBy[needed] << mod;
            }
        }
    }

    // The mods others need go first, otherwise in the order given. That way the second half of the suspects, which is what gets
    // turned off, holds every suspect that needs one of them.
    QSet<QString> placed;
    QStringList left = mods;
    while (!left.isEmpty()) {
        bool placedAny = false;
        for (auto it = left.begin(); it != left.end();) {
            const auto& mod = *it;
            const auto needed = needs.value(mod);
            if (std::ranges::all_of(
                    needed, [&](const QString& other) { return other == mod || !known.contains(other) || placed.contains(other); })) {
                placed << mod;
                m_mods << mod;
                it = left.erase(it);
                placedAny = true;
            } else {
                ++it;
            }
        }
        if (!placedAny) {
            // mods that need each other have no order to keep
            placed << left.first();
            m_mods << left.takeFirst();
        }
    }

    m_suspects = m_mods;
    if (m_suspects.isEmpty()) {
        m_state = State::NotFound;
    }
    planNextTest();
}

int ModBisect::testsLeft() const
{
    switch (m_state) {
        case State::Searching:
            return static_cast<int>(std::ceil(std::log2(m_suspects.size()))) + 1;
        case State::Confirming:
            return 1;
        default:
            return 0;
    }
}

void ModBisect::report(bool problemStillHappens)
{
    if (m_state == State::Searching) {
        const QSet<QString> off(m_off.begin(), m_off.end());
        // With the problem still there it isn't one of the mods that were off. Without it, it's one of them, and a suspect: the
        // others were only off because they need one of those.
        m_suspects.removeIf([&](const QString& mod) { return off.contains(mod) == problemStillHappens; });
        m_offWhenGone = problemStillHappens ? QStringList() : m_off;
    } else if (m_state == State::Confirming) {
        m_state = problemStillHappens ? State::NotFound : State::Found;
    }
    planNextTest();
}

void ModBisect::planNextTest()
{
    if (m_state == State::Searching) {
        // Turning off half of the suspects turns off the ones that need them too. When that is every suspect whichever half it
        // is, the ones left need each other and can only be tested together.
        const auto half = m_suspects.size() / 2;
        for (const auto& part : { m_suspects.mid(half), m_suspects.first(half) }) {
            if (part.isEmpty()) {
                continue;
            }
            auto off = withModsThatNeedThem(part);
            const QSet<QString> offSet(off.begin(), off.end());
            if (!std::ranges::all_of(m_suspects, [&](const QString& mod) { return offSet.contains(mod); })) {
                m_off = off;
                return;
            }
        }
        m_state = State::Confirming;
        // the last test may have been this very one already
        if (withModsThatNeedThem(m_suspects) == m_offWhenGone) {
            m_state = State::Found;
        }
    }

    if (m_state == State::Confirming) {
        m_off = withModsThatNeedThem(m_suspects);
    } else {
        m_off.clear();
    }
}

QStringList ModBisect::withModsThatNeedThem(const QStringList& mods) const
{
    QSet<QString> found(mods.begin(), mods.end());
    QStringList toVisit = mods;
    while (!toVisit.isEmpty()) {
        for (const auto& dependent : m_requiredBy.value(toVisit.takeLast())) {
            if (!found.contains(dependent)) {
                found << dependent;
                toVisit << dependent;
            }
        }
    }

    QStringList ordered;
    for (const auto& mod : m_mods) {
        if (found.contains(mod)) {
            ordered << mod;
        }
    }
    return ordered;
}
