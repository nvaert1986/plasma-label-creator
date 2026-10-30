/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include "labeldocument.h"

#include <QImage>
#include <QList>
#include <QRectF>
#include <QSizeF>
#include <QStringList>

#include <optional>

namespace LabelCreator
{

struct PrinterInfo {
    QString name;
    QString description;
    QString makeAndModel;
    QString deviceUri;
    bool isDefault = false;

    /** Whether the queue drives a DYMO LabelWriter. */
    bool isLabelWriter() const;
    /** Whether the queue drives a DYMO LabelManager (tape) or MobileLabeler. */
    bool isLabelManager() const;
    /**
     * Whether the queue has a label printer driver of its own: DYMO's, or
     * one of CUPS's label drivers ("Zebra ZPL Label Printer", ...). Those
     * are printed to directly; other queues need to be told the model.
     */
    bool hasLabelDriver() const;
    /** Whether the queue's driver speaks ZPL, so the printer can be asked about its labels. */
    bool speaksZpl() const;
};

/** An installed DYMO LabelWriter driver (PPD). */
struct LabelWriterDriver {
    QString ppdName; // as CUPS lists it, e.g. "lw450t.ppd"
    QString modelName; // e.g. "DYMO LabelWriter 450 Turbo"

    bool operator==(const LabelWriterDriver &) const = default;
};

/** A page size offered by a printer's driver (PPD), in points. */
struct PaperSize {
    QString keyword; // e.g. "w79h252"
    QString name; // e.g. "30252 Address", as DLS calls the paper
    QSizeF size;
    QRectF imageableArea; // from the top-left corner of the page
    /** A heading to list the size under, such as the product; empty for driver sizes. */
    QString group;
    /**
     * The part of the label that takes print, in points, across the print
     * head and along the feed like size: a wrap label's silver strip, next
     * to clear film. Empty when the whole label does.
     */
    QRectF printArea;
    /** Millimetres of tape for a LabelManager's tape label types; 0 for other labels. */
    int tape = 0;
    /** How the labels sit on their roll: across, the gap between them, continuous. */
    LabelCreator::RollLayout roll;

    /**
     * A label size the driver does not list, printed with CUPS's custom
     * page size (PageSize=Custom.50x25mm). @p mm is across the print head,
     * then along the feed.
     */
    static PaperSize custom(const QString &name, const QSizeF &mm);
    bool isCustom() const;

    bool operator==(const PaperSize &) const = default;
};

/**
 * Prints labels on a CUPS queue with a DYMO LabelWriter driver. The label is
 * rendered here, at the printer's resolution and in pure black and white,
 * and sent as CUPS raster, so the driver passes it to the print head as is.
 */
class LabelPrinter
{
public:
    struct Options {
        int copies = 1;
        /** DymoPrintQuality: "Text" or "Graphics"; empty keeps the queue's default. */
        QString printQuality;
        /** DymoPrintDensity: "Light", "Medium", "Normal" or "Dark"; empty keeps the default. */
        QString printDensity;
        QString jobTitle;
        /**
         * Extra shift of everything on the label, in points (right and
         * down are positive), to calibrate a printer.
         */
        QPointF offset;
        /**
         * Moves the label up by the driver's unprintable top margin. Prints
         * on a LabelWriter 450 came out that much too low otherwise.
         */
        bool compensateTopMargin = true;
    };

    /**
     * Prints on CUPS queue @p printerName. A queue without the DYMO driver
     * (raw, on another computer, or with another driver) can still print to
     * a LabelWriter: pass the LabelWriter @p driver to use, e.g.
     * "lw450t.ppd". The label is then converted with that driver on this
     * computer and sent to the queue as raw printer data.
     */
    explicit LabelPrinter(const QString &printerName, const QString &driver = {});

    static QList<PrinterInfo> printers();
    /** The LabelWriter drivers CUPS knows, from the installed DYMO driver package. */
    static QList<LabelWriterDriver> installedDrivers();
    /**
     * The driver matching @p hints best: a queue's make and model, its
     * device address (usb://DYMO/LabelWriter%20450%20Turbo?...), a USB
     * product name. The most specific model wins ("450 Turbo" over "450").
     */
    static std::optional<LabelWriterDriver> guessDriver(const QStringList &hints, const QList<LabelWriterDriver> &drivers);
    /**
     * The installed CUPS filter @p name turning CUPS raster into LabelWriter
     * data (raster2dymolw for the 4xx, raster2dymolw_v2 for the 550 series);
     * empty if it is not installed.
     */
    static QString filterProgram(const QString &name = QStringLiteral("raster2dymolw"));
    /** The raster filter DYMO driver @p ppdName uses, from its PPD; empty if unknown. */
    static QString driverFilter(const QString &ppdName);
    /**
     * Whether @p filter has to talk to the printer while converting. The
     * 550 series' filter (raster2dymolw_v2) reads the printer's status before
     * every job over CUPS's back channel, so it only works on a queue that
     * reaches the printer itself (USB or network), never converted here and
     * sent to a raw or shared queue.
     */
    static bool needsDirectConnection(const QString &filter);
    /**
     * Converts the CUPS raster in @p rasterFile into LabelWriter printer data
     * in @p outputFile with the DYMO @p driver, as CUPS would on the queue.
     */
    static bool convertWithDriver(const QString &rasterFile,
                                  const QString &driver,
                                  const PaperSize &paper,
                                  const Options &options,
                                  const QString &outputFile,
                                  QString *errorString = nullptr);

    QString driver() const;

    QString printerName() const;

    /** Reads the queue's driver. Called by print() if needed. */
    bool load();
    QList<PaperSize> paperSizes() const;
    /**
     * Whether the driver prints label sizes it does not list (CUPS custom
     * page sizes). Never for DYMO's drivers: they accept custom sizes, but a
     * LabelWriter only prints well on the DYMO labels its driver lists.
     */
    bool supportsCustomSizes() const;
    /**
     * The shift in paper coordinates (across the print head, along the feed)
     * that moves everything on a label of @p orientation by @p canvasShift as
     * seen on screen (right and down positive). For print offsets.
     */
    static QPointF paperShift(PaperOrientation orientation, const QPointF &canvasShift);
    /** The other way round: what a paper shift looks like on screen. */
    static QPointF canvasShift(PaperOrientation orientation, const QPointF &paperShift);
    /**
     * Where @p paperRect, on paper @p paperSize (across, along; any unit),
     * is on the label as laid out on screen, in the same unit.
     */
    static QRectF canvasRect(PaperOrientation orientation, const QSizeF &paperSize, const QRectF &paperRect);
    /** The other way round. */
    static QRectF paperRect(PaperOrientation orientation, const QSizeF &paperSize, const QRectF &canvasRect);
    /**
     * The page size @p document prints on with this driver: one it lists
     * (matchPaper), else a custom size if the driver takes those. Call load() first.
     */
    std::optional<PaperSize> paperFor(const LabelDocument &document) const;
    /**
     * A LabelManager driver's tapes as label types, one per width (its
     * "Continuous" page sizes); empty for other drivers.
     */
    static QList<PaperSize> tapeTypes(const QList<PaperSize> &driverSizes);
    /** Whether @p modelName (a PPD's) is a DYMO LabelManager's. */
    static bool isLabelManagerModel(const QString &modelName);
    qreal resolution() const;

    /** Prints @p document; returns the CUPS job id, or 0 on failure. */
    int print(const LabelDocument &document, const Options &options);
    /**
     * Prints each of @p labels (all on the same paper) options.copies times
     * in one job. Counters advance with every label printed.
     */
    int print(const QList<LabelDocument> &labels, const Options &options);
    /** Number of labels the last successful print() produced. */
    int printedLabels() const;

    QString errorString() const;
    /** Problems found while rendering the last label. */
    QStringList warnings() const;

    /** The driver's page size for @p document: by DLS paper name, then by size. */
    static std::optional<PaperSize> matchPaper(const LabelDocument &document, const QList<PaperSize> &sizes);

    /**
     * Renders @p document onto @p paper as it feeds through the printer: one
     * row per print-head line, landscape labels turned the way DLS turns
     * them. Black and white only; images are dithered.
     */
    static QImage paperImage(const LabelDocument &document,
                             const PaperSize &paper,
                             qreal dpi,
                             QStringList *warnings = nullptr,
                             int counterStep = 0,
                             const QDateTime &when = {},
                             const QPointF &offset = {});

    /**
     * The pages print() would send for @p labels: one per label printed, as
     * they feed through the printer, with counters advancing; with labels
     * across the roll, one per row of them, side by side. Empty, with
     * errorString() set, if the printer has no paper for the label.
     */
    QList<QImage> pages(const QList<LabelDocument> &labels, const Options &options);
    /** The paper pages() and print() use for @p document. */
    std::optional<PaperSize> paperOf(const LabelDocument &document);

    /** Writes @p page @p copies times as 1-bit CUPS raster to @p fd. */
    static bool writeRaster(int fd, const QImage &page, int copies, const PaperSize &paper, qreal dpi, QString *errorString = nullptr);
    /**
     * Puts @p labels, images of one label each, side by side in rows of
     * @p paper's labels across, with its gap between them.
     */
    static QList<QImage> rowsAcross(const QList<QImage> &labels, const PaperSize &paper, qreal dpi);
    /** Writes @p pages as 1-bit CUPS raster to @p fd. */
    static bool writeRaster(int fd, const QList<QImage> &pages, const PaperSize &paper, qreal dpi, QString *errorString = nullptr);

private:
    int sendThroughLocalDriver(const QString &rasterFile, const PaperSize &paper, const Options &options, const QByteArray &title);

    QString m_printerName;
    QString m_driver;
    QList<PaperSize> m_paperSizes;
    qreal m_resolution = 300;
    bool m_loaded = false;
    bool m_customSizes = false; // the driver takes sizes it does not list
    bool m_dymoOptions = false; // one of DYMO's drivers (LabelWriter or LabelManager)
    bool m_mediaTracking = false; // CUPS's Zebra driver: continuous or gapped media
    int m_pagedLabels = 0; // labels on the pages pages() made last
    int m_printedLabels = 0;
    QString m_errorString;
    QStringList m_warnings;
};

} // namespace LabelCreator
