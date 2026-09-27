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

#include <QByteArrayView>

namespace NbtSizeCheck {

/// Goes through NBT data that is a named compound, as level.dat and servers.dat are, without building it, to tell whether
/// every array, list and string in it fits in what is left of the data. The NBT library sets aside room for as many entries
/// as are claimed before it reads any, and reads an int array's on past the end of the data, so damaged or made-up data
/// claiming billions has it take gigabytes.
bool fits(QByteArrayView data);

}  // namespace NbtSizeCheck
