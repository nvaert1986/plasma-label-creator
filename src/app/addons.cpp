/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "addons.h"

#include <KConfigGroup>
#include <KSharedConfig>

using namespace Qt::Literals::StringLiterals;

namespace Addons
{

QList<KPluginMetaData> installed()
{
    return KPluginMetaData::findPlugins(QLatin1StringView(Namespace));
}

static KConfigGroup group()
{
    return KSharedConfig::openConfig()->group(u"Add-ons"_s);
}

QStringList switchedOff()
{
    return group().readEntry("SwitchedOff", QStringList());
}

void setSwitchedOff(const QStringList &ids)
{
    KConfigGroup config = group();
    config.writeEntry("SwitchedOff", ids);
    config.sync();
}

} // namespace Addons
