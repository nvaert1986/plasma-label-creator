/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QDialog>
#include <QHash>

class QTableWidget;

/**
 * Edits a label's layers: their names, and whether each shows on screen,
 * prints and can be edited. Objects not on a layer are on the label itself,
 * which always shows and prints.
 */
class LayersDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LayersDialog(const QList<LabelCreator::Layer> &layers, QWidget *parent = nullptr);

    QList<LabelCreator::Layer> layers() const;
    /** Old name to new name for every layer there was: empty when removed. */
    QHash<QString, QString> renamed() const;

private:
    void addLayer();
    void removeLayer();
    void addRow(const LabelCreator::Layer &layer, const QString &originalName);

    QTableWidget *m_table;
    QStringList m_originalNames;
};
