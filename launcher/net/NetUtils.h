// SPDX-License-Identifier: GPL-3.0-only
/*
 * Timeless Launcher - Minecraft Launcher
 * Copyright (c) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QLocale>
#include <QNetworkReply>
#include <QSet>
#include <QTimeZone>
#include <optional>

namespace Net {
inline bool isApplicationError(QNetworkReply::NetworkError x)
{
    // Mainly taken from https://github.com/qt/qtbase/blob/dev/src/network/access/qhttpthreaddelegate.cpp
    static QSet<QNetworkReply::NetworkError> errors = { QNetworkReply::ProtocolInvalidOperationError,
                                                        QNetworkReply::AuthenticationRequiredError,
                                                        QNetworkReply::ContentAccessDenied,
                                                        QNetworkReply::ContentNotFoundError,
                                                        QNetworkReply::ContentOperationNotPermittedError,
                                                        QNetworkReply::ProxyAuthenticationRequiredError,
                                                        QNetworkReply::ContentConflictError,
                                                        QNetworkReply::ContentGoneError,
                                                        QNetworkReply::InternalServerError,
                                                        QNetworkReply::OperationNotImplementedError,
                                                        QNetworkReply::ServiceUnavailableError,
                                                        QNetworkReply::UnknownServerError,
                                                        QNetworkReply::UnknownContentError };
    return errors.contains(x);
}

// 500 class errors, see https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Status/500
// microsoft may send these error codes when services (auth) are down.
// We treat this as a reason to launch in offline mode.
inline bool isServerError(QNetworkReply::NetworkError x)
{
    static QSet<QNetworkReply::NetworkError> errors = { QNetworkReply::InternalServerError,
                                                        QNetworkReply::OperationNotImplementedError,
                                                        QNetworkReply::ServiceUnavailableError,     // 503 | seen in logs in 2026
                                                        //QNetworkReply::GatewayTimeoutError,       // 504 | seen in logs in 2024
                                                        // Qt doesn't have it mapped. Unknown covers it
                                                        QNetworkReply::UnknownServerError };
    return errors.contains(x);
}

/// The wait that a Retry-After header asks for, in seconds. The header is a number of seconds or an HTTP date, which is always in
/// GMT and in English. Gives nothing when it can't be read, or when it doesn't ask for a wait.
inline std::optional<qint64> retryAfterSeconds(const QByteArray& header, const QDateTime& now = QDateTime::currentDateTimeUtc())
{
    const auto value = header.trimmed();
    qint64 seconds = 0;
    if (value.endsWith("GMT")) {
        auto date = QLocale::c().toDateTime(QString::fromUtf8(value), "ddd, dd MMM yyyy HH:mm:ss 'GMT'");
        if (!date.isValid()) {
            return std::nullopt;
        }
        // the format has no zone, so the date was read as local time
        date.setTimeZone(QTimeZone::UTC);
        seconds = now.secsTo(date);
    } else {
        bool ok = false;
        seconds = value.toLongLong(&ok);
        if (!ok) {
            return std::nullopt;
        }
    }
    if (seconds <= 0) {
        return std::nullopt;
    }
    return seconds;
}
}  // namespace Net
