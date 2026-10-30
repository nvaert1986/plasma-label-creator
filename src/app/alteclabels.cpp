/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "alteclabels.h"

#include "units.h"

#include <KLocalizedString>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace AltecLabels
{

QList<PaperSize> all()
{
    static const QList<PaperSize> labels = []() {
        QList<PaperSize> list;
        QFile file(u":/plasma-label-creator/data/altec-labels.json"_s);
        if (!file.open(QIODevice::ReadOnly)) {
            return list;
        }
        for (const QJsonValue &value : QJsonDocument::fromJson(file.readAll()).array()) {
            const QJsonObject label = value.toObject();
            const QString product = label.value("product"_L1).toString();
            const qreal width = label.value("width"_L1).toDouble();
            const qreal height = label.value("height"_L1).toDouble();
            const int across = label.value("across"_L1).toInt(1);
            QString name = label.value("round"_L1).toBool() ? i18nc("product, round label diameter", "%1 Ø %2 mm", product, width)
                                                             : i18nc("product, label width x height", "%1 %2 × %3 mm", product, width, height);
            if (across > 1) {
                name = i18nc("label name, labels across the roll", "%1 (%2 across)", name, across);
            }
            PaperSize paper = PaperSize::custom(name, QSizeF(width, height));
            paper.group = product;
            // Wrap labels print on a white strip, the first part off the
            // printer; the clear rest wraps over it. Altec's article codes
            // give its length (24x12x36 mm: 12 mm of a 36 mm label).
            const qreal strip = label.value("print"_L1).toDouble();
            if (strip > 0 && strip < height) {
                paper.printArea = QRectF(0, 0, width * 72.0 / Units::MmPerInch, strip * 72.0 / Units::MmPerInch);
            }
            list << paper;
        }
        return list;
    }();
    return labels;
}

bool isAltec(const QString &text)
{
    // "Altec", or an ATP model as the printer names itself: ATP-600Pro,
    // ATP-23, ATP-3000 (not just any "ATP-" in a queue name).
    static const QRegularExpression model(u"\\bATP-\\d{2,4}"_s, QRegularExpression::CaseInsensitiveOption);
    return text.contains(u"Altec"_s, Qt::CaseInsensitive) || model.match(text).hasMatch();
}

bool fitsLoaded(qreal labelLengthMm, qreal loadedLengthMm)
{
    return labelLengthMm <= loadedLengthMm + 0.5 && labelLengthMm >= loadedLengthMm - 3.0;
}

QList<PaperSize> fitting(const QList<PaperSize> &labels, qreal loadedLengthMm)
{
    QList<PaperSize> list;
    for (const PaperSize &paper : labels) {
        if (fitsLoaded(paper.size.height() * Units::MmPerInch / 72.0, loadedLengthMm)) {
            list << paper;
        }
    }
    return list;
}

} // namespace AltecLabels
