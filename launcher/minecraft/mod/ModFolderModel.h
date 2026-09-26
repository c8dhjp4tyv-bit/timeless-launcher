// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Timeless Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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

#include <QAbstractListModel>
#include <QDir>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>

#include "Mod.h"
#include "ResourceFolderModel.h"
#include "minecraft/mod/Resource.h"

class BaseInstance;
class QFileSystemWatcher;

/**
 * A legacy mod list.
 * Backed by a folder.
 */
class ModFolderModel : public ResourceFolderModel {
    Q_OBJECT
   public:
    enum Columns : std::uint8_t {
        ActiveColumn = 0,
        ImageColumn,
        NameColumn,
        VersionColumn,
        DateColumn,
        ProviderColumn,
        SizeColumn,
        SideColumn,
        LoadersColumn,
        McVersionsColumn,
        ReleaseTypeColumn,
        RequiresColumn,
        RequiredByColumn,
        FileNameColumn,
        NumColumns
    };
    ModFolderModel(const QDir& dir, MinecraftInstance* instance, bool isIndexed, bool createDir, QObject* parent = nullptr);

    QString id() const override { return "mods"; }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] Resource* createResource(const QFileInfo& file) override { return new Mod(file); }
    [[nodiscard]] Task* createParseTask(Resource& /*unused*/) override;

    bool isValid();

    bool setResourceEnabled(const QModelIndexList& indexes, EnableAction action) override;
    /** Enables or disables exactly these mods, without asking about the mods they need or the mods that need them */
    bool setModsEnabled(const QModelIndexList& indexes, EnableAction action);
    bool deleteResources(const QModelIndexList& indexes) override;

    QModelIndexList getAffectedMods(const QModelIndexList& indexes, EnableAction action);

    RESOURCE_HELPERS(Mod)

   public:
    QStringList requiresList(const QString& id) const;
    /** The mods in this folder that the mod with that ID needs */
    QList<Mod*> requiredMods(const QString& id) const;
    QStringList requiredByList(const QString& id) const;

    /**
     * The file names of the other enabled mods that have the same mod ID as the given one and are read by the same
     * loader. Mod loaders refuse to start with two copies of a mod, so every one of them but one has to go.
     */
    QStringList duplicatesOf(const QString& internalId) const { return m_duplicates.value(internalId); }

    /** The file names of each set of enabled mods that share a mod ID and would all be read by one of these loaders */
    QList<QStringList> duplicateGroups(ModPlatform::ModLoaderTypes loaders);

   private slots:
    void onParseSucceeded(int ticket, const QString& resourceId) override;
    void onParseFinished();
    void onUpdateSucceeded() override;

   private:
    void updateDuplicates();

    QHash<QString, QSet<Mod*>> m_requiredBy;
    QHash<QString, QSet<Mod*>> m_requires;
    // internal ID -> file names of its duplicates
    QHash<QString, QStringList> m_duplicates;
};
