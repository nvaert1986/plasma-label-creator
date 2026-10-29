/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "labelformat.h"

#include "tapelayout.h"
#include "units.h"

#include <QBuffer>
#include <QFile>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace
{

template<typename Enum>
struct EnumName {
    Enum value;
    QLatin1StringView name;
};

// The first entry for a value is the one written; later ones are accepted
// spellings seen in the wild.
constexpr EnumName<PaperOrientation> paperOrientationNames[] = {
    {PaperOrientation::Portrait, "Portrait"_L1},
    {PaperOrientation::Landscape, "Landscape"_L1},
};

constexpr EnumName<Rotation> rotationNames[] = {
    {Rotation::Rotation0, "Rotation0"_L1},
    {Rotation::Rotation90, "Rotation90"_L1},
    {Rotation::Rotation180, "Rotation180"_L1},
    {Rotation::Rotation270, "Rotation270"_L1},
};

constexpr EnumName<HorizontalAlignment> horizontalAlignmentNames[] = {
    {HorizontalAlignment::Left, "Left"_L1},
    {HorizontalAlignment::Center, "Center"_L1},
    {HorizontalAlignment::Right, "Right"_L1},
};

constexpr EnumName<VerticalAlignment> verticalAlignmentNames[] = {
    {VerticalAlignment::Top, "Top"_L1},
    {VerticalAlignment::Middle, "Middle"_L1},
    {VerticalAlignment::Middle, "Center"_L1},
    {VerticalAlignment::Bottom, "Bottom"_L1},
};

constexpr EnumName<TextFitMode> textFitModeNames[] = {
    {TextFitMode::None, "None"_L1},
    {TextFitMode::ShrinkToFit, "ShrinkToFit"_L1},
    {TextFitMode::AlwaysFit, "AlwaysFit"_L1},
};

constexpr EnumName<BarcodeSize> barcodeSizeNames[] = {
    {BarcodeSize::Small, "Small"_L1},
    {BarcodeSize::Medium, "Medium"_L1},
    {BarcodeSize::Large, "Large"_L1},
    {BarcodeSize::SmallMedium, "SmallMedium"_L1},
};

constexpr EnumName<BarcodeTextPosition> barcodeTextPositionNames[] = {
    {BarcodeTextPosition::None, "None"_L1},
    {BarcodeTextPosition::Top, "Top"_L1},
    {BarcodeTextPosition::Bottom, "Bottom"_L1},
};

constexpr EnumName<ImageScaleMode> imageScaleModeNames[] = {
    {ImageScaleMode::None, "None"_L1},
    {ImageScaleMode::Uniform, "Uniform"_L1},
    {ImageScaleMode::Fill, "Fill"_L1},
};

constexpr EnumName<ShapeType> shapeTypeNames[] = {
    {ShapeType::HorizontalLine, "HorizontalLine"_L1},
    {ShapeType::VerticalLine, "VerticalLine"_L1},
    {ShapeType::Rectangle, "Rectangle"_L1},
    {ShapeType::Ellipse, "Ellipse"_L1},
};

constexpr EnumName<DateFormat> dateFormatNames[] = {
    {DateFormat::None, "None"_L1},
    {DateFormat::LongSystemDate, "LongSystemDate"_L1},
    {DateFormat::ShortSystemDate, "ShortSystemDate"_L1},
    {DateFormat::WeekdayLongMonthDayLongYear, "WeekdayLongMonthDayLongYear"_L1},
    {DateFormat::WeekdayDayLongMonthLongYear, "WeekdayDayLongMonthLongYear"_L1},
    {DateFormat::AbbrWeekdayAbbrMonthDayLongYear, "AbbrWeekdayAbbrMonthDayLongYear"_L1},
    {DateFormat::AbbrWeekdayDayAbbrMonthLongYear, "AbbrWeekdayDayAbbrMonthLongYear"_L1},
    {DateFormat::LongMonthDayLongYear, "LongMonthDayLongYear"_L1},
    {DateFormat::DayLongMonthLongYear, "DayLongMonthLongYear"_L1},
    {DateFormat::MonthDayLongYear, "MonthDayLongYear"_L1},
    {DateFormat::DayMonthLongYear, "DayMonthLongYear"_L1},
    {DateFormat::MonthDayYear, "MonthDayYear"_L1},
    {DateFormat::DayMonthYear, "DayMonthYear"_L1},
    {DateFormat::AbbrMonthDayLongYear, "AbbrMonthDayLongYear"_L1},
    {DateFormat::AbbrMonthDayYear, "AbbrMonthDayYear"_L1},
    {DateFormat::DayAbbrMonthLongYear, "DayAbbrMonthLongYear"_L1},
    {DateFormat::DayAbbrMonthYear, "DayAbbrMonthYear"_L1},
};

constexpr EnumName<CircularTextMode> circularTextModeNames[] = {
    {CircularTextMode::ArcTop, "ArcTextTop"_L1},
    {CircularTextMode::ArcBottom, "ArcTextBottom"_L1},
    {CircularTextMode::Circle, "CircularText"_L1},
};

constexpr EnumName<CircleAlignment> circleAlignmentNames[] = {
    {CircleAlignment::CenterAtTop, "CenterAtTop"_L1},
    {CircleAlignment::CenterAtBottom, "CenterAtBottom"_L1},
    {CircleAlignment::StartAngle, "StartAngle"_L1},
};

constexpr EnumName<CellLengthMode> cellLengthModeNames[] = {
    {CellLengthMode::Auto, "Auto"_L1},
    {CellLengthMode::Fixed, "Fixed"_L1},
    {CellLengthMode::Star, "Star"_L1},
};

constexpr EnumName<Qt::Orientation> orientationNames[] = {
    {Qt::Horizontal, "Horizontal"_L1},
    {Qt::Vertical, "Vertical"_L1},
};

constexpr EnumName<LineAlignment> lineAlignmentNames[] = {
    {LineAlignment::Center, "Center"_L1},
    {LineAlignment::LeftOrTop, "LeftOrTop"_L1},
    {LineAlignment::RightOrBottom, "RightOrBottom"_L1},
};

template<typename Enum, std::size_t N>
QLatin1StringView enumName(const EnumName<Enum> (&names)[N], Enum value)
{
    for (const auto &entry : names) {
        if (entry.value == value) {
            return entry.name;
        }
    }
    Q_UNREACHABLE();
}

QString formatReal(qreal value)
{
    return QString::number(value, 'g', 15);
}

QString formatBool(bool value)
{
    return value ? u"True"_s : u"False"_s;
}

// Copies the element the reader is on, including its children, into a
// string. Indentation-only text is dropped so the writer can re-indent.
QString captureElement(QXmlStreamReader &xml)
{
    QString captured;
    QXmlStreamWriter writer(&captured);
    int depth = 0;
    while (!xml.hasError()) {
        if (xml.isStartElement()) {
            ++depth;
        } else if (xml.isEndElement()) {
            --depth;
        }
        if (!(xml.isWhitespace() && xml.text().contains(u'\n'))) {
            writer.writeCurrentToken(xml);
        }
        if (depth == 0) {
            break;
        }
        xml.readNext();
    }
    return captured;
}

void replayElement(QXmlStreamWriter &writer, const QString &captured)
{
    QXmlStreamReader xml(captured);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() || xml.isEndElement() || xml.isCharacters() || xml.isComment()) {
            writer.writeCurrentToken(xml);
        }
    }
}

QPainterPath parseDrawCommands(const QString &captured)
{
    QPainterPath outline;
    QXmlStreamReader xml(captured);
    const auto attribute = [&xml](QLatin1StringView name) {
        return xml.attributes().value(name).toDouble();
    };

    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) {
            continue;
        }
        const QStringView name = xml.name();
        if (name == "RoundRectangle"_L1) {
            const QRectF rect(attribute("X"_L1), attribute("Y"_L1), attribute("Width"_L1), attribute("Height"_L1));
            outline.addRoundedRect(rect, attribute("Rx"_L1), attribute("Ry"_L1));
        } else if (name == "Path"_L1) {
            // Each Path is a separate figure; the holes of a two-up label are
            // drawn as figures inside the outer one.
            outline.closeSubpath();
        } else if (name == "FillMode"_L1) {
            outline.setFillRule(xml.readElementText() == "EvenOdd"_L1 ? Qt::OddEvenFill : Qt::WindingFill);
        } else if (name == "Line"_L1) {
            outline.moveTo(attribute("X1"_L1), attribute("Y1"_L1));
            outline.lineTo(attribute("X2"_L1), attribute("Y2"_L1));
        } else if (name == "LineTo"_L1) {
            outline.lineTo(attribute("X"_L1), attribute("Y"_L1));
        } else if (name == "Arc"_L1) {
            const qreal radius = attribute("Radius"_L1);
            const QRectF circle(attribute("CenterX"_L1) - radius, attribute("CenterY"_L1) - radius, 2 * radius, 2 * radius);
            // DLS angles run clockwise (GDI+); Qt's run counter-clockwise.
            const qreal start = attribute("StartAngle"_L1);
            outline.arcTo(circle, -start, -(attribute("EndAngle"_L1) - start));
        }
    }
    outline.closeSubpath();
    return outline;
}

class Parser
{
public:
    Parser(QXmlStreamReader &xml, QStringList &warnings)
        : m_xml(xml)
        , m_warnings(warnings)
    {
    }

    std::optional<LabelDocument> parseDocument(QString &errorString);

private:
    void parseObjectInfo(LabelDocument &document);
    bool parseCommon(ObjectBase &object);
    bool parseText(TextObject &object);
    TextObject parseTextObject();
    AddressObject parseAddressObject();
    BarcodeObject parseBarcodeObject();
    ImageObject parseImageObject();
    ShapeObject parseShapeObject();
    bool parseLayout(TextLayout &layout);
    CounterObject parseCounterObject();
    DateTimeObject parseDateTimeObject();
    CircularTextObject parseCircularTextObject();
    QList<TextSpan> parseStyledText();
    TextSpan parseStyledElement();
    /** The object element the reader is at; nullopt if it is no object. */
    std::optional<LabelObject> parseObject();
    std::optional<LabelDocument> parseContinuousLabel(QString &errorString);
    void parseCell(LabelDocument &document, TapeCell &cell);

    QColor readColor();
    LabelFont readFont();
    QRectF readRect();
    QMarginsF readMargins();
    bool readBool();
    qreal readReal();
    int readInt();

    template<typename Enum, std::size_t N>
    Enum readEnum(const EnumName<Enum> (&names)[N], Enum fallback);

    void skipUnknown(QStringView context);

    QXmlStreamReader &m_xml;
    QStringList &m_warnings;
};

QColor Parser::readColor()
{
    const QXmlStreamAttributes attributes = m_xml.attributes();
    m_xml.skipCurrentElement();
    return QColor(attributes.value("Red"_L1).toInt(),
                  attributes.value("Green"_L1).toInt(),
                  attributes.value("Blue"_L1).toInt(),
                  attributes.hasAttribute("Alpha"_L1) ? attributes.value("Alpha"_L1).toInt() : 255);
}

LabelFont Parser::readFont()
{
    const QXmlStreamAttributes attributes = m_xml.attributes();
    m_xml.skipCurrentElement();
    const auto flag = [&attributes](QLatin1StringView name) {
        return attributes.value(name).compare("True"_L1, Qt::CaseInsensitive) == 0;
    };
    LabelFont font;
    font.family = attributes.value("Family"_L1).toString();
    font.size = attributes.value("Size"_L1).toDouble();
    font.bold = flag("Bold"_L1);
    font.italic = flag("Italic"_L1);
    font.underline = flag("Underline"_L1);
    font.strikeout = flag("Strikeout"_L1);
    return font;
}

QRectF Parser::readRect()
{
    const QXmlStreamAttributes attributes = m_xml.attributes();
    m_xml.skipCurrentElement();
    return QRectF(attributes.value("X"_L1).toDouble(),
                  attributes.value("Y"_L1).toDouble(),
                  attributes.value("Width"_L1).toDouble(),
                  attributes.value("Height"_L1).toDouble());
}

QMarginsF Parser::readMargins()
{
    const QXmlStreamAttributes attributes = m_xml.attributes();
    m_xml.skipCurrentElement();
    return QMarginsF(attributes.value("Left"_L1).toDouble(),
                     attributes.value("Top"_L1).toDouble(),
                     attributes.value("Right"_L1).toDouble(),
                     attributes.value("Bottom"_L1).toDouble());
}

bool Parser::readBool()
{
    return m_xml.readElementText().trimmed().compare("True"_L1, Qt::CaseInsensitive) == 0;
}

qreal Parser::readReal()
{
    return m_xml.readElementText().trimmed().toDouble();
}

int Parser::readInt()
{
    return m_xml.readElementText().trimmed().toInt();
}

template<typename Enum, std::size_t N>
Enum Parser::readEnum(const EnumName<Enum> (&names)[N], Enum fallback)
{
    const QString elementName = m_xml.name().toString();
    const QString text = m_xml.readElementText().trimmed();
    for (const auto &entry : names) {
        if (text.compare(entry.name, Qt::CaseInsensitive) == 0) {
            return entry.value;
        }
    }
    m_warnings << u"Unknown %1 \"%2\", using the default"_s.arg(elementName, text);
    return fallback;
}

void Parser::skipUnknown(QStringView context)
{
    m_warnings << u"Ignoring <%1> in <%2>"_s.arg(m_xml.name(), context);
    m_xml.skipCurrentElement();
}

std::optional<LabelDocument> Parser::parseDocument(QString &errorString)
{
    if (!m_xml.readNextStartElement()) {
        errorString = m_xml.hasError() ? m_xml.errorString() : u"The file is empty"_s;
        return std::nullopt;
    }
    if (m_xml.name() == "ContinuousLabel"_L1) {
        return parseContinuousLabel(errorString);
    }
    if (m_xml.name() != "DieCutLabel"_L1) {
        errorString = u"Not a DYMO label file: unexpected <%1>"_s.arg(m_xml.name());
        return std::nullopt;
    }
    const QStringView units = m_xml.attributes().value("Units"_L1);
    if (!units.isEmpty() && units != "twips"_L1) {
        errorString = u"Unsupported units \"%1\""_s.arg(units);
        return std::nullopt;
    }

    LabelDocument document;
    if (m_xml.attributes().hasAttribute("MediaType"_L1)) {
        document.mediaType = m_xml.attributes().value("MediaType"_L1).toString();
    }

    while (m_xml.readNextStartElement()) {
        const QStringView name = m_xml.name();
        if (name == "PaperOrientation"_L1) {
            document.orientation = readEnum(paperOrientationNames, PaperOrientation::Landscape);
        } else if (name == "Id"_L1) {
            document.id = m_xml.readElementText();
        } else if (name == "IsOutlined"_L1) {
            document.outlined = readBool();
        } else if (name == "PaperName"_L1) {
            document.paperName = m_xml.readElementText();
        } else if (name == "DrawCommands"_L1) {
            document.drawCommandsXml = captureElement(m_xml);
            document.paperOutline = parseDrawCommands(document.drawCommandsXml);
        } else if (name == "ObjectInfo"_L1) {
            parseObjectInfo(document);
        } else {
            skipUnknown(u"DieCutLabel");
        }
    }

    if (m_xml.hasError()) {
        errorString = m_xml.errorString();
        return std::nullopt;
    }
    if (document.paperOutline.isEmpty()) {
        errorString = u"The label has no paper outline (DrawCommands)"_s;
        return std::nullopt;
    }
    return document;
}

std::optional<LabelObject> Parser::parseObject()
{
    const QStringView name = m_xml.name();
    if (name == "TextObject"_L1) {
        return parseTextObject();
    } else if (name == "AddressObject"_L1) {
        return parseAddressObject();
    } else if (name == "BarcodeObject"_L1) {
        return parseBarcodeObject();
    } else if (name == "ImageObject"_L1) {
        return parseImageObject();
    } else if (name == "ShapeObject"_L1) {
        return parseShapeObject();
    } else if (name == "CounterObject"_L1) {
        return parseCounterObject();
    } else if (name == "DateTimeObject"_L1) {
        return parseDateTimeObject();
    } else if (name == "CircularTextObject"_L1) {
        return parseCircularTextObject();
    } else if (name.endsWith("Object"_L1)) {
        UnknownObject unknown;
        unknown.elementName = name.toString();
        unknown.xml = captureElement(m_xml);
        m_warnings << u"Keeping unsupported <%1> as is; it is not drawn"_s.arg(unknown.elementName);
        return unknown;
    }
    return std::nullopt;
}

std::optional<LabelDocument> Parser::parseContinuousLabel(QString &errorString)
{
    // DLS 8's tape labels: the paper by name, the length, and a tree of
    // cells holding the objects (which have no bounds of their own).
    const QStringView units = m_xml.attributes().value("Units"_L1);
    if (!units.isEmpty() && units != "twips"_L1) {
        errorString = u"Unsupported units \"%1\""_s.arg(units);
        return std::nullopt;
    }
    LabelDocument document;
    TapeLabel tape;
    while (m_xml.readNextStartElement()) {
        const QStringView name = m_xml.name();
        if (name == "PaperOrientation"_L1) {
            document.orientation = readEnum(paperOrientationNames, PaperOrientation::Landscape);
        } else if (name == "Id"_L1) {
            document.id = m_xml.readElementText();
        } else if (name == "PaperName"_L1) {
            document.paperName = m_xml.readElementText();
        } else if (name == "LengthMode"_L1) {
            tape.lengthMode = readEnum(cellLengthModeNames, CellLengthMode::Auto);
        } else if (name == "LabelLength"_L1) {
            tape.length = readReal();
        } else if (name == "RootCell"_L1) {
            document.tape = tape;
            parseCell(document, document.tape->root);
            tape.root = document.tape->root;
        } else {
            skipUnknown(u"ContinuousLabel");
        }
    }
    if (m_xml.hasError()) {
        errorString = m_xml.errorString();
        return std::nullopt;
    }

    // The tape's width comes from its name: Tape12mm, 12mm.
    const auto millimetres = TapeLayout::millimetresOf(document.id).value_or(TapeLayout::millimetresOf(document.paperName).value_or(0));
    const auto known = TapeLayout::tape(millimetres);
    if (!known) {
        m_warnings << u"Unknown tape \"%1\", using 12 mm"_s.arg(document.paperName);
    }
    tape.width = Units::pointsToTwips(known.value_or(*TapeLayout::tape(12)).width);
    document.tape = tape;
    document.paperOutline.addRect(QRectF(QPointF(0, 0), document.paperSize()));
    return document;
}

void Parser::parseCell(LabelDocument &document, TapeCell &cell)
{
    while (m_xml.readNextStartElement()) {
        const QStringView name = m_xml.name();
        if (name == "ObjectMargin"_L1) {
            cell.objectMargin = readMargins();
        } else if (name == "Length"_L1) {
            cell.length = readReal();
        } else if (name == "LengthMode"_L1) {
            cell.lengthMode = readEnum(cellLengthModeNames, CellLengthMode::Auto);
        } else if (name == "BorderWidth"_L1) {
            cell.borderWidth = readReal();
        } else if (name == "BorderStyle"_L1) {
            cell.borderStyle = m_xml.readElementText();
        } else if (name == "BorderColor"_L1) {
            cell.borderColor = readColor();
        } else if (name == "SubcellsOrientation"_L1) {
            cell.subcellsOrientation = readEnum(orientationNames, Qt::Horizontal);
        } else if (name == "Subcells"_L1) {
            while (m_xml.readNextStartElement()) {
                if (m_xml.name() == "Cell"_L1) {
                    TapeCell child;
                    parseCell(document, child);
                    cell.subcells << child;
                } else {
                    skipUnknown(u"Subcells");
                }
            }
        } else if (cell.object < 0) {
            if (auto object = parseObject()) {
                document.objects << std::move(*object);
                cell.object = int(document.objects.size() - 1);
            } else {
                skipUnknown(u"Cell");
            }
        } else {
            skipUnknown(u"Cell");
        }
    }
}

void Parser::parseObjectInfo(LabelDocument &document)
{
    std::optional<LabelObject> object;
    QRectF bounds;

    while (m_xml.readNextStartElement()) {
        const QStringView name = m_xml.name();
        if (name == "Bounds"_L1) {
            bounds = readRect();
        } else if (object) {
            skipUnknown(u"ObjectInfo");
        } else {
            object = parseObject();
        }
    }

    if (!object) {
        m_warnings << u"Skipping an empty <ObjectInfo>"_s;
        return;
    }
    objectBase(*object).bounds = bounds;
    document.objects.append(std::move(*object));
}

bool Parser::parseCommon(ObjectBase &object)
{
    const QStringView name = m_xml.name();
    if (name == "Name"_L1) {
        object.name = m_xml.readElementText();
    } else if (name == "ForeColor"_L1) {
        object.foreColor = readColor();
    } else if (name == "BackColor"_L1) {
        object.backColor = readColor();
    } else if (name == "LinkedObjectName"_L1) {
        object.linkedObjectName = m_xml.readElementText();
    } else if (name == "Rotation"_L1) {
        object.rotation = readEnum(rotationNames, Rotation::Rotation0);
    } else if (name == "IsMirrored"_L1) {
        object.mirrored = readBool();
    } else if (name == "IsVariable"_L1) {
        object.variable = readBool();
    } else if (name == "GroupID"_L1) {
        object.groupId = m_xml.readElementText().trimmed().toInt();
    } else if (name == "IsOutlined"_L1) {
        object.outlined = readBool();
    } else {
        return false;
    }
    return true;
}

bool Parser::parseText(TextObject &object)
{
    const QStringView name = m_xml.name();
    if (name == "HorizontalAlignment"_L1) {
        object.horizontalAlignment = readEnum(horizontalAlignmentNames, HorizontalAlignment::Left);
    } else if (name == "VerticalAlignment"_L1) {
        object.verticalAlignment = readEnum(verticalAlignmentNames, VerticalAlignment::Middle);
    } else if (name == "TextFitMode"_L1) {
        object.fitMode = readEnum(textFitModeNames, TextFitMode::ShrinkToFit);
    } else if (name == "UseFullFontHeight"_L1) {
        object.useFullFontHeight = readBool();
    } else if (name == "Verticalized"_L1) {
        object.verticalized = readBool();
    } else if (name == "StyledText"_L1) {
        object.spans = parseStyledText();
    } else {
        return false;
    }
    return true;
}

TextObject Parser::parseTextObject()
{
    TextObject object;
    while (m_xml.readNextStartElement()) {
        if (!parseCommon(object) && !parseText(object)) {
            skipUnknown(u"TextObject");
        }
    }
    return object;
}

AddressObject Parser::parseAddressObject()
{
    AddressObject object;
    while (m_xml.readNextStartElement()) {
        if (parseCommon(object) || parseText(object)) {
            continue;
        }
        const QStringView name = m_xml.name();
        if (name == "ShowBarcodeFor9DigitZipOnly"_L1) {
            object.showBarcodeFor9DigitZipOnly = readBool();
        } else if (name == "BarcodePosition"_L1) {
            object.barcodePosition = m_xml.readElementText();
        } else if (name == "LineFonts"_L1) {
            while (m_xml.readNextStartElement()) {
                if (m_xml.name() == "Font"_L1) {
                    object.lineFonts.append(readFont());
                } else {
                    skipUnknown(u"LineFonts");
                }
            }
        } else {
            skipUnknown(u"AddressObject");
        }
    }
    return object;
}

QList<TextSpan> Parser::parseStyledText()
{
    QList<TextSpan> spans;
    while (m_xml.readNextStartElement()) {
        if (m_xml.name() == "Element"_L1) {
            spans.append(parseStyledElement());
        } else {
            skipUnknown(u"StyledText");
        }
    }
    return spans;
}

TextSpan Parser::parseStyledElement()
{
    TextSpan span;
    while (m_xml.readNextStartElement()) {
        const QStringView name = m_xml.name();
        if (name == "String"_L1) {
            span.text = m_xml.readElementText();
        } else if (name == "Attributes"_L1) {
            while (m_xml.readNextStartElement()) {
                if (m_xml.name() == "Font"_L1) {
                    span.font = readFont();
                } else if (m_xml.name() == "ForeColor"_L1) {
                    span.color = readColor();
                } else {
                    skipUnknown(u"Attributes");
                }
            }
        } else {
            skipUnknown(u"Element");
        }
    }
    return span;
}

BarcodeObject Parser::parseBarcodeObject()
{
    BarcodeObject object;
    while (m_xml.readNextStartElement()) {
        if (parseCommon(object)) {
            continue;
        }
        const QStringView name = m_xml.name();
        if (name == "Text"_L1) {
            object.text = m_xml.readElementText();
        } else if (name == "Type"_L1) {
            object.type = m_xml.readElementText().trimmed();
        } else if (name == "Size"_L1) {
            object.size = readEnum(barcodeSizeNames, BarcodeSize::Medium);
        } else if (name == "TextPosition"_L1) {
            object.textPosition = readEnum(barcodeTextPositionNames, BarcodeTextPosition::Bottom);
        } else if (name == "TextFont"_L1) {
            object.textFont = readFont();
        } else if (name == "CheckSumFont"_L1) {
            object.checkSumFont = readFont();
        } else if (name == "TextEmbedding"_L1) {
            object.textEmbedding = m_xml.readElementText().trimmed();
        } else if (name == "ECLevel"_L1) {
            object.errorCorrectionLevel = m_xml.readElementText().trimmed().toInt();
        } else if (name == "HorizontalAlignment"_L1) {
            object.horizontalAlignment = readEnum(horizontalAlignmentNames, HorizontalAlignment::Center);
        } else if (name == "QuietZonesPadding"_L1) {
            object.quietZonesPadding = readMargins();
        } else {
            skipUnknown(u"BarcodeObject");
        }
    }
    return object;
}

ImageObject Parser::parseImageObject()
{
    ImageObject object;
    while (m_xml.readNextStartElement()) {
        if (parseCommon(object)) {
            continue;
        }
        const QStringView name = m_xml.name();
        if (name == "Image"_L1) {
            object.imageData = QByteArray::fromBase64(m_xml.readElementText().toLatin1());
        } else if (name == "ImageLocation"_L1) {
            object.imageLocation = m_xml.readElementText();
        } else if (name == "ScaleMode"_L1) {
            object.scaleMode = readEnum(imageScaleModeNames, ImageScaleMode::Uniform);
        } else if (name == "BorderWidth"_L1) {
            object.borderWidth = readReal();
        } else if (name == "BorderColor"_L1) {
            object.borderColor = readColor();
        } else if (name == "HorizontalAlignment"_L1) {
            object.horizontalAlignment = readEnum(horizontalAlignmentNames, HorizontalAlignment::Center);
        } else if (name == "VerticalAlignment"_L1) {
            object.verticalAlignment = readEnum(verticalAlignmentNames, VerticalAlignment::Middle);
        } else {
            skipUnknown(u"ImageObject");
        }
    }
    return object;
}

ShapeObject Parser::parseShapeObject()
{
    ShapeObject object;
    while (m_xml.readNextStartElement()) {
        if (parseCommon(object)) {
            continue;
        }
        const QStringView name = m_xml.name();
        if (name == "ShapeType"_L1) {
            object.shapeType = readEnum(shapeTypeNames, ShapeType::HorizontalLine);
        } else if (name == "LineWidth"_L1) {
            object.lineWidth = readReal();
        } else if (name == "LineAlignment"_L1) {
            object.lineAlignment = readEnum(lineAlignmentNames, LineAlignment::Center);
        } else if (name == "FillColor"_L1) {
            object.fillColor = readColor();
        } else {
            skipUnknown(u"ShapeObject");
        }
    }
    return object;
}

bool Parser::parseLayout(TextLayout &layout)
{
    const QStringView name = m_xml.name();
    if (name == "HorizontalAlignment"_L1) {
        layout.horizontalAlignment = readEnum(horizontalAlignmentNames, HorizontalAlignment::Center);
    } else if (name == "VerticalAlignment"_L1) {
        layout.verticalAlignment = readEnum(verticalAlignmentNames, VerticalAlignment::Middle);
    } else if (name == "TextFitMode"_L1) {
        layout.fitMode = readEnum(textFitModeNames, TextFitMode::ShrinkToFit);
    } else if (name == "UseFullFontHeight"_L1) {
        layout.useFullFontHeight = readBool();
    } else if (name == "Verticalized"_L1) {
        layout.verticalized = readBool();
    } else {
        return false;
    }
    return true;
}

CounterObject Parser::parseCounterObject()
{
    CounterObject object;
    while (m_xml.readNextStartElement()) {
        if (parseCommon(object) || parseLayout(object.layout)) {
            continue;
        }
        const QStringView name = m_xml.name();
        if (name == "Font"_L1) {
            object.font = readFont();
        } else if (name == "PreText"_L1) {
            object.preText = m_xml.readElementText();
        } else if (name == "PostText"_L1) {
            object.postText = m_xml.readElementText();
        } else if (name == "Start"_L1) {
            object.start = readInt();
        } else if (name == "Current"_L1) {
            object.current = readInt();
        } else if (name == "Increment"_L1) {
            object.increment = readInt();
        } else if (name == "FormatWidth"_L1) {
            object.formatWidth = readInt();
        } else if (name == "UseLeadingZeros"_L1) {
            object.useLeadingZeros = readBool();
        } else {
            skipUnknown(u"CounterObject");
        }
    }
    return object;
}

DateTimeObject Parser::parseDateTimeObject()
{
    DateTimeObject object;
    while (m_xml.readNextStartElement()) {
        if (parseCommon(object) || parseLayout(object.layout)) {
            continue;
        }
        const QStringView name = m_xml.name();
        if (name == "DateTimeFormat"_L1) {
            object.format = readEnum(dateFormatNames, DateFormat::LongSystemDate);
        } else if (name == "Font"_L1) {
            object.font = readFont();
        } else if (name == "PreText"_L1) {
            object.preText = m_xml.readElementText();
        } else if (name == "PostText"_L1) {
            object.postText = m_xml.readElementText();
        } else if (name == "IncludeTime"_L1) {
            object.includeTime = readBool();
        } else if (name == "Use24HourFormat"_L1) {
            object.use24HourFormat = readBool();
        } else if (name == "Culture"_L1) {
            object.culture = m_xml.readElementText().trimmed();
        } else {
            skipUnknown(u"DateTimeObject");
        }
    }
    return object;
}

CircularTextObject Parser::parseCircularTextObject()
{
    CircularTextObject object;
    while (m_xml.readNextStartElement()) {
        if (parseCommon(object)) {
            continue;
        }
        const QStringView name = m_xml.name();
        if (name == "Text"_L1) {
            object.text = m_xml.readElementText();
        } else if (name == "Font"_L1) {
            object.font = readFont();
        } else if (name == "StartAngle"_L1) {
            object.startAngle = readReal();
        } else if (name == "Mode"_L1) {
            object.mode = readEnum(circularTextModeNames, CircularTextMode::ArcTop);
        } else if (name == "CircleAlignment"_L1) {
            object.circleAlignment = readEnum(circleAlignmentNames, CircleAlignment::CenterAtTop);
        } else if (name == "TextAlignment"_L1) {
            object.textAlignment = readEnum(horizontalAlignmentNames, HorizontalAlignment::Center);
        } else if (name == "VerticalAlignment"_L1) {
            object.verticalAlignment = readEnum(verticalAlignmentNames, VerticalAlignment::Middle);
        } else {
            skipUnknown(u"CircularTextObject");
        }
    }
    return object;
}

class Serializer
{
public:
    explicit Serializer(QIODevice *device)
        : m_xml(device)
    {
        m_xml.setAutoFormatting(true);
        m_xml.setAutoFormattingIndent(-1); // tabs, like DLS
    }

    void writeDocument(const LabelDocument &document);
    bool hasError() const
    {
        return m_xml.hasError();
    }

private:
    void writeObject(const LabelObject &object);
    void writeContinuousLabel(const LabelDocument &document);
    void writeCell(const LabelDocument &document, const TapeCell &cell);
    void writeCommon(const ObjectBase &object);
    void writeText(const TextObject &object);
    void writeObjectBody(const TextObject &object);
    void writeObjectBody(const AddressObject &object);
    void writeObjectBody(const BarcodeObject &object);
    void writeObjectBody(const ImageObject &object);
    void writeObjectBody(const ShapeObject &object);
    void writeObjectBody(const CounterObject &object);
    void writeObjectBody(const DateTimeObject &object);
    void writeObjectBody(const CircularTextObject &object);
    void writeObjectBody(const UnknownObject &object);
    void writeLayout(const TextLayout &layout);

    void writeColor(QLatin1StringView name, const QColor &color);
    void writeFont(QLatin1StringView name, const LabelFont &font);

    template<typename Enum, std::size_t N>
    void writeEnum(QLatin1StringView name, const EnumName<Enum> (&names)[N], Enum value)
    {
        m_xml.writeTextElement(name, enumName(names, value));
    }

    QXmlStreamWriter m_xml;
};

void Serializer::writeContinuousLabel(const LabelDocument &document)
{
    m_xml.writeStartElement("ContinuousLabel"_L1);
    m_xml.writeAttribute("Version"_L1, "8.0"_L1);
    m_xml.writeAttribute("Units"_L1, "twips"_L1);
    writeEnum("PaperOrientation"_L1, paperOrientationNames, document.orientation);
    m_xml.writeTextElement("Id"_L1, document.id);
    m_xml.writeTextElement("PaperName"_L1, document.paperName);
    writeEnum("LengthMode"_L1, cellLengthModeNames, document.tape->lengthMode);
    m_xml.writeTextElement("LabelLength"_L1, formatReal(document.tape->length));
    m_xml.writeStartElement("RootCell"_L1);
    writeCell(document, document.tape->root);
    m_xml.writeEndElement();
    m_xml.writeEndElement();
}

void Serializer::writeCell(const LabelDocument &document, const TapeCell &cell)
{
    // In DLS's order: the object and its margin, the cell, its subcells.
    if (cell.object >= 0 && cell.object < document.objects.size()) {
        std::visit(
            [this](const auto &o) {
                writeObjectBody(o);
            },
            document.objects.at(cell.object));
        m_xml.writeEmptyElement("ObjectMargin"_L1);
        m_xml.writeAttribute("Left"_L1, formatReal(cell.objectMargin.left()));
        m_xml.writeAttribute("Top"_L1, formatReal(cell.objectMargin.top()));
        m_xml.writeAttribute("Right"_L1, formatReal(cell.objectMargin.right()));
        m_xml.writeAttribute("Bottom"_L1, formatReal(cell.objectMargin.bottom()));
    }
    m_xml.writeTextElement("Length"_L1, formatReal(cell.length));
    writeEnum("LengthMode"_L1, cellLengthModeNames, cell.lengthMode);
    m_xml.writeTextElement("BorderWidth"_L1, formatReal(cell.borderWidth));
    m_xml.writeTextElement("BorderStyle"_L1, cell.borderStyle);
    writeColor("BorderColor"_L1, cell.borderColor);
    if (!cell.subcells.isEmpty()) {
        writeEnum("SubcellsOrientation"_L1, orientationNames, cell.subcellsOrientation);
        m_xml.writeStartElement("Subcells"_L1);
        for (const TapeCell &child : cell.subcells) {
            m_xml.writeStartElement("Cell"_L1);
            writeCell(document, child);
            m_xml.writeEndElement();
        }
        m_xml.writeEndElement();
    }
}

void Serializer::writeDocument(const LabelDocument &document)
{
    m_xml.writeStartDocument();
    if (document.tape) {
        writeContinuousLabel(document);
        m_xml.writeEndDocument();
        return;
    }
    m_xml.writeStartElement("DieCutLabel"_L1);
    m_xml.writeAttribute("Version"_L1, "8.0"_L1);
    m_xml.writeAttribute("Units"_L1, "twips"_L1);
    if (document.mediaType) {
        m_xml.writeAttribute("MediaType"_L1, *document.mediaType);
    }

    writeEnum("PaperOrientation"_L1, paperOrientationNames, document.orientation);
    m_xml.writeTextElement("Id"_L1, document.id);
    if (document.outlined) {
        m_xml.writeTextElement("IsOutlined"_L1, formatBool(*document.outlined));
    }
    m_xml.writeTextElement("PaperName"_L1, document.paperName);

    if (!document.drawCommandsXml.isEmpty()) {
        replayElement(m_xml, document.drawCommandsXml);
    } else {
        const QSizeF paper = document.paperSize();
        m_xml.writeStartElement("DrawCommands"_L1);
        m_xml.writeEmptyElement("RoundRectangle"_L1);
        m_xml.writeAttribute("X"_L1, "0"_L1);
        m_xml.writeAttribute("Y"_L1, "0"_L1);
        m_xml.writeAttribute("Width"_L1, formatReal(paper.width()));
        m_xml.writeAttribute("Height"_L1, formatReal(paper.height()));
        m_xml.writeAttribute("Rx"_L1, "270"_L1);
        m_xml.writeAttribute("Ry"_L1, "270"_L1);
        m_xml.writeEndElement();
    }

    for (const LabelObject &object : document.objects) {
        writeObject(object);
    }

    m_xml.writeEndElement();
    m_xml.writeEndDocument();
}

void Serializer::writeObject(const LabelObject &object)
{
    m_xml.writeStartElement("ObjectInfo"_L1);
    std::visit(
        [this](const auto &o) {
            writeObjectBody(o);
        },
        object);

    const QRectF bounds = objectBase(object).bounds;
    m_xml.writeEmptyElement("Bounds"_L1);
    m_xml.writeAttribute("X"_L1, formatReal(bounds.x()));
    m_xml.writeAttribute("Y"_L1, formatReal(bounds.y()));
    m_xml.writeAttribute("Width"_L1, formatReal(bounds.width()));
    m_xml.writeAttribute("Height"_L1, formatReal(bounds.height()));
    m_xml.writeEndElement();
}

void Serializer::writeCommon(const ObjectBase &object)
{
    m_xml.writeTextElement("Name"_L1, object.name);
    writeColor("ForeColor"_L1, object.foreColor);
    writeColor("BackColor"_L1, object.backColor);
    m_xml.writeTextElement("LinkedObjectName"_L1, object.linkedObjectName);
    writeEnum("Rotation"_L1, rotationNames, object.rotation);
    m_xml.writeTextElement("IsMirrored"_L1, formatBool(object.mirrored));
    m_xml.writeTextElement("IsVariable"_L1, formatBool(object.variable));
    if (object.groupId) {
        m_xml.writeTextElement("GroupID"_L1, QString::number(*object.groupId));
    }
    if (object.outlined) {
        m_xml.writeTextElement("IsOutlined"_L1, formatBool(*object.outlined));
    }
}

void Serializer::writeText(const TextObject &object)
{
    writeEnum("HorizontalAlignment"_L1, horizontalAlignmentNames, object.horizontalAlignment);
    writeEnum("VerticalAlignment"_L1, verticalAlignmentNames, object.verticalAlignment);
    writeEnum("TextFitMode"_L1, textFitModeNames, object.fitMode);
    m_xml.writeTextElement("UseFullFontHeight"_L1, formatBool(object.useFullFontHeight));
    m_xml.writeTextElement("Verticalized"_L1, formatBool(object.verticalized));

    if (object.spans.isEmpty()) {
        m_xml.writeEmptyElement("StyledText"_L1);
        return;
    }
    m_xml.writeStartElement("StyledText"_L1);
    for (const TextSpan &span : object.spans) {
        m_xml.writeStartElement("Element"_L1);
        m_xml.writeStartElement("String"_L1);
        if (!span.text.isEmpty() && (span.text.front().isSpace() || span.text.back().isSpace())) {
            m_xml.writeAttribute("http://www.w3.org/XML/1998/namespace"_L1, "space"_L1, "preserve"_L1);
        }
        m_xml.writeCharacters(span.text);
        m_xml.writeEndElement();
        m_xml.writeStartElement("Attributes"_L1);
        writeFont("Font"_L1, span.font);
        writeColor("ForeColor"_L1, span.color);
        m_xml.writeEndElement();
        m_xml.writeEndElement();
    }
    m_xml.writeEndElement();
}

void Serializer::writeObjectBody(const TextObject &object)
{
    m_xml.writeStartElement("TextObject"_L1);
    writeCommon(object);
    writeText(object);
    m_xml.writeEndElement();
}

void Serializer::writeObjectBody(const AddressObject &object)
{
    m_xml.writeStartElement("AddressObject"_L1);
    writeCommon(object);
    writeText(object);
    m_xml.writeTextElement("ShowBarcodeFor9DigitZipOnly"_L1, formatBool(object.showBarcodeFor9DigitZipOnly));
    m_xml.writeTextElement("BarcodePosition"_L1, object.barcodePosition);
    m_xml.writeStartElement("LineFonts"_L1);
    for (const LabelFont &font : object.lineFonts) {
        writeFont("Font"_L1, font);
    }
    m_xml.writeEndElement();
    m_xml.writeEndElement();
}

void Serializer::writeObjectBody(const BarcodeObject &object)
{
    m_xml.writeStartElement("BarcodeObject"_L1);
    writeCommon(object);
    m_xml.writeTextElement("Text"_L1, object.text);
    m_xml.writeTextElement("Type"_L1, object.type);
    writeEnum("Size"_L1, barcodeSizeNames, object.size);
    writeEnum("TextPosition"_L1, barcodeTextPositionNames, object.textPosition);
    writeFont("TextFont"_L1, object.textFont);
    writeFont("CheckSumFont"_L1, object.checkSumFont);
    m_xml.writeTextElement("TextEmbedding"_L1, object.textEmbedding);
    m_xml.writeTextElement("ECLevel"_L1, QString::number(object.errorCorrectionLevel));
    writeEnum("HorizontalAlignment"_L1, horizontalAlignmentNames, object.horizontalAlignment);
    m_xml.writeEmptyElement("QuietZonesPadding"_L1);
    m_xml.writeAttribute("Left"_L1, formatReal(object.quietZonesPadding.left()));
    m_xml.writeAttribute("Right"_L1, formatReal(object.quietZonesPadding.right()));
    m_xml.writeAttribute("Top"_L1, formatReal(object.quietZonesPadding.top()));
    m_xml.writeAttribute("Bottom"_L1, formatReal(object.quietZonesPadding.bottom()));
    m_xml.writeEndElement();
}

void Serializer::writeObjectBody(const ImageObject &object)
{
    m_xml.writeStartElement("ImageObject"_L1);
    writeCommon(object);
    if (object.imageLocation) {
        m_xml.writeTextElement("ImageLocation"_L1, *object.imageLocation);
    } else {
        m_xml.writeTextElement("Image"_L1, QString::fromLatin1(object.imageData.toBase64()));
    }
    writeEnum("ScaleMode"_L1, imageScaleModeNames, object.scaleMode);
    m_xml.writeTextElement("BorderWidth"_L1, formatReal(object.borderWidth));
    writeColor("BorderColor"_L1, object.borderColor);
    writeEnum("HorizontalAlignment"_L1, horizontalAlignmentNames, object.horizontalAlignment);
    writeEnum("VerticalAlignment"_L1, verticalAlignmentNames, object.verticalAlignment);
    m_xml.writeEndElement();
}

void Serializer::writeObjectBody(const ShapeObject &object)
{
    m_xml.writeStartElement("ShapeObject"_L1);
    writeCommon(object);
    writeEnum("ShapeType"_L1, shapeTypeNames, object.shapeType);
    m_xml.writeTextElement("LineWidth"_L1, formatReal(object.lineWidth));
    writeEnum("LineAlignment"_L1, lineAlignmentNames, object.lineAlignment);
    writeColor("FillColor"_L1, object.fillColor);
    m_xml.writeEndElement();
}

void Serializer::writeLayout(const TextLayout &layout)
{
    writeEnum("HorizontalAlignment"_L1, horizontalAlignmentNames, layout.horizontalAlignment);
    writeEnum("VerticalAlignment"_L1, verticalAlignmentNames, layout.verticalAlignment);
    writeEnum("TextFitMode"_L1, textFitModeNames, layout.fitMode);
    m_xml.writeTextElement("UseFullFontHeight"_L1, formatBool(layout.useFullFontHeight));
    m_xml.writeTextElement("Verticalized"_L1, formatBool(layout.verticalized));
}

void Serializer::writeObjectBody(const CounterObject &object)
{
    m_xml.writeStartElement("CounterObject"_L1);
    writeCommon(object);
    writeLayout(object.layout);
    writeFont("Font"_L1, object.font);
    m_xml.writeTextElement("PreText"_L1, object.preText);
    m_xml.writeTextElement("PostText"_L1, object.postText);
    m_xml.writeTextElement("Start"_L1, QString::number(object.start));
    m_xml.writeTextElement("Current"_L1, QString::number(object.current));
    m_xml.writeTextElement("Increment"_L1, QString::number(object.increment));
    m_xml.writeTextElement("FormatWidth"_L1, QString::number(object.formatWidth));
    m_xml.writeTextElement("UseLeadingZeros"_L1, formatBool(object.useLeadingZeros));
    m_xml.writeEndElement();
}

void Serializer::writeObjectBody(const DateTimeObject &object)
{
    m_xml.writeStartElement("DateTimeObject"_L1);
    writeCommon(object);
    writeLayout(object.layout);
    writeEnum("DateTimeFormat"_L1, dateFormatNames, object.format);
    writeFont("Font"_L1, object.font);
    m_xml.writeTextElement("PreText"_L1, object.preText);
    m_xml.writeTextElement("PostText"_L1, object.postText);
    m_xml.writeTextElement("IncludeTime"_L1, formatBool(object.includeTime));
    m_xml.writeTextElement("Use24HourFormat"_L1, formatBool(object.use24HourFormat));
    m_xml.writeTextElement("Culture"_L1, object.culture);
    m_xml.writeEndElement();
}

void Serializer::writeObjectBody(const CircularTextObject &object)
{
    m_xml.writeStartElement("CircularTextObject"_L1);
    writeCommon(object);
    m_xml.writeTextElement("Text"_L1, object.text);
    writeFont("Font"_L1, object.font);
    m_xml.writeTextElement("StartAngle"_L1, formatReal(object.startAngle));
    writeEnum("Mode"_L1, circularTextModeNames, object.mode);
    writeEnum("CircleAlignment"_L1, circleAlignmentNames, object.circleAlignment);
    writeEnum("TextAlignment"_L1, horizontalAlignmentNames, object.textAlignment);
    writeEnum("VerticalAlignment"_L1, verticalAlignmentNames, object.verticalAlignment);
    m_xml.writeEndElement();
}

void Serializer::writeObjectBody(const UnknownObject &object)
{
    replayElement(m_xml, object.xml);
}

void Serializer::writeColor(QLatin1StringView name, const QColor &color)
{
    m_xml.writeEmptyElement(name);
    m_xml.writeAttribute("Alpha"_L1, QString::number(color.alpha()));
    m_xml.writeAttribute("Red"_L1, QString::number(color.red()));
    m_xml.writeAttribute("Green"_L1, QString::number(color.green()));
    m_xml.writeAttribute("Blue"_L1, QString::number(color.blue()));
}

void Serializer::writeFont(QLatin1StringView name, const LabelFont &font)
{
    m_xml.writeEmptyElement(name);
    m_xml.writeAttribute("Family"_L1, font.family);
    m_xml.writeAttribute("Size"_L1, formatReal(font.size));
    m_xml.writeAttribute("Bold"_L1, formatBool(font.bold));
    m_xml.writeAttribute("Italic"_L1, formatBool(font.italic));
    m_xml.writeAttribute("Underline"_L1, formatBool(font.underline));
    m_xml.writeAttribute("Strikeout"_L1, formatBool(font.strikeout));
}

} // namespace

std::optional<LabelDocument> LabelReader::read(QIODevice *device)
{
    m_errorString.clear();
    m_warnings.clear();

    QXmlStreamReader xml(device);
    Parser parser(xml, m_warnings);
    return parser.parseDocument(m_errorString);
}

std::optional<LabelDocument> LabelReader::readFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        m_errorString = file.errorString();
        m_warnings.clear();
        return std::nullopt;
    }
    return read(&file);
}

QString LabelReader::errorString() const
{
    return m_errorString;
}

QStringList LabelReader::warnings() const
{
    return m_warnings;
}

bool LabelWriter::write(const LabelDocument &document, QIODevice *device)
{
    Serializer serializer(device);
    serializer.writeDocument(document);
    return !serializer.hasError();
}

bool LabelWriter::writeFile(const LabelDocument &document, const QString &fileName, QString *errorString)
{
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return false;
    }
    if (!write(document, &file) || !file.commit()) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return false;
    }
    return true;
}

QByteArray LabelWriter::toByteArray(const LabelDocument &document)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    write(document, &buffer);
    return data;
}

} // namespace LabelCreator
