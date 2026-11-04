/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Thumbnails of label files for Dolphin and the file dialogs: the label as
// the app shows it, drawn by the app's own renderer.

#include "labelformat.h"
#include "labelrenderer.h"
#include "units.h"

#include <KIO/ThumbnailCreator>
#include <KPluginFactory>

#include <algorithm>

using namespace LabelCreator;

class LabelThumbnail : public KIO::ThumbnailCreator
{
    Q_OBJECT

public:
    LabelThumbnail(QObject *parent, const QVariantList &args)
        : KIO::ThumbnailCreator(parent, args)
    {
    }

    KIO::ThumbnailResult create(const KIO::ThumbnailRequest &request) override
    {
        if (!request.url().isLocalFile()) {
            return KIO::ThumbnailResult::fail();
        }
        LabelReader reader;
        const auto document = reader.readFile(request.url().toLocalFile());
        if (!document) {
            return KIO::ThumbnailResult::fail();
        }
        // The resolution that makes the label's longer side the size asked
        // for, within reason: a tape label can be very long.
        const QSizeF size = document->paperSize() / Units::TwipsPerInch;
        const qreal longest = std::max(size.width(), size.height());
        if (longest <= 0) {
            return KIO::ThumbnailResult::fail();
        }
        const qreal wanted = std::max(request.targetSize().width(), request.targetSize().height());
        const qreal dpi = std::clamp(wanted / longest, 10.0, 600.0);
        LabelRenderer renderer(LabelRenderer::Mode::Preview);
        QImage image = renderer.renderImage(*document, dpi);
        if (image.isNull()) {
            return KIO::ThumbnailResult::fail();
        }
        image.setDevicePixelRatio(request.devicePixelRatio());
        return KIO::ThumbnailResult::pass(image);
    }
};

K_PLUGIN_CLASS_WITH_JSON(LabelThumbnail, "labelthumbnail.json")

#include "labelthumbnail.moc"
