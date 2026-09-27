#pragma once
#include <QAbstractItemView>
#include <QPlainTextEdit>

#include "MessageLevel.h"

class QAbstractItemModel;

class LogView : public QPlainTextEdit {
    Q_OBJECT
   public:
    explicit LogView(QWidget* parent = nullptr);
    virtual ~LogView();

    virtual void setModel(QAbstractItemModel* model);
    QAbstractItemModel* model() const;

   public slots:
    void setWordWrap(bool wrapping);
    void setColorLines(bool colorLines);
    /// Shows only the lines at this level or a higher one
    void setMinimumLevel(MessageLevel level);
    void findNext(const QString& what, bool reverse);
    void scrollToBottom();

   protected slots:
    void repopulate();
    // note: this supports only appending
    void rowsInserted(const QModelIndex& parent, int first, int last);
    void rowsAboutToBeInserted(const QModelIndex& parent, int first, int last);
    // note: this supports only removing from front
    void rowsRemoved(const QModelIndex& parent, int first, int last);
    void modelDestroyed(QObject* model);

   protected:
    /// Shows the lines anew, staying on the one that was being looked at
    void redisplay();
    /// Moves to the given line of what the given row of the model shows, or to the first line after it that is shown
    void showLine(int row, int lineOfRow);

    QAbstractItemModel* m_model = nullptr;
    QTextCharFormat* m_defaultFormat = nullptr;
    bool m_scroll = false;
    bool m_scrolling = false;
    bool m_colorLines = true;
    MessageLevel m_minimumLevel = MessageLevel::Unknown;
    /// How many rows the model has dropped from its start. Each line of the view keeps, as its block's user state, the row it
    /// comes from plus this, so that it still points to that row once the rows before it are gone.
    int m_rowsRemoved = 0;
};
