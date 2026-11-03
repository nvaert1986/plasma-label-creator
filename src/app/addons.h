/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KPluginMetaData>

#include <QList>
#include <QStringList>

/**
 * Add-ons: Qt plugins in the "plasma-label-creator" plugin folder (see
 * src/plugin/plasmalabelcreatorplugin.h). None are needed; each one
 * installed is on until it is switched off in the settings.
 */
namespace Addons
{

/** The plugin folder's name, under Qt's plugin folders. */
inline constexpr auto Namespace = "plasma-label-creator";

/** The add-ons installed, switched on or not. */
QList<KPluginMetaData> installed();

/** The ids of the add-ons switched off. */
QStringList switchedOff();
void setSwitchedOff(const QStringList &ids);

} // namespace Addons
