#include "McClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTcpSocket>
#include <chrono>
#include <utility>

#include "Exception.h"
#include "Json.h"

// Long enough for a busy server on the other side of the world, short enough not to leave the list waiting on one that's gone
constexpr auto g_timeout = std::chrono::seconds(15);

McClient::McClient(QObject* parent, QString domain, QString ip, const uint16_t port)
    : QObject(parent), m_domain(std::move(domain)), m_ip(std::move(ip)), m_port(port)
{}

void McClient::getStatusData()
{
    qDebug() << "Connecting to socket..";

    connect(&m_socket, &QTcpSocket::connected, this, [this]() {
        qDebug() << "Connected to socket successfully";
        sendRequest();

        connect(&m_socket, &QTcpSocket::readyRead, this, &McClient::readRawResponse);
    });

    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this]() { emitFail("Socket disconnected: " + m_socket.errorString()); });

    // Without this, a server that drops the connection attempt or never answers keeps the query going for minutes
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        emitFail("Timed out");
        m_socket.abort();
    });
    m_timeout.start(g_timeout);

    m_socket.connectToHost(m_ip, m_port);
}

void McClient::sendRequest()
{
    QByteArray data;
    writeVarInt(data, 0x00);      // packet ID
    writeVarInt(data, 763);       // hardcoded protocol version (763 = 1.20.1)
    writeString(data, m_domain);  // server address
    writeUInt16(data, m_port);    // server port
    writeVarInt(data, 0x01);      // next state
    writePacketToSocket(data);    // send handshake packet

    writeVarInt(data, 0x00);    // packet ID
    writePacketToSocket(data);  // send status packet
}

void McClient::readRawResponse()
{
    if (m_responseReadState == ResponseReadState::Finished) {
        return;
    }

    m_resp.append(m_socket.readAll());
    if (m_responseReadState == ResponseReadState::Waiting && m_resp.size() >= 5) {
        // whatever the other end sends, it must not throw out of the slot
        try {
            m_wantedRespLength = readVarInt(m_resp);
        } catch (const Exception& e) {
            m_responseReadState = ResponseReadState::Finished;
            emitFail(e.cause());
            return;
        }
        m_responseReadState = ResponseReadState::GotLength;
    }

    if (m_responseReadState == ResponseReadState::GotLength && m_resp.size() >= m_wantedRespLength) {
        if (m_resp.size() > m_wantedRespLength) {
            qDebug().nospace() << "Warning: Packet length doesn't match actual packet size (" << m_wantedRespLength << " expected vs "
                               << m_resp.size() << " received)";
        }
        try {
            parseResponse();
        } catch (const Exception& e) {
            emitFail(e.cause());
        }
        m_responseReadState = ResponseReadState::Finished;
    }
}

void McClient::parseResponse()
{
    qDebug() << "Received response successfully";

    const int packetID = readVarInt(m_resp);
    if (packetID != 0x00) {
        throw Exception(QString("Packet ID doesn't match expected value (0x00 vs 0x%1)").arg(packetID, 0, 16));
    }

    Q_UNUSED(readVarInt(m_resp));  // json length

    // 'resp' should now be the JSON string
    QJsonParseError parseError;
    const QJsonDocument doc = Json::parseUntilGarbage(m_resp, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "Failed to parse JSON:" << parseError.errorString();
        emitFail(parseError.errorString());
        return;
    }
    emitSucceed(doc.object());
}

// NOLINTBEGIN(*-signed-bitwise)

// From https://wiki.vg/Protocol#VarInt_and_VarLong
constexpr uint8_t g_varIntValueMask = 0x7F;
constexpr uint8_t g_varIntContinue = 0x80;

void McClient::writeVarInt(QByteArray& data, int value)
{
    while ((value & ~g_varIntValueMask) != 0) {  // check if the value is too big to fit in 7 bits
        // Write 7 bits
        data.append(static_cast<uint8_t>((value & g_varIntValueMask) | g_varIntContinue));  // NOLINT(*-narrowing-conversions)

        // Erase theses 7 bits from the value to write
        // Note: >>> means that the sign bit is shifted with the rest of the number rather than being left alone
        value >>= 7;
    }
    data.append(static_cast<uint8_t>(value)); // NOLINT(*-narrowing-conversions)
}

// From https://wiki.vg/Protocol#VarInt_and_VarLong
int McClient::readVarInt(QByteArray& data)
{
    int value = 0;
    int position = 0;

    while (position < 32) {
        const uint8_t currentByte = readByte(data);
        value |= (currentByte & g_varIntValueMask) << position;

        if ((currentByte & g_varIntContinue) == 0) {
            break;
        }

        position += 7;
    }

    if (position >= 32) {
        throw Exception("VarInt is too big");
    }

    return value;
}

// NOLINTEND(*-signed-bitwise)

uint8_t McClient::readByte(QByteArray& data)
{
    if (data.isEmpty()) {
        throw Exception("No more bytes to read");
    }

    const uint8_t byte = data.at(0);
    data.remove(0, 1);
    return byte;
}

void McClient::writeUInt16(QByteArray& data, const uint16_t value)
{
    QDataStream stream(&data, QIODeviceBase::Append);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << value;
}

void McClient::writeString(QByteArray& data, const QString& value)
{
    // the length is that of the encoded string, which differs from the number of characters as soon as one isn't ASCII
    const QByteArray utf8 = value.toUtf8();
    writeVarInt(data, static_cast<int32_t>(utf8.size()));
    data.append(utf8);
}

void McClient::writePacketToSocket(QByteArray& data)
{
    // we prefix the packet with its length
    QByteArray dataWithSize;
    writeVarInt(dataWithSize, static_cast<int32_t>(data.size()));
    dataWithSize.append(data);

    // write it to the socket
    m_socket.write(dataWithSize);
    m_socket.flush();

    data.clear();
}

void McClient::emitFail(const QString& error)
{
    // the socket can still report the server hanging up after the query is over, which must not end it a second time
    if (m_done) {
        return;
    }
    m_done = true;
    m_timeout.stop();

    qDebug() << "Minecraft server ping for status error:" << error;
    emit failed(error);
    emit finished();
}

void McClient::emitSucceed(QJsonObject data)
{
    if (m_done) {
        return;
    }
    m_done = true;
    m_timeout.stop();

    emit succeeded(std::move(data));
    emit finished();
}
