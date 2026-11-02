/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "objectfactory.h"

#include "dlslabelids.h"
#include "tapelayout.h"

#include "units.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace ObjectFactory
{

namespace
{

constexpr qreal Inset = 144; // twips, 1/10 inch from the label's edges
constexpr qreal DefaultLineWidth = 15; // twips, as DLS draws lines

QRectF usableArea(const LabelDocument &document)
{
    const QSizeF canvas = document.canvasSize();
    return QRectF(QPointF(Inset, Inset), canvas - QSizeF(2 * Inset, 2 * Inset));
}

// A box of the given proportions of the usable area, centred on it.
QRectF centredBox(const LabelDocument &document, qreal widthFraction, qreal heightFraction)
{
    const QRectF area = usableArea(document);
    QRectF box(QPointF(0, 0), QSizeF(area.width() * widthFraction, area.height() * heightFraction));
    box.moveCenter(area.center());
    return box;
}

LabelFont &newTextFont()
{
    static LabelFont font{u"Arial"_s, 12};
    return font;
}

TextSpan span(const QString &text)
{
    return {text, newTextFont(), Qt::black};
}

} // namespace

void setTextFont(const LabelFont &font)
{
    newTextFont() = font;
}

LabelFont textFont()
{
    return newTextFont();
}

QString uniqueName(const LabelDocument &document, const QString &base)
{
    QSet<QString> used;
    for (const LabelObject &object : document.objects) {
        used.insert(objectBase(object).name);
    }
    if (!used.contains(base)) {
        return base;
    }
    for (int i = 1;; ++i) {
        const QString name = u"%1_%2"_s.arg(base).arg(i);
        if (!used.contains(name)) {
            return name;
        }
    }
}

QString copyName(const LabelDocument &document, const QString &name)
{
    // Only a number the numbering added comes off; other underscores stay.
    static const QRegularExpression numbered(u"^(.+)_\\d+$"_s);
    const QRegularExpressionMatch match = numbered.match(name);
    return uniqueName(document, match.hasMatch() ? match.captured(1) : name);
}

void centreOn(LabelObject &object, const LabelDocument &document, const QPointF &centre)
{
    QRectF &bounds = objectBase(object).bounds;
    bounds.moveCenter(centre);
    const QSizeF canvas = document.canvasSize();
    bounds.moveLeft(std::clamp(bounds.left(), 0.0, std::max(0.0, canvas.width() - bounds.width())));
    bounds.moveTop(std::clamp(bounds.top(), 0.0, std::max(0.0, canvas.height() - bounds.height())));
}

void alignOnLabel(LabelObject &object, const LabelDocument &document, Qt::Alignment alignment)
{
    QRectF &bounds = objectBase(object).bounds;
    const QRectF area = usableArea(document);
    if (alignment & Qt::AlignLeft) {
        bounds.moveLeft(area.left());
    } else if (alignment & Qt::AlignHCenter) {
        bounds.moveLeft(area.center().x() - bounds.width() / 2);
    } else if (alignment & Qt::AlignRight) {
        bounds.moveRight(area.right());
    }
    if (alignment & Qt::AlignTop) {
        bounds.moveTop(area.top());
    } else if (alignment & Qt::AlignVCenter) {
        bounds.moveTop(area.center().y() - bounds.height() / 2);
    } else if (alignment & Qt::AlignBottom) {
        bounds.moveBottom(area.bottom());
    }
}

LabelObject create(Kind kind, const LabelDocument &document, const QByteArray &imageData)
{
    switch (kind) {
    case Kind::Text: {
        TextObject text;
        text.name = uniqueName(document, u"TEXT"_s);
        text.horizontalAlignment = HorizontalAlignment::Center;
        text.spans = {span(u"Text"_s)};
        text.bounds = centredBox(document, 1.0, 0.4);
        return text;
    }
    case Kind::Address: {
        AddressObject address;
        address.name = uniqueName(document, u"ADDRESS"_s);
        address.variable = true;
        address.spans = {span(u"Name\nStreet 1\n1234 AB City"_s)};
        address.lineFonts = {newTextFont(), newTextFont(), newTextFont()};
        address.bounds = centredBox(document, 1.0, 1.0);
        return address;
    }
    case Kind::Barcode: {
        BarcodeObject barcode;
        barcode.name = uniqueName(document, u"BARCODE"_s);
        barcode.text = u"123456"_s;
        barcode.textFont = LabelFont{u"Arial"_s, 8};
        barcode.checkSumFont = LabelFont{u"Arial"_s, 8};
        barcode.bounds = centredBox(document, 1.0, 0.6);
        return barcode;
    }
    case Kind::Image: {
        ImageObject image;
        image.name = uniqueName(document, u"GRAPHIC"_s);
        // An SVG drawing is kept as a drawing.
        (imageData.left(1024).contains("<svg") ? image.svgData : image.imageData) = imageData;
        const QRectF area = usableArea(document);
        const qreal side = std::min(area.width(), area.height()) * 0.6;
        image.bounds = QRectF(QPointF(0, 0), QSizeF(side, side));
        image.bounds.moveCenter(area.center());
        return image;
    }
    case Kind::Counter: {
        CounterObject counter;
        counter.name = uniqueName(document, u"COUNTER"_s);
        counter.font = newTextFont();
        counter.bounds = centredBox(document, 0.6, 0.4);
        return counter;
    }
    case Kind::DateTime: {
        DateTimeObject date;
        date.name = uniqueName(document, u"DATETIME"_s);
        date.font = newTextFont();
        date.bounds = centredBox(document, 1.0, 0.4);
        return date;
    }
    case Kind::CurvedText: {
        CircularTextObject curved;
        curved.name = uniqueName(document, u"CIRCULARTEXT"_s);
        curved.text = u"Curved text"_s;
        curved.font = newTextFont();
        curved.bounds = centredBox(document, 0.8, 0.8);
        return curved;
    }
    case Kind::HorizontalLine:
    case Kind::VerticalLine:
    case Kind::Rectangle:
    case Kind::Ellipse:
    case Kind::Polygon:
    case Kind::Star:
    case Kind::Arrow:
        break;
    }

    ShapeObject shape;
    shape.name = uniqueName(document, u"SHAPE"_s);
    shape.lineWidth = DefaultLineWidth;
    const QRectF area = usableArea(document);
    switch (kind) {
    case Kind::HorizontalLine:
        shape.shapeType = ShapeType::HorizontalLine;
        shape.bounds = QRectF(area.left(), area.center().y() - 3 * DefaultLineWidth / 2, area.width(), 3 * DefaultLineWidth);
        break;
    case Kind::VerticalLine:
        shape.shapeType = ShapeType::VerticalLine;
        shape.bounds = QRectF(area.center().x() - 3 * DefaultLineWidth / 2, area.top(), 3 * DefaultLineWidth, area.height());
        break;
    case Kind::Ellipse:
        shape.shapeType = ShapeType::Ellipse;
        shape.bounds = centredBox(document, 0.6, 0.6);
        break;
    case Kind::Polygon:
        shape.shapeType = ShapeType::Polygon;
        shape.corners = 6;
        shape.bounds = centredBox(document, 0.6, 0.6);
        break;
    case Kind::Star:
        shape.shapeType = ShapeType::Star;
        shape.corners = 5;
        shape.bounds = centredBox(document, 0.6, 0.6);
        break;
    case Kind::Arrow:
        shape.shapeType = ShapeType::Arrow;
        shape.bounds = centredBox(document, 0.6, 0.3);
        shape.fillColor = Qt::black;
        break;
    default:
        shape.shapeType = ShapeType::Rectangle;
        shape.bounds = centredBox(document, 0.8, 0.6);
        break;
    }
    return shape;
}

LabelDocument newLabel(const PaperSize &paper)
{
    if (paper.tape > 0) {
        LabelDocument document = TapeLayout::newLabel(paper.tape);
        document.paperName = paper.name;
        return document;
    }
    return newLabel(paper.name, paper.keyword, paper.size);
}

LabelDocument newLabel(const QString &paperName, const QString &id, const QSizeF &paperSize)
{
    LabelDocument document;
    document.paperName = paperName;
    // DYMO's own id where the label has one, else the driver's page keyword.
    document.id = DlsLabelIds::preferred(id, paperName);
    const QSizeF twips = paperSize * Units::TwipsPerPoint;
    document.paperOutline.addRoundedRect(QRectF(QPointF(0, 0), twips), 180, 180);
    // DLS lays labels out with their long side horizontal.
    document.orientation = twips.height() > twips.width() ? PaperOrientation::Landscape : PaperOrientation::Portrait;
    return document;
}

bool canInvert(const LabelObject &object)
{
    return std::holds_alternative<TextObject>(object) || std::holds_alternative<AddressObject>(object) || std::holds_alternative<CounterObject>(object)
        || std::holds_alternative<DateTimeObject>(object) || std::holds_alternative<CircularTextObject>(object);
}

bool isInverted(const LabelObject &object)
{
    if (!canInvert(object)) {
        return false;
    }
    const ObjectBase &base = objectBase(object);
    if (base.backColor != QColor(Qt::black)) {
        return false;
    }
    // Text and addresses colour their text per run; the others use the
    // object's colour.
    if (const auto *text = std::get_if<TextObject>(&object)) {
        return std::all_of(text->spans.cbegin(), text->spans.cend(), [](const TextSpan &span) {
            return span.color == QColor(Qt::white);
        });
    }
    if (const auto *address = std::get_if<AddressObject>(&object)) {
        return std::all_of(address->spans.cbegin(), address->spans.cend(), [](const TextSpan &span) {
            return span.color == QColor(Qt::white);
        });
    }
    return base.foreColor == QColor(Qt::white);
}

void setInverted(LabelObject &object, bool inverted)
{
    if (!canInvert(object)) {
        return;
    }
    const QColor text = inverted ? QColor(Qt::white) : QColor(Qt::black);
    ObjectBase &base = objectBase(object);
    base.backColor = inverted ? QColor(Qt::black) : QColor(255, 255, 255, 0);
    base.foreColor = text;
    std::visit(
        [&text](auto &o) {
            using T = std::decay_t<decltype(o)>;
            if constexpr (std::is_same_v<T, TextObject> || std::is_same_v<T, AddressObject>) {
                for (TextSpan &span : o.spans) {
                    span.color = text;
                }
            }
        },
        object);
}

} // namespace ObjectFactory
