/*
 * Copyright 2015 Petr Mrazek <peterix@gmail.com>
 * Copyright 2021 Jamie Mansfield <jmansfield@cadixdev.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <MMCTime.h>

#include <QDateTime>
#include <QObject>
#include <QStringList>

#include <chrono>
#include <cmath>
#include <limits>

QString Time::prettifyDuration(int64_t duration, bool noDays)
{
    int seconds = (int)(duration % 60);
    duration /= 60;
    int minutes = (int)(duration % 60);
    duration /= 60;
    int hours = (int)(noDays ? duration : (duration % 24));
    int days = (int)(noDays ? 0 : (duration / 24));
    if ((hours == 0) && (days == 0)) {
        return QObject::tr("%1min %2s").arg(minutes).arg(seconds);
    }
    if (days == 0) {
        return QObject::tr("%1h %2min").arg(hours).arg(minutes);
    }
    return QObject::tr("%1d %2h %3min").arg(days).arg(hours).arg(minutes);
}

QString Time::humanReadableDuration(double duration, int precision)
{
    using days = std::chrono::duration<int, std::ratio<86400>>;

    // A transfer that has not received anything yet has an infinite ETA. Converting that, or anything too long for the
    // day count, to the integer durations below is undefined and printed values like "-2147483648days".
    if (!std::isfinite(duration) || std::abs(duration) / days::period::num >= std::numeric_limits<days::rep>::max()) {
        return QStringLiteral("∞");
    }

    bool neg = false;
    if (duration < 0) {
        neg = true;      // flag
        duration *= -1;  // invert
    }

    auto std_duration = std::chrono::duration<double>(duration);
    auto d = std::chrono::duration_cast<days>(std_duration);
    std_duration -= d;
    auto h = std::chrono::duration_cast<std::chrono::hours>(std_duration);
    std_duration -= h;
    auto m = std::chrono::duration_cast<std::chrono::minutes>(std_duration);
    std_duration -= m;
    auto s = std::chrono::duration_cast<std::chrono::seconds>(std_duration);
    std_duration -= s;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std_duration);

    auto dc = d.count();
    auto hc = h.count();
    auto mc = m.count();
    auto sc = s.count();
    auto msc = ms.count();

    // Each part is a number with its unit right after it, and the parts are separated by one space: "1d 2h 3m 4s".
    QStringList parts;
    if (dc) {
        parts << QString::number(dc) + QObject::tr("d");  // days
    }
    if (hc) {
        parts << QString::number(hc) + QObject::tr("h");  // hours
    }
    if (mc) {
        parts << QString::number(mc) + QObject::tr("m");  // minutes
    }
    if (sc) {
        parts << QString::number(sc) + QObject::tr("s");  // seconds
    }
    if ((msc && (precision > 0)) || parts.isEmpty()) {
        parts << QString::number(msc) + QObject::tr("ms");  // milliseconds
    }

    return (neg ? QStringLiteral("-") : QString()) + parts.join(' ');
}
