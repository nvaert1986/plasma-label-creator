/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <QByteArray>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QList>
#include <QMarginsF>
#include <QPainterPath>
#include <QRectF>
#include <QString>

#include <optional>
#include <variant>

/**
 * The in-memory model of a DYMO Label Software 8 file. It mirrors the XML
 * closely so files survive a load/save cycle unchanged; all lengths are in
 * twips, see Units.
 */
namespace LabelCreator
{

enum class PaperOrientation {
    Portrait,
    Landscape,
};

enum class Rotation {
    Rotation0,
    Rotation90,
    Rotation180,
    Rotation270,
};

enum class HorizontalAlignment {
    Left,
    Center,
    Right,
};

enum class VerticalAlignment {
    Top,
    Middle,
    Bottom,
};

enum class TextFitMode {
    None,
    ShrinkToFit,
    AlwaysFit,
};

enum class BarcodeSize {
    Small,
    Medium,
    Large,
    SmallMedium,
};

enum class BarcodeTextPosition {
    None,
    Top,
    Bottom,
};

enum class ImageScaleMode {
    None,
    Uniform,
    Fill,
};

enum class ShapeType {
    HorizontalLine,
    VerticalLine,
    Rectangle,
    Ellipse,
};

enum class DateFormat {
    None,
    LongSystemDate,
    ShortSystemDate,
    WeekdayLongMonthDayLongYear,
    WeekdayDayLongMonthLongYear,
    AbbrWeekdayAbbrMonthDayLongYear,
    AbbrWeekdayDayAbbrMonthLongYear,
    LongMonthDayLongYear,
    DayLongMonthLongYear,
    MonthDayLongYear,
    DayMonthLongYear,
    MonthDayYear,
    DayMonthYear,
    AbbrMonthDayLongYear,
    AbbrMonthDayYear,
    DayAbbrMonthLongYear,
    DayAbbrMonthYear,
};

enum class CircularTextMode {
    ArcTop,
    ArcBottom,
    Circle,
};

enum class CircleAlignment {
    CenterAtTop,
    CenterAtBottom,
    StartAngle,
};

enum class LineAlignment {
    Center,
    LeftOrTop,
    RightOrBottom,
};

struct LabelFont {
    QString family = QStringLiteral("Arial");
    qreal size = 12; // points
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikeout = false;

    bool operator==(const LabelFont &) const = default;
};

/** A run of text in one font and colour; the text may contain line breaks. */
struct TextSpan {
    QString text;
    LabelFont font;
    QColor color = Qt::black;

    bool operator==(const TextSpan &) const = default;
};

/** The properties every object shares, plus its position on the label. */
struct ObjectBase {
    QString name;
    QColor foreColor = Qt::black;
    QColor backColor = QColor(255, 255, 255, 0);
    QString linkedObjectName;
    Rotation rotation = Rotation::Rotation0;
    bool mirrored = false;
    bool variable = false;
    // Only some versions of DLS write these; kept so they are written back.
    std::optional<int> groupId;
    std::optional<bool> outlined;
    QRectF bounds;

    bool operator==(const ObjectBase &) const = default;
};

struct TextObject : ObjectBase {
    HorizontalAlignment horizontalAlignment = HorizontalAlignment::Left;
    VerticalAlignment verticalAlignment = VerticalAlignment::Middle;
    TextFitMode fitMode = TextFitMode::ShrinkToFit;
    bool useFullFontHeight = true;
    bool verticalized = false;
    QList<TextSpan> spans;

    QString plainText() const;

    bool operator==(const TextObject &) const = default;
};

struct AddressObject : TextObject {
    bool showBarcodeFor9DigitZipOnly = false;
    QString barcodePosition = QStringLiteral("AboveAddress"); // US POSTNET, not rendered
    QList<LabelFont> lineFonts; // one per line of the address, as DLS keeps them

    /**
     * Gives lineFonts one font per line of the text again, after the text
     * changed: extra lines get the last line's font (or the text's), and
     * fonts for lines that are gone are dropped.
     */
    void fitLineFonts();

    bool operator==(const AddressObject &) const = default;
};

struct BarcodeObject : ObjectBase {
    QString text;
    QString type = QStringLiteral("Code128Auto"); // kept verbatim, mapped when rendering
    BarcodeSize size = BarcodeSize::Medium;
    BarcodeTextPosition textPosition = BarcodeTextPosition::Bottom;
    LabelFont textFont = {QStringLiteral("Helvetica"), 10};
    LabelFont checkSumFont = {QStringLiteral("Helvetica"), 10};
    QString textEmbedding = QStringLiteral("None");
    int errorCorrectionLevel = 0;
    HorizontalAlignment horizontalAlignment = HorizontalAlignment::Center;
    QMarginsF quietZonesPadding;

    bool operator==(const BarcodeObject &) const = default;
};

struct ImageObject : ObjectBase {
    QByteArray imageData; // the embedded file (PNG, GIF...), as stored
    std::optional<QString> imageLocation; // a linked file instead of embedded data
    ImageScaleMode scaleMode = ImageScaleMode::Uniform;
    qreal borderWidth = 0;
    QColor borderColor = Qt::black;
    HorizontalAlignment horizontalAlignment = HorizontalAlignment::Center;
    VerticalAlignment verticalAlignment = VerticalAlignment::Middle;

    bool operator==(const ImageObject &) const = default;
};

struct ShapeObject : ObjectBase {
    ShapeType shapeType = ShapeType::HorizontalLine;
    qreal lineWidth = 15;
    LineAlignment lineAlignment = LineAlignment::Center;
    QColor fillColor = QColor(255, 255, 255, 0);

    bool operator==(const ShapeObject &) const = default;
};

/** Layout shared by the objects that draw one line of generated text. */
struct TextLayout {
    HorizontalAlignment horizontalAlignment = HorizontalAlignment::Center;
    VerticalAlignment verticalAlignment = VerticalAlignment::Middle;
    TextFitMode fitMode = TextFitMode::ShrinkToFit;
    bool useFullFontHeight = true;
    bool verticalized = false;

    bool operator==(const TextLayout &) const = default;
};

/** A number that goes up with every label printed. */
struct CounterObject : ObjectBase {
    TextLayout layout;
    LabelFont font;
    QString preText;
    QString postText;
    int start = 1;
    int current = 1;
    int increment = 1;
    int formatWidth = 1; // minimum number of digits
    bool useLeadingZeros = false;

    /** The text for the label printed @p step labels after the current one. */
    QString text(int step = 0) const;

    bool operator==(const CounterObject &) const = default;
};

/** The date (and time) of printing. */
struct DateTimeObject : ObjectBase {
    TextLayout layout;
    DateFormat format = DateFormat::LongSystemDate;
    LabelFont font;
    QString preText;
    QString postText;
    bool includeTime = false;
    bool use24HourFormat = true;
    QString culture; // e.g. "nl-NL"; empty means the system's

    QString text(const QDateTime &when) const;

    bool operator==(const DateTimeObject &) const = default;
};

/** Text along an arc or a full circle. */
struct CircularTextObject : ObjectBase {
    QString text;
    LabelFont font;
    qreal startAngle = 0; // degrees, for CircleAlignment::StartAngle
    CircularTextMode mode = CircularTextMode::ArcTop;
    CircleAlignment circleAlignment = CircleAlignment::CenterAtTop;
    HorizontalAlignment textAlignment = HorizontalAlignment::Center;
    VerticalAlignment verticalAlignment = VerticalAlignment::Middle;

    bool operator==(const CircularTextObject &) const = default;
};

/**
 * An object type Plasma Label Creator does not understand. Its XML is kept verbatim so
 * saving does not drop it.
 */
struct UnknownObject : ObjectBase {
    QString elementName;
    QString xml;

    bool operator==(const UnknownObject &) const = default;
};

using LabelObject =
    std::variant<TextObject, AddressObject, BarcodeObject, ImageObject, ShapeObject, CounterObject, DateTimeObject, CircularTextObject, UnknownObject>;

const ObjectBase &objectBase(const LabelObject &object);
ObjectBase &objectBase(LabelObject &object);

/** How long a tape cell is along its parent's direction (DLS's LengthMode). */
enum class CellLengthMode {
    Auto, // as long as its content needs
    Fixed, // its length
    Star, // a share of the room a fixed-length label has left
};

/**
 * A part of a tape label (DLS's ContinuousLabel). Cells sit side by side
 * along the tape, or stacked across it; each holds one object, or more
 * cells. Objects on tape are placed by their cells, not by their bounds.
 */
struct TapeCell {
    int object = -1; // index into LabelDocument::objects; -1 for none
    QMarginsF objectMargin; // twips, as on screen: left/right along the tape
    qreal length = 0; // twips along its parent's direction; laid out for Auto and Star
    CellLengthMode lengthMode = CellLengthMode::Auto;
    qreal borderWidth = 0; // twips
    QString borderStyle = QStringLiteral("Solid");
    QColor borderColor = Qt::black;
    Qt::Orientation subcellsOrientation = Qt::Horizontal; // Horizontal: along the tape
    QList<TapeCell> subcells;
    /** Where the layout put the cell, in canvas twips; not saved. */
    QRectF bounds;

    bool operator==(const TapeCell &) const = default;
};

/** What makes a document a tape label rather than a die-cut one. */
struct TapeLabel {
    qreal width = 0; // twips across the tape, as the LabelManager driver has it
    CellLengthMode lengthMode = CellLengthMode::Auto; // Auto or Fixed
    qreal length = 0; // twips along the tape; laid out for Auto
    TapeCell root;

    bool operator==(const TapeLabel &) const = default;
};

/**
 * A question asked when printing, such as a batch number. The answer fills
 * {name} wherever it appears on the label.
 */
struct PrintQuestion {
    enum class Allowed {
        Anything,
        Digits,
        Letters,
        LettersAndDigits,
        /** The whole answer matches a regular expression. */
        Pattern,
    };

    QString name; // the field it fills: {name}
    QString prompt; // what is asked; the name when empty
    QString defaultValue;
    bool required = true;
    int minimumLength = 0;
    int maximumLength = 0; // 0 for no limit
    Allowed allowed = Allowed::Anything;
    QString pattern; // for Allowed::Pattern

    bool operator==(const PrintQuestion &) const = default;
};

/**
 * Where the rows come from when printing a label per row: a file (CSV,
 * Excel, OpenDocument) or a database query. Passwords are never kept.
 */
struct DataSource {
    enum class Kind {
        None,
        File,
        Database,
    };

    Kind kind = Kind::None;
    QString fileName; // a .csv, .txt, .xlsx or .ods file
    QString sheet; // spreadsheets: the sheet; empty for the first
    QChar delimiter; // CSV: the separator; null to detect it
    bool firstRowNames = true; // files: the first row names the columns
    QString driver; // Qt SQL driver: QSQLITE, QODBC, QPSQL, QMYSQL...
    QString database; // a file, a database name, or an ODBC data source or connection string
    QString host;
    int port = 0; // 0 for the driver's default
    QString user;
    QString query;

    bool operator==(const DataSource &) const = default;
};

struct LabelDocument {
    PaperOrientation orientation = PaperOrientation::Landscape;
    QString id;
    QString paperName;
    std::optional<QString> mediaType;
    std::optional<bool> outlined;

    /**
     * The die-cut outline in paper coordinates, i.e. as the label feeds
     * through the printer. Landscape documents are laid out rotated.
     */
    QPainterPath paperOutline;
    /** The DrawCommands element verbatim; written back unchanged. */
    QString drawCommandsXml;

    QList<LabelObject> objects;

    /** Set for a tape label (DYMO LabelManager), which DLS calls a ContinuousLabel. */
    std::optional<TapeLabel> tape;

    // What this program adds to DLS's format, kept in the file as a comment
    // DLS ignores: questions asked when printing, and where data comes from.
    QList<PrintQuestion> questions;
    DataSource dataSource;

    /**
     * Adds @p object and returns its index. On tape it gets a cell of its
     * own at the end of the label.
     */
    int addObject(LabelObject object);
    /** Removes object @p index; on tape its cell too. */
    void removeObject(int index);
    /** Moves object @p from to @p to in the drawing order; tape cells keep their objects. */
    void moveObject(int from, int to);
    /** The tape cell holding object @p index; nullptr on die-cut labels. */
    TapeCell *cellOf(int index);
    /**
     * Moves the cell of object @p index @p steps places among its neighbours
     * (negative: towards the start of the tape). False if it cannot move.
     */
    bool moveCell(int index, int steps);

    /**
     * Turns the label a quarter between landscape and portrait, and every
     * object with it: positions move round, rotations follow, tape cells
     * turn. Only the view turns: the label prints exactly as before.
     */
    void rotateQuarter();

    /** Size of the paper as it feeds, in twips. */
    QSizeF paperSize() const;
    /** Size of the area objects are laid out on, in twips. */
    QSizeF canvasSize() const;

    bool operator==(const LabelDocument &) const = default;
};

/** Converts a label font to a QFont whose pixel size is the font size in twips. */
QFont toQFont(const LabelFont &font, qreal scale = 1.0);

} // namespace LabelCreator
