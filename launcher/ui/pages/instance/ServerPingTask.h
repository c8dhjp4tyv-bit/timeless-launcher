#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <tasks/Task.h>

/// What a server tells about itself when asked for its status
struct ServerStatus {
    int onlinePlayers = 0;
    int maxPlayers = 0;
    /// the name of the version it runs, which proxies and server software often put their own name in
    QString version;
    /// the message of the day, with § formatting codes
    QString motd;
    /// its icon as PNG data, empty if it has none
    QByteArray icon;
    /// how long it took to start answering, in milliseconds, or -1 if that isn't known
    qint64 latency = -1;

    static ServerStatus fromJson(const QJsonObject& status);
};

class ServerPingTask : public Task {
    Q_OBJECT
   public:
    explicit ServerPingTask(QString domain, int port) : Task(), m_domain(domain), m_port(port) {}
    ~ServerPingTask() override = default;
    ServerStatus m_outputStatus;

   private:
    QString m_domain;
    int m_port;

   protected:
    virtual void executeTask() override;
};
