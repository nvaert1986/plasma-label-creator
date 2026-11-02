/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dlslabelids.h"

#include <QHash>
#include <QRegularExpression>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator::DlsLabelIds
{

namespace
{

// Paper names from DYMO's LabelWriter drivers (their PPDs' PageSize names),
// with the id DYMO's software gives the same label. Where the names differ
// (European product numbers, older names) the label's size and kind match.
const QHash<QString, QString> &table()
{
    static const QHash<QString, QString> ids = {
        {u"1/5 File"_s, u"SuspensionFile99017"_s},
        {u"11352 Return Address Int"_s, u"ReturnAddressInt"_s},
        {u"11353 Multi-Purpose"_s, u"Small30332"_s},
        {u"11354 Multi-Purpose"_s, u"Small30334"_s},
        {u"11355 Multi-Purpose"_s, u"MultiPurpose11355"_s},
        {u"11356 White Name badge"_s, u"WhiteNameBadge11356"_s},
        {u"1744907 4 in x 6 in"_s, u"Shipping4x6"_s},
        {u"1933081 Durable 1 in x 3-1/2 in"_s, u"LW_DURABLE_25X89mm"_s},
        {u"1933083 Durable 1 in x 1 in"_s, u"LW_DURABLE_25X25mm"_s},
        {u"1933084 Durable 2-1/4 in x 1-1/4 in"_s, u"LW_DURABLE_57X32mm"_s},
        {u"1933085 Durable 3/4 in x 2-1/2 in"_s, u"LW_DURABLE_19X64mm"_s},
        {u"1933086 Durable 4 in x 6 in"_s, u"LW_DURABLE_104X159mm"_s},
        {u"1933087 Durable 2-5/16 in x 7-1/2 in"_s, u"LW_DURABLE_59X190mm"_s},
        {u"1933088 Durable 2-5/16 in x 4 in"_s, u"LW_DURABLE_59X102mm"_s},
        {u"30252 Address"_s, u"Address"_s},
        {u"30253 Address (2 up)"_s, u"Address2up"_s},
        {u"30256 Shipping"_s, u"LargeShipping"_s},
        {u"30258 Diskette"_s, u"Storage"_s},
        {u"30277 File Folder (2 up)"_s, u"FileFolder2up"_s},
        {u"30299 Jewelry Label (2 up)"_s, u"Jewelry30299"_s},
        {u"30320 Address"_s, u"Address"_s},
        {u"30321 Large Address"_s, u"LargeAddress"_s},
        {u"30323 Shipping"_s, u"Shipping"_s},
        {u"30324 Diskette"_s, u"Storage"_s},
        {u"30325 Video Spine"_s, u"Binder"_s},
        {u"30326 Video Top"_s, u"Prescription"_s},
        {u"30327 File Folder"_s, u"FileFolder"_s},
        {u"30330 Return Address"_s, u"ReturnAddress"_s},
        {u"30332 1 in x 1 in"_s, u"Small30332"_s},
        {u"30333 1/2 in x 1 in (2 up)"_s, u"Small30333"_s},
        {u"30334 2-1/4 in x 1-1/4 in"_s, u"Small30334"_s},
        {u"30335 1/2 in x 1/2 in (4 up)"_s, u"Small30335"_s},
        {u"30336 1 in x 2-1/8 in"_s, u"Small30336"_s},
        {u"30337 Audio Cassette"_s, u"AudioCassette30337"_s},
        {u"30339 8mm Video (2 up)"_s, u"Video8mm30339"_s},
        {u"30345 3/4 in x 2-1/2 in"_s, u"DYMOFile"_s},
        {u"30346 1/2 in x 1-7/8 in"_s, u"Small30346"_s},
        {u"30347 1 in x 1-1/2 in"_s, u"Small30347"_s},
        {u"30365 Name Badge Card"_s, u"NameBadgeTag"_s},
        {u"30370 Zip Disk"_s, u"ZipDisk"_s},
        {u"30373 Price Tag Label"_s, u"PriceTag"_s},
        {u"30374 Appointment Card"_s, u"Appointment"_s},
        {u"30376 Hanging File Insert"_s, u"HangingFileInsert"_s},
        {u"30854 CD Label"_s, u"CDRing"_s},
        {u"30856 Badge Card Label"_s, u"NameBadgeCard"_s},
        {u"30886 CD Label"_s, u"CDRing30886"_s},
        {u"99010 Standard Address"_s, u"Address"_s},
        {u"99012 Large Address"_s, u"LargeAddress"_s},
        {u"99014 Name Badge Label"_s, u"Shipping"_s},
        {u"99014 Shipping"_s, u"Shipping"_s},
        {u"99015 Diskette"_s, u"NameBadge99015"_s},
        {u"99017 Suspension File"_s, u"SuspensionFile99017"_s},
        {u"99018 Small Lever Arch"_s, u"SmallLeverArch"_s},
        {u"99019 Large Lever Arch"_s, u"LargeLeverArch99019"_s},
        {u"Address Label (Horizontal)"_s, u"AddressHorizontal"_s},
        {u"Shipping Label (Horizontal)"_s, u"ShippingHorizontal"_s},
        {u"Small Pendaflex"_s, u"SuspensionFile99017"_s},
    };
    return ids;
}

} // namespace

std::optional<QString> forPaper(const QString &paperName)
{
    const auto it = table().constFind(paperName.trimmed());
    return it == table().cend() ? std::nullopt : std::optional(*it);
}

bool isDriverKeyword(const QString &id)
{
    // w<width>h<height> in points, perhaps with a variant; or the 550
    // drivers' product number and name joined with underscores.
    static const QRegularExpression keyword(u"^(w\\d+h\\d+(\\.\\d+)?|\\d{5,7}_[A-Za-z0-9_.-]+)$"_s);
    return keyword.match(id).hasMatch();
}

QString preferred(const QString &id, const QString &paperName)
{
    if (!id.isEmpty() && !isDriverKeyword(id)) {
        return id;
    }
    return forPaper(paperName).value_or(id);
}

} // namespace LabelCreator::DlsLabelIds
