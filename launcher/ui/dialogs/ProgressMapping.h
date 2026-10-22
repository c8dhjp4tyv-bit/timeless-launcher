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

#include <limits>
#include <tuple>
#include <type_traits>

/// Maps a value in a numeric range of an arbitrary type to between 0 and INT_MAX, for getting the best precision out of the Qt
/// progress bar. A value outside of the range (a download that is longer than it said, say) is held at its ends, and an empty
/// range gives 0.
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
std::tuple<int, int> map_int_zero_max(T current, T range_max, T range_min)
{
    constexpr int int_max = std::numeric_limits<int>::max();

    const auto type_range = static_cast<double>(range_max) - static_cast<double>(range_min);
    if (!(type_range > 0)) {
        return { 0, int_max };
    }
    const double percentage = (static_cast<double>(current) - static_cast<double>(range_min)) / type_range;
    if (!(percentage > 0)) {  // also true for NaN
        return { 0, int_max };
    }
    if (percentage >= 1) {
        return { int_max, int_max };
    }
    return { static_cast<int>(percentage * int_max), int_max };
}
