/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "labelkinds.h"

#include "tapelayout.h"
#include "units.h"

#include <QXmlStreamReader>

#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace LabelKinds
{

namespace
{

constexpr qreal SeparatorWidth = 10; // twips, half a point

qreal twips(qreal mm)
{
    return Units::mmToTwips(mm);
}

TextObject textObject(const QString &name, const QString &text, const LabelFont &font, Rotation rotation = Rotation::Rotation0)
{
    TextObject object;
    object.name = name;
    object.rotation = rotation;
    object.horizontalAlignment = HorizontalAlignment::Center;
    object.verticalAlignment = VerticalAlignment::Middle;
    object.fitMode = TextFitMode::ShrinkToFit;
    object.spans = {{text, font, Qt::black}};
    return object;
}

ShapeObject lineObject(const QString &name, qreal x, qreal height)
{
    ShapeObject line;
    line.name = name;
    line.shapeType = ShapeType::VerticalLine;
    line.lineWidth = SeparatorWidth;
    line.bounds = QRectF(x - SeparatorWidth, 0, 2 * SeparatorWidth, height);
    return line;
}

ShapeObject boxObject(const QString &name, const QRectF &bounds)
{
    ShapeObject box;
    box.name = name;
    box.shapeType = ShapeType::Rectangle;
    box.lineWidth = SeparatorWidth;
    box.bounds = bounds;
    return box;
}

// A blank of the label type in use: no objects, and on tape a fixed length
// and no cells.
LabelDocument blank(LabelDocument base, qreal lengthTwips)
{
    base.objects.clear();
    if (base.tape) {
        base.tape->root = TapeCell();
        base.tape->root.subcellsOrientation = base.orientation == PaperOrientation::Landscape ? Qt::Horizontal : Qt::Vertical;
        base.tape->lengthMode = CellLengthMode::Fixed;
        base.tape->length = lengthTwips;
    }
    return base;
}

// Appends a fixed tape cell, holding @p object if given.
void addCell(LabelDocument &document, qreal length, std::optional<LabelObject> object, qreal border = 0, CellLengthMode mode = CellLengthMode::Fixed)
{
    TapeCell cell;
    cell.length = length;
    cell.lengthMode = mode;
    cell.borderWidth = border;
    if (object) {
        document.objects << *object;
        cell.object = int(document.objects.size() - 1);
        cell.objectMargin = QMarginsF(36, 0, 36, 0);
    }
    document.tape->root.subcells << cell;
}

qreal lineHeight(const LabelFont &font)
{
    return Units::pointsToTwips(font.size) * 1.2;
}

} // namespace

LabelDocument cableWrap(const LabelDocument &base, const CableWrap &kind)
{
    const qreal circumference = std::max(twips(10), twips(M_PI * kind.diameterMm));
    const qreal pitch = lineHeight(kind.font) * 1.4;
    if (base.tape) {
        // Around the cable is along the tape; the text turned a quarter
        // reads along the cable, once per line's height around it.
        LabelDocument document = blank(base, circumference);
        const int copies = std::max(1, int(circumference / pitch));
        for (int i = 0; i < copies; ++i) {
            addCell(document, 0, textObject(u"Wrap%1"_s.arg(i + 1), kind.text, kind.font, Rotation::Rotation90), 0, CellLengthMode::Star);
        }
        return document;
    }
    // A die-cut label wraps with its height: the text in rows over it.
    LabelDocument document = blank(base, 0);
    const QSizeF canvas = document.canvasSize();
    const int rows = std::max(1, int(canvas.height() / pitch));
    for (int i = 0; i < rows; ++i) {
        TextObject text = textObject(u"Wrap%1"_s.arg(i + 1), kind.text, kind.font);
        text.bounds = QRectF(0, i * canvas.height() / rows, canvas.width(), canvas.height() / rows);
        document.objects << text;
    }
    return document;
}

LabelDocument cableFlag(const LabelDocument &base, const CableFlag &kind)
{
    const qreal around = twips(M_PI * kind.diameterMm);
    if (base.tape) {
        // Folded around the cable with the adhesive inside, the second half
        // reads the same way from the other side.
        const qreal flag = twips(kind.flagLengthMm);
        LabelDocument document = blank(base, 2 * flag + around);
        addCell(document, flag, textObject(u"Front"_s, kind.text, kind.font));
        addCell(document, around, std::nullopt);
        addCell(document, flag, textObject(u"Back"_s, kind.text, kind.font));
        return document;
    }
    LabelDocument document = blank(base, 0);
    const QSizeF canvas = document.canvasSize();
    const qreal flag = std::max<qreal>(0, (canvas.width() - around) / 2);
    TextObject front = textObject(u"Front"_s, kind.text, kind.font);
    front.bounds = QRectF(0, 0, flag, canvas.height());
    TextObject back = textObject(u"Back"_s, kind.text, kind.font);
    back.bounds = QRectF(canvas.width() - flag, 0, flag, canvas.height());
    document.objects << front << back;
    return document;
}

QString portText(const Ports &kind, int index)
{
    if (index < kind.texts.size() && !kind.texts.at(index).isEmpty()) {
        return kind.texts.at(index);
    }
    if (!kind.numberEmptyCells) {
        return {};
    }
    const int position = kind.reversed ? kind.count - 1 - index : index;
    const int number = kind.first + position * kind.step;
    QString text = kind.prefix + QString::number(number).rightJustified(kind.digits, u'0');
    if (kind.arrow == Arrow::Up) {
        text = u"▲ "_s + text;
    } else if (kind.arrow == Arrow::Down) {
        text = u"▼ "_s + text;
    }
    return text;
}

LabelDocument ports(const LabelDocument &base, const Ports &kind)
{
    const int count = std::max(1, kind.count);
    const qreal pitch = twips(kind.pitchMm);
    const qreal lead = twips(kind.leadMm);
    const qreal tail = twips(kind.tailMm);
    const qreal border = kind.separators ? SeparatorWidth : 0;
    if (base.tape) {
        LabelDocument document = blank(base, lead + count * pitch + tail);
        if (lead > 0) {
            addCell(document, lead, std::nullopt);
        }
        for (int i = 0; i < count; ++i) {
            addCell(document, pitch, textObject(u"Port%1"_s.arg(i + 1), portText(kind, i), kind.font), border);
        }
        if (tail > 0) {
            addCell(document, tail, std::nullopt);
        }
        return document;
    }
    // On a die-cut label the ports run along it, as far as it is long.
    LabelDocument document = blank(base, 0);
    const QSizeF canvas = document.canvasSize();
    for (int i = 0; i < count; ++i) {
        const qreal left = lead + i * pitch;
        if (left >= canvas.width()) {
            break;
        }
        TextObject text = textObject(u"Port%1"_s.arg(i + 1), portText(kind, i), kind.font);
        text.bounds = QRectF(left, 0, std::min(pitch, canvas.width() - left), canvas.height());
        document.objects << text;
        if (kind.separators) {
            if (i == 0) {
                document.objects << lineObject(u"Line0"_s, left, canvas.height());
            }
            if (left + pitch <= canvas.width()) {
                document.objects << lineObject(u"Line%1"_s.arg(i + 1), left + pitch, canvas.height());
            }
        }
    }
    return document;
}

LabelDocument rackMarkers(const LabelDocument &base, const RackMarkers &kind)
{
    const int first = std::min(kind.firstUnit, kind.lastUnit);
    const int last = std::max(kind.firstUnit, kind.lastUnit);
    const int units = last - first + 1;
    const qreal unit = twips(kind.unitMm);
    const auto text = [&](int i) {
        return kind.prefix + QString::number(kind.descending ? last - i : first + i);
    };
    if (base.tape) {
        LabelDocument document = blank(base, units * unit);
        for (int i = 0; i < units; ++i) {
            addCell(document, unit, textObject(u"Unit%1"_s.arg(i + 1), text(i), kind.font), kind.boxes ? SeparatorWidth : 0);
        }
        return document;
    }
    LabelDocument document = blank(base, 0);
    const QSizeF canvas = document.canvasSize();
    for (int i = 0; i < units && i * unit < canvas.width(); ++i) {
        const QRectF cell(i * unit, 0, std::min(unit, canvas.width() - i * unit), canvas.height());
        TextObject label = textObject(u"Unit%1"_s.arg(i + 1), text(i), kind.font);
        label.bounds = cell;
        document.objects << label;
        if (kind.boxes) {
            document.objects << boxObject(u"Box%1"_s.arg(i + 1), cell);
        }
    }
    return document;
}

namespace
{

// An Apple property list's value: dict, array, string, number or boolean.
QVariant readPlistValue(QXmlStreamReader &xml)
{
    const QStringView name = xml.name();
    if (name == "dict"_L1) {
        QVariantMap map;
        QString key;
        while (xml.readNextStartElement()) {
            if (xml.name() == "key"_L1) {
                key = xml.readElementText();
            } else {
                map.insert(key, readPlistValue(xml));
            }
        }
        return map;
    }
    if (name == "array"_L1) {
        QVariantList list;
        while (xml.readNextStartElement()) {
            list << readPlistValue(xml);
        }
        return list;
    }
    if (name == "true"_L1 || name == "false"_L1) {
        const bool value = name == "true"_L1;
        xml.skipCurrentElement();
        return value;
    }
    if (name == "integer"_L1) {
        return xml.readElementText().toLongLong();
    }
    if (name == "real"_L1) {
        return xml.readElementText().toDouble();
    }
    return xml.readElementText();
}

} // namespace

std::optional<RhinoTemplate> readRhinoTemplate(const QByteArray &data, QString *errorString)
{
    const auto fail = [errorString](const QString &message) -> std::optional<RhinoTemplate> {
        if (errorString) {
            *errorString = message;
        }
        return std::nullopt;
    };
    QXmlStreamReader xml(data);
    if (!xml.readNextStartElement() || xml.name() != "plist"_L1 || !xml.readNextStartElement()) {
        return fail(u"Not a DYMO Rhino template"_s);
    }
    const QVariantMap root = readPlistValue(xml).toMap();
    const QVariantMap label = root.value(u"LabelTemplate"_s).toMap();
    if (label.isEmpty()) {
        return fail(u"Not a DYMO Rhino template: it has no LabelTemplate"_s);
    }

    RhinoTemplate result;
    result.kind = label.value(u"TemplateClass"_s).toString().section(u'.', -1);
    // "tw12": 12 mm tape.
    result.tapeMillimetres = label.value(u"TapeWidth"_s).toString().mid(2).toInt();
    const QVariantMap font = label.value(u"Font"_s).toMap();
    result.font.family = font.value(u"FontFamily"_s, u"Arial"_s).toString();
    result.font.size = font.value(u"FontSize"_s, 12).toDouble();
    result.font.bold = font.value(u"Bold"_s).toBool();
    result.font.italic = font.value(u"Italic"_s).toBool();
    result.font.underline = font.value(u"Underline"_s).toBool();
    result.font.strikeout = font.value(u"Strikeout"_s).toBool();

    // The labels it carries: rows of cells of text items.
    for (const QVariant &row : root.value(u"Labeldata"_s).toMap().value(u"Rows"_s).toList()) {
        QStringList texts;
        for (const QVariant &cell : row.toList()) {
            QStringList items;
            for (const QVariant &item : cell.toMap().value(u"Items"_s).toList()) {
                items << item.toMap().value(u"Text"_s).toString();
            }
            texts << items.join(u'\n');
        }
        result.labels << texts;
    }

    if (result.kind == u"PatchPanelLabel"_s || result.kind == u"TerminalBlockLabel"_s) {
        Ports ports;
        ports.count = label.value(u"CellCount"_s, 1).toInt();
        // Lengths are in twips, as DLS has them.
        ports.pitchMm = Units::twipsToMm(label.value(u"CellLength"_s).toDouble());
        ports.separators = label.value(u"HasCellSeparator"_s).toBool();
        ports.reversed = label.value(u"IsReversed"_s).toBool();
        ports.font = result.font;
        if (!result.labels.isEmpty()) {
            ports.texts = result.labels.constFirst();
        }
        result.ports = ports;
    } else if (result.kind == u"FixedLabel"_s) {
        result.fixedLengthMm = Units::twipsToMm(label.value(u"FixedLabelLength"_s).toDouble());
    } else {
        return fail(u"DYMO Rhino templates of the kind \"%1\" are not supported"_s.arg(result.kind));
    }
    return result;
}

LabelDocument fromRhinoTemplate(const RhinoTemplate &rhino, int labelIndex)
{
    const int tape = TapeLayout::tape(rhino.tapeMillimetres) ? rhino.tapeMillimetres : 12;
    const QStringList texts = labelIndex >= 0 && labelIndex < rhino.labels.size() ? rhino.labels.at(labelIndex) : QStringList();
    if (rhino.ports) {
        Ports kind = *rhino.ports;
        kind.texts = texts;
        // A template with texts leaves its empty cells empty.
        kind.numberEmptyCells = texts.isEmpty();
        return ports(TapeLayout::newLabel(tape), kind);
    }
    // A fixed label: its text across its length.
    LabelDocument document = blank(TapeLayout::newLabel(tape), twips(rhino.fixedLengthMm));
    addCell(document, 0, textObject(u"Text"_s, texts.isEmpty() ? QString() : texts.join(u' '), rhino.font), 0, CellLengthMode::Star);
    return document;
}

} // namespace LabelKinds

} // namespace LabelCreator
