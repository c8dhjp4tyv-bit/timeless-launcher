#include "LocalModParseTask.h"

#include <archive.h>
#include <qdcss.h>
#include <toml++/toml.h>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <algorithm>
#include <memory>
#include <optional>

#include "Json.h"
#include "archive/ArchiveReader.h"
#include "minecraft/mod/ModDetails.h"
#include "settings/INIFile.h"

namespace {
// Fabric and Quilt both key an icon map by the width of each image: {"32": "icon32.png", "128": "icon128.png"}
QString largestIcon(const QJsonObject& icons)
{
    int largest = 0;
    QString bestIcon;
    for (auto it = icons.begin(); it != icons.end(); ++it) {
        auto size = it.key().split('x').first().toInt();
        if (size > largest) {
            largest = size;
            bestIcon = it.value().toString();
        }
    }
    if (bestIcon.isEmpty() && !icons.isEmpty()) {
        // parsing the sizes failed, take the first
        bestIcon = icons.begin().value().toString();
    }
    return bestIcon;
}

/// Notes that the file gives the mod loader the mod with that ID, in that version
void provide(ModDetails& details, const QString& id, const QString& version)
{
    if (!details.providedMods.contains(id)) {
        details.providedMods << id;
    }
    details.providedVersions[id] << version;
}

/// A string, or an array of strings, as Fabric and Quilt give versions; anything else, or an array holding more than strings,
/// gives nothing
QStringList stringsOf(const QJsonValue& value)
{
    if (value.isString()) {
        return { value.toString() };
    }
    QStringList strings;
    for (const auto& item : value.toArray()) {
        if (!item.isString()) {
            return {};
        }
        strings << item.toString();
    }
    return strings;
}
}  // namespace

namespace ModUtils {

// NEW format
// https://github.com/MinecraftForge/FML/wiki/FML-mod-information-file/c8d8f1929aff9979e322af79a59ce81f3e02db6a

// OLD format:
// https://github.com/MinecraftForge/FML/wiki/FML-mod-information-file/5bf6a2d05145ec79387acc0d45c958642fb049fc
ModDetails ReadMCModInfo(QByteArray contents)
{
    auto getInfoFromArray = [](QJsonArray arr) -> ModDetails {
        if (!arr.at(0).isObject()) {
            return {};
        }
        ModDetails details;
        auto firstObj = arr.at(0).toObject();
        details.mod_id = firstObj.value("modid").toString();
        auto name = firstObj.value("name").toString();
        // NOTE: ignore stupid example mods copies where the author didn't even bother to change the name
        if (name != "Example Mod") {
            details.name = name;
        }
        details.version = firstObj.value("version").toString();
        auto homeurl = firstObj.value("url").toString().trimmed();
        if (!homeurl.isEmpty()) {
            // fix up url.
            if (!homeurl.startsWith("http://") && !homeurl.startsWith("https://") && !homeurl.startsWith("ftp://")) {
                homeurl.prepend("http://");
            }
        }
        details.homeurl = homeurl;
        details.description = firstObj.value("description").toString();
        QJsonArray authors = firstObj.value("authorList").toArray();
        if (authors.size() == 0) {
            // FIXME: what is the format of this? is there any?
            authors = firstObj.value("authors").toArray();
        }

        if (firstObj.contains("logoFile")) {
            details.icon_file = firstObj.value("logoFile").toString();
        }

        for (auto author : authors) {
            details.authors.append(author.toString());
        }

        if (details.mod_id.startsWith("mod_")) {
            details.mod_id = details.mod_id.mid(4);
        }

        auto addDep = [&details](QString dep) {
            if (dep == "mod_MinecraftForge" || dep == "Forge")
                return;
            if (dep.contains(":")) {
                dep = dep.section(":", 1);
            }
            if (dep.contains("@")) {
                dep = dep.section("@", 0, 0);
            }
            if (dep.startsWith("mod_")) {
                dep = dep.mid(4);
            }
            details.dependencies.append(dep);
        };

        if (firstObj.contains("requiredMods")) {
            for (auto dep : firstObj.value("requiredMods").toArray()) {
                addDep(dep.toString());
            }
        } else if (firstObj.contains("dependencies")) {
            for (auto dep : firstObj.value("dependencies").toArray()) {
                addDep(dep.toString());
            }
        }

        return details;
    };
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
    // this is the very old format that had just the array
    if (jsonDoc.isArray()) {
        return getInfoFromArray(jsonDoc.array());
    } else if (jsonDoc.isObject()) {
        auto val = jsonDoc.object().value("modinfoversion");
        if (val.isUndefined()) {
            val = jsonDoc.object().value("modListVersion");
        }

        int version = val.toInt(-1);

        // Some mods set the number with "", so it's a String instead
        if (version < 0)
            version = val.toString("").toInt();

        if (version != 2) {
            qWarning() << QString(R"(The value of 'modListVersion' is "%1" (expected "2")! The file may be corrupted.)").arg(version);
            qWarning() << "The contents of 'mcmod.info' are as follows:";
            qWarning() << contents;
        }

        auto arrVal = jsonDoc.object().value("modlist");
        if (arrVal.isUndefined()) {
            arrVal = jsonDoc.object().value("modList");
        }
        if (arrVal.isArray()) {
            return getInfoFromArray(arrVal.toArray());
        }
    }
    return {};
}

// https://github.com/MinecraftForge/Documentation/blob/5ab4ba6cf9abc0ac4c0abd96ad187461aefd72af/docs/gettingstarted/structuring.md
ModDetails ReadMCModTOML(QByteArray contents)
{
    ModDetails details;

    toml::table tomlData;
#if TOML_EXCEPTIONS
    try {
        tomlData = toml::parse(contents.toStdString());
    } catch ([[maybe_unused]] const toml::parse_error& err) {
        return {};
    }
#else
    toml::parse_result result = toml::parse(contents.toStdString());
    if (!result) {
        return {};
    }
    tomlData = result.table();
#endif

    // array defined by [[mods]]
    auto tomlModsArr = tomlData["mods"].as_array();
    if (!tomlModsArr) {
        qWarning() << "Corrupted mods.toml? Couldn't find [[mods]] array!";
        return {};
    }

    // we only really care about the first element, since multiple mods in one file is not supported by us at the moment
    auto tomlModsTable0 = tomlModsArr->get(0);
    if (!tomlModsTable0) {
        qWarning() << "Corrupted mods.toml? [[mods]] didn't have an element at index 0!";
        return {};
    }
    auto modsTable = tomlModsTable0->as_table();
    if (!modsTable) {
        qWarning() << "Corrupted mods.toml? [[mods]] was not a table!";
        return {};
    }

    // mandatory properties - always in [[mods]]
    if (auto modIdDatum = (*modsTable)["modId"].as_string()) {
        details.mod_id = QString::fromStdString(modIdDatum->get());
    }
    if (auto versionDatum = (*modsTable)["version"].as_string()) {
        details.version = QString::fromStdString(versionDatum->get());
    }
    if (auto displayNameDatum = (*modsTable)["displayName"].as_string()) {
        details.name = QString::fromStdString(displayNameDatum->get());
    }
    if (auto descriptionDatum = (*modsTable)["description"].as_string()) {
        details.description = QString::fromStdString(descriptionDatum->get());
    }

    // optional properties - can be in the root table or [[mods]]
    QString authors = "";
    if (auto authorsDatum = tomlData["authors"].as_string()) {
        authors = QString::fromStdString(authorsDatum->get());
    } else if (auto authorsDatumMods = (*modsTable)["authors"].as_string()) {
        authors = QString::fromStdString(authorsDatumMods->get());
    }
    if (!authors.isEmpty()) {
        details.authors.append(authors);
    }

    QString homeurl = "";
    if (auto homeurlDatum = tomlData["displayURL"].as_string()) {
        homeurl = QString::fromStdString(homeurlDatum->get());
    } else if (auto homeurlDatumMods = (*modsTable)["displayURL"].as_string()) {
        homeurl = QString::fromStdString(homeurlDatumMods->get());
    }
    // fix up url.
    if (!homeurl.isEmpty() && !homeurl.startsWith("http://") && !homeurl.startsWith("https://") && !homeurl.startsWith("ftp://")) {
        homeurl.prepend("http://");
    }
    details.homeurl = homeurl;

    QString issueTrackerURL = "";
    if (auto issueTrackerURLDatum = tomlData["issueTrackerURL"].as_string()) {
        issueTrackerURL = QString::fromStdString(issueTrackerURLDatum->get());
    } else if (auto issueTrackerURLDatumMods = (*modsTable)["issueTrackerURL"].as_string()) {
        issueTrackerURL = QString::fromStdString(issueTrackerURLDatumMods->get());
    }
    details.issue_tracker = issueTrackerURL;

    QString license = "";
    if (auto licenseDatum = tomlData["license"].as_string()) {
        license = QString::fromStdString(licenseDatum->get());
    } else if (auto licenseDatumMods = (*modsTable)["license"].as_string()) {
        license = QString::fromStdString(licenseDatumMods->get());
    }
    if (!license.isEmpty())
        details.licenses.append(ModLicense(license));

    QString logoFile = "";
    if (auto logoFileDatum = tomlData["logoFile"].as_string()) {
        logoFile = QString::fromStdString(logoFileDatum->get());
    } else if (auto logoFileDatumMods = (*modsTable)["logoFile"].as_string()) {
        logoFile = QString::fromStdString(logoFileDatumMods->get());
    }
    details.icon_file = logoFile;

    auto parseDep = [&details](toml::array* dependencies) {
        static const QStringList ignoreModIds = { "", "forge", "neoforge", "minecraft" };
        if (!dependencies) {
            return;
        }
        auto isNeoForgeDep = [](toml::table* t) {
            auto type = (*t)["type"].as_string();
            return type && type->get() == "required";
        };
        auto isForgeDep = [](toml::table* t) {
            auto mandatory = (*t)["mandatory"].as_boolean();
            return mandatory && mandatory->get();
        };
        for (auto& dep : *dependencies) {
            auto dep_table = dep.as_table();
            if (!dep_table) {
                continue;
            }
            auto modId = (*dep_table)["modId"].as_string();
            // the versions of Minecraft the mod works with, which the loader holds it to when the dependency is required
            if (modId && modId->get() == "minecraft" && (isNeoForgeDep(dep_table) || isForgeDep(dep_table))) {
                if (auto* range = (*dep_table)["versionRange"].as_string()) {
                    details.minecraft = GameVersionRequirement::fromMaven(QString::fromStdString(range->get()));
                }
            }
            if (!modId || ignoreModIds.contains(QString::fromStdString(modId->get()))) {
                continue;
            }
            if (isNeoForgeDep(dep_table) || isForgeDep(dep_table)) {
                details.dependencies.append(QString::fromStdString(modId->get()));
            }
        }
    };

    // every mod in the file, which the loader knows it by, and the mods each can't be loaded without on a client
    for (const auto& mod : *tomlModsArr) {
        if (const auto* table = mod.as_table()) {
            if (const auto* modId = (*table)["modId"].as_string()) {
                const auto* version = (*table)["version"].as_string();
                provide(details, QString::fromStdString(modId->get()), version ? QString::fromStdString(version->get()) : QString());
            }
        }
    }
    if (const auto* dependencies = tomlData["dependencies"].as_table()) {
        for (const auto& modId : std::as_const(details.providedMods)) {
            const auto* list = (*dependencies)[modId.toStdString()].as_array();
            if (list == nullptr) {
                continue;
            }
            for (const auto& dependency : *list) {
                const auto* table = dependency.as_table();
                if (table == nullptr) {
                    continue;
                }
                const auto* id = (*table)["modId"].as_string();
                // Forge marks a mod the loader stops without as mandatory, and NeoForge as required
                const auto* mandatory = (*table)["mandatory"].as_boolean();
                const auto* type = (*table)["type"].as_string();
                const auto* side = (*table)["side"].as_string();
                const bool required = (mandatory != nullptr && mandatory->get()) || (type != nullptr && type->get() == "required");
                const bool onlyOnServers = side != nullptr && side->get() == "SERVER";
                if (id != nullptr && required && !onlyOnServers) {
                    details.requiredMods << QString::fromStdString(id->get());
                    if (const auto* range = (*table)["versionRange"].as_string()) {
                        details.requiredVersions.insert(details.requiredMods.last(),
                                                        GameVersionRequirement::fromMaven(QString::fromStdString(range->get())));
                    }
                }
                // NeoForge won't load the mod along with one it says it is incompatible with, and only warns of a discouraged one
                if (id != nullptr && type != nullptr && type->get() == "incompatible" && !onlyOnServers) {
                    const auto* range = (*table)["versionRange"].as_string();
                    details.incompatibleMods.insert(
                        QString::fromStdString(id->get()),
                        GameVersionRequirement::fromMaven(range ? QString::fromStdString(range->get()) : QString()));
                }
            }
        }
    }

    if (tomlData.contains("dependencies")) {
        auto depValue = tomlData["dependencies"];
        if (auto array = depValue.as_array()) {
            parseDep(array);
        } else if (auto depTable = depValue.as_table()) {
            auto expectedKey = details.mod_id.toStdString();
            if (!depTable->contains(expectedKey)) {
                if (auto it = depTable->begin(); it != depTable->end()) {
                    expectedKey = it->first;
                }
            }
            if ((array = (*depTable)[expectedKey].as_array())) {
                parseDep(array);
            }
        }
    }

    return details;
}

// https://fabricmc.net/wiki/documentation:fabric_mod_json
ModDetails ReadFabricModInfo(QByteArray contents)
{
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
    auto object = jsonDoc.object();
    auto schemaVersion = object.contains("schemaVersion") ? object.value("schemaVersion").toInt(0) : 0;

    ModDetails details;

    details.mod_id = object.value("id").toString();
    details.version = object.value("version").toString();

    details.name = object.contains("name") ? object.value("name").toString() : details.mod_id;
    details.description = object.value("description").toString();

    if (schemaVersion >= 1) {
        QJsonArray authors = object.value("authors").toArray();
        for (auto author : authors) {
            if (author.isObject()) {
                details.authors.append(author.toObject().value("name").toString());
            } else {
                details.authors.append(author.toString());
            }
        }

        if (object.contains("contact")) {
            QJsonObject contact = object.value("contact").toObject();

            if (contact.contains("homepage")) {
                details.homeurl = contact.value("homepage").toString();
            }
            if (contact.contains("issues")) {
                details.issue_tracker = contact.value("issues").toString();
            }
        }

        if (object.contains("license")) {
            auto license = object.value("license");
            if (license.isArray()) {
                for (auto l : license.toArray()) {
                    if (l.isString()) {
                        details.licenses.append(ModLicense(l.toString()));
                    } else if (l.isObject()) {
                        auto obj = l.toObject();
                        details.licenses.append(ModLicense(obj.value("name").toString(), obj.value("id").toString(),
                                                           obj.value("url").toString(), obj.value("description").toString()));
                    }
                }
            } else if (license.isString()) {
                details.licenses.append(ModLicense(license.toString()));
            } else if (license.isObject()) {
                auto obj = license.toObject();
                details.licenses.append(ModLicense(obj.value("name").toString(), obj.value("id").toString(), obj.value("url").toString(),
                                                   obj.value("description").toString()));
            }
        }

        if (object.contains("icon")) {
            auto icon = object.value("icon");
            if (icon.isObject()) {
                details.icon_file = largestIcon(icon.toObject());
            } else if (icon.isString()) {
                details.icon_file = icon.toString();
            }
        }

        if (object.contains("depends")) {
            auto depends = object.value("depends");
            if (depends.isObject()) {
                auto obj = depends.toObject();
                for (auto key : obj.keys()) {
                    if (key != "fabricloader" && key != "minecraft" && !key.startsWith("fabric-")) {
                        details.dependencies.append(key);
                    }
                }
                // the versions of Minecraft the mod works with: a predicate, or a list of them any one of which will do
                details.minecraft = GameVersionRequirement::fromFabric(stringsOf(obj.value("minecraft")));
            }
        }

        // what the loader needs for the mod, in which versions, and what the file gives it besides the mod: the IDs the mod says it
        // provides, in the mod's version, and the mods nested in it
        const auto depends = object.value("depends").toObject();
        details.requiredMods = depends.keys();
        for (auto it = depends.begin(); it != depends.end(); ++it) {
            if (const auto versions = GameVersionRequirement::fromFabric(stringsOf(it.value())); versions.isSet()) {
                details.requiredVersions.insert(it.key(), versions);
            }
        }
        // the mods the loader won't load along with this one, in the versions given, where an empty list gives none
        const auto breaks = object.value("breaks").toObject();
        for (auto it = breaks.begin(); it != breaks.end(); ++it) {
            if (const auto versions = stringsOf(it.value()); !versions.isEmpty()) {
                details.incompatibleMods.insert(it.key(), GameVersionRequirement::fromFabric(versions));
            }
        }
        if (!details.mod_id.isEmpty()) {
            for (const auto& id : stringsOf(object.value("provides"))) {
                provide(details, id, details.version);
            }
        }
        for (const auto& jar : object.value("jars").toArray()) {
            details.nestedJars << jar.toObject().value("file").toString();
        }
        details.serverOnly = object.value("environment").toString() == "server";
    }
    if (!details.mod_id.isEmpty()) {
        details.providedMods.prepend(details.mod_id);
        details.providedVersions[details.mod_id].prepend(details.version);
    }
    return details;
}

// https://github.com/QuiltMC/rfcs/blob/master/specification/0002-quilt.mod.json.md
ModDetails ReadQuiltModInfo(QByteArray contents)
{
    ModDetails details;
    try {
        QJsonParseError jsonError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
        auto object = Json::requireObject(jsonDoc, "quilt.mod.json");
        auto schemaVersion = object.value("schema_version").toInt();

        // https://github.com/QuiltMC/rfcs/blob/be6ba280d785395fefa90a43db48e5bfc1d15eb4/specification/0002-quilt.mod.json.md
        if (schemaVersion == 1) {
            auto modInfo = Json::requireObject(object.value("quilt_loader"), "Quilt mod info");

            details.mod_id = Json::requireString(modInfo.value("id"), "Mod ID");
            details.version = Json::requireString(modInfo.value("version"), "Mod version");

            // what the file gives the loader besides the mod: the IDs the mod says it provides, each alone or in an object with its
            // version, and the mods nested in it
            provide(details, details.mod_id, details.version);
            for (const auto& provided : modInfo.value("provides").toArray()) {
                const auto providedObject = provided.toObject();
                const auto id = provided.isObject() ? providedObject.value("id").toString() : provided.toString();
                if (!id.isEmpty()) {
                    provide(details, id.section(':', -1), providedObject.value("version").toString(details.version));
                }
            }
            for (const auto& jar : modInfo.value("jars").toArray()) {
                details.nestedJars << jar.toString();
            }
            details.serverOnly = object.value("minecraft").toObject().value("environment").toString() == "dedicated_server";

            auto modMetadata = modInfo.value("metadata").toObject();

            details.name = modMetadata.value("name").toString(details.mod_id);
            details.description = modMetadata.value("description").toString();

            auto modContributors = modMetadata.value("contributors").toObject();

            // We don't really care about the role of a contributor here
            details.authors += modContributors.keys();

            auto modContact = modMetadata.value("contact").toObject();

            if (modContact.contains("homepage")) {
                details.homeurl = Json::requireString(modContact.value("homepage"));
            }
            if (modContact.contains("issues")) {
                details.issue_tracker = Json::requireString(modContact.value("issues"));
            }

            if (modMetadata.contains("license")) {
                auto license = modMetadata.value("license");
                if (license.isArray()) {
                    for (auto l : license.toArray()) {
                        if (l.isString()) {
                            details.licenses.append(ModLicense(l.toString()));
                        } else if (l.isObject()) {
                            auto obj = l.toObject();
                            details.licenses.append(ModLicense(obj.value("name").toString(), obj.value("id").toString(),
                                                               obj.value("url").toString(), obj.value("description").toString()));
                        }
                    }
                } else if (license.isString()) {
                    details.licenses.append(ModLicense(license.toString()));
                } else if (license.isObject()) {
                    auto obj = license.toObject();
                    details.licenses.append(ModLicense(obj.value("name").toString(), obj.value("id").toString(),
                                                       obj.value("url").toString(), obj.value("description").toString()));
                }
            }

            if (modMetadata.contains("icon")) {
                auto icon = modMetadata.value("icon");
                if (icon.isObject()) {
                    details.icon_file = largestIcon(icon.toObject());
                } else if (icon.isString()) {
                    details.icon_file = icon.toString();
                }
            }

            // depends lives in quilt_loader, next to id and version
            const auto depends = modInfo.value("depends").toArray();
            for (const auto& dependency : depends) {
                QString modId;
                if (dependency.isString()) {
                    modId = dependency.toString();
                } else if (dependency.isObject()) {
                    auto dependencyObject = dependency.toObject();
                    if (dependencyObject.value("optional").toBool()) {
                        continue;
                    }
                    modId = dependencyObject.value("id").toString();
                    // the versions of Minecraft the mod works with, unless it only needs them in some case; versions given as an
                    // object of any and all aren't read, and hold the mod to nothing
                    if (modId == "minecraft" && !dependencyObject.contains("unless")) {
                        details.minecraft = GameVersionRequirement::fromFabric(stringsOf(dependencyObject.value("versions")));
                    }
                } else {
                    // an array is satisfied by any one of its entries, so none of them is required on its own
                    continue;
                }
                // the id may be written as mavenGroup:modId, but mods are matched by their bare id
                modId = modId.section(':', -1);
                // one needed unless another mod is there may not be needed at all
                if (!modId.isEmpty() && !dependency.toObject().contains("unless")) {
                    details.requiredMods << modId;
                    const auto versions = GameVersionRequirement::fromFabric(stringsOf(dependency.toObject().value("versions")));
                    if (versions.isSet()) {
                        details.requiredVersions.insert(modId, versions);
                    }
                }
                if (!modId.isEmpty() && modId != "minecraft" && !modId.startsWith("quilt_")) {
                    details.dependencies.append(modId);
                }
            }

            // The mods the loader won't load along with this one: an ID alone for any version of it, or an object with the versions.
            // One it is only warned of, one that is fine along with another mod, and an array, which is only incompatible along with
            // all of the mods in it, aren't. Neither is one whose versions are an object of any and all, which aren't read.
            for (const auto& broken : modInfo.value("breaks").toArray()) {
                const auto brokenObject = broken.toObject();
                if (broken.isObject() && (brokenObject.value("optional").toBool() || brokenObject.contains("unless") ||
                                          brokenObject.value("versions").isObject())) {
                    continue;
                }
                const auto modId = (broken.isObject() ? brokenObject.value("id").toString() : broken.toString()).section(':', -1);
                const auto versions = stringsOf(brokenObject.value("versions"));
                if (!modId.isEmpty() && (!brokenObject.contains("versions") || !versions.isEmpty())) {
                    details.incompatibleMods.insert(modId, GameVersionRequirement::fromFabric(versions));
                }
            }
        }

    } catch (const Exception& e) {
        qWarning() << "Unable to parse mod info:" << e.cause();
    }
    return details;
}

ModDetails ReadForgeInfo(QByteArray contents)
{
    ModDetails details;
    // Read the data
    details.name = "Minecraft Forge";
    details.mod_id = "Forge";
    details.homeurl = "http://www.minecraftforge.net/forum/";
    INIFile ini;
    if (!ini.loadFile(contents))
        return details;

    QString major = ini.get("forge.major.number", "0").toString();
    QString minor = ini.get("forge.minor.number", "0").toString();
    QString revision = ini.get("forge.revision.number", "0").toString();
    QString build = ini.get("forge.build.number", "0").toString();

    details.version = major + "." + minor + "." + revision + "." + build;
    return details;
}

ModDetails ReadLiteModInfo(QByteArray contents)
{
    ModDetails details;
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
    auto object = jsonDoc.object();
    if (object.contains("name")) {
        details.mod_id = details.name = object.value("name").toString();
    }
    if (object.contains("version")) {
        details.version = object.value("version").toString("");
    } else {
        details.version = object.value("revision").toString("");
    }
    details.mcversion = object.value("mcversion").toString();
    auto author = object.value("author").toString();
    if (!author.isEmpty()) {
        details.authors.append(author);
    }
    details.description = object.value("description").toString();
    details.homeurl = object.value("url").toString();
    return details;
}

// https://git.sleeping.town/unascribed/NilLoader/src/commit/d7fc87b255fc31019ff90f80d45894927fac6efc/src/main/java/nilloader/api/NilMetadata.java#L64
ModDetails ReadNilModInfo(QByteArray contents, QString fname)
{
    ModDetails details;

    QDCSS cssData = QDCSS(contents);
    // get() hands out a new object with every answer, for the caller to delete
    auto ask = [&cssData](const QString& key) { return std::unique_ptr<std::optional<QString>>(cssData.get(key)); };
    const auto name = ask("@nilmod.name");
    const auto desc = ask("@nilmod.description");
    const auto authors = ask("@nilmod.authors");

    if (name->has_value()) {
        details.name = name->value();
    }
    if (desc->has_value()) {
        details.description = desc->value();
    }
    if (authors->has_value()) {
        details.authors.append(authors->value());
    }
    details.version = ask("@nilmod.version")->value_or("?");

    details.mod_id = fname.remove(".nilmod.css");

    return details;
}

bool process(Mod& mod, ProcessingLevel level)
{
    switch (mod.type()) {
        case ResourceType::ZIPFILE:
            return processZIP(mod, level);
        case ResourceType::LITEMOD:
            return processLitemod(mod);
        default:
            qWarning() << "Invalid type" << mod.type() << "for mod parse task!";
            return false;
    }
}

namespace {

/// Reads the metadata of the mod in the archive into the details, and returns whether there was any
bool readMetadata(MMCZip::ArchiveReader& zip, ModDetails& details)
{
    bool baseForgePopulated = false;
    bool isNilMod = false;
    bool isValid = false;
    QString manifestVersion = {};
    QByteArray nilData = {};
    QString nilFilePath = {};

    // A mod file that breaks off, as when its download was cut short, can hold its metadata before that, which names the mod on
    // the Mods page all the same
    if (!zip.parse([&details, &baseForgePopulated, &manifestVersion, &isValid, &nilData, &isNilMod, &nilFilePath](
                       MMCZip::ArchiveReader::File* file, bool& stop) {
            auto filePath = file->filename();

            if (filePath == "META-INF/mods.toml" || filePath == "META-INF/neoforge.mods.toml") {
                details = ReadMCModTOML(file->readAll());
                // NeoForge kept reading Forge's file until it got one of its own
                details.loaders = filePath == "META-INF/neoforge.mods.toml" ? ModPlatform::ModLoaderTypes(ModPlatform::NeoForge)
                                                                            : ModPlatform::Forge | ModPlatform::NeoForge;
                isValid = true;
                if (details.version == "${file.jarVersion}" && !manifestVersion.isEmpty()) {
                    details.version = manifestVersion;
                }
                stop = details.version != "${file.jarVersion}";
                baseForgePopulated = true;
                return true;
            }
            if (filePath == "META-INF/MANIFEST.MF") {
                // quick and dirty line-by-line parser, which every jar, and every jar nested in one, goes through
                const auto manifest = QString::fromUtf8(file->readAll());
                manifestVersion = "";
                for (auto line : QStringView(manifest).tokenize(u'\n')) {
                    line = line.trimmed();
                    if (line.startsWith(u"Implementation-Version: ", Qt::CaseInsensitive)) {
                        manifestVersion = line.sliced(24).toString();
                        break;
                    }
                }

                // some mods use ${projectversion} in their build.gradle, causing this mess to show up in MANIFEST.MF
                // also keep with forge's behavior of setting the version to "NONE" if none is found
                if (manifestVersion.contains("task ':jar' property 'archiveVersion'") || manifestVersion == "") {
                    manifestVersion = "NONE";
                }
                if (baseForgePopulated) {
                    details.version = manifestVersion;
                    stop = true;
                }
                return true;
            }
            if (filePath == "mcmod.info") {
                details = ReadMCModInfo(file->readAll());
                details.loaders = ModPlatform::Forge;
                isValid = true;
                stop = true;
                return true;
            }
            if (filePath == "quilt.mod.json") {
                details = ReadQuiltModInfo(file->readAll());
                details.loaders = ModPlatform::Quilt;
                isValid = true;
                stop = true;
                return true;
            }
            if (filePath == "fabric.mod.json") {
                details = ReadFabricModInfo(file->readAll());
                // Quilt loads Fabric mods too
                details.loaders = ModPlatform::Fabric | ModPlatform::Quilt;
                isValid = true;
                stop = true;
                return true;
            }
            if (filePath == "forgeversion.properties") {
                details = ReadForgeInfo(file->readAll());
                details.loaders = ModPlatform::Forge;
                isValid = true;
                stop = true;
                return true;
            }
            if (filePath == "META-INF/nil/mappings.json") {
                // nilloader uses the filename of the metadata file for the modid, so we can't know the exact filename
                // thankfully, there is a good file to use as a canary so we don't look for nil meta all the time
                isNilMod = true;
                stop = !nilFilePath.isEmpty();
                file->skip();
                return true;
            }
            // nilmods can shade nilloader to be able to run as a standalone agent - which includes nilloader's own meta file
            if (filePath.endsWith(".nilmod.css") && filePath != "nilloader.nilmod.css") {
                nilData = file->readAll();
                nilFilePath = filePath;
                stop = isNilMod;
                return true;
            }
            file->skip();
            return true;
        }) &&
        !zip.endedEarly()) {
        return false;
    }
    if (isNilMod) {
        details = ReadNilModInfo(nilData, nilFilePath);
        isValid = true;
    }
    // a Forge mod whose version comes from the manifest provides itself in that version
    for (auto& versions : details.providedVersions) {
        for (auto& version : versions) {
            if (version == "${file.jarVersion}") {
                version = details.version;
            }
        }
    }
    // what the metadata names, before the mods nested in the file are added
    details.ownVersions = details.providedVersions;
    return isValid;
}

/// How many levels of mods nested in mods are read, which is more than any mod nests them
constexpr int g_nestedModLevels = 4;

void readNestedMods(MMCZip::ArchiveReader& zip, ModDetails& details, int levels);

/// Adds what a jar nested in a mod's file gives the mod loader to the mod's details
void readNestedMod(const QByteArray& jar, const QString& name, ModDetails& details, int levels)
{
    MMCZip::ArchiveReader nested(jar, name);
    ModDetails nestedDetails;
    if (!readMetadata(nested, nestedDetails)) {
        // a library that isn't a mod gives the loader nothing, while one that couldn't be read may have
        details.unreadNestedMods |= !nested.errorString().isEmpty();
        return;
    }
    if (nestedDetails.mod_id.isEmpty() || nested.endedEarly()) {
        details.unreadNestedMods = true;
        return;
    }
    // the loader leaves out a mod only for servers, along with what is nested in it
    if (nestedDetails.serverOnly) {
        return;
    }
    readNestedMods(nested, nestedDetails, levels);
    for (const auto& id : std::as_const(nestedDetails.providedMods)) {
        for (const auto& version : nestedDetails.providedVersions.value(id)) {
            provide(details, id, version);
        }
    }
    details.unreadNestedMods |= nestedDetails.unreadNestedMods;
}

/// Adds what the jars nested in a mod's file give the mod loader to the mod's details, reading as many levels of them as given
void readNestedMods(MMCZip::ArchiveReader& zip, ModDetails& details, int levels)
{
    // Fabric and Quilt load the jars the metadata lists, and Forge and NeoForge the ones in META-INF/jarjar, which a list there only
    // picks versions of
    const bool jarJar = details.loaders.testFlag(ModPlatform::NeoForge);
    if (details.nestedJars.isEmpty() && !jarJar) {
        return;
    }
    if (levels == 0) {
        details.unreadNestedMods = true;
        return;
    }

    auto unread = details.nestedJars;
    const bool read = zip.parse([&zip, &details, &unread, jarJar, levels](MMCZip::ArchiveReader::File* file) {
        const auto path = file->filename();
        const bool listed = unread.removeAll(path) > 0;
        const bool inJarJar = jarJar && path.startsWith("META-INF/jarjar/") && path.endsWith(".jar") && path.count('/') == 2;
        if (!listed && !inJarJar) {
            file->skip();
            return true;
        }
        int status = ARCHIVE_OK;
        const auto jar = file->readAll(&status);
        if (status != ARCHIVE_EOF) {
            details.unreadNestedMods = true;
            return true;
        }
        readNestedMod(jar, zip.getZipName() + "/" + path, details, levels - 1);
        return true;
    });
    // a listed jar that isn't there, or that the file broke off before
    details.unreadNestedMods |= !read || !unread.isEmpty();
}

}  // namespace

bool processZIP(Mod& mod, ProcessingLevel level)
{
    ModDetails details;
    MMCZip::ArchiveReader zip(mod.fileinfo().filePath());
    if (!readMetadata(zip, details)) {
        return false;
    }
    if (level == ProcessingLevel::Full) {
        readNestedMods(zip, details, g_nestedModLevels);
    }
    mod.setDetails(details);
    return true;
}

bool processLitemod(Mod& mod, [[maybe_unused]] ProcessingLevel level)
{
    ModDetails details;

    MMCZip::ArchiveReader zip(mod.fileinfo().filePath());

    if (auto file = zip.goToFile("litemod.json"); file) {
        details = ReadLiteModInfo(file->readAll());
        details.loaders = ModPlatform::LiteLoader;

        mod.setDetails(details);
        return true;
    }

    return false;  // no valid litemod.json found in archive
}

bool isMissingZipEnd(const QString& path)
{
    // The record takes 22 bytes and ends with a comment of up to 65535, so, as Java does, it is looked for in the last 65557
    // bytes of the file, from their end.
    constexpr qint64 recordSize = 22;
    constexpr qint64 window = recordSize + 0xFFFF;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    if (!file.seek(std::max<qint64>(0, file.size() - window))) {
        return false;
    }
    const auto tail = file.readAll();
    static const QByteArray s_signature("PK\x05\x06", 4);
    for (auto at = tail.lastIndexOf(s_signature); at >= 0; at = at > 0 ? tail.lastIndexOf(s_signature, at - 1) : -1) {
        if (at + recordSize <= tail.size()) {
            return false;
        }
    }
    return true;
}

/** Checks whether a file is valid as a mod or not. */
bool validate(QFileInfo file)
{
    Mod mod{ file };
    return ModUtils::process(mod, ProcessingLevel::BasicInfoOnly) && mod.valid();
}

bool processIconPNG(const Mod& mod, QByteArray&& raw_data, QPixmap* pixmap)
{
    auto img = QImage::fromData(raw_data);
    if (!img.isNull()) {
        *pixmap = mod.setIcon(img);
    } else {
        qWarning() << "Failed to parse mod logo:" << mod.iconPath() << "from" << mod.name();
        return false;
    }
    return true;
}

bool loadIconFile(const Mod& mod, QPixmap* pixmap)
{
    if (mod.iconPath().isEmpty()) {
        qWarning() << "No Iconfile set, be sure to parse the mod first";
        return false;
    }

    auto png_invalid = [&mod](const QString& reason) {
        qWarning() << "Mod at" << mod.fileinfo().filePath() << "does not have a valid icon:" << reason;
        return false;
    };

    switch (mod.type()) {
        case ResourceType::ZIPFILE: {
            MMCZip::ArchiveReader zip(mod.fileinfo().filePath());
            auto file = zip.goToFile(mod.iconPath());
            if (file) {
                auto data = file->readAll();

                bool icon_result = ModUtils::processIconPNG(mod, std::move(data), pixmap);

                if (!icon_result) {
                    return png_invalid("invalid png image");  // icon png invalid
                }
                return true;
            }
            return png_invalid("Failed to set '" + mod.iconPath() +
                               "' as current file in zip archive");  // could not set icon as current file.
        }
        case ResourceType::LITEMOD: {
            return png_invalid("litemods do not have icons");  // can lightmods even have icons?
        }
        default:
            return png_invalid("Invalid type for mod, can not load icon.");
    }
}

}  // namespace ModUtils

LocalModParseTask::LocalModParseTask(int token, ResourceType type, const QFileInfo& modFile)
    : Task(false), m_token(token), m_type(type), m_modFile(modFile), m_result(new Result())
{}

bool LocalModParseTask::abort()
{
    m_aborted.store(true);
    return true;
}

void LocalModParseTask::executeTask()
{
    Mod mod{ m_modFile };
    ModUtils::process(mod, ModUtils::ProcessingLevel::Full);

    m_result->details = mod.details();
    if (mod.type() == ResourceType::ZIPFILE || mod.type() == ResourceType::LITEMOD) {
        m_result->details.damaged = ModUtils::isMissingZipEnd(m_modFile.filePath());
    }

    if (m_aborted)
        emitAborted();
    else
        emitSucceeded();
}
