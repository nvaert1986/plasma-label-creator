/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labelclipboard.h"

#include "labelformat.h"
#include "objectfactory.h"

#include <QBuffer>
#include <QHash>
#include <QImage>
#include <QMimeData>

#include <algorithm>

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

QMimeData *mimeData(const QList<LabelObject> &objects, const LabelDocument &document)
{
    LabelDocument holder = document;
    holder.objects = objects;
    auto *data = new QMimeData;
    data->setData(ObjectsMimeType, LabelWriter::toByteArray(holder));
    QStringList texts;
    for (const LabelObject &object : objects) {
        if (const QString text = textOf(object); !text.isEmpty()) {
            texts << text;
        }
    }
    if (!texts.isEmpty()) {
        data->setText(texts.join(u'\n'));
    }
    if (objects.size() == 1) {
        if (const auto *image = std::get_if<ImageObject>(&objects.constFirst())) {
            data->setImageData(QImage::fromData(image->imageData));
        }
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

    // No object hidden exactly on top of another: all move on together, so
    // they keep their places to each other.
    const auto occupied = [&document, &objects]() {
        return std::any_of(objects.cbegin(), objects.cend(), [&document](const LabelObject &object) {
            return std::any_of(document.objects.cbegin(), document.objects.cend(), [&object](const LabelObject &other) {
                return objectBase(other).bounds == objectBase(object).bounds;
            });
        });
    };
    for (int tries = 0; tries < 100 && occupied(); ++tries) {
        for (LabelObject &object : objects) {
            objectBase(object).bounds.translate(PasteOffset, PasteOffset);
        }
    }

    // Unique names; pasted groups become new groups, not part of one on the label.
    int nextGroup = 0;
    for (const LabelObject &object : document.objects) {
        nextGroup = std::max(nextGroup, objectBase(object).groupId.value_or(-1) + 1);
    }
    QHash<int, int> groups;
    LabelDocument target = document;
    for (LabelObject &object : objects) {
        ObjectBase &base = objectBase(object);
        base.name = ObjectFactory::copyName(target, base.name);
        if (base.groupId && *base.groupId >= 0) {
            if (!groups.contains(*base.groupId)) {
                groups.insert(*base.groupId, nextGroup++);
            }
            base.groupId = groups.value(*base.groupId);
        }
        target.objects << object;
    }
    return objects;
}

} // namespace LabelClipboard
