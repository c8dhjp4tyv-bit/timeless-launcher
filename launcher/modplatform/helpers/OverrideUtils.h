#pragma once

#include <QString>

namespace Override {

/** This creates a file in `parent_folder` that holds information about which
 *  overrides are in `override_path`.
 *
 *  If there's already an existing such file, it will be ovewritten.
 */
void createOverrides(const QString& name, const QString& parent_folder, const QString& override_path);

/** This reads an existing overrides archive, returning a list of overrides.
 *
 *  If there's no such file in `parent_folder`, it will return an empty list.
 */
QStringList readOverrides(const QString& name, const QString& parent_folder);

/** Turns the overrides of an update of a pack on or off as the player has the files with the same paths.
 *
 *  `game_root` is the game folder the update is being put together in, with its overrides in place, and `overrides` their
 *  paths in it. `old_game_root` is the game folder of the installed instance, where the player may have turned off one of the
 *  pack's mods by adding .disabled to its name, or turned on one the pack comes with turned off.
 */
void keepEnabledStates(const QString& game_root, const QStringList& overrides, const QString& old_game_root);

/** Merges the player's game options into the ones an update of a pack comes with, and returns the result.
 *
 *  Each is the text of an options.txt: `player` the one the game saved, `newPack` the one the update comes with, and
 *  `oldPack` the one the installed version came with, which is empty when that isn't known. The player's options win,
 *  except those still as the installed version set them, which take the update's value. The update's options the player
 *  doesn't have are added.
 */
QString mergeGameOptions(const QString& player, const QString& oldPack, const QString& newPack);

/** Keeps the player's game options through an update of a pack, once its overrides are in place.
 *
 *  `game_root` is the game folder the pack is being put together in, and `pack_folder` the folder the launcher keeps the
 *  information on the pack in, next to it. A copy of the options.txt the pack comes with is kept there for the next update.
 *  When updating, `old_game_root` and `old_pack_folder` are the same folders of the installed instance, and the options.txt
 *  of the game folder becomes the player's options merged into the pack's. Without an options.txt in the pack, the player's
 *  is kept as it is.
 */
void keepGameOptions(const QString& game_root,
                     const QString& pack_folder,
                     const QString& old_game_root = {},
                     const QString& old_pack_folder = {});

}  // namespace Override
