// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Timeless Launcher - Minecraft Launcher
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
 */

#pragma once

#include <QDialog>
#include <QHash>
#include <QSet>
#include <QStringList>

#include <memory>

#include "minecraft/mod/ModBisect.h"

class MinecraftInstance;
class Mod;
class ModFolderModel;
class QLabel;
class QListWidget;
class QPushButton;

/// Walks the player through finding the mod behind a problem: it turns off half of the mods at a time, and after each step the
/// player starts the game and says whether the problem is still there. The mods that were on are turned back on when it is over,
/// however it ends. What they were is also kept in the instance's folder, so that they can be turned back on even if the launcher
/// closes in the middle of it.
class ModBisectDialog : public QDialog {
    Q_OBJECT

   public:
    ModBisectDialog(MinecraftInstance* instance, ModFolderModel* mods, QWidget* parent = nullptr);
    ~ModBisectDialog() override;

    /// If a search didn't get to turn the mods back on, offers to do it now
    static void offerToFinishInterruptedSearch(MinecraftInstance* instance, ModFolderModel* mods, QWidget* parent);

    void done(int result) override;

   private:
    void start();
    void answer(bool problemStillHappens);
    void showTest();
    void showResult();
    /// Turns on the mods that were on at the start, except the ones given; false if that has to wait for the game to close
    bool apply(const QSet<QString>& off);
    /// Ends the search, leaving the given mods off and turning the others back on
    void finish(const QSet<QString>& keepOff);
    QString names(const QStringList& mods) const;

    static QString stateFile(MinecraftInstance* instance);
    /// The file name of a mod as it is when it's on
    static QString fileNameWhenOn(const Mod& mod);

    MinecraftInstance* m_instance;
    ModFolderModel* m_mods;
    std::unique_ptr<ModBisect> m_bisect;
    QStringList m_onAtStart;
    QHash<QString, QString> m_modNames;
    int m_test = 0;
    bool m_over = false;

    QLabel* m_heading;
    QLabel* m_text;
    QListWidget* m_offList;
    QPushButton* m_startButton;
    QPushButton* m_launchButton;
    QPushButton* m_stillThereButton;
    QPushButton* m_goneButton;
    QPushButton* m_keepOffButton;
    QPushButton* m_allOnButton;
    QPushButton* m_stopButton;
};
