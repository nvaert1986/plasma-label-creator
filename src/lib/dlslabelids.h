/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

#include <optional>

namespace LabelCreator
{

/**
 * The ids DYMO's own label software writes in a label's <Id> (Address,
 * Small30334), by the paper name the LabelWriter drivers give the label
 * ("30252 Address"). Labels without one keep the driver's page keyword
 * (w79h252), which DYMO's software opens as well.
 */
namespace DlsLabelIds
{

/** DYMO's id for the label called @p paperName; nullopt when it has none. */
std::optional<QString> forPaper(const QString &paperName);

/**
 * Whether @p id is a LabelWriter driver's page keyword (w79h252,
 * w79h252.1, 1933081_Durable_1_in_x_3-1_2_in) rather than an id.
 */
bool isDriverKeyword(const QString &id);

/** The id to write for a label of @p paperName now called @p id: DYMO's for a driver keyword. */
QString preferred(const QString &id, const QString &paperName);

} // namespace DlsLabelIds

} // namespace LabelCreator
