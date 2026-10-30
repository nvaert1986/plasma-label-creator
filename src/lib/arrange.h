/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QList>

namespace LabelCreator::Arrange
{

/** Whether object @p index is in a group (DLS writes -1 for none). */
bool isGrouped(const LabelDocument &document, int index);

/** @p indices and every other member of their groups, sorted. */
QList<int> withGroups(const LabelDocument &document, const QList<int> &indices);

/** How many things @p indices are that move as one: groups and objects on their own. */
int unitCount(const LabelDocument &document, const QList<int> &indices);

/** Whether any of @p indices can be moved: some are not locked. */
bool canMove(const LabelDocument &document, const QList<int> &indices);

/**
 * Aligns the objects @p indices to each other: along the edge or centre of
 * the box around all of them, as @p alignment says (a horizontal flag, a
 * vertical one or both). A group moves as one; locked objects stay and the
 * others line up with them. False if nothing moved.
 */
bool align(LabelDocument &document, const QList<int> &indices, Qt::Alignment alignment);

/**
 * Spaces the objects @p indices evenly between the first and the last in
 * @p orientation, which stay. Needs three objects or groups; a group counts
 * as one and locked objects stay. False if nothing moved.
 */
bool distribute(LabelDocument &document, const QList<int> &indices, Qt::Orientation orientation);

/** Puts @p indices, with the groups they are in, into one new group; returns its id. */
int group(LabelDocument &document, const QList<int> &indices);

/** Takes @p indices, and every member of their groups, out of their groups. */
void ungroup(LabelDocument &document, const QList<int> &indices);

/**
 * Moves @p indices on top of the other objects, keeping their order among
 * themselves; returns where each one is now, in the order of @p indices.
 */
QList<int> bringToFront(LabelDocument &document, const QList<int> &indices);

/** As bringToFront(), below the other objects. */
QList<int> sendToBack(LabelDocument &document, const QList<int> &indices);

/** Locks or unlocks @p indices against moving and resizing. */
void setLocked(LabelDocument &document, const QList<int> &indices, bool locked);

} // namespace LabelCreator::Arrange
