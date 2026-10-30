/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "gs1.h"

#include <QDialog>

class QLabel;
class QPushButton;
class QTableWidget;

/**
 * Builds GS1 data from application identifiers and their values, checking
 * each, instead of typing [AI] brackets: for GS1-128, GS1 Data Matrix and
 * GS1 DataBar Expanded. A value may use fields, checked when printing.
 */
class Gs1Dialog : public QDialog
{
    Q_OBJECT

public:
    /** Starts from @p data when it is written with [AI] brackets. */
    explicit Gs1Dialog(const QString &data, QWidget *parent = nullptr);

    /** The data with [AI] brackets; a missing check digit is added. */
    QString data() const;
    QList<LabelCreator::Gs1::Element> elements() const;

    void addElement(const LabelCreator::Gs1::Element &element);

private:
    void removeElement();
    void update();

    QTableWidget *m_table;
    QLabel *m_result;
    QPushButton *m_ok;
};
