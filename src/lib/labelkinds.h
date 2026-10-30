/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include "labeldocument.h"

#include <optional>

namespace LabelCreator
{

/**
 * Labels of a kind, built from a few values: what DYMO's industrial
 * printers call label applications. Each builds on a blank label of the
 * label type in use: on tape the parts are cells (so DLS opens the label),
 * on a die-cut label they are objects side by side.
 */
namespace LabelKinds
{

/** Text around a cable, repeated so it reads from every side, along the cable. */
struct CableWrap {
    QString text;
    qreal diameterMm = 6;
    LabelFont font;
};

/** Text on both halves of a flag folded around a cable. */
struct CableFlag {
    QString text;
    qreal diameterMm = 6;
    qreal flagLengthMm = 30; // each half
    LabelFont font;
};

enum class Arrow {
    None,
    Up, // ▲ before the text: the label goes under the ports
    Down, // ▼ before the text: the label goes above them
};

/** A row of numbered cells at a fixed pitch: patch panels, terminal blocks, breakers. */
struct Ports {
    int count = 24;
    qreal pitchMm = 16.5;
    int first = 1;
    int step = 1;
    QString prefix;
    int digits = 0; // leading zeros up to this many digits; 0 for none
    bool reversed = false; // numbered from the far end
    bool separators = true;
    Arrow arrow = Arrow::None;
    qreal leadMm = 0; // before the first cell
    qreal tailMm = 0; // after the last
    QStringList texts; // given texts per cell, in place of numbers where set
    bool numberEmptyCells = true; // cells without a given text get their number
    LabelFont font;
};

/** Height units along a rack's rail, each in a box. */
struct RackMarkers {
    int firstUnit = 1;
    int lastUnit = 8;
    qreal unitMm = 44.45; // 1U, 1.75 inch
    bool descending = false; // the highest unit first
    bool boxes = true;
    QString prefix = QStringLiteral("U");
    LabelFont font;
};

LabelDocument cableWrap(const LabelDocument &base, const CableWrap &kind);
LabelDocument cableFlag(const LabelDocument &base, const CableFlag &kind);
LabelDocument ports(const LabelDocument &base, const Ports &kind);
LabelDocument rackMarkers(const LabelDocument &base, const RackMarkers &kind);

/** The text of port @p index (from 0, in tape order) of @p kind. */
QString portText(const Ports &kind, int index);

/** A DYMO Rhino template (.rhino), as far as these labels can be made from it. */
struct RhinoTemplate {
    QString kind; // "PatchPanelLabel", "TerminalBlockLabel", "FixedLabel"
    int tapeMillimetres = 12;
    std::optional<Ports> ports; // patch panels and terminal blocks
    qreal fixedLengthMm = 0; // fixed labels
    QList<QStringList> labels; // the texts it carries: one list of cell texts per label
    LabelFont font;
};

/** Reads a .rhino file's contents; nullopt and @p errorString if it is not one. */
std::optional<RhinoTemplate> readRhinoTemplate(const QByteArray &data, QString *errorString = nullptr);

/**
 * The tape label a Rhino template describes, with the texts of label
 * @p labelIndex of those it carries (numbers where it has none).
 */
LabelDocument fromRhinoTemplate(const RhinoTemplate &rhino, int labelIndex = 0);

} // namespace LabelKinds

} // namespace LabelCreator
