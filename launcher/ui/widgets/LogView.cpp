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

#include "LogView.h"
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocumentFragment>
#include <QTimer>

#include "launch/LogModel.h"

LogView::LogView(QWidget* parent) : QPlainTextEdit(parent)
{
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_defaultFormat = new QTextCharFormat(currentCharFormat());
    setUndoRedoEnabled(false);
}

LogView::~LogView()
{
    delete m_defaultFormat;
}

void LogView::setWordWrap(bool wrapping)
{
    if (wrapping) {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setLineWrapMode(QPlainTextEdit::WidgetWidth);
    } else {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setLineWrapMode(QPlainTextEdit::NoWrap);
    }
}

void LogView::setColorLines(bool colorLines)
{
    if (m_colorLines == colorLines)
        return;
    m_colorLines = colorLines;
    repopulate();
}

void LogView::setMinimumLevel(MessageLevel level)
{
    if (m_minimumLevel == level) {
        return;
    }
    m_minimumLevel = level;
    redisplay();
}

void LogView::redisplay()
{
    // the line the cursor is on if it's in view, such as one Find went to
    auto cursor = textCursor();
    if (!viewport()->rect().contains(cursorRect(cursor).center())) {
        auto* bar = verticalScrollBar();
        if (bar->value() == bar->maximum()) {
            // at the end of the log, the view keeps up with it
            m_scroll = true;
            repopulate();
            return;
        }
        // otherwise the one in the middle
        cursor = cursorForPosition(viewport()->rect().center());
    }
    const auto block = cursor.block();
    const int row = block.userState();
    int lineOfRow = 0;
    for (auto previous = block.previous(); previous.isValid() && previous.userState() == row; previous = previous.previous()) {
        lineOfRow++;
    }

    m_scroll = false;
    repopulate();
    if (row >= 0) {
        // once the lines are laid out
        QTimer::singleShot(0, this, [this, row, lineOfRow] { showLine(row, lineOfRow); });
    }
}

void LogView::showLine(int row, int lineOfRow)
{
    auto block = document()->firstBlock();
    while (block.isValid() && block.userState() < row) {
        block = block.next();
    }
    if (!block.isValid()) {
        scrollToBottom();
        return;
    }
    for (int line = 0; line < lineOfRow && block.userState() == row && block.next().userState() == row; line++) {
        block = block.next();
    }
    setTextCursor(QTextCursor(block));
    centerCursor();
}

void LogView::setModel(QAbstractItemModel* model)
{
    if (m_model) {
        disconnect(m_model, &QAbstractItemModel::modelReset, this, &LogView::repopulate);
        disconnect(m_model, &QAbstractItemModel::rowsInserted, this, &LogView::rowsInserted);
        disconnect(m_model, &QAbstractItemModel::rowsAboutToBeInserted, this, &LogView::rowsAboutToBeInserted);
        disconnect(m_model, &QAbstractItemModel::rowsRemoved, this, &LogView::rowsRemoved);
    }
    m_model = model;
    m_rowsRemoved = 0;
    if (m_model) {
        connect(m_model, &QAbstractItemModel::modelReset, this, &LogView::repopulate);
        connect(m_model, &QAbstractItemModel::rowsInserted, this, &LogView::rowsInserted);
        connect(m_model, &QAbstractItemModel::rowsAboutToBeInserted, this, &LogView::rowsAboutToBeInserted);
        connect(m_model, &QAbstractItemModel::rowsRemoved, this, &LogView::rowsRemoved);
        connect(m_model, &QAbstractItemModel::destroyed, this, &LogView::modelDestroyed);
    }
    repopulate();
}

QAbstractItemModel* LogView::model() const
{
    return m_model;
}

void LogView::modelDestroyed(QObject* model)
{
    if (m_model == model) {
        setModel(nullptr);
    }
}

void LogView::repopulate()
{
    auto doc = document();
    doc->clear();
    if (!m_model) {
        return;
    }
    rowsInserted(QModelIndex(), 0, m_model->rowCount() - 1);
}

void LogView::rowsAboutToBeInserted(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)
    QScrollBar* bar = verticalScrollBar();
    int max_bar = bar->maximum();
    int val_bar = bar->value();
    if (m_scroll) {
        m_scroll = (max_bar - val_bar) <= 1;
    } else {
        m_scroll = val_bar == max_bar;
    }
}

void LogView::rowsInserted(const QModelIndex& parent, int first, int last)
{
    QTextDocument document;
    QTextCursor cursor(&document);
    // for each line, the row it comes from, counted the way m_rowsRemoved describes
    QList<int> rows;

    cursor.movePosition(QTextCursor::End);
    cursor.beginEditBlock();
    for (int i = first; i <= last; i++) {
        auto idx = m_model->index(i, 0, parent);
        const MessageLevel level = static_cast<MessageLevel::Enum>(m_model->data(idx, LogModel::LevelRole).toInt());
        if (level < m_minimumLevel) {
            continue;
        }
        auto text = m_model->data(idx, Qt::DisplayRole).toString();
        QTextCharFormat format(*m_defaultFormat);
        auto font = m_model->data(idx, Qt::FontRole);
        if (font.isValid()) {
            format.setFont(font.value<QFont>());
        }
        auto fg = m_model->data(idx, Qt::ForegroundRole);
        if (fg.isValid() && m_colorLines) {
            format.setForeground(fg.value<QColor>());
        }
        auto bg = m_model->data(idx, Qt::BackgroundRole);
        if (bg.isValid() && m_colorLines) {
            format.setBackground(bg.value<QColor>());
        }
        cursor.insertText(text, format);
        // one row can take several lines
        while (rows.size() <= cursor.blockNumber()) {
            rows << m_rowsRemoved + i;
        }
        cursor.insertBlock();
    }
    cursor.endEditBlock();
    if (rows.isEmpty()) {
        return;
    }

    QTextDocumentFragment fragment(&document);
    QTextCursor workCursor = textCursor();
    workCursor.movePosition(QTextCursor::End);
    const int start = workCursor.position();
    workCursor.insertFragment(fragment);
    for (auto block = QPlainTextEdit::document()->findBlock(start); const int row : rows) {
        block.setUserState(row);
        block = block.next();
    }

    if (m_scroll && !m_scrolling) {
        m_scrolling = true;
        QMetaObject::invokeMethod(this, "scrollToBottom", Qt::QueuedConnection);
    }
}

void LogView::rowsRemoved(const QModelIndex& parent, int first, int last)
{
    // TODO: take the lines out some day... maybe
    Q_UNUSED(parent)
    m_rowsRemoved += last - first + 1;
}

void LogView::scrollToBottom()
{
    m_scrolling = false;
    verticalScrollBar()->setSliderPosition(verticalScrollBar()->maximum());
}

void LogView::findNext(const QString& what, bool reverse)
{
    if (what.isEmpty())
        return;

    const QTextDocument::FindFlags flags(reverse ? QTextDocument::FindBackward : 0);

    if (find(what, flags))
        return;

    QTextCursor cursor = textCursor();

    if (reverse) {
        if (cursor.atEnd())
            return;

        cursor.movePosition(QTextCursor::End);
    } else {
        if (cursor.atStart())
            return;

        cursor.movePosition(QTextCursor::Start);
    }

    cursor = document()->find(what, cursor, flags);

    if (!cursor.isNull())
        setTextCursor(cursor);
}
