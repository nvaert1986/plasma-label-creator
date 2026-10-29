/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QTextEdit>

/**
 * Edits the text of a text, address or curved text object right on the
 * label. Text in several fonts keeps them: each run remembers which of the
 * object's fonts it came from, and typed text takes the font where it goes.
 *
 * Ctrl+Return or clicking elsewhere finishes; Escape cancels. Return starts
 * a new line, except in curved text, which has one line and finishes.
 */
class InlineTextEditor : public QTextEdit
{
    Q_OBJECT

public:
    InlineTextEditor(const LabelCreator::LabelObject &object, qreal scale, QWidget *parent = nullptr);

    static bool canEdit(const LabelCreator::LabelObject &object);

    /** Puts the edited text into @p object; false if the text did not change. */
    bool apply(LabelCreator::LabelObject &object) const;
    /**
     * Takes over the fonts of @p object, changed while editing (with the
     * Format toolbar, say), keeping the text typed so far. False if they
     * cannot be matched up, as when the object's runs changed.
     */
    bool updateFonts(const LabelCreator::LabelObject &object);

Q_SIGNALS:
    /** Editing ended; @p accepted unless cancelled. */
    void finished(bool accepted);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void finish(bool accepted);
    QTextCharFormat format(int fontIndex) const;
    QList<LabelCreator::TextSpan> spans() const;

    QList<LabelCreator::TextSpan> m_originalSpans; // fonts and colours by index
    QString m_originalText;
    qreal m_scale;
    bool m_singleLine = false;
    bool m_finished = false;
};
