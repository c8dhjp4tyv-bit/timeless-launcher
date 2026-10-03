#include "EntitlementsStep.h"

#include <QList>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>
#include <memory>

#include "Application.h"
#include "Logging.h"
#include "minecraft/auth/Parsers.h"
#include "net/NetJob.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"
#include "net/Request.h"
#include "tasks/Task.h"

EntitlementsStep::EntitlementsStep(AccountData* data) : AuthStep(data) {}

QString EntitlementsStep::describe()
{
    return tr("Determining game ownership.");
}

void EntitlementsStep::perform()
{
    m_entitlements_request_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QUrl url("https://api.minecraftservices.com/entitlements/license?requestId=" + m_entitlements_request_id);
    auto headers = QList<Net::HeaderPair>{ { "Content-Type", "application/json" },
                                           { "Accept", "application/json" },
                                           { "Authorization", QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8() } };

    auto [request, response] = Net::Request::makeByteArray(url);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("EntitlementsStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
    qDebug() << "Getting entitlements...";
}

void EntitlementsStep::onRequestDone(QByteArray* response)
{
    qCDebug(authCredentials()) << *response;

    if (m_request->error() != QNetworkReply::NoError) {
        // What came back from a request that failed, as when Mojang limits the requests it answers, says nothing about what the
        // account owns, and must not take the game away from it
        qWarning() << "Error getting entitlements:" << m_request->error() << m_request->errorString();
        auto message = tr("Failed to determine game ownership: %1").arg(m_request->errorString());
        if (const auto reason = Parsers::parseMojangError(*response); !reason.isEmpty()) {
            message += "\n" + reason;
        }
        if (Net::isApplicationError(m_request->error()) && !Net::isServerError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT, message);
        } else {
            m_data->networkError = m_request->error();
            emit finished(AccountTaskState::STATE_OFFLINE, message);
        }
        return;
    }

    // TODO: check presence of same entitlementsRequestId?
    // TODO: validate JWTs?
    if (!Parsers::parseMinecraftEntitlements(*response, m_data->minecraftEntitlement)) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to determine game ownership: the answer could not be read."));
        return;
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Got entitlements"));
}
