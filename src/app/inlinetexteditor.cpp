/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inlinetexteditor.h"

#include <QKeyEvent>
#include <QTextBlock>
#include <QTextCursor>

using namespace LabelCreator;

namespace
{

// Which of the object's runs a piece of text took its font from.
constexpr int FontIndexProperty = QTextFormat::UserProperty + 1;

Qt::Alignment toQt(HorizontalAlignment alignment)
{
    switch (alignment) {
    case HorizontalAlignment::Center:
        return Qt::AlignHCenter;
    case HorizontalAlignment::Right:
        return Qt::AlignRight;
    case HorizontalAlignment::Left:
        break;
    }
    return Qt::AlignLeft;
}

// The runs and alignment of an editable object; empty runs for others.
std::pair<QList<TextSpan>, Qt::Alignment> textOf(const LabelObject &object)
{
    const TextObject *text = std::get_if<TextObject>(&object);
    if (!text) {
        text = std::get_if<AddressObject>(&object);
    }
    if (text) {
        QList<TextSpan> spans = text->spans;
        if (spans.isEmpty()) {
            spans << TextSpan{QString(), LabelFont(), text->foreColor};
        }
        return {spans, toQt(text->horizontalAlignment)};
    }
    if (const auto *curved = std::get_if<CircularTextObject>(&object)) {
        return {{TextSpan{curved->text, curved->font, curved->foreColor}}, toQt(curved->textAlignment)};
    }
    return {};
}

} // namespace

InlineTextEditor::InlineTextEditor(const LabelObject &object, qreal scale, QWidget *parent)
    : QTextEdit(parent)
    , m_scale(scale)
    , m_singleLine(std::holds_alternative<CircularTextObject>(object))
{
    setAcceptRichText(false);
    setTabChangesFocus(true);
    // Lines on a label only break where the text says so.
    setLineWrapMode(QTextEdit::NoWrap);
    document()->setDocumentMargin(2);

    const auto [spans, alignment] = textOf(object);
    m_originalSpans = spans;
    QTextOption option = document()->defaultTextOption();
    option.setAlignment(alignment);
    document()->setDefaultTextOption(option);
    QTextCursor cursor(document());
    for (int i = 0; i < m_originalSpans.size(); ++i) {
        cursor.insertText(m_originalSpans.at(i).text, format(i));
    }
    m_originalText = toPlainText();
    // Text typed into an empty object takes its font too.
    setCurrentCharFormat(format(0));
    moveCursor(QTextCursor::End);
    document()->clearUndoRedoStacks();
}

bool InlineTextEditor::canEdit(const LabelObject &object)
{
    return std::holds_alternative<TextObject>(object) || std::holds_alternative<AddressObject>(object) || std::holds_alternative<CircularTextObject>(object);
}

QTextCharFormat InlineTextEditor::format(int fontIndex) const
{
    const TextSpan &span = m_originalSpans.at(std::clamp(fontIndex, 0, int(m_originalSpans.size() - 1)));
    QTextCharFormat format;
    format.setFont(toQFont(span.font, m_scale));
    format.setForeground(span.color);
    format.setProperty(FontIndexProperty, fontIndex);
    return format;
}

QList<TextSpan> InlineTextEditor::spans() const
{
    // Runs of text by font, line breaks kept in the text as the label has them.
    QList<std::pair<int, QString>> runs;
    const auto append = [&runs](int index, const QString &text) {
        if (!runs.isEmpty() && runs.constLast().first == index) {
            runs.last().second += text;
        } else {
            runs.append({index, text});
        }
    };
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        if (block != document()->begin()) {
            append(runs.isEmpty() ? 0 : runs.constLast().first, QStringLiteral("\n"));
        }
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const int index = fragment.charFormat().hasProperty(FontIndexProperty) ? fragment.charFormat().intProperty(FontIndexProperty) : 0;
            append(std::clamp(index, 0, int(m_originalSpans.size() - 1)), fragment.text());
        }
    }

    QList<TextSpan> spans;
    for (const auto &[index, text] : std::as_const(runs)) {
        spans << TextSpan{text, m_originalSpans.at(index).font, m_originalSpans.at(index).color};
    }
    if (spans.isEmpty()) {
        spans << TextSpan{QString(), m_originalSpans.constFirst().font, m_originalSpans.constFirst().color};
    }
    return spans;
}

bool InlineTextEditor::apply(LabelObject &object) const
{
    const QString text = toPlainText();
    if (text == m_originalText) {
        return false;
    }
    if (auto *curved = std::get_if<CircularTextObject>(&object)) {
        curved->text = QString(text).replace(u'\n', u' ');
        return true;
    }
    TextObject *target = std::get_if<TextObject>(&object);
    if (!target) {
        target = std::get_if<AddressObject>(&object);
    }
    if (!target) {
        return false;
    }
    target->spans = spans();
    if (auto *address = std::get_if<AddressObject>(&object)) {
        address->fitLineFonts();
    }
    return true;
}

bool InlineTextEditor::updateFonts(const LabelObject &object)
{
    const auto [spans, alignment] = textOf(object);
    if (spans.size() != m_originalSpans.size()) {
        return false;
    }
    for (int i = 0; i < spans.size(); ++i) {
        m_originalSpans[i].font = spans.at(i).font;
        m_originalSpans[i].color = spans.at(i).color;
    }

    // Every piece of text again in the (new) font of its run.
    QList<std::tuple<int, int, int>> pieces; // position, length, font index
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            pieces.append({fragment.position(), fragment.length(), fragment.charFormat().intProperty(FontIndexProperty)});
        }
    }
    QTextCursor cursor(document());
    for (const auto &[position, length, index] : std::as_const(pieces)) {
        cursor.setPosition(position);
        cursor.setPosition(position + length, QTextCursor::KeepAnchor);
        cursor.setCharFormat(format(index));
    }
    QTextOption option = document()->defaultTextOption();
    option.setAlignment(alignment);
    document()->setDefaultTextOption(option);
    return true;
}

void InlineTextEditor::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        finish(false);
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_singleLine || (event->modifiers() & Qt::ControlModifier)) {
            finish(true);
        } else {
            // A new line; Shift+Return too, which would otherwise insert a
            // line separator the label does not know.
            textCursor().insertBlock();
        }
        return;
    default:
        QTextEdit::keyPressEvent(event);
    }
}

void InlineTextEditor::focusOutEvent(QFocusEvent *event)
{
    QTextEdit::focusOutEvent(event);
    // A context menu or another window taking the focus for a while does
    // not end editing; clicking elsewhere in the window does.
    if (event->reason() != Qt::PopupFocusReason && event->reason() != Qt::ActiveWindowFocusReason) {
        finish(true);
    }
}

void InlineTextEditor::finish(bool accepted)
{
    if (!m_finished) {
        m_finished = true;
        Q_EMIT finished(accepted);
    }
}
