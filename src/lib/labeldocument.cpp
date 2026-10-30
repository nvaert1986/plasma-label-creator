/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "labeldocument.h"

#include <algorithm>
#include <functional>

#include "units.h"

#include <QLocale>
#include <QTransform>

#include <cmath>

namespace LabelCreator
{

QString TextObject::plainText() const
{
    QString text;
    for (const TextSpan &span : spans) {
        text += span.text;
    }
    return text;
}

void AddressObject::fitLineFonts()
{
    const qsizetype lines = plainText().count(u'\n') + 1;
    const LabelFont last = !lineFonts.isEmpty() ? lineFonts.constLast() : (!spans.isEmpty() ? spans.constFirst().font : LabelFont());
    while (lineFonts.size() < lines) {
        lineFonts.append(last);
    }
    lineFonts.resize(lines);
}

QString CounterObject::text(int step) const
{
    const qint64 value = qint64(current) + qint64(step) * increment;
    QString number = QString::number(std::abs(value));
    if (useLeadingZeros && number.size() < formatWidth) {
        number = QString(formatWidth - number.size(), u'0') + number;
    }
    if (value < 0) {
        number.prepend(u'-');
    }
    return preText + number + postText;
}

namespace
{

// The formats DLS offers, written with the order and separators of the
// label's culture where the format leaves room for it.
QString dateText(DateFormat format, const QDate &date, const QLocale &locale)
{
    // The separator of the culture's short date, e.g. "-" for nl-NL.
    QChar separator = u'/';
    for (const QChar c : locale.dateFormat(QLocale::ShortFormat)) {
        if (!c.isLetterOrNumber()) {
            separator = c;
            break;
        }
    }
    const QString sep(separator);
    switch (format) {
    case DateFormat::None:
        return {};
    case DateFormat::LongSystemDate:
        return locale.toString(date, QLocale::LongFormat);
    case DateFormat::ShortSystemDate:
        return locale.toString(date, QLocale::ShortFormat);
    case DateFormat::WeekdayLongMonthDayLongYear:
        return locale.toString(date, u"dddd, MMMM d, yyyy");
    case DateFormat::WeekdayDayLongMonthLongYear:
        return locale.toString(date, u"dddd d MMMM yyyy");
    case DateFormat::AbbrWeekdayAbbrMonthDayLongYear:
        return locale.toString(date, u"ddd, MMM d, yyyy");
    case DateFormat::AbbrWeekdayDayAbbrMonthLongYear:
        return locale.toString(date, u"ddd d MMM yyyy");
    case DateFormat::LongMonthDayLongYear:
        return locale.toString(date, u"MMMM d, yyyy");
    case DateFormat::DayLongMonthLongYear:
        return locale.toString(date, u"d MMMM yyyy");
    case DateFormat::MonthDayLongYear:
        return locale.toString(date, u"M" + sep + u"d" + sep + u"yyyy");
    case DateFormat::DayMonthLongYear:
        return locale.toString(date, u"d" + sep + u"M" + sep + u"yyyy");
    case DateFormat::MonthDayYear:
        return locale.toString(date, u"M" + sep + u"d" + sep + u"yy");
    case DateFormat::DayMonthYear:
        return locale.toString(date, u"d" + sep + u"M" + sep + u"yy");
    case DateFormat::AbbrMonthDayLongYear:
        return locale.toString(date, u"MMM d, yyyy");
    case DateFormat::AbbrMonthDayYear:
        return locale.toString(date, u"MMM d, yy");
    case DateFormat::DayAbbrMonthLongYear:
        return locale.toString(date, u"d MMM yyyy");
    case DateFormat::DayAbbrMonthYear:
        return locale.toString(date, u"d MMM yy");
    }
    Q_UNREACHABLE();
}

} // namespace

QString DateTimeObject::text(const QDateTime &when) const
{
    const QLocale locale = culture.isEmpty() ? QLocale() : QLocale(QString(culture).replace(u'-', u'_'));
    QString result = dateText(format, when.date(), locale);
    if (includeTime) {
        const QString time = locale.toString(when.time(), use24HourFormat ? u"HH:mm" : u"h:mm AP");
        result = result.isEmpty() ? time : result + u' ' + time;
    }
    return preText + result + postText;
}

QPolygonF objectOutline(const ObjectBase &object)
{
    // Counter-clockwise on screen is a negative angle with y downwards.
    QTransform turn;
    turn.translate(object.bounds.center().x(), object.bounds.center().y());
    turn.rotate(-object.angle);
    turn.translate(-object.bounds.center().x(), -object.bounds.center().y());
    return turn.map(QPolygonF(object.bounds));
}

const ObjectBase &objectBase(const LabelObject &object)
{
    return std::visit(
        [](const auto &o) -> const ObjectBase & {
            return o;
        },
        object);
}

ObjectBase &objectBase(LabelObject &object)
{
    return std::visit(
        [](auto &o) -> ObjectBase & {
            return o;
        },
        object);
}

namespace
{

// Calls @p change for every cell's object index.
void forEachCell(TapeCell &cell, const std::function<void(TapeCell &)> &change)
{
    change(cell);
    for (TapeCell &child : cell.subcells) {
        forEachCell(child, change);
    }
}

// Drops the cells that held object @p index and are empty now.
void removeCellsOf(TapeCell &cell, int index)
{
    if (cell.object == index) {
        cell.object = -1;
    }
    for (TapeCell &child : cell.subcells) {
        removeCellsOf(child, index);
    }
    cell.subcells.removeIf([](const TapeCell &child) {
        return child.object < 0 && child.subcells.isEmpty();
    });
}

} // namespace

int LabelDocument::addObject(LabelObject object)
{
    objects.append(std::move(object));
    const int index = int(objects.size() - 1);
    if (tape) {
        TapeCell cell;
        cell.object = index;
        cell.objectMargin = QMarginsF(72, 0, 72, 0); // 1/20 inch apart along the tape
        if (tape->root.object >= 0) {
            // A root holding one object becomes a row of cells.
            TapeCell first = tape->root;
            first.subcells.clear();
            first.bounds = QRectF();
            tape->root.object = -1;
            tape->root.objectMargin = QMarginsF();
            tape->root.subcells = {first};
        }
        tape->root.subcellsOrientation = Qt::Horizontal;
        tape->root.subcells.append(cell);
    }
    return index;
}

void LabelDocument::removeObject(int index)
{
    if (index < 0 || index >= objects.size()) {
        return;
    }
    objects.removeAt(index);
    if (tape) {
        removeCellsOf(tape->root, index);
        forEachCell(tape->root, [index](TapeCell &cell) {
            if (cell.object > index) {
                --cell.object;
            }
        });
    }
}

void LabelDocument::moveObject(int from, int to)
{
    if (from < 0 || from >= objects.size() || to < 0 || to >= objects.size() || from == to) {
        return;
    }
    objects.move(from, to);
    if (tape) {
        forEachCell(tape->root, [from, to](TapeCell &cell) {
            if (cell.object == from) {
                cell.object = to;
            } else if (from < to && cell.object > from && cell.object <= to) {
                --cell.object;
            } else if (from > to && cell.object >= to && cell.object < from) {
                ++cell.object;
            }
        });
    }
}

TapeCell *LabelDocument::cellOf(int index)
{
    if (!tape) {
        return nullptr;
    }
    TapeCell *found = nullptr;
    forEachCell(tape->root, [&found, index](TapeCell &cell) {
        if (!found && cell.object == index) {
            found = &cell;
        }
    });
    return found;
}

bool LabelDocument::moveCell(int index, int steps)
{
    if (!tape) {
        return false;
    }
    bool moved = false;
    const std::function<void(TapeCell &)> search = [&](TapeCell &parent) {
        for (qsizetype i = 0; i < parent.subcells.size() && !moved; ++i) {
            if (parent.subcells.at(i).object == index) {
                const qsizetype to = i + steps;
                if (to >= 0 && to < parent.subcells.size()) {
                    parent.subcells.move(i, to);
                    moved = true;
                }
                return;
            }
            search(parent.subcells[i]);
        }
    };
    search(tape->root);
    return moved;
}

namespace
{

// Rotations a quarter turn on, clockwise as seen on the paper.
Rotation clockwise(Rotation rotation)
{
    switch (rotation) {
    case Rotation::Rotation0:
        return Rotation::Rotation270; // Rotation270 turns clockwise
    case Rotation::Rotation270:
        return Rotation::Rotation180;
    case Rotation::Rotation180:
        return Rotation::Rotation90;
    case Rotation::Rotation90:
        return Rotation::Rotation0;
    }
    Q_UNREACHABLE();
}

Rotation counterClockwise(Rotation rotation)
{
    return clockwise(clockwise(clockwise(rotation)));
}

} // namespace

void LabelDocument::rotateQuarter()
{
    // A landscape canvas is the paper turned: canvas (x, y) is paper
    // (width - y, x). Going to portrait, every object turns a quarter
    // clockwise to stay where and how it is on the paper; going back,
    // counter-clockwise.
    const bool toPortrait = orientation == PaperOrientation::Landscape;
    const qreal width = paperSize().width();
    const auto turnRect = [&](const QRectF &r) {
        return toPortrait ? QRectF(width - r.bottom(), r.x(), r.height(), r.width()) : QRectF(r.y(), width - r.right(), r.height(), r.width());
    };
    for (LabelObject &object : objects) {
        ObjectBase &base = objectBase(object);
        base.rotation = toPortrait ? clockwise(base.rotation) : counterClockwise(base.rotation);
        if (!tape) {
            base.bounds = turnRect(base.bounds);
        }
    }
    if (tape) {
        // Cells turn with the label; the layout places them. Margins move
        // round the same way as the sides of a box, and cells stacked
        // across the tape swap order, as top-to-bottom on a landscape
        // canvas is right-to-left on the paper.
        const std::function<void(TapeCell &)> turn = [&](TapeCell &cell) {
            const QMarginsF m = cell.objectMargin;
            cell.objectMargin = toPortrait ? QMarginsF(m.bottom(), m.left(), m.top(), m.right()) : QMarginsF(m.top(), m.right(), m.bottom(), m.left());
            const bool across = toPortrait ? cell.subcellsOrientation == Qt::Vertical : cell.subcellsOrientation == Qt::Horizontal;
            if (across) {
                std::reverse(cell.subcells.begin(), cell.subcells.end());
            }
            cell.subcellsOrientation = cell.subcellsOrientation == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal;
            for (TapeCell &child : cell.subcells) {
                turn(child);
            }
        };
        turn(tape->root);
    }
    orientation = toPortrait ? PaperOrientation::Portrait : PaperOrientation::Landscape;
}

void LabelDocument::changeLayers(const QList<Layer> &newLayers, const QHash<QString, QString> &renamed)
{
    layers = newLayers;
    for (LabelObject &object : objects) {
        QString &layer = objectBase(object).layer;
        layer = renamed.value(layer, layer);
        const bool exists = std::any_of(layers.cbegin(), layers.cend(), [&layer](const Layer &l) {
            return l.name == layer;
        });
        if (!exists) {
            layer.clear();
        }
    }
}

const Layer *LabelDocument::layerOf(int index) const
{
    if (index < 0 || index >= objects.size()) {
        return nullptr;
    }
    const QString &name = objectBase(objects.at(index)).layer;
    if (name.isEmpty()) {
        return nullptr;
    }
    const auto it = std::find_if(layers.cbegin(), layers.cend(), [&name](const Layer &layer) {
        return layer.name == name;
    });
    return it != layers.cend() ? &*it : nullptr;
}

bool LabelDocument::isShown(int index) const
{
    const Layer *layer = layerOf(index);
    return !layer || layer->visible;
}

bool LabelDocument::isPrinted(int index) const
{
    const Layer *layer = layerOf(index);
    return index >= 0 && index < objects.size() && objectBase(objects.at(index)).printed && (!layer || layer->printed);
}

bool LabelDocument::isLocked(int index) const
{
    const Layer *layer = layerOf(index);
    return index >= 0 && index < objects.size() && (objectBase(objects.at(index)).locked || (layer && layer->locked));
}

bool LabelDocument::isSelectable(int index) const
{
    const Layer *layer = layerOf(index);
    return index >= 0 && index < objects.size() && (!layer || (layer->visible && !layer->locked));
}

QSizeF LabelDocument::paperSize() const
{
    if (tape) {
        return QSizeF(tape->width, tape->length);
    }
    // Outlines start at the origin; holes and notches lie inside them.
    const QRectF bounds = paperOutline.boundingRect();
    return QSizeF(bounds.right(), bounds.bottom());
}

QSizeF LabelDocument::canvasSize() const
{
    const QSizeF paper = paperSize();
    return orientation == PaperOrientation::Landscape ? paper.transposed() : paper;
}

QFont toQFont(const LabelFont &font, qreal scale)
{
    QFont qfont(font.family);
    qfont.setPixelSize(std::max(1, int(std::lround(Units::pointsToTwips(font.size) * scale))));
    qfont.setBold(font.bold);
    qfont.setItalic(font.italic);
    qfont.setUnderline(font.underline);
    qfont.setStrikeOut(font.strikeout);
    // Hinting snaps metrics to device pixels, which breaks the linear
    // scaling between twips, screen and printer.
    qfont.setHintingPreference(QFont::PreferNoHinting);
    return qfont;
}

} // namespace LabelCreator
