/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QDialog>

#include <functional>

class QLabel;
class QListWidget;
class QStackedWidget;

/**
 * Makes a label of a kind (cable wrap, cable flag, patch panel, terminal
 * block, breaker panel, rack markers) from a few values, on the label type
 * in use, with a preview.
 */
class LabelKindsDialog : public QDialog
{
    Q_OBJECT

public:
    /** @p base is the label in use: its type (tape or die-cut) is kept. */
    explicit LabelKindsDialog(const LabelCreator::LabelDocument &base, QWidget *parent = nullptr);

    /** The label made, as the values stand. */
    LabelCreator::LabelDocument label() const;

private:
    void addKind(const QString &icon, const QString &name, const QString &description, QWidget *form, std::function<LabelCreator::LabelDocument()> make);
    void updatePreview();

    LabelCreator::LabelDocument m_base;
    QListWidget *m_kinds;
    QStackedWidget *m_forms;
    QLabel *m_preview;
    QLabel *m_size;
    QList<std::function<LabelCreator::LabelDocument()>> m_makers;
};
