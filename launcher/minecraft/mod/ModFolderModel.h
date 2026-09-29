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
    /**
     * Enables or disables exactly these mods, without asking about the mods they need or the mods that need them, or whether to
     * go ahead while the instance is running
     */
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
    /**
     * Of each set that duplicateGroups() gives, the file names of every copy but the one to keep, which is the one with the latest
     * version, or among those with the same version, the file changed last
     */
    QStringList olderDuplicates(ModPlatform::ModLoaderTypes loaders);

    /** The file names of the enabled mods Java can't open, as when their download was cut short, which stop the mod loader too */
    QStringList damagedMods();

    /**
     * The enabled mods whose own metadata says they are made for another version of Minecraft than this one, which their mod
     * loaders refuse to start the game with, by file name
     */
    QList<Mod*> otherGameVersionMods(const QString& minecraftVersion);

    /** An enabled mod its mod loader won't load, as mods it needs aren't there, or not in the versions it needs */
    struct MissingMods {
        Mod* mod = nullptr;
        /** The IDs of the mods it needs that no enabled mod provides */
        QStringList ids;
        /** The IDs of the mods it needs that enabled mods provide, but none in a version it takes */
        QStringList otherVersions;
    };
    /**
     * The enabled mods these loaders read that need mods no enabled mod provides, by file name, as if the given ones were turned
     * off too. None are when a mod that may provide them couldn't be read, or the loader was told to change what mods need.
     */
    QList<MissingMods> modsMissingDependencies(ModPlatform::ModLoaderTypes loaders, const QSet<Mod*>& turnedOff = {});
    /** The disabled mods that provide the mod with this ID to one of these loaders, the one to turn on first */
    QList<Mod*> disabledProviders(const QString& id, ModPlatform::ModLoaderTypes loaders);
    /** The versions the enabled mods, or the mod loader itself, provide the mod with this ID in, where they can be told */
    QStringList providedVersions(const QString& id, ModPlatform::ModLoaderTypes loaders);
    /** What the mod needs of the mods it lacks, each ID with the versions it takes and those that are here, for messages */
    QStringList describeNeeds(const MissingMods& missing, ModPlatform::ModLoaderTypes loaders);
    /** The mod ID, saying what it is part of when it names a module of Fabric API, which mods nest a few of */
    static QString describeModId(const QString& id);
    /** These mods, along with the enabled mods the loaders won't load without them, and those that need those in turn */
    QSet<Mod*> withDependents(QSet<Mod*> mods, ModPlatform::ModLoaderTypes loaders);

    /** An enabled mod whose metadata says its mod loader mustn't load it along with other enabled mods */
    struct IncompatibleMods {
        Mod* mod = nullptr;
        /** The ID of the mod it mustn't be loaded with */
        QString id;
        /** The enabled mods that are that one, in a version it mustn't be loaded with */
        QList<Mod*> others;
    };
    /**
     * The enabled mods these loaders read that they mustn't load along with other enabled mods, by file name. Only mods in the
     * folder count, not ones nested in them, which the loader may leave out rather than stop, and only versions that can be told.
     */
    QList<IncompatibleMods> incompatibleMods(ModPlatform::ModLoaderTypes loaders);
    /** The mod the mod mustn't be loaded with, in the versions it mustn't, for messages */
    static QString describeIncompatibility(const IncompatibleMods& incompatible);
    /** Whether these loaders mustn't load the mod along with the enabled mods, as its metadata or one of theirs says */
    bool incompatibleWithEnabled(const Mod& mod, ModPlatform::ModLoaderTypes loaders);

   private slots:
    void onParseSucceeded(int ticket, const QString& resourceId) override;
    void onParseFinished();
    void onUpdateSucceeded() override;

   private:
    void updateDuplicates();
    void updateMissingDependencies();
    void updateIncompatibleMods();
    /** Whether Fabric or Quilt was told to change what mods need, and what they mustn't be loaded with */
    bool dependenciesOverridden(ModPlatform::ModLoaderTypes loaders) const;
    /** The version of Minecraft the instance runs, or nothing without an instance */
    QString minecraftVersion() const;
    /** The mod loaders the instance runs, or none without an instance */
    ModPlatform::ModLoaderTypes modLoaders() const;
    /** The mods the loaders provide themselves, each with its version where the instance tells it */
    QHash<QString, QStringList> builtInMods(ModPlatform::ModLoaderTypes loaders) const;

    QHash<QString, QSet<Mod*>> m_requiredBy;
    QHash<QString, QSet<Mod*>> m_requires;
    // internal ID -> file names of its duplicates
    QHash<QString, QStringList> m_duplicates;
    // internal ID -> what it needs that no enabled mod provides, or not in a version it takes
    QHash<QString, QStringList> m_missingDependencies;
    // internal ID -> the enabled mods it mustn't be loaded along with, whichever of them says so
    QHash<QString, QStringList> m_incompatible;
};
