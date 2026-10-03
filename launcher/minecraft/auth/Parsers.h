#pragma once

#include "AccountData.h"

namespace Parsers {
bool getDateTime(QJsonValue value, QDateTime& out);
bool getString(QJsonValue value, QString& out);
bool getNumber(QJsonValue value, double& out);
bool getNumber(QJsonValue value, int64_t& out);
bool getBool(QJsonValue value, bool& out);

bool parseXTokenResponse(QByteArray& data, Token& output, QString name);
bool parseMojangResponse(QByteArray& data, Token& output);

bool parseMinecraftProfile(QByteArray& data, MinecraftProfile& output);
bool parseMinecraftProfileMojang(QByteArray& data, MinecraftProfile& output);
bool parseMinecraftEntitlements(QByteArray& data, MinecraftEntitlement& output);
bool parseRolloutResponse(QByteArray& data, bool& result);

/// The reason a service of Mojang gives for refusing a request, or nothing if the answer doesn't give one
QString parseMojangError(const QByteArray& data);
}  // namespace Parsers
