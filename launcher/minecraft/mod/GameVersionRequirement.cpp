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

#include "GameVersionRequirement.h"

#include <QRegularExpression>

#include <algorithm>
#include <optional>

namespace {

/// The numbers of a release of Minecraft, as 1.20.1, and nothing for anything else, as a snapshot, which the loaders give
/// versions of their own that this doesn't work out
std::optional<QList<int>> releaseNumbers(const QString& version)
{
    static const QRegularExpression s_release(R"(^\d+(?:\.\d+)*$)");
    if (!s_release.match(version).hasMatch()) {
        return std::nullopt;
    }
    QList<int> numbers;
    for (const auto& part : version.split('.')) {
        bool ok = false;
        numbers << part.toInt(&ok);
        if (!ok) {
            return std::nullopt;
        }
    }
    return numbers;
}

/// Compares numbers the way both loaders do, a missing one counting as 0, so that 1.20 is 1.20.0
int compareNumbers(const QList<int>& a, const QList<int>& b)
{
    for (qsizetype i = 0; i < std::max(a.size(), b.size()); i++) {
        const int x = i < a.size() ? a.at(i) : 0;
        const int y = i < b.size() ? b.at(i) : 0;
        if (x != y) {
            return x < y ? -1 : 1;
        }
    }
    return 0;
}

/// A version in one of Fabric's conditions: its numbers; whether it is a pre-release, which comes just before the release of
/// its numbers ("1.21-" is the first of them); and whether it ends in a wildcard ("1.20.x"), which takes any numbers from there
struct FabricVersion {
    QList<int> numbers;
    bool preRelease = false;
    bool wildcard = false;
};

std::optional<FabricVersion> parseFabricVersion(QString text)
{
    // what comes after a + is build metadata, which doesn't count
    text = text.section('+', 0, 0);
    FabricVersion version;
    if (const auto dash = text.indexOf('-'); dash >= 0) {
        version.preRelease = true;
        text.truncate(dash);
    }
    for (const auto& part : text.split('.')) {
        if (part == "x" || part == "X" || part == "*") {
            version.wildcard = true;
            continue;
        }
        bool ok = false;
        const int number = part.toInt(&ok);
        if (version.wildcard || !ok || number < 0) {
            return std::nullopt;
        }
        version.numbers << number;
    }
    return version;
}

/// Whether one of Fabric's conditions, as ">=1.20.1", holds for a release, or nothing if it can't be read
std::optional<bool> fabricCondition(const QList<int>& release, const QString& condition)
{
    static const QStringList s_operators = { ">=", "<=", ">", "<", "=", "~", "^" };
    const auto op = std::ranges::find_if(s_operators, [&condition](const QString& candidate) { return condition.startsWith(candidate); });
    const QString opText = op == s_operators.end() ? QString() : *op;
    const auto version = parseFabricVersion(condition.mid(opText.size()));
    if (!version) {
        return std::nullopt;
    }

    if (version->wildcard) {
        // only on its own or after =: the numbers before it have to be the same
        if (!opText.isEmpty() && opText != "=") {
            return std::nullopt;
        }
        for (qsizetype i = 0; i < version->numbers.size(); i++) {
            if ((i < release.size() ? release.at(i) : 0) != version->numbers.at(i)) {
                return false;
            }
        }
        return true;
    }
    if (version->numbers.isEmpty()) {
        return std::nullopt;
    }

    int comparison = compareNumbers(release, version->numbers);
    if (comparison == 0 && version->preRelease) {
        comparison = 1;
    }
    if (opText.isEmpty() || opText == "=") {
        return comparison == 0;
    }
    if (opText == ">=") {
        return comparison >= 0;
    }
    if (opText == "<=") {
        return comparison <= 0;
    }
    if (opText == ">") {
        return comparison > 0;
    }
    if (opText == "<") {
        return comparison < 0;
    }
    // ~ keeps to the same minor version and ^ to the same major one, from the version given up to the next
    if (comparison < 0) {
        return false;
    }
    const auto& numbers = version->numbers;
    const QList<int> next =
        opText == "^" || numbers.size() < 2 ? QList<int>{ numbers.at(0) + 1 } : QList<int>{ numbers.at(0), numbers.at(1) + 1 };
    return compareNumbers(release, next) < 0;
}

/// The numbers of a bound of a Maven range, or nothing if it has more than numbers
std::optional<QList<int>> mavenNumbers(const QString& text)
{
    return releaseNumbers(text.trimmed());
}

/// Whether a Maven range takes a release, or nothing if the range can't be read
std::optional<bool> mavenRange(const QList<int>& release, const QString& text)
{
    // A version on its own only recommends one, which the loaders don't hold a mod to
    if (!text.startsWith('[') && !text.startsWith('(')) {
        return true;
    }
    qsizetype at = 0;
    while (at < text.size()) {
        // ranges one after the other, any one of which will do: [1.19.2],[1.20.1]
        while (at < text.size() && (text.at(at) == ',' || text.at(at).isSpace())) {
            at++;
        }
        if (at >= text.size()) {
            break;
        }
        const auto open = text.at(at);
        qsizetype close = at + 1;
        while (close < text.size() && text.at(close) != ']' && text.at(close) != ')') {
            close++;
        }
        if ((open != '[' && open != '(') || close >= text.size()) {
            return std::nullopt;
        }
        const auto bounds = text.mid(at + 1, close - at - 1).split(',');
        const bool lowerIncluded = open == '[';
        const bool upperIncluded = text.at(close) == ']';
        at = close + 1;

        if (bounds.size() == 1) {
            // [1.20.1] is that version alone
            const auto only = mavenNumbers(bounds.at(0));
            if (!only || !lowerIncluded || !upperIncluded) {
                return std::nullopt;
            }
            if (compareNumbers(release, *only) == 0) {
                return true;
            }
            continue;
        }
        if (bounds.size() != 2) {
            return std::nullopt;
        }
        bool inside = true;
        if (!bounds.at(0).trimmed().isEmpty()) {
            const auto lower = mavenNumbers(bounds.at(0));
            if (!lower) {
                return std::nullopt;
            }
            const int comparison = compareNumbers(release, *lower);
            inside = inside && (lowerIncluded ? comparison >= 0 : comparison > 0);
        }
        if (!bounds.at(1).trimmed().isEmpty()) {
            const auto upper = mavenNumbers(bounds.at(1));
            if (!upper) {
                return std::nullopt;
            }
            const int comparison = compareNumbers(release, *upper);
            inside = inside && (upperIncluded ? comparison <= 0 : comparison < 0);
        }
        if (inside) {
            return true;
        }
    }
    return false;
}

}  // namespace

GameVersionRequirement GameVersionRequirement::fromFabric(const QStringList& anyOf)
{
    GameVersionRequirement requirement;
    for (const auto& alternative : anyOf) {
        requirement.m_alternatives << alternative.simplified();
    }
    if (!requirement.m_alternatives.isEmpty()) {
        requirement.m_syntax = Syntax::Fabric;
    }
    return requirement;
}

GameVersionRequirement GameVersionRequirement::fromMaven(const QString& range)
{
    GameVersionRequirement requirement;
    if (!range.trimmed().isEmpty()) {
        requirement.m_syntax = Syntax::Maven;
        requirement.m_alternatives << range.trimmed();
    }
    return requirement;
}

QString GameVersionRequirement::text() const
{
    return m_alternatives.join(", ");
}

bool GameVersionRequirement::accepts(const QString& minecraftVersion) const
{
    const auto release = releaseNumbers(minecraftVersion);
    if (!release || m_syntax == Syntax::None) {
        return true;
    }

    if (m_syntax == Syntax::Maven) {
        // a placeholder the mod's build didn't fill in, as ${minecraft_version_range}, says nothing
        const auto& range = m_alternatives.first();
        return range.contains('$') || mavenRange(*release, range).value_or(true);
    }

    for (const auto& alternative : m_alternatives) {
        bool all = true;
        for (const auto& condition : alternative.split(' ', Qt::SkipEmptyParts)) {
            const auto holds = fabricCondition(*release, condition);
            if (!holds) {
                return true;
            }
            all = all && *holds;
        }
        if (all) {
            return true;
        }
    }
    return false;
}

bool GameVersionRequirement::acceptsModVersion(const QString& version) const
{
    return accepts(m_syntax == Syntax::Fabric ? version.section('+', 0, 0) : version);
}
