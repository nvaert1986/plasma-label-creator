/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QObject>
#include <QUndoStack>

/**
 * The label being edited: the document, its file, the selection and the
 * undo history. Every change goes through apply(), which records the
 * document before and after, so undo is a matter of swapping snapshots.
 */
class LabelEditor : public QObject
{
    Q_OBJECT

public:
    explicit LabelEditor(QObject *parent = nullptr);

    const LabelCreator::LabelDocument &document() const;
    QUndoStack *undoStack();

    /** Starts over with @p document, e.g. a new or opened label; clears undo. */
    void reset(const LabelCreator::LabelDocument &document, const QString &fileName = {});

    /**
     * Replaces the document with @p document as one undoable step, then
     * selects @p selection (-1 for none); by default the selection stays.
     */
    void apply(const LabelCreator::LabelDocument &document, const QString &description, std::optional<int> selection = std::nullopt);

    QString fileName() const;
    bool isModified() const;

    bool load(const QString &fileName, QString *errorString, QStringList *warnings = nullptr);
    bool save(const QString &fileName, QString *errorString);

    /** Index of the selected object, or -1. */
    int selection() const;
    void setSelection(int index);

    /** Replaces the document without recording an undo step; used by undo itself. */
    void setDocument(const LabelCreator::LabelDocument &document);

Q_SIGNALS:
    void documentChanged();
    void selectionChanged();
    void fileNameChanged();
    void modifiedChanged(bool modified);

private:
    LabelCreator::LabelDocument m_document;
    QUndoStack m_undoStack;
    QString m_fileName;
    int m_selection = -1;
};
