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

#include <QString>
#include <QStringList>

#include <cstdint>

/// The versions of Minecraft a mod's own metadata says it works with, read the way its mod loader reads them, which refuses to
/// start the game with a mod made for another version. Anything this can't read, as a snapshot or a placeholder left in by the
/// mod's build, counts as accepted, so that a mod is never held back for what may well work.
class GameVersionRequirement {
   public:
    GameVersionRequirement() = default;

    /// Fabric's and Quilt's version predicates: the mod takes any one of the alternatives, each a list of conditions separated
    /// by spaces that must all hold, as ">=1.20.1 <1.21", "~1.20", "1.20.x" or "1.20.1"
    static GameVersionRequirement fromFabric(const QStringList& anyOf);
    /// Forge's and NeoForge's Maven version ranges, as "[1.20.1,1.21)", "[1.20,)" or "[1.19.2],[1.20.1]"
    static GameVersionRequirement fromMaven(const QString& range);

    bool isSet() const { return m_syntax != Syntax::None; }
    /// The requirement as the mod wrote it
    QString text() const;
    /// Whether the mod takes that version of Minecraft
    bool accepts(const QString& minecraftVersion) const;
    /// Whether a mod that needs another takes that version of it, which unlike a version of Minecraft may carry build metadata
    /// after a +, which Fabric and Quilt leave out of comparisons
    bool acceptsModVersion(const QString& version) const;

    bool operator==(const GameVersionRequirement& other) const = default;

   private:
    enum class Syntax : std::uint8_t { None, Fabric, Maven };
    Syntax m_syntax = Syntax::None;
    QStringList m_alternatives;
};
