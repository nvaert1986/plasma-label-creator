/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dragsupport.h"

#include <QImageReader>
#include <QMimeData>
#include <QUrl>

using namespace Qt::Literals::StringLiterals;

namespace
{

const QString ObjectMimeType = u"application/x-plasma-label-creator-object"_s;

} // namespace

namespace DragSupport
{

QMimeData *objectMimeData(ObjectFactory::Kind kind)
{
    auto *data = new QMimeData;
    data->setData(ObjectMimeType, QByteArray::number(int(kind)));
    return data;
}

QMimeData *imageMimeData(const QStringList &fileNames)
{
    auto *data = new QMimeData;
    QList<QUrl> urls;
    for (const QString &fileName : fileNames) {
        urls << QUrl::fromLocalFile(fileName);
    }
    data->setUrls(urls);
    return data;
}

std::optional<ObjectFactory::Kind> objectKind(const QMimeData *data)
{
    if (!data || !data->hasFormat(ObjectMimeType)) {
        return std::nullopt;
    }
    bool ok = false;
    const int kind = data->data(ObjectMimeType).toInt(&ok);
    if (!ok || kind < 0 || kind > int(ObjectFactory::Kind::CurvedText)) {
        return std::nullopt;
    }
    return ObjectFactory::Kind(kind);
}

QStringList imageFiles(const QMimeData *data)
{
    QStringList files;
    if (!data || !data->hasUrls()) {
        return files;
    }
    for (const QUrl &url : data->urls()) {
        if (url.isLocalFile() && !QImageReader::imageFormat(url.toLocalFile()).isEmpty()) {
            files << url.toLocalFile();
        }
    }
    return files;
}

bool canDrop(const QMimeData *data)
{
    return objectKind(data).has_value() || !imageFiles(data).isEmpty();
}

} // namespace DragSupport

DragListWidget::DragListWidget(MimeDataFunction mimeData, QWidget *parent)
    : QListWidget(parent)
    , m_mimeData(std::move(mimeData))
{
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
}

QMimeData *DragListWidget::mimeData(const QList<QListWidgetItem *> &items) const
{
    return items.isEmpty() ? nullptr : m_mimeData(items);
}

QStringList DragListWidget::mimeTypes() const
{
    return {ObjectMimeType, u"text/uri-list"_s};
}
