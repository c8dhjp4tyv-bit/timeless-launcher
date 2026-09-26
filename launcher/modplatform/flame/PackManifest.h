// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Timeless Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QUrl>
#include "modplatform/ModIndex.h"

namespace Flame {
struct File {
    int projectId = 0;
    int fileId = 0;
    // NOTE: the opposite to 'optional'
    bool required = true;

    ModPlatform::IndexedPack pack;
    ModPlatform::IndexedVersion version;

    // our
    QString targetFolder = QStringLiteral("mods");
};

struct Modloader {
    QString id;
    bool primary = false;
};

struct Minecraft {
    QString version;
    QString libraries;
    QList<Flame::Modloader> modLoaders;
    int recommendedRAM;
};

struct Manifest {
    QString manifestType;
    int manifestVersion = 0;
    Flame::Minecraft minecraft;
    QString name;
    QString version;
    QString author;
    // File id -> File
    QMap<int, Flame::File> files;
    QString overrides;

    bool isLoaded = false;
};

void loadManifest(Flame::Manifest& m, const QString& filepath);

/**
 * Drops the files two versions of a pack share from both lists, as they need neither downloading nor removing.
 * What is left in `files` is new, and what is left in `oldFiles` is gone from the pack or was replaced.
 * Both lists are keyed by file id, so a file found in both is the very same file.
 */
void dropUnchangedFiles(QMap<int, Flame::File>& files, QMap<int, Flame::File>& oldFiles);

/**
 * The folders, relative to the game folder, that a file from a pack may have been installed to. Everything but a
 * world is downloaded to mods/, and FlameCreationTask::validateOtherResources() then moves resource packs, shader
 * packs and the like to where they belong. Worlds are unpacked into saves/, so there is no file left to find there.
 */
QStringList installFolders();
}  // namespace Flame
