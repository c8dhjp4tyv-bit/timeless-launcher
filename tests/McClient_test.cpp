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

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QtEndian>

#include <ui/pages/instance/McClient.h>

namespace {

// VarInts, the way the protocol lays them out: 7 bits at a time, lowest first, the top bit saying whether more follow
void writeVarInt(QByteArray& data, int value)
{
    auto bits = static_cast<uint32_t>(value);
    while (bits > 0x7FU) {
        data.append(static_cast<char>((bits & 0x7FU) | 0x80U));
        bits >>= 7U;
    }
    data.append(static_cast<char>(bits));
}

int readVarInt(QByteArray& data)
{
    uint32_t value = 0;
    for (int shift = 0; shift < 35 && !data.isEmpty(); shift += 7) {
        const auto byte = static_cast<uint32_t>(static_cast<uint8_t>(data.front()));
        data.remove(0, 1);
        value |= (byte & 0x7FU) << static_cast<uint32_t>(shift);
        if ((byte & 0x80U) == 0) {
            break;
        }
    }
    return static_cast<int>(value);
}

QByteArray packet(const QByteArray& payload)
{
    QByteArray data;
    writeVarInt(data, static_cast<int>(payload.size()));
    return data + payload;
}

/// What a server answers a status request with
QByteArray statusResponse(const QJsonObject& status)
{
    const auto json = QJsonDocument(status).toJson(QJsonDocument::Compact);
    QByteArray payload;
    writeVarInt(payload, 0x00);  // packet ID
    writeVarInt(payload, static_cast<int>(json.size()));
    payload.append(json);
    return packet(payload);
}

const QJsonObject g_status{
    { "version", QJsonObject{ { "name", "1.20.1" }, { "protocol", 763 } } },
    { "players", QJsonObject{ { "max", 20 }, { "online", 5 } } },
    { "description",
      QJsonObject{ { "text", "A Minecraft server with a description long enough to need more than a byte for its length" } } },
};

struct Handshake {
    int protocolVersion = -1;
    QString address;
    int port = -1;
    int nextState = -1;
};

}  // namespace

class McClientTest : public QObject {
    Q_OBJECT

    QTcpServer m_server;
    QByteArray m_received;
    QByteArray m_reply;
    bool m_hangUp = false;

    /// Whether the client's handshake and status request have both come in
    bool gotRequest() const
    {
        auto data = m_received;
        for (int i = 0; i < 2; i++) {
            if (data.isEmpty()) {
                return false;
            }
            const int length = readVarInt(data);
            if (data.size() < length) {
                return false;
            }
            data.remove(0, length);
        }
        return true;
    }

    Handshake receivedHandshake() const
    {
        auto data = m_received;
        const int length = readVarInt(data);
        auto payload = data.left(length);

        Handshake handshake;
        if (readVarInt(payload) != 0x00) {
            return handshake;
        }
        handshake.protocolVersion = readVarInt(payload);
        const int addressLength = readVarInt(payload);
        handshake.address = QString::fromUtf8(payload.left(addressLength));
        payload.remove(0, addressLength);
        handshake.port = qFromBigEndian<quint16>(payload.constData());
        payload.remove(0, 2);
        handshake.nextState = readVarInt(payload);
        return handshake;
    }

   private slots:
    void init()
    {
        m_received.clear();
        m_reply = statusResponse(g_status);
        m_hangUp = false;

        QVERIFY(m_server.listen(QHostAddress::LocalHost));
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            auto* socket = m_server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                m_received.append(socket->readAll());
                if (!gotRequest()) {
                    return;
                }
                socket->write(m_reply);
                if (m_hangUp) {
                    socket->disconnectFromHost();
                }
            });
        });
    }

    void cleanup()
    {
        m_server.close();
        m_server.disconnect(this);
        qDeleteAll(m_server.findChildren<QTcpSocket*>());
    }

    void queryStatus()
    {
        McClient client(nullptr, "localhost", "127.0.0.1", m_server.serverPort());
        QSignalSpy succeeded(&client, &McClient::succeeded);
        QSignalSpy finished(&client, &McClient::finished);
        client.getStatusData();

        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(succeeded.count(), 1);
        const auto status = succeeded.first().first().toJsonObject();
        QCOMPARE(status["players"].toObject()["online"].toInt(), 5);
        QVERIFY(client.latency() >= 0);

        const auto handshake = receivedHandshake();
        QCOMPARE(handshake.protocolVersion, 763);
        QCOMPARE(handshake.address, "localhost");
        QCOMPARE(handshake.port, int(m_server.serverPort()));
        QCOMPARE(handshake.nextState, 1);
    }

    void nonAsciiAddress()
    {
        const QString address = QString::fromUtf8("münchen.example");
        McClient client(nullptr, address, "127.0.0.1", m_server.serverPort());
        QSignalSpy finished(&client, &McClient::finished);
        client.getStatusData();
        QTRY_COMPARE(finished.count(), 1);

        const auto handshake = receivedHandshake();
        QCOMPARE(handshake.address, address);
        QCOMPARE(handshake.port, int(m_server.serverPort()));
    }

    void garbageResponse()
    {
        // not a Minecraft server: the bytes where the length should be never end
        m_reply = QByteArray(8, '\xff');

        McClient client(nullptr, "localhost", "127.0.0.1", m_server.serverPort());
        QSignalSpy failed(&client, &McClient::failed);
        QSignalSpy finished(&client, &McClient::finished);
        client.getStatusData();

        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(failed.count(), 1);
    }

    void serverHangsUpAfterAnswering()
    {
        m_hangUp = true;

        McClient client(nullptr, "localhost", "127.0.0.1", m_server.serverPort());
        QSignalSpy succeeded(&client, &McClient::succeeded);
        QSignalSpy failed(&client, &McClient::failed);
        QSignalSpy finished(&client, &McClient::finished);
        client.getStatusData();

        QTRY_COMPARE(finished.count(), 1);
        // the connection closing after the answer is not a second outcome
        QTest::qWait(200);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(succeeded.count(), 1);
        QCOMPARE(failed.count(), 0);
    }
};

QTEST_GUILESS_MAIN(McClientTest)

#include "McClient_test.moc"
