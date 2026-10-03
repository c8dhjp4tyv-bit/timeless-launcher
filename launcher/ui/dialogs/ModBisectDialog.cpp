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

#include "ModBisectDialog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "Application.h"
#include "Exception.h"
#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModFolderModel.h"
#include "ui/dialogs/CustomMessageBox.h"

ModBisectDialog::ModBisectDialog(MinecraftInstance* instance, ModFolderModel* mods, QWidget* parent)
    : QDialog(parent)
    , m_instance(instance)
    , m_mods(mods)
    , m_heading(new QLabel(this))
    , m_text(new QLabel(this))
    , m_offList(new QListWidget(this))
    , m_startButton(new QPushButton(tr("Start"), this))
    , m_launchButton(new QPushButton(tr("Launch"), this))
    , m_stillThereButton(new QPushButton(tr("The Problem Is Still There"), this))
    , m_goneButton(new QPushButton(tr("The Problem Is Gone"), this))
    , m_keepOffButton(new QPushButton(this))
    , m_allOnButton(new QPushButton(tr("Turn Everything Back On"), this))
    , m_stopButton(new QPushButton(tr("Cancel"), this))
{
    setWindowTitle(tr("Find the Mod Behind a Problem"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(560, 420);

    auto* layout = new QVBoxLayout(this);
    auto font = m_heading->font();
    font.setBold(true);
    m_heading->setFont(font);
    // the heading and the text start at the top, one below the other, and what is left over goes below them
    m_heading->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_heading->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    layout->addWidget(m_heading);
    m_text->setWordWrap(true);
    m_text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(m_text);
    m_offList->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(m_offList, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_stopButton);
    buttons->addStretch();
    for (auto* button : { m_startButton, m_launchButton, m_stillThereButton, m_goneButton, m_keepOffButton, m_allOnButton }) {
        buttons->addWidget(button);
    }
    layout->addLayout(buttons);

    connect(m_startButton, &QPushButton::clicked, this, &ModBisectDialog::start);
    connect(m_launchButton, &QPushButton::clicked, this, [this] { APPLICATION->launch(m_instance); });
    connect(m_stillThereButton, &QPushButton::clicked, this, [this] { answer(true); });
    connect(m_goneButton, &QPushButton::clicked, this, [this] { answer(false); });
    connect(m_keepOffButton, &QPushButton::clicked, this, [this] {
        const auto found = m_bisect->suspects();
        finish({ found.begin(), found.end() });
    });
    connect(m_allOnButton, &QPushButton::clicked, this, [this] { finish({}); });
    connect(m_stopButton, &QPushButton::clicked, this, &QDialog::reject);

    // the mods that are on and can be turned off: a mod in a folder of its own can't
    for (int row = 0; row < m_mods->size(); row++) {
        const auto& mod = m_mods->at(row);
        if (mod.enabled() && mod.type() != ResourceType::FOLDER && mod.type() != ResourceType::UNKNOWN) {
            m_onAtStart << fileNameWhenOn(mod);
            m_modNames[fileNameWhenOn(mod)] = mod.name();
        }
    }

    m_heading->setText(tr("Find the mod behind a problem"));
    if (m_onAtStart.isEmpty()) {
        m_text->setText(tr("No mods are turned on, so there is nothing to search among."));
        m_startButton->setEnabled(false);
    } else {
        const ModBisect preview(m_onAtStart, {});
        m_text->setText(tr("When a crash or another problem comes from one of the mods, this finds which one. It turns off half of the "
                           "mods at a time, and after each step you start the game and say whether the problem is still there. A mod "
                           "that others need is turned off together with them, so that the game can still start.") +
                        "\n\n" +
                        tr("For the %n mod(s) that are on, it takes at most %1 tests. When it's over, or when you stop, the mods are "
                           "turned back on as they were.",
                           "", static_cast<int>(m_onAtStart.size()))
                            .arg(preview.testsLeft()));
    }
    m_offList->hide();
    for (auto* button : { m_launchButton, m_stillThereButton, m_goneButton, m_keepOffButton, m_allOnButton }) {
        button->hide();
    }
}

ModBisectDialog::~ModBisectDialog()
{
    // closing the instance's window takes this along without asking it first
    if (m_bisect && !m_over && !m_instance->isRunning()) {
        apply({});
        QFile::remove(stateFile(m_instance));
    }
}

void ModBisectDialog::done(int result)
{
    if (m_bisect && !m_over) {
        if (!apply({})) {
            return;
        }
        m_over = true;
        QFile::remove(stateFile(m_instance));
    }
    QDialog::done(result);
}

void ModBisectDialog::start()
{
    if (m_instance->isRunning()) {
        QMessageBox::information(this, windowTitle(), tr("Close the game first: mods can't be turned on or off while it runs."));
        return;
    }

    // what was on, in the instance's folder in case the launcher closes before this is over
    try {
        FS::write(stateFile(m_instance), QJsonDocument(QJsonObject{ { "on", QJsonArray::fromStringList(m_onAtStart) } }).toJson());
    } catch (const Exception& e) {
        qWarning() << "Couldn't keep the mods that are on in" << stateFile(m_instance) << ":" << e.cause();
    }

    const QSet<QString> onAtStart(m_onAtStart.begin(), m_onAtStart.end());
    QHash<QString, QStringList> needs;
    for (int row = 0; row < m_mods->size(); row++) {
        const auto& mod = m_mods->at(row);
        if (!onAtStart.contains(fileNameWhenOn(mod))) {
            continue;
        }
        for (const auto* needed : m_mods->requiredMods(mod.mod_id())) {
            if (needed->enabled()) {
                needs[fileNameWhenOn(mod)] << fileNameWhenOn(*needed);
            }
        }
    }

    m_bisect = std::make_unique<ModBisect>(m_onAtStart, needs);
    m_test = 1;
    m_stopButton->setText(tr("Stop"));
    m_startButton->hide();
    const auto off = m_bisect->modsToTurnOff();
    apply({ off.begin(), off.end() });
    showTest();
}

void ModBisectDialog::answer(bool problemStillHappens)
{
    if (m_instance->isRunning()) {
        QMessageBox::information(this, windowTitle(), tr("Close the game first: mods can't be turned on or off while it runs."));
        return;
    }

    m_bisect->report(problemStillHappens);
    m_test++;
    if (m_bisect->state() == ModBisect::State::Found || m_bisect->state() == ModBisect::State::NotFound) {
        showResult();
        return;
    }
    const auto off = m_bisect->modsToTurnOff();
    apply({ off.begin(), off.end() });
    showTest();
}

void ModBisectDialog::showTest()
{
    const auto off = m_bisect->modsToTurnOff();
    if (m_bisect->state() == ModBisect::State::Confirming) {
        m_heading->setText(tr("Last test"));
        m_text->setText(tr("It looks like the problem comes from %1. To make sure, only that is off now, along with the mods that need "
                           "it. Start the game, check whether the problem is gone, then close the game and say how it went.")
                            .arg(names(m_bisect->suspects())));
    } else {
        m_heading->setText(tr("Test %1").arg(m_test));
        m_text->setText(tr("%n mod(s) are off now, listed below. Start the game, check whether the problem happens, then close the "
                           "game and say how it went.",
                           "", static_cast<int>(off.size())) +
                        "\n\n" + tr("At most %n more test(s) after this one.", "", m_bisect->testsLeft() - 1));
    }

    m_offList->clear();
    for (const auto& mod : off) {
        m_offList->addItem(QString("%1 (%2)").arg(m_modNames.value(mod, mod), mod));
    }
    m_offList->show();
    for (auto* button : { m_launchButton, m_stillThereButton, m_goneButton }) {
        button->show();
    }
}

void ModBisectDialog::showResult()
{
    for (auto* button : { m_launchButton, m_stillThereButton, m_goneButton }) {
        button->hide();
    }
    const auto suspects = m_bisect->suspects();
    m_offList->clear();
    for (const auto& mod : suspects) {
        m_offList->addItem(QString("%1 (%2)").arg(m_modNames.value(mod, mod), mod));
    }

    if (m_bisect->state() == ModBisect::State::Found) {
        m_heading->setText(tr("Found it"));
        m_text->setText(suspects.size() == 1
                            ? tr("The problem comes from %1. You can keep it turned off and the other mods back on, then update or "
                                 "remove it on the Mods page.")
                                  .arg(names(suspects))
                            : tr("The problem comes from one of these mods, which need each other and so could only be tested "
                                 "together. You can keep them turned off and the other mods back on."));
        m_keepOffButton->setText(suspects.size() == 1 ? tr("Keep It Off") : tr("Keep Them Off"));
        m_keepOffButton->show();
    } else {
        m_heading->setText(tr("It isn't one mod"));
        m_text->setText(tr("The problem was still there with %1 turned off, so it doesn't come from one mod alone. It may come from two "
                           "mods together, or not from the mods at all.")
                            .arg(names(suspects)));
    }
    m_allOnButton->show();
    m_stopButton->hide();
}

bool ModBisectDialog::apply(const QSet<QString>& off)
{
    if (m_instance->isRunning()) {
        QMessageBox::information(this, windowTitle(), tr("Close the game first: mods can't be turned on or off while it runs."));
        return false;
    }

    const QSet<QString> onAtStart(m_onAtStart.begin(), m_onAtStart.end());
    QModelIndexList toEnable;
    QModelIndexList toDisable;
    for (int row = 0; row < m_mods->size(); row++) {
        const auto& mod = m_mods->at(row);
        const auto name = fileNameWhenOn(mod);
        if (!onAtStart.contains(name)) {
            continue;
        }
        const bool on = !off.contains(name);
        if (mod.enabled() != on) {
            (on ? toEnable : toDisable) << m_mods->index(row, 0);
        }
    }
    m_mods->setModsEnabled(toDisable, EnableAction::DISABLE);
    m_mods->setModsEnabled(toEnable, EnableAction::ENABLE);
    return true;
}

void ModBisectDialog::finish(const QSet<QString>& keepOff)
{
    if (!apply(keepOff)) {
        return;
    }
    m_over = true;
    QFile::remove(stateFile(m_instance));
    accept();
}

QString ModBisectDialog::names(const QStringList& mods) const
{
    QStringList named;
    for (const auto& mod : mods) {
        named << QString("%1 (%2)").arg(m_modNames.value(mod, mod), mod);
    }
    return named.join(", ");
}

QString ModBisectDialog::stateFile(MinecraftInstance* instance)
{
    return FS::PathCombine(instance->instanceRoot(), "problem-mod-search.json");
}

QString ModBisectDialog::fileNameWhenOn(const Mod& mod)
{
    auto name = mod.fileinfo().fileName();
    if (name.endsWith(".disabled")) {
        name.chop(9);
    }
    return name;
}

void ModBisectDialog::offerToFinishInterruptedSearch(MinecraftInstance* instance, ModFolderModel* mods, QWidget* parent)
{
    const auto file = stateFile(instance);
    if (!QFileInfo::exists(file) || instance->isRunning()) {
        return;
    }

    QByteArray state;
    try {
        state = FS::read(file);
    } catch (const Exception& e) {
        qWarning() << "Couldn't read" << file << ":" << e.cause();
        return;
    }

    QStringList stillOff;
    const QDir dir = mods->dir();
    for (const auto& value : QJsonDocument::fromJson(state).object().value("on").toArray()) {
        const auto name = value.toString();
        if (!name.isEmpty() && QFileInfo::exists(dir.filePath(name + ".disabled")) && !QFileInfo::exists(dir.filePath(name))) {
            stillOff << name;
        }
    }

    if (!stillOff.isEmpty()) {
        auto* box = CustomMessageBox::selectable(parent, tr("Find the Mod Behind a Problem"),
                                                 tr("A search for the mod behind a problem didn't finish, and %n mod(s) it turned off "
                                                    "are still off. Turn them back on?",
                                                    "", static_cast<int>(stillOff.size())),
                                                 QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        box->setDetailedText(stillOff.join('\n'));
        if (box->exec() == QMessageBox::Yes) {
            for (const auto& name : stillOff) {
                QFile::rename(dir.filePath(name + ".disabled"), dir.filePath(name));
            }
            mods->update();
        }
    }
    QFile::remove(file);
}
