/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "arrange.h"

#include <algorithm>

namespace LabelCreator::Arrange
{

namespace
{

// What moves as one: a group, or an object on its own.
struct Unit {
    QList<int> members;
    QRectF bounds;
    bool locked = false;
};

QList<Unit> unitsOf(const LabelDocument &document, const QList<int> &indices)
{
    QList<Unit> units;
    QList<int> done;
    for (const int index : withGroups(document, indices)) {
        if (done.contains(index)) {
            continue;
        }
        Unit unit;
        unit.members = isGrouped(document, index) ? withGroups(document, {index}) : QList<int>{index};
        for (const int member : std::as_const(unit.members)) {
            const ObjectBase &base = objectBase(document.objects.at(member));
            unit.bounds = unit.bounds.isNull() ? base.bounds : unit.bounds.united(base.bounds);
            unit.locked = unit.locked || document.isLocked(member);
            done << member;
        }
        units << unit;
    }
    return units;
}

bool move(LabelDocument &document, const Unit &unit, const QPointF &delta)
{
    if (unit.locked || delta.isNull()) {
        return false;
    }
    for (const int member : unit.members) {
        objectBase(document.objects[member]).bounds.translate(delta);
    }
    return true;
}

} // namespace

bool isGrouped(const LabelDocument &document, int index)
{
    const std::optional<int> &id = objectBase(document.objects.at(index)).groupId;
    return id && *id >= 0;
}

QList<int> withGroups(const LabelDocument &document, const QList<int> &indices)
{
    QList<int> groups;
    QList<int> result;
    for (const int index : indices) {
        if (index < 0 || index >= document.objects.size()) {
            continue;
        }
        if (!result.contains(index)) {
            result << index;
        }
        if (isGrouped(document, index)) {
            groups << *objectBase(document.objects.at(index)).groupId;
        }
    }
    for (int i = 0; i < document.objects.size(); ++i) {
        if (isGrouped(document, i) && groups.contains(*objectBase(document.objects.at(i)).groupId) && !result.contains(i)) {
            result << i;
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

int unitCount(const LabelDocument &document, const QList<int> &indices)
{
    return int(unitsOf(document, indices).size());
}

bool canMove(const LabelDocument &document, const QList<int> &indices)
{
    return std::any_of(indices.cbegin(), indices.cend(), [&document](int index) {
        return index >= 0 && index < document.objects.size() && !document.isLocked(index);
    });
}

bool align(LabelDocument &document, const QList<int> &indices, Qt::Alignment alignment)
{
    const QList<Unit> units = unitsOf(document, indices);
    QRectF around;
    for (const Unit &unit : units) {
        around = around.isNull() ? unit.bounds : around.united(unit.bounds);
    }
    bool moved = false;
    for (const Unit &unit : units) {
        QPointF delta;
        if (alignment & Qt::AlignLeft) {
            delta.setX(around.left() - unit.bounds.left());
        } else if (alignment & Qt::AlignHCenter) {
            delta.setX(around.center().x() - unit.bounds.center().x());
        } else if (alignment & Qt::AlignRight) {
            delta.setX(around.right() - unit.bounds.right());
        }
        if (alignment & Qt::AlignTop) {
            delta.setY(around.top() - unit.bounds.top());
        } else if (alignment & Qt::AlignVCenter) {
            delta.setY(around.center().y() - unit.bounds.center().y());
        } else if (alignment & Qt::AlignBottom) {
            delta.setY(around.bottom() - unit.bounds.bottom());
        }
        moved = move(document, unit, delta) || moved;
    }
    return moved;
}

bool distribute(LabelDocument &document, const QList<int> &indices, Qt::Orientation orientation)
{
    QList<Unit> units = unitsOf(document, indices);
    if (units.size() < 3) {
        return false;
    }
    const bool horizontal = orientation == Qt::Horizontal;
    const auto start = [horizontal](const QRectF &r) {
        return horizontal ? r.left() : r.top();
    };
    const auto length = [horizontal](const QRectF &r) {
        return horizontal ? r.width() : r.height();
    };
    std::stable_sort(units.begin(), units.end(), [&](const Unit &a, const Unit &b) {
        return start(a.bounds) + length(a.bounds) / 2 < start(b.bounds) + length(b.bounds) / 2;
    });
    // Equal gaps between neighbours, from the first one's start to the last one's end.
    const QRectF &first = units.constFirst().bounds;
    const QRectF &last = units.constLast().bounds;
    qreal total = start(last) + length(last) - start(first);
    for (const Unit &unit : std::as_const(units)) {
        total -= length(unit.bounds);
    }
    const qreal gap = total / (units.size() - 1);
    bool moved = false;
    qreal position = start(first) + length(first) + gap;
    for (qsizetype i = 1; i < units.size() - 1; ++i) {
        const qreal shift = position - start(units.at(i).bounds);
        moved = move(document, units.at(i), horizontal ? QPointF(shift, 0) : QPointF(0, shift)) || moved;
        position += length(units.at(i).bounds) + gap;
    }
    return moved;
}

int group(LabelDocument &document, const QList<int> &indices)
{
    int id = 0;
    for (const LabelObject &object : std::as_const(document.objects)) {
        id = std::max(id, objectBase(object).groupId.value_or(-1) + 1);
    }
    for (const int index : withGroups(document, indices)) {
        objectBase(document.objects[index]).groupId = id;
    }
    return id;
}

void ungroup(LabelDocument &document, const QList<int> &indices)
{
    for (const int index : withGroups(document, indices)) {
        objectBase(document.objects[index]).groupId = -1;
    }
}

namespace
{

// Moves @p indices to @p first, first + 1, ... in drawing order.
QList<int> moveTogether(LabelDocument &document, const QList<int> &indices, bool toFront)
{
    QList<int> sorted;
    for (const int index : indices) {
        if (index >= 0 && index < document.objects.size() && !sorted.contains(index)) {
            sorted << index;
        }
    }
    std::sort(sorted.begin(), sorted.end());
    const int first = toFront ? int(document.objects.size() - sorted.size()) : 0;
    // Moving from the far end first leaves the indices still to move unchanged.
    if (toFront) {
        for (qsizetype i = sorted.size() - 1; i >= 0; --i) {
            document.moveObject(sorted.at(i), first + int(i));
        }
    } else {
        for (qsizetype i = 0; i < sorted.size(); ++i) {
            document.moveObject(sorted.at(i), first + int(i));
        }
    }
    QList<int> now;
    for (const int index : indices) {
        now << (sorted.contains(index) ? first + int(sorted.indexOf(index)) : -1);
    }
    return now;
}

} // namespace

QList<int> bringToFront(LabelDocument &document, const QList<int> &indices)
{
    return moveTogether(document, indices, true);
}

QList<int> sendToBack(LabelDocument &document, const QList<int> &indices)
{
    return moveTogether(document, indices, false);
}

void setLocked(LabelDocument &document, const QList<int> &indices, bool locked)
{
    for (const int index : indices) {
        if (index >= 0 && index < document.objects.size()) {
            objectBase(document.objects[index]).locked = locked;
        }
    }
}

} // namespace LabelCreator::Arrange
