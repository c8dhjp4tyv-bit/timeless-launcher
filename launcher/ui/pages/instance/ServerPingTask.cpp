#include <QFutureWatcher>
#include <QHash>
#include <QJsonArray>

#include <array>
#include <utility>

#include "McClient.h"
#include "McResolver.h"
#include "ServerPingTask.h"

namespace {

/// The style a text component gives its text, and passes on to the components under it
struct ChatStyle {
    QChar color;  // the § code of the color, null for the default one
    bool bold = false;
    bool italic = false;
    bool underlined = false;
    bool strikethrough = false;
    bool obfuscated = false;

    /// Formatting codes that give text this style whatever came before it
    QString codes() const
    {
        QString codes;
        const auto add = [&codes](QChar code) {
            codes += u'\u00A7';
            codes += code;
        };
        // start over: a color code would end the formatting before it, and formatting codes would keep the color
        add(QLatin1Char('r'));
        if (!color.isNull()) {
            add(color);
        }
        const std::array<std::pair<bool, char>, 5> formats = { {
            { bold, 'l' },
            { italic, 'o' },
            { underlined, 'n' },
            { strikethrough, 'm' },
            { obfuscated, 'k' },
        } };
        for (const auto& [on, code] : formats) {
            if (on) {
                add(QLatin1Char(code));
            }
        }
        return codes;
    }
};

/// A text component (https://minecraft.wiki/w/Text_component_format) flattened into text with § formatting codes, which is
/// how a message of the day comes from older servers anyway
QString componentToText(const QJsonValue& component, ChatStyle style = {})
{
    if (component.isString()) {
        return style.codes() + component.toString();
    }
    if (component.isArray()) {
        QString text;
        for (const auto& part : component.toArray()) {
            text += componentToText(part, style);
        }
        return text;
    }
    if (!component.isObject()) {
        return {};
    }

    static const QHash<QString, QChar> s_colorCodes = {
        { "black", '0' }, { "dark_blue", '1' },    { "dark_green", '2' }, { "dark_aqua", '3' }, { "dark_red", '4' }, { "dark_purple", '5' },
        { "gold", '6' },  { "gray", '7' },         { "dark_gray", '8' },  { "blue", '9' },      { "green", 'a' },    { "aqua", 'b' },
        { "red", 'c' },   { "light_purple", 'd' }, { "yellow", 'e' },     { "white", 'f' },
    };
    const auto object = component.toObject();
    // what isn't set here is inherited, and hex colors have no code to stand for them
    if (const auto color = s_colorCodes.value(object.value("color").toString()); !color.isNull()) {
        style.color = color;
    }
    const auto flag = [&object](const char* key, bool inherited) {
        const auto value = object.value(key);
        return value.isBool() ? value.toBool() : inherited;
    };
    style.bold = flag("bold", style.bold);
    style.italic = flag("italic", style.italic);
    style.underlined = flag("underlined", style.underlined);
    style.strikethrough = flag("strikethrough", style.strikethrough);
    style.obfuscated = flag("obfuscated", style.obfuscated);

    QString text = style.codes() + object.value("text").toString();
    for (const auto& extra : object.value("extra").toArray()) {
        text += componentToText(extra, style);
    }
    return text;
}

}  // namespace

ServerStatus ServerStatus::fromJson(const QJsonObject& status)
{
    // anything missing or odd is left empty, a server that answers at all is up
    ServerStatus result;
    const auto players = status.value("players").toObject();
    result.onlinePlayers = players.value("online").toInt();
    result.maxPlayers = players.value("max").toInt();
    result.version = status.value("version").toObject().value("name").toString();
    result.motd = componentToText(status.value("description"));

    static const QString s_pngPrefix = "data:image/png;base64,";
    if (const auto favicon = status.value("favicon").toString(); favicon.startsWith(s_pngPrefix)) {
        result.icon = QByteArray::fromBase64(favicon.mid(s_pngPrefix.size()).toLatin1());
    }
    return result;
}

void ServerPingTask::executeTask()
{
    qDebug() << "Querying status of" << QString("%1:%2").arg(m_domain).arg(m_port);

    // Resolve the actual IP and port for the server
    McResolver* resolver = new McResolver(nullptr, m_domain, m_port);
    connect(resolver, &McResolver::succeeded, this, [this](QString ip, int port) {
        qDebug().nospace().noquote() << "Resolved address for " << m_domain << ": " << ip << ":" << port;

        // Now that we have the IP and port, query the server
        McClient* client = new McClient(nullptr, m_domain, ip, port);

        connect(client, &McClient::succeeded, this, [this, client](const QJsonObject& data) {
            m_outputStatus = ServerStatus::fromJson(data);
            m_outputStatus.latency = client->latency();
            qDebug() << "Online players:" << m_outputStatus.onlinePlayers << "of" << m_outputStatus.maxPlayers;
            emitSucceeded();
        });
        connect(client, &McClient::failed, this, [this](QString error) { emitFailed(error); });

        // Delete McClient object when done
        connect(client, &McClient::finished, this, [client]() { client->deleteLater(); });
        client->getStatusData();
    });
    connect(resolver, &McResolver::failed, this, [this](QString error) { emitFailed(error); });

    // Delete McResolver object when done
    connect(resolver, &McResolver::finished, resolver, &McResolver::deleteLater);
    resolver->ping();
}
