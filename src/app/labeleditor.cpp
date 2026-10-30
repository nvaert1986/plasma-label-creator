/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labeleditor.h"

#include "labelformat.h"
#include "tapelayout.h"

#include <algorithm>

using namespace LabelCreator;

namespace
{

class SnapshotCommand : public QUndoCommand
{
public:
    SnapshotCommand(LabelEditor *editor, const LabelDocument &before, const LabelDocument &after, const QList<int> &selectionAfter, int currentAfter, const QString &text)
        : QUndoCommand(text)
        , m_editor(editor)
        , m_before(before)
        , m_after(after)
        , m_selectionBefore(editor->selectedObjects())
        , m_currentBefore(editor->selection())
        , m_selectionAfter(selectionAfter)
        , m_currentAfter(currentAfter)
    {
    }

    void undo() override
    {
        m_editor->setDocument(m_before);
        m_editor->setSelectedObjects(m_selectionBefore, m_currentBefore);
    }

    void redo() override
    {
        m_editor->setDocument(m_after);
        m_editor->setSelectedObjects(m_selectionAfter, m_currentAfter);
    }

private:
    LabelEditor *m_editor;
    LabelDocument m_before;
    LabelDocument m_after;
    QList<int> m_selectionBefore;
    int m_currentBefore;
    QList<int> m_selectionAfter;
    int m_currentAfter;
};

} // namespace

LabelEditor::LabelEditor(QObject *parent)
    : QObject(parent)
{
    connect(&m_undoStack, &QUndoStack::cleanChanged, this, [this](bool clean) {
        Q_EMIT modifiedChanged(!clean);
    });
}

const LabelDocument &LabelEditor::document() const
{
    return m_document;
}

QUndoStack *LabelEditor::undoStack()
{
    return &m_undoStack;
}

void LabelEditor::reset(const LabelDocument &original, const QString &fileName)
{
    // Tape labels are kept laid out: their cells place the objects.
    LabelDocument document = original;
    TapeLayout::layout(document);
    m_undoStack.clear();
    m_fileName = fileName;
    setDocument(document);
    setSelection(-1);
    m_undoStack.setClean();
    Q_EMIT fileNameChanged();
    Q_EMIT modifiedChanged(false);
}

void LabelEditor::apply(const LabelDocument &document, const QString &description, std::optional<int> selection)
{
    // Without a selection given, the selection stays as it is.
    if (!selection) {
        apply(document, description, m_selected);
        return;
    }
    apply(document, description, *selection >= 0 ? QList<int>{*selection} : QList<int>());
}

void LabelEditor::apply(const LabelDocument &original, const QString &description, const QList<int> &selection, int current)
{
    LabelDocument document = original;
    TapeLayout::layout(document);
    if (!selection.contains(current)) {
        current = selection.contains(m_selection) ? m_selection : (selection.isEmpty() ? -1 : selection.constLast());
    }
    if (document == m_document) {
        setSelectedObjects(selection, current);
        return;
    }
    m_undoStack.push(new SnapshotCommand(this, m_document, document, selection, current, description));
}

void LabelEditor::setDocument(const LabelDocument &document)
{
    m_document = document;
    // Objects that are gone are no longer selected.
    QList<int> kept;
    for (const int index : std::as_const(m_selected)) {
        if (index < m_document.objects.size()) {
            kept << index;
        }
    }
    if (kept != m_selected) {
        setSelectedObjects(kept, m_selection < m_document.objects.size() ? m_selection : -1);
    }
    Q_EMIT documentChanged();
}

QString LabelEditor::fileName() const
{
    return m_fileName;
}

bool LabelEditor::isModified() const
{
    return !m_undoStack.isClean();
}

bool LabelEditor::load(const QString &fileName, QString *errorString, QStringList *warnings)
{
    LabelReader reader;
    const auto document = reader.readFile(fileName);
    if (warnings) {
        *warnings = reader.warnings();
    }
    if (!document) {
        *errorString = reader.errorString();
        return false;
    }
    reset(*document, fileName);
    return true;
}

bool LabelEditor::save(const QString &fileName, QString *errorString)
{
    if (!LabelWriter::writeFile(m_document, fileName, errorString)) {
        return false;
    }
    m_fileName = fileName;
    m_undoStack.setClean();
    Q_EMIT fileNameChanged();
    return true;
}

int LabelEditor::selection() const
{
    return m_selection;
}

void LabelEditor::setSelection(int index)
{
    setSelectedObjects(index >= 0 ? QList<int>{index} : QList<int>(), index);
}

QList<int> LabelEditor::selectedObjects() const
{
    return m_selected;
}

bool LabelEditor::isSelected(int index) const
{
    return m_selected.contains(index);
}

void LabelEditor::setSelectedObjects(const QList<int> &indices, int current)
{
    QList<int> selected;
    for (const int index : indices) {
        if (index >= 0 && index < m_document.objects.size() && !selected.contains(index)) {
            selected << index;
        }
    }
    std::sort(selected.begin(), selected.end());
    // Without a current one given, the last one given (clicked last) is.
    if (!selected.contains(current)) {
        current = -1;
        for (auto it = indices.crbegin(); it != indices.crend() && current < 0; ++it) {
            if (selected.contains(*it)) {
                current = *it;
            }
        }
    }
    if (selected != m_selected || current != m_selection) {
        m_selected = selected;
        m_selection = current;
        Q_EMIT selectionChanged();
    }
}
