/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "objectfactory.h"

#include <QListWidget>

#include <functional>

class QMimeData;

/** Dragging objects and images from the sidebar onto the label. */
namespace DragSupport
{

/** Drag data for a new object of @p kind. */
QMimeData *objectMimeData(ObjectFactory::Kind kind);
/** Drag data for image files, which other programs understand too. */
QMimeData *imageMimeData(const QStringList &fileNames);

/** The kind of object dragged, if @p data is a new object. */
std::optional<ObjectFactory::Kind> objectKind(const QMimeData *data);
/** The local image files dragged, if any. */
QStringList imageFiles(const QMimeData *data);

/** Whether the label accepts @p data dropped on it. */
bool canDrop(const QMimeData *data);

} // namespace DragSupport

/** A list whose items are dragged as the data a function gives for them. */
class DragListWidget : public QListWidget
{
public:
    using MimeDataFunction = std::function<QMimeData *(const QList<QListWidgetItem *> &items)>;

    explicit DragListWidget(MimeDataFunction mimeData, QWidget *parent = nullptr);

protected:
    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override;
    QStringList mimeTypes() const override;

private:
    MimeDataFunction m_mimeData;
};
