/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labelclipboard.h"

#include "labelformat.h"
#include "objectfactory.h"

#include <QBuffer>
#include <QImage>
#include <QMimeData>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

const QString ObjectsMimeType = u"application/x-plasma-label-creator-objects"_s;
constexpr qreal PasteOffset = 144; // twips, 1/10 inch, as Duplicate

// The text of an object, for pasting into other programs.
QString textOf(const LabelObject &object)
{
    if (const auto *text = std::get_if<TextObject>(&object)) {
        return text->plainText();
    }
    if (const auto *address = std::get_if<AddressObject>(&object)) {
        return address->plainText();
    }
    if (const auto *curved = std::get_if<CircularTextObject>(&object)) {
        return curved->text;
    }
    if (const auto *barcode = std::get_if<BarcodeObject>(&object)) {
        return barcode->text;
    }
    return {};
}

QByteArray toPng(const QImage &image)
{
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return png;
}

} // namespace

namespace LabelClipboard
{

QMimeData *mimeData(const LabelObject &object, const LabelDocument &document)
{
    LabelDocument holder = document;
    holder.objects = {object};
    auto *data = new QMimeData;
    data->setData(ObjectsMimeType, LabelWriter::toByteArray(holder));
    const QString text = textOf(object);
    if (!text.isEmpty()) {
        data->setText(text);
    }
    if (const auto *image = std::get_if<ImageObject>(&object)) {
        data->setImageData(QImage::fromData(image->imageData));
    }
    return data;
}

bool canPaste(const QMimeData *data)
{
    return data && (data->hasFormat(ObjectsMimeType) || data->hasImage() || !data->text().trimmed().isEmpty());
}

QList<LabelObject> objectsToPaste(const QMimeData *data, const LabelDocument &document)
{
    QList<LabelObject> objects;
    if (!data) {
        return objects;
    }
    if (data->hasFormat(ObjectsMimeType)) {
        QByteArray xml = data->data(ObjectsMimeType);
        QBuffer buffer(&xml);
        buffer.open(QIODevice::ReadOnly);
        LabelReader reader;
        if (const auto copied = reader.read(&buffer)) {
            objects = copied->objects;
        }
    } else if (data->hasImage()) {
        const QImage image = qvariant_cast<QImage>(data->imageData());
        if (!image.isNull()) {
            objects << ObjectFactory::create(ObjectFactory::Kind::Image, document, toPng(image));
        }
    } else if (!data->text().trimmed().isEmpty()) {
        auto text = std::get<TextObject>(ObjectFactory::create(ObjectFactory::Kind::Text, document));
        text.spans.first().text = data->text().trimmed();
        objects << text;
    }

    // Unique names, and no object hidden exactly on top of another.
    LabelDocument target = document;
    for (LabelObject &object : objects) {
        ObjectBase &base = objectBase(object);
        base.name = ObjectFactory::copyName(target, base.name);
        const auto occupied = [&target](const QRectF &bounds) {
            return std::any_of(target.objects.cbegin(), target.objects.cend(), [&bounds](const LabelObject &other) {
                return objectBase(other).bounds == bounds;
            });
        };
        while (occupied(base.bounds)) {
            base.bounds.translate(PasteOffset, PasteOffset);
        }
        target.objects << object;
    }
    return objects;
}

} // namespace LabelClipboard
