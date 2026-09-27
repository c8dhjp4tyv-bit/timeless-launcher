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

#include "NbtSizeCheck.h"

#include <QtEndian>

namespace {

/// Goes through the data the way the NBT library reads it, keeping track of how much of it is left
class Walker {
   public:
    explicit Walker(QByteArrayView data) : m_data(data) {}

    bool namedCompound()
    {
        quint8 type = 0;
        return byte(type) && type == Compound && string() && payload(type, 0);
    }

   private:
    enum Type : quint8 { End, Byte, Short, Int, Long, Float, Double, ByteArray, String, List, Compound, IntArray, LongArray };

    QByteArrayView m_data;
    qsizetype m_next = 0;

    qsizetype left() const { return m_data.size() - m_next; }

    bool skip(qint64 bytes)
    {
        if (bytes < 0 || bytes > left()) {
            return false;
        }
        m_next += bytes;
        return true;
    }

    bool byte(quint8& value)
    {
        if (left() < 1) {
            return false;
        }
        value = static_cast<quint8>(m_data.at(m_next++));
        return true;
    }

    bool length(qint32& value)
    {
        if (left() < 4) {
            return false;
        }
        value = qFromBigEndian<qint32>(m_data.sliced(m_next, 4).data());
        m_next += 4;
        return value >= 0;
    }

    bool string()
    {
        if (left() < 2) {
            return false;
        }
        const auto size = qFromBigEndian<quint16>(m_data.sliced(m_next, 2).data());
        m_next += 2;
        return skip(size);
    }

    bool payload(quint8 type, int depth)
    {
        // as deep as the NBT library reads
        constexpr int maxDepth = 1024;
        if (depth > maxDepth) {
            return false;
        }
        qint32 count = 0;
        switch (type) {
            case Byte:
                return skip(1);
            case Short:
                return skip(2);
            case Int:
            case Float:
                return skip(4);
            case Long:
            case Double:
                return skip(8);
            case ByteArray:
                return length(count) && skip(count);
            case IntArray:
                return length(count) && skip(qint64{ 4 } * count);
            case LongArray:
                return length(count) && skip(qint64{ 8 } * count);
            case String:
                return string();
            case List: {
                quint8 entryType = End;
                if (!byte(entryType) || !length(count)) {
                    return false;
                }
                if (entryType == End) {
                    return true;  // what it claims is ignored, as there can't be any
                }
                // one by one, which stops at the end of the data however many are claimed
                for (qint32 i = 0; i < count; i++) {
                    if (!payload(entryType, depth + 1)) {
                        return false;
                    }
                }
                return true;
            }
            case Compound:
                for (quint8 entryType = End; byte(entryType);) {
                    if (entryType == End) {
                        return true;
                    }
                    if (!string() || !payload(entryType, depth + 1)) {
                        return false;
                    }
                }
                return false;
            default:
                return false;
        }
    }
};

}  // namespace

bool NbtSizeCheck::fits(QByteArrayView data)
{
    return Walker(data).namedCompound();
}
