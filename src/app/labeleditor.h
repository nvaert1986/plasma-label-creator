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
    /** As apply(), selecting @p selection afterwards. */
    void apply(const LabelCreator::LabelDocument &document, const QString &description, const QList<int> &selection);

    QString fileName() const;
    bool isModified() const;

    bool load(const QString &fileName, QString *errorString, QStringList *warnings = nullptr);
    bool save(const QString &fileName, QString *errorString);

    /**
     * The current object: the one selected last, which properties, editing
     * text and the format toolbar's state follow. -1 for none.
     */
    int selection() const;
    /** Selects only @p index (nothing for -1). */
    void setSelection(int index);
    /** Every selected object, in drawing order. */
    QList<int> selectedObjects() const;
    bool isSelected(int index) const;
    /** Selects @p indices; @p current is the current one, or the last of them. */
    void setSelectedObjects(const QList<int> &indices, int current = -1);

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
    QList<int> m_selected; // sorted; contains m_selection when that is >= 0
};
