// SPDX-FileCopyrightText: 2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ModrinthAPI.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include <array>

#include "Application.h"
#include "Json.h"
#include "modplatform/ResourceType.h"
#include "net/ApiRequest.h"
#include "net/NetJob.h"

std::pair<Task::Ptr, QByteArray*> ModrinthAPI::currentVersion(const QString& hash, const QString& hashFormat)
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetCurrentVersion"), APPLICATION->network());

    auto [action, response] =
        Net::ApiRequest::makeByteArray(QString(BuildConfig.MODRINTH_PROD_URL + "/version_file/%1?algorithm=%2").arg(hash, hashFormat));
    netJob->addNetAction(action);

    return { netJob, response };
}

std::pair<Task::Ptr, QByteArray*> ModrinthAPI::currentVersions(const QStringList& hashes, const QString& hashFormat)
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetCurrentVersions"), APPLICATION->network());

    QJsonObject bodyObj;

    Json::writeStringList(bodyObj, "hashes", hashes);
    Json::writeString(bodyObj, "algorithm", hashFormat);

    QJsonDocument body(bodyObj);
    auto bodyRaw = body.toJson();

    auto [action, response] = Net::ApiRequest::makeByteArray(QString(BuildConfig.MODRINTH_PROD_URL + "/version_files"), bodyRaw);
    netJob->addNetAction(action);
    netJob->setAskRetry(false);
    return { netJob, response };
}

std::pair<Task::Ptr, QByteArray*> ModrinthAPI::latestVersion(const QString& hash,
                                                             const QString& hashFormat,
                                                             std::optional<std::vector<Version>> mcVersions,
                                                             std::optional<ModPlatform::ModLoaderTypes> loaders) const
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetLatestVersion"), APPLICATION->network());

    QJsonObject bodyObj;

    if (loaders.has_value()) {
        Json::writeStringList(bodyObj, "loaders", getModLoaderStrings(loaders.value()));
    }

    if (mcVersions.has_value()) {
        QStringList gameVersions;
        for (auto& ver : mcVersions.value()) {
            gameVersions.append(mapMCVersionToModrinth(ver));
        }
        Json::writeStringList(bodyObj, "game_versions", gameVersions);
    }

    QJsonDocument body(bodyObj);
    auto bodyRaw = body.toJson();

    auto [action, response] = Net::ApiRequest::makeByteArray(
        QString(BuildConfig.MODRINTH_PROD_URL + "/version_file/%1/update?algorithm=%2").arg(hash, hashFormat), bodyRaw);
    netJob->addNetAction(action);

    return { netJob, response };
}

std::pair<Task::Ptr, QByteArray*> ModrinthAPI::latestVersions(const QStringList& hashes,
                                                              const QString& hashFormat,
                                                              std::optional<std::vector<Version>> mcVersions,
                                                              std::optional<ModPlatform::ModLoaderTypes> loaders) const
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetLatestVersions"), APPLICATION->network());

    QJsonObject bodyObj;

    Json::writeStringList(bodyObj, "hashes", hashes);
    Json::writeString(bodyObj, "algorithm", hashFormat);

    if (loaders.has_value()) {
        Json::writeStringList(bodyObj, "loaders", getModLoaderStrings(loaders.value()));
    }

    if (mcVersions.has_value()) {
        QStringList gameVersions;
        for (auto& ver : mcVersions.value()) {
            gameVersions.append(mapMCVersionToModrinth(ver));
        }
        Json::writeStringList(bodyObj, "game_versions", gameVersions);
    }

    QJsonDocument body(bodyObj);
    auto bodyRaw = body.toJson();
    auto [action, response] = Net::ApiRequest::makeByteArray(QString(BuildConfig.MODRINTH_PROD_URL + "/version_files/update"), bodyRaw);
    netJob->addNetAction(action);

    return { netJob, response };
}

QList<QStringList> ModrinthAPI::splitProjectIds(const QStringList& ids, qsizetype perRequest)
{
    QList<QStringList> groups;
    perRequest = std::max<qsizetype>(perRequest, 1);
    for (qsizetype i = 0; i < ids.size(); i += perRequest) {
        groups.append(ids.mid(i, perRequest));
    }
    return groups;
}

QByteArray ModrinthAPI::mergeProjectReplies(const QList<QByteArray>& replies)
{
    if (replies.size() == 1) {
        return replies.first();
    }
    QJsonArray merged;
    for (const auto& reply : replies) {
        const auto doc = QJsonDocument::fromJson(reply);
        if (!doc.isArray()) {
            return reply;
        }
        for (const auto& project : doc.array()) {
            merged.append(project);
        }
    }
    return QJsonDocument(merged).toJson(QJsonDocument::Compact);
}

std::pair<Task::Ptr, QByteArray*> ModrinthAPI::getProjects(QStringList addonIds) const
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetProjects"), APPLICATION->network());

    // One request for each group of ids; the replies are put together into the first one's, which is what the caller reads.
    QList<QByteArray*> responses;
    for (const auto& group : splitProjectIds(addonIds)) {
        auto [action, response] = Net::ApiRequest::makeByteArray(QUrl(getMultipleModInfoURL(group)));
        netJob->addNetAction(action);
        responses.append(response);
    }
    if (responses.isEmpty()) {
        auto [action, response] = Net::ApiRequest::makeByteArray(QUrl(getMultipleModInfoURL({})));
        netJob->addNetAction(action);
        responses.append(response);
    }

    if (responses.size() > 1) {
        // the job keeps the requests, and with them the replies, until it is gone; this runs before whoever called reads them
        QObject::connect(netJob.get(), &Task::succeeded, netJob.get(), [responses] {
            QList<QByteArray> replies;
            for (const auto* response : responses) {
                replies.append(*response);
            }
            *responses.first() = mergeProjectReplies(replies);
        });
    }

    return { netJob, responses.first() };
}

QList<ResourceAPI::SortingMethod> ModrinthAPI::getSortingMethods() const
{
    // https://docs.modrinth.com/api-spec/#tag/projects/operation/searchProjects
    return { { .index = 1, .name = "relevance", .readableName = QObject::tr("Sort by Relevance") },
             { .index = 2, .name = "downloads", .readableName = QObject::tr("Sort by Downloads") },
             { .index = 3, .name = "follows", .readableName = QObject::tr("Sort by Follows") },
             { .index = 4, .name = "newest", .readableName = QObject::tr("Sort by Newest") },
             { .index = 5, .name = "updated", .readableName = QObject::tr("Sort by Last Updated") } };
}
namespace {
const auto g_resourceTypeMap = std::array{
    std::pair{ ModPlatform::ResourceType::Mod, "mod" },           std::pair{ ModPlatform::ResourceType::ResourcePack, "resourcepack" },
    std::pair{ ModPlatform::ResourceType::ShaderPack, "shader" }, std::pair{ ModPlatform::ResourceType::DataPack, "datapack" },
    std::pair{ ModPlatform::ResourceType::Modpack, "modpack" },
};
}

ModPlatform::ResourceType ModrinthAPI::getResourceType(const QString& param)
{
    for (const auto& [key, value] : g_resourceTypeMap) {
        if (value == param) {
            return key;
        }
    }

    qWarning() << "Invalid resource type for Modrinth API!" << param;
    return ModPlatform::ResourceType::Unknown;
}

QString ModrinthAPI::resourceTypeParameter(ModPlatform::ResourceType type)
{
    for (const auto& [key, value] : g_resourceTypeMap) {
        if (key == type) {
            return value;
        }
    }

    qWarning() << "Invalid resource type for Modrinth API!" << static_cast<std::uint8_t>(type);
    return "";
}

std::pair<Task::Ptr, QByteArray*> ModrinthAPI::getModCategories() const
{
    auto netJob = makeShared<NetJob>(QString("Modrinth::GetCategories"), APPLICATION->network());
    auto [action, response] = Net::ApiRequest::makeByteArray(QUrl(BuildConfig.MODRINTH_PROD_URL + "/tag/category"));
    netJob->addNetAction(action);
    QObject::connect(netJob.get(), &Task::failed, netJob.get(),
                     [](const QString& msg) { qDebug() << "Modrinth failed to get categories:" << msg; });

    return { netJob, response };
}

QList<ModPlatform::Category> ModrinthAPI::loadCategories(const QByteArray& response, const QString& projectType)
{
    QList<ModPlatform::Category> categories;
    QJsonParseError parseError{};
    QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Error while parsing JSON response from categories at" << parseError.offset << "reason:" << parseError.errorString();
        qWarning() << response;
        return categories;
    }

    try {
        auto arr = Json::requireArray(doc);

        for (auto val : arr) {
            auto cat = Json::requireObject(val);
            auto name = Json::requireString(cat, "name");
            if (cat["project_type"].toString() == projectType) {
                categories.push_back({ .name = name, .id = name });
            }
        }

    } catch (Json::JsonException& e) {
        qCritical() << "Failed to parse response from a version request.";
        qCritical() << e.what();
        qDebug() << doc;
    }
    return categories;
}

QList<ModPlatform::Category> ModrinthAPI::loadModCategories(const QByteArray& response) const
{
    return loadCategories(response, "mod");
}
