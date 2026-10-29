/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labeleditor.h"

#include "labelformat.h"
#include "tapelayout.h"

using namespace LabelCreator;

namespace
{

class SnapshotCommand : public QUndoCommand
{
public:
    SnapshotCommand(LabelEditor *editor, const LabelDocument &before, const LabelDocument &after, int selectionAfter, const QString &text)
        : QUndoCommand(text)
        , m_editor(editor)
        , m_before(before)
        , m_after(after)
        , m_selectionBefore(editor->selection())
        , m_selectionAfter(selectionAfter)
    {
    }

    void undo() override
    {
        m_editor->setDocument(m_before);
        m_editor->setSelection(m_selectionBefore);
    }

    void redo() override
    {
        m_editor->setDocument(m_after);
        m_editor->setSelection(m_selectionAfter);
    }

private:
    LabelEditor *m_editor;
    LabelDocument m_before;
    LabelDocument m_after;
    int m_selectionBefore;
    int m_selectionAfter;
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

void LabelEditor::apply(const LabelDocument &original, const QString &description, std::optional<int> selection)
{
    LabelDocument document = original;
    TapeLayout::layout(document);
    if (document == m_document) {
        setSelection(selection.value_or(m_selection));
        return;
    }
    m_undoStack.push(new SnapshotCommand(this, m_document, document, selection.value_or(m_selection), description));
}

void LabelEditor::setDocument(const LabelDocument &document)
{
    m_document = document;
    if (m_selection >= m_document.objects.size()) {
        setSelection(-1);
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
    if (index < -1 || index >= m_document.objects.size()) {
        index = -1;
    }
    if (m_selection != index) {
        m_selection = index;
        Q_EMIT selectionChanged();
    }
}
