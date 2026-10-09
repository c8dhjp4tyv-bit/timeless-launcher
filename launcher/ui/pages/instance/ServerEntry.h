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

#include <tag_compound.h>
#include <tag_primitive.h>
#include <tag_string.h>

#include <QByteArray>
#include <QObject>
#include <QString>

#include <optional>
#include <string>

#include "ServerPingTask.h"

/// An entry of the servers.dat of an instance
struct Server {
    // Types
    enum class AcceptsTextures : int { ASK = 0, ALWAYS = 1, NEVER = 2 };

    // Methods
    Server() { m_name = QObject::tr("Minecraft Server"); }
    Server(const QString& name, const QString& address)
    {
        m_name = name;
        m_address = address;
    }
    Server(nbt::tag_compound& server)
    {
        // What the launcher doesn't edit stays as the game wrote it ("hidden", which marks a server the game doesn't list, for one)
        for (const auto& [key, tag] : server) {
            if (!isEdited(key)) {
                m_otherTags.put(key, nbt::value(tag));
            }
        }

        m_address = QString::fromStdString(readString(server, "ip"));
        m_name = QString::fromStdString(readString(server, "name"));
        m_icon = QByteArray::fromBase64(QByteArray::fromStdString(readString(server, "icon")));

        if (server.has_key("acceptTextures", nbt::tag_type::Byte)) {
            bool value = server["acceptTextures"].as<nbt::tag_byte>().get();
            if (value) {
                m_acceptsTextures = AcceptsTextures::ALWAYS;
            } else {
                m_acceptsTextures = AcceptsTextures::NEVER;
            }
        }
    }

    void serialize(nbt::tag_compound& server)
    {
        for (const auto& [key, tag] : m_otherTags) {
            server.put(key, nbt::value(tag));
        }
        server.insert("name", m_name.trimmed().toUtf8().toStdString());
        server.insert("ip", m_address.trimmed().toUtf8().toStdString());
        if (m_icon.size()) {
            server.insert("icon", m_icon.toBase64().toStdString());
        }
        if (m_acceptsTextures != AcceptsTextures::ASK) {
            server.insert("acceptTextures", nbt::tag_byte(m_acceptsTextures == AcceptsTextures::ALWAYS));
        }
    }

    // Data - persistent and user changeable
    QString m_name;
    QString m_address;
    AcceptsTextures m_acceptsTextures = AcceptsTextures::ASK;

    // Data - persistent and automatically updated
    QByteArray m_icon;

    // Data - persistent and not touched
    nbt::tag_compound m_otherTags;

    // Data - temporary
    std::optional<ServerStatus> m_status;  // nullopt if not queried (yet)
    QString m_pingError;                   // why the server couldn't be queried, empty if it could

   private:
    static bool isEdited(const std::string& key) { return key == "name" || key == "ip" || key == "icon" || key == "acceptTextures"; }

    /// Like the game, reads a missing entry or one that isn't a string as an empty string rather than failing over it.
    static std::string readString(const nbt::tag_compound& server, const std::string& key)
    {
        if (!server.has_key(key, nbt::tag_type::String)) {
            return {};
        }
        return server.at(key).as<nbt::tag_string>().get();
    }
};
