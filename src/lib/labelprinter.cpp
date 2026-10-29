/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "labelprinter.h"

#include "labelrenderer.h"
#include "tapelayout.h"
#include "units.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>
#include <QPainter>

#include <cups/cups.h>
#include <cups/raster.h>

// The DYMO drivers are PPD-based, so their page sizes and options only
// exist in the PPD API, which CUPS has marked deprecated since 1.6.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <cups/ppd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unistd.h>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace
{

// Paper sizes in a label file are in twips; drivers round them to points.
constexpr qreal PaperSizeTolerance = 1.5; // points

} // namespace

bool PrinterInfo::isLabelWriter() const
{
    return makeAndModel.contains(u"LabelWriter"_s, Qt::CaseInsensitive);
}

bool PrinterInfo::isLabelManager() const
{
    return LabelPrinter::isLabelManagerModel(makeAndModel);
}

bool LabelPrinter::isLabelManagerModel(const QString &modelName)
{
    return modelName.contains(u"LabelManager"_s, Qt::CaseInsensitive) || modelName.contains(u"MobileLabeler"_s, Qt::CaseInsensitive);
}

bool PrinterInfo::hasLabelDriver() const
{
    // CUPS's own label drivers are all called "... Label Printer".
    return isLabelWriter() || isLabelManager() || makeAndModel.contains(u"Label Printer"_s, Qt::CaseInsensitive);
}

bool PrinterInfo::speaksZpl() const
{
    return makeAndModel.contains(u"ZPL"_s);
}

PaperSize PaperSize::custom(const QString &name, const QSizeF &mm)
{
    PaperSize paper;
    paper.keyword = u"Custom.%1x%2mm"_s.arg(mm.width()).arg(mm.height());
    paper.name = name;
    paper.size = mm * 72.0 / Units::MmPerInch;
    paper.imageableArea = QRectF(QPointF(0, 0), paper.size);
    return paper;
}

bool PaperSize::isCustom() const
{
    return keyword.startsWith(u"Custom."_s);
}

LabelPrinter::LabelPrinter(const QString &printerName, const QString &driver)
    : m_printerName(printerName)
    , m_driver(driver)
{
}

QString LabelPrinter::driver() const
{
    return m_driver;
}

QList<LabelWriterDriver> LabelPrinter::installedDrivers()
{
    // CUPS-Get-PPDs, as lpinfo -m does; unlike listing devices it needs no
    // administrator rights.
    ipp_t *request = ippNewRequest(IPP_OP_CUPS_GET_PPDS);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_TEXT, "ppd-make", nullptr, "DYMO");
    static const char *const attributes[] = {"ppd-name", "ppd-make-and-model"};
    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", 2, nullptr, attributes);
    ipp_t *response = cupsDoRequest(CUPS_HTTP_DEFAULT, request, "/");

    QList<LabelWriterDriver> drivers;
    LabelWriterDriver driver;
    const auto finish = [&]() {
        if (!driver.ppdName.isEmpty() && (driver.modelName.contains(u"LabelWriter"_s, Qt::CaseInsensitive) || isLabelManagerModel(driver.modelName))) {
            drivers << driver;
        }
        driver = LabelWriterDriver();
    };
    for (ipp_attribute_t *attribute = ippFirstAttribute(response); attribute; attribute = ippNextAttribute(response)) {
        const char *name = ippGetName(attribute);
        if (!name) {
            finish(); // the next PPD starts
        } else if (std::strcmp(name, "ppd-name") == 0) {
            driver.ppdName = QString::fromUtf8(ippGetString(attribute, 0, nullptr));
        } else if (std::strcmp(name, "ppd-make-and-model") == 0) {
            driver.modelName = QString::fromUtf8(ippGetString(attribute, 0, nullptr));
        }
    }
    finish();
    ippDelete(response);

    std::sort(drivers.begin(), drivers.end(), [](const LabelWriterDriver &a, const LabelWriterDriver &b) {
        return QString::localeAwareCompare(a.modelName, b.modelName) < 0;
    });
    return drivers;
}

std::optional<LabelWriterDriver> LabelPrinter::guessDriver(const QStringList &hints, const QList<LabelWriterDriver> &drivers)
{
    // Compare letters and digits only: "usb://DYMO/LabelWriter%20450%20Turbo"
    // and "DYMO LabelWriter 450 Turbo" both become "labelwriter450turbo".
    const auto normalize = [](const QString &text) {
        // A named copy: remove() returns a reference into its temporary,
        // which a range-for would not keep alive.
        QString decoded = QUrl::fromPercentEncoding(text.toUtf8()).toLower();
        decoded.remove(u"dymo"_s);
        QString result;
        for (const QChar c : std::as_const(decoded)) {
            if (c.isLetterOrNumber()) {
                result += c;
            }
        }
        return result;
    };
    QStringList normalized;
    for (const QString &hint : hints) {
        normalized << normalize(hint);
    }

    std::optional<LabelWriterDriver> best;
    qsizetype bestLength = 0;
    for (const LabelWriterDriver &driver : drivers) {
        const QString model = normalize(driver.modelName);
        if (model.size() <= bestLength) {
            continue;
        }
        if (std::any_of(normalized.cbegin(), normalized.cend(), [&model](const QString &hint) {
                return hint.contains(model);
            })) {
            best = driver;
            bestLength = model.size();
        }
    }
    return best;
}

namespace
{

// The program the PPD at @p ppdFile runs on CUPS raster, from its *cupsFilter lines.
QString rasterFilter(const char *ppdFile)
{
    ppd_file_t *ppd = ppdOpenFile(ppdFile);
    if (!ppd) {
        return {};
    }
    QString program;
    for (int i = 0; i < ppd->num_filters; ++i) {
        // "application/vnd.cups-raster 0 raster2dymolw_v2"
        const QStringList fields = QString::fromUtf8(ppd->filters[i]).simplified().split(u' ');
        if (fields.size() >= 3 && fields.constFirst() == "application/vnd.cups-raster"_L1) {
            program = QFileInfo(fields.constLast()).fileName();
        }
    }
    ppdClose(ppd);
    return program;
}

} // namespace

QString LabelPrinter::driverFilter(const QString &ppdName)
{
    const char *ppdFile = cupsGetServerPPD(CUPS_HTTP_DEFAULT, ppdName.toUtf8().constData());
    if (!ppdFile) {
        return {};
    }
    const QString filter = rasterFilter(ppdFile);
    unlink(ppdFile);
    return filter;
}

bool LabelPrinter::needsDirectConnection(const QString &filter)
{
    return filter.endsWith(u"_v2"_s);
}

QString LabelPrinter::filterProgram(const QString &name)
{
    QStringList directories;
    if (qEnvironmentVariableIsSet("CUPS_SERVERBIN")) {
        directories << qEnvironmentVariable("CUPS_SERVERBIN");
    }
    directories << u"/usr/libexec/cups"_s << u"/usr/lib/cups"_s << u"/usr/lib64/cups"_s;
    for (const QString &directory : std::as_const(directories)) {
        const QString program = directory + u"/filter/"_s + name;
        if (QFileInfo(program).isExecutable()) {
            return program;
        }
    }
    return {};
}

QList<PrinterInfo> LabelPrinter::printers()
{
    cups_dest_t *dests = nullptr;
    const int count = cupsGetDests2(CUPS_HTTP_DEFAULT, &dests);

    QList<PrinterInfo> printers;
    for (int i = 0; i < count; ++i) {
        const cups_dest_t &dest = dests[i];
        if (dest.instance) {
            continue;
        }
        PrinterInfo info;
        info.name = QString::fromUtf8(dest.name);
        info.description = QString::fromUtf8(cupsGetOption("printer-info", dest.num_options, dest.options));
        info.makeAndModel = QString::fromUtf8(cupsGetOption("printer-make-and-model", dest.num_options, dest.options));
        info.deviceUri = QString::fromUtf8(cupsGetOption("device-uri", dest.num_options, dest.options));
        info.isDefault = dest.is_default;
        printers.append(info);
    }
    cupsFreeDests(count, dests);
    return printers;
}

QString LabelPrinter::printerName() const
{
    return m_printerName;
}

bool LabelPrinter::load()
{
    m_paperSizes.clear();
    m_loaded = false;

    // The queue's own driver, or the LabelWriter driver chosen for it.
    const QByteArray name = (m_driver.isEmpty() ? m_printerName : m_driver).toUtf8();
    const char *ppdFile = m_driver.isEmpty() ? cupsGetPPD2(CUPS_HTTP_DEFAULT, name.constData()) : cupsGetServerPPD(CUPS_HTTP_DEFAULT, name.constData());
    if (!ppdFile) {
        m_errorString = u"Could not get the driver of %1: %2"_s.arg(m_printerName, QString::fromUtf8(cupsLastErrorString()));
        return false;
    }
    ppd_file_t *ppd = ppdOpenFile(ppdFile);
    unlink(ppdFile);
    if (!ppd) {
        m_errorString = u"Could not read the driver of %1"_s.arg(m_printerName);
        return false;
    }
    // Opening a PPD marks nothing: without this, no resolution is "marked"
    // and every printer would be taken for 300 dpi, a 600 dpi Altec too.
    ppdMarkDefaults(ppd);

    // DYMO's drivers, LabelWriter and LabelManager alike, have DYMO's halftoning.
    m_dymoOptions = ppdFindOption(ppd, "DymoHalftoning") != nullptr;
    m_customSizes = ppd->variable_sizes && !m_dymoOptions;
    const ppd_option_t *pageSize = ppdFindOption(ppd, "PageSize");
    for (int i = 0; i < ppd->num_sizes; ++i) {
        const ppd_size_t &size = ppd->sizes[i];
        PaperSize paper;
        paper.keyword = QString::fromUtf8(size.name);
        paper.name = paper.keyword;
        for (int c = 0; pageSize && c < pageSize->num_choices; ++c) {
            if (std::strcmp(pageSize->choices[c].choice, size.name) == 0) {
                paper.name = QString::fromUtf8(pageSize->choices[c].text);
                break;
            }
        }
        paper.size = QSizeF(size.width, size.length);
        // PPD imageable areas are measured from the bottom-left corner.
        paper.imageableArea = QRectF(QPointF(size.left, size.length - size.top), QPointF(size.right, size.length - size.bottom));
        m_paperSizes.append(paper);
    }

    if (const ppd_choice_t *resolution = ppdFindMarkedChoice(ppd, "Resolution")) {
        const qreal dpi = QString::fromUtf8(resolution->choice).section(u'x', 0, 0).remove(u"dpi"_s).toDouble();
        if (dpi > 0) {
            m_resolution = dpi;
        }
    }

    ppdClose(ppd);
    m_loaded = true;
    return true;
}

QList<PaperSize> LabelPrinter::paperSizes() const
{
    return m_paperSizes;
}

bool LabelPrinter::supportsCustomSizes() const
{
    return m_customSizes;
}

QPointF LabelPrinter::paperShift(PaperOrientation orientation, const QPointF &canvasShift)
{
    // Landscape labels are drawn turned: canvas (x, y) is paper (width - y, x).
    return orientation == PaperOrientation::Landscape ? QPointF(-canvasShift.y(), canvasShift.x()) : canvasShift;
}

QPointF LabelPrinter::canvasShift(PaperOrientation orientation, const QPointF &paperShift)
{
    return orientation == PaperOrientation::Landscape ? QPointF(paperShift.y(), -paperShift.x()) : paperShift;
}

QRectF LabelPrinter::canvasRect(PaperOrientation orientation, const QSizeF &paperSize, const QRectF &paperRect)
{
    if (orientation != PaperOrientation::Landscape) {
        return paperRect;
    }
    // Paper (x, y) is canvas (y, width - x).
    return QRectF(paperRect.y(), paperSize.width() - paperRect.right(), paperRect.height(), paperRect.width());
}

QRectF LabelPrinter::paperRect(PaperOrientation orientation, const QSizeF &paperSize, const QRectF &canvasRect)
{
    if (orientation != PaperOrientation::Landscape) {
        return canvasRect;
    }
    return QRectF(paperSize.width() - canvasRect.bottom(), canvasRect.x(), canvasRect.height(), canvasRect.width());
}

QList<PaperSize> LabelPrinter::tapeTypes(const QList<PaperSize> &driverSizes)
{
    QList<PaperSize> tapes;
    for (const PaperSize &size : driverSizes) {
        // "12 mm (1/2\") Continuous": the tape, as long as needed.
        if (!size.name.contains(u"Continuous"_s, Qt::CaseInsensitive)) {
            continue;
        }
        const auto millimetres = TapeLayout::millimetresOf(size.name);
        if (!millimetres || !TapeLayout::tape(*millimetres)) {
            continue;
        }
        PaperSize tape = size;
        tape.name = u"%1 mm tape"_s.arg(*millimetres);
        tape.tape = *millimetres;
        tapes << tape;
    }
    return tapes;
}

std::optional<PaperSize> LabelPrinter::paperFor(const LabelDocument &document) const
{
    // A tape label: the tape's page size, as long as the label.
    if (document.tape) {
        auto tape = matchPaper(document, tapeTypes(m_paperSizes));
        if (tape) {
            tape->size = QSizeF(tape->size.width(), Units::twipsToPoints(document.tape->length));
            tape->imageableArea = QRectF(QPointF(0, 0), tape->size);
        }
        return tape;
    }
    if (const auto paper = matchPaper(document, m_paperSizes)) {
        return paper;
    }
    // A label the driver does not list, such as Altec's labels on CUPS's
    // Zebra driver: printed at its own size, to a tenth of a millimetre.
    if (!m_customSizes) {
        return std::nullopt;
    }
    const QSizeF twips = document.paperSize();
    const auto tenths = [](qreal value) {
        return std::round(Units::twipsToMm(value) * 10) / 10;
    };
    return PaperSize::custom(document.paperName, QSizeF(tenths(twips.width()), tenths(twips.height())));
}

qreal LabelPrinter::resolution() const
{
    return m_resolution;
}

QString LabelPrinter::errorString() const
{
    return m_errorString;
}

QStringList LabelPrinter::warnings() const
{
    return m_warnings;
}

std::optional<PaperSize> LabelPrinter::matchPaper(const LabelDocument &document, const QList<PaperSize> &sizes)
{
    // A tape label matches its tape, whatever it is called.
    if (document.tape) {
        const auto millimetres = TapeLayout::millimetresOf(document.id).value_or(TapeLayout::millimetresOf(document.paperName).value_or(0));
        for (const PaperSize &paper : sizes) {
            if (paper.tape > 0 && paper.tape == millimetres) {
                return paper;
            }
        }
        return std::nullopt;
    }
    for (const PaperSize &paper : sizes) {
        if (!document.paperName.isEmpty() && paper.name.compare(document.paperName, Qt::CaseInsensitive) == 0) {
            return paper;
        }
    }
    const QSizeF wanted = document.paperSize() / Units::TwipsPerPoint;
    for (const PaperSize &paper : sizes) {
        if (std::abs(paper.size.width() - wanted.width()) <= PaperSizeTolerance && std::abs(paper.size.height() - wanted.height()) <= PaperSizeTolerance) {
            return paper;
        }
    }
    return std::nullopt;
}

QImage LabelPrinter::paperImage(const LabelDocument &document,
                                const PaperSize &paper,
                                qreal dpi,
                                QStringList *warnings,
                                int counterStep,
                                const QDateTime &when,
                                const QPointF &offset)
{
    const QSize size(qRound(paper.size.width() * dpi / 72.0), qRound(paper.size.height() * dpi / 72.0));
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.scale(Units::twipsToDots(1.0, dpi), Units::twipsToDots(1.0, dpi));
    // The offset is along the paper as it feeds: x across the head, y along
    // the feed.
    painter.translate(Units::pointsToTwips(offset.x()), Units::pointsToTwips(offset.y()));
    if (document.orientation == PaperOrientation::Landscape) {
        // DLS lays landscape labels out with the paper's leading edge on the
        // left: canvas (x, y) is paper (width - y, x).
        painter.translate(document.paperSize().width(), 0);
        painter.rotate(90);
    }
    LabelRenderer renderer(LabelRenderer::Mode::Print);
    renderer.setPrinterResolution(dpi);
    renderer.setCounterStep(counterStep);
    renderer.setDateTime(when);
    renderer.paint(&painter, document);
    painter.end();

    if (warnings) {
        *warnings = renderer.warnings();
    }
    // Text, barcodes and shapes are already pure black and white and come
    // through unchanged; only images get dithered.
    return image.convertToFormat(QImage::Format_Mono, Qt::MonoOnly | Qt::DiffuseDither);
}

bool LabelPrinter::writeRaster(int fd, const QImage &page, int copies, const PaperSize &paper, qreal dpi, QString *errorString)
{
    return writeRaster(fd, QList<QImage>(std::max(1, copies), page), paper, dpi, errorString);
}

bool LabelPrinter::writeRaster(int fd, const QList<QImage> &pages, const PaperSize &paper, qreal dpi, QString *errorString)
{
    if (pages.isEmpty()) {
        if (errorString) {
            *errorString = u"Nothing to print"_s;
        }
        return false;
    }
    const QImage &first = pages.constFirst();
    cups_raster_t *raster = cupsRasterOpen(fd, CUPS_RASTER_WRITE);
    if (!raster) {
        if (errorString) {
            *errorString = QString::fromUtf8(cupsRasterErrorString());
        }
        return false;
    }

    cups_page_header2_t header;
    std::memset(&header, 0, sizeof(header));
    const QByteArray keyword = paper.keyword.toUtf8();
    std::strncpy(header.cupsPageSizeName, keyword.constData(), sizeof(header.cupsPageSizeName) - 1);
    header.HWResolution[0] = header.HWResolution[1] = unsigned(qRound(dpi));
    header.PageSize[0] = unsigned(qRound(paper.size.width()));
    header.PageSize[1] = unsigned(qRound(paper.size.height()));
    header.ImagingBoundingBox[2] = header.PageSize[0];
    header.ImagingBoundingBox[3] = header.PageSize[1];
    header.cupsPageSize[0] = float(paper.size.width());
    header.cupsPageSize[1] = float(paper.size.height());
    header.NumCopies = 1;
    header.cupsWidth = unsigned(first.width());
    header.cupsHeight = unsigned(first.height());
    header.cupsBitsPerColor = 1;
    header.cupsBitsPerPixel = 1;
    header.cupsBytesPerLine = (header.cupsWidth + 7) / 8;
    header.cupsColorOrder = CUPS_ORDER_CHUNKED;
    header.cupsColorSpace = CUPS_CSPACE_K; // 1 is black
    // LabelWriters: die-cut, 0. LabelManagers read the tape from the low
    // byte (6 to 24 mm: 0 to 4); its high byte would make the driver cut
    // after the last printed line, which is left off: the label is exactly
    // as long as it is laid out, margins included.
    header.cupsMediaType = 0;
    if (paper.tape > 0) {
        if (const auto tape = TapeLayout::tape(paper.tape)) {
            header.cupsMediaType = unsigned(tape->mediaType);
        }
    }

    // One print-head line per row, leftmost dot in the high bit.
    QByteArray line(int(header.cupsBytesPerLine), '\0');
    const QRgb black = qRgb(0, 0, 0);
    bool ok = true;
    for (const QImage &page : pages) {
        if (!ok) {
            break;
        }
        ok = cupsRasterWriteHeader2(raster, &header);
        for (int y = 0; ok && y < page.height(); ++y) {
            line.fill('\0');
            for (int x = 0; x < page.width(); ++x) {
                if (page.pixel(x, y) == black) {
                    line[x >> 3] = char(uchar(line.at(x >> 3)) | (0x80 >> (x & 7)));
                }
            }
            ok = cupsRasterWritePixels(raster, reinterpret_cast<unsigned char *>(line.data()), header.cupsBytesPerLine) == header.cupsBytesPerLine;
        }
    }
    cupsRasterClose(raster);

    if (!ok && errorString) {
        *errorString = u"Could not write the print data: %1"_s.arg(QString::fromUtf8(cupsRasterErrorString()));
    }
    return ok;
}

int LabelPrinter::printedLabels() const
{
    return m_printedLabels;
}

int LabelPrinter::print(const LabelDocument &document, const Options &options)
{
    return print(QList<LabelDocument>{document}, options);
}

int LabelPrinter::print(const QList<LabelDocument> &labels, const Options &options)
{
    m_errorString.clear();
    m_warnings.clear();
    m_printedLabels = 0;
    if (labels.isEmpty()) {
        m_errorString = u"Nothing to print"_s;
        return 0;
    }
    if (!m_loaded && !load()) {
        return 0;
    }

    const LabelDocument &document = labels.constFirst();
    const auto paper = paperFor(document);
    if (!paper) {
        m_errorString = u"%1 has no paper size for \"%2\""_s.arg(m_printerName, document.paperName);
        return 0;
    }

    // One page per label printed. Labels without counters look the same in
    // every copy and are rendered once.
    const QDateTime when = QDateTime::currentDateTime();
    QPointF offset = options.offset;
    if (options.compensateTopMargin) {
        offset.ry() -= paper->imageableArea.top();
    }
    QList<QImage> pages;
    int step = 0;
    for (const LabelDocument &label : labels) {
        const bool counts = std::any_of(label.objects.cbegin(), label.objects.cend(), [](const LabelObject &object) {
            return std::holds_alternative<CounterObject>(object);
        });
        QImage page;
        for (int copy = 0; copy < std::max(1, options.copies); ++copy, ++step) {
            if (page.isNull() || counts) {
                QStringList warnings;
                page = paperImage(label, *paper, m_resolution, &warnings, step, when, offset);
                for (const QString &warning : std::as_const(warnings)) {
                    if (!m_warnings.contains(warning)) {
                        m_warnings << warning;
                    }
                }
            }
            pages << page;
        }
    }

    char fileName[1024];
    const int fd = cupsTempFd(fileName, sizeof(fileName));
    if (fd < 0) {
        m_errorString = u"Could not create a temporary file for the print job"_s;
        return 0;
    }
    const bool written = writeRaster(fd, pages, *paper, m_resolution, &m_errorString);
    close(fd);
    if (!written) {
        unlink(fileName);
        return 0;
    }

    cups_option_t *cupsOptions = nullptr;
    int optionCount = 0;
    optionCount = cupsAddOption("PageSize", paper->keyword.toUtf8().constData(), optionCount, &cupsOptions);
    // DYMO's own options, for DYMO's drivers (or when converting with one here).
    if (m_dymoOptions || !m_driver.isEmpty()) {
        if (!options.printQuality.isEmpty()) {
            optionCount = cupsAddOption("DymoPrintQuality", options.printQuality.toUtf8().constData(), optionCount, &cupsOptions);
        }
        if (!options.printDensity.isEmpty()) {
            optionCount = cupsAddOption("DymoPrintDensity", options.printDensity.toUtf8().constData(), optionCount, &cupsOptions);
        }
        // The raster is already black and white; keep the driver from dithering.
        optionCount = cupsAddOption("DymoHalftoning", "Default", optionCount, &cupsOptions);
    }

    const QByteArray title = (options.jobTitle.isEmpty() ? document.paperName : options.jobTitle).toUtf8();
    if (!m_driver.isEmpty()) {
        cupsFreeOptions(optionCount, cupsOptions);
        const int job = sendThroughLocalDriver(QString::fromLocal8Bit(fileName), *paper, options, title);
        unlink(fileName);
        if (job > 0) {
            m_printedLabels = int(pages.size());
        }
        return job;
    }
    const int job = cupsPrintFile2(CUPS_HTTP_DEFAULT, m_printerName.toUtf8().constData(), fileName, title.constData(), optionCount, cupsOptions);
    cupsFreeOptions(optionCount, cupsOptions);
    unlink(fileName);

    if (job == 0) {
        m_errorString = u"Printing on %1 failed: %2"_s.arg(m_printerName, QString::fromUtf8(cupsLastErrorString()));
    } else {
        m_printedLabels = int(pages.size());
    }
    return job;
}

bool LabelPrinter::convertWithDriver(const QString &rasterFile,
                                     const QString &driver,
                                     const PaperSize &paper,
                                     const Options &options,
                                     const QString &outputFile,
                                     QString *errorString)
{
    const auto fail = [errorString](const QString &message) {
        if (errorString) {
            *errorString = message;
        }
        return false;
    };
    const char *ppdFile = cupsGetServerPPD(CUPS_HTTP_DEFAULT, driver.toUtf8().constData());
    if (!ppdFile) {
        return fail(u"Could not get the driver %1: %2"_s.arg(driver, QString::fromUtf8(cupsLastErrorString())));
    }
    // The driver says which filter it uses: raster2dymolw for the 4xx,
    // raster2dymolw_v2 for the 550 series.
    const QString filterName = rasterFilter(ppdFile);
    if (needsDirectConnection(filterName)) {
        unlink(ppdFile);
        return fail(u"The LabelWriter 550 series cannot print through this queue. Its DYMO driver reads the printer's status before every label, "
                    "which only works when the printer is set up in CUPS with that driver itself, over USB or the network, not through a raw "
                    "or shared queue"_s);
    }
    const QString filter = filterProgram(filterName.isEmpty() ? u"raster2dymolw"_s : filterName);
    if (filter.isEmpty()) {
        unlink(ppdFile);
        return fail(u"The DYMO printer driver (%1) is not installed on this computer"_s.arg(filterName.isEmpty() ? u"raster2dymolw"_s : filterName));
    }

    // Run the filter as CUPS would: job, user, title, copies, options, with
    // the raster on standard input and the printer data on standard output.
    QStringList filterOptions = {u"PageSize="_s + paper.keyword, u"DymoHalftoning=Default"_s};
    if (!options.printQuality.isEmpty()) {
        filterOptions << u"DymoPrintQuality="_s + options.printQuality;
    }
    if (!options.printDensity.isEmpty()) {
        filterOptions << u"DymoPrintDensity="_s + options.printDensity;
    }
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(u"PPD"_s, QString::fromLocal8Bit(ppdFile));
    // Without a usb:// device the driver does not wait for status from the
    // printer over CUPS's back channel, which nothing provides here.
    environment.insert(u"DEVICE_URI"_s, u"file:///dev/null"_s);
    process.setProcessEnvironment(environment);
    process.setStandardInputFile(rasterFile);
    process.setStandardOutputFile(outputFile);
    process.start(filter, {u"1"_s, QString::fromUtf8(cupsUser()), options.jobTitle, u"1"_s, filterOptions.join(u' ')});
    const bool finished = process.waitForFinished(60000);
    unlink(ppdFile);
    if (!finished || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || QFileInfo(outputFile).size() == 0) {
        // The filter reports on standard error in CUPS's "LEVEL: message" form.
        QStringList errors;
        for (const QString &line : QString::fromLocal8Bit(process.readAllStandardError()).split(u'\n')) {
            if (line.startsWith(u"ERROR:"_s)) {
                errors << line.mid(6).trimmed();
            }
        }
        return fail(u"The DYMO driver could not convert the label. %1"_s.arg(errors.join(u' ')));
    }
    return true;
}

int LabelPrinter::sendThroughLocalDriver(const QString &rasterFile, const PaperSize &paper, const Options &options, const QByteArray &title)
{
    char outputName[1024];
    const int outputFd = cupsTempFd(outputName, sizeof(outputName));
    if (outputFd < 0) {
        m_errorString = u"Could not create a temporary file for the print job"_s;
        return 0;
    }
    close(outputFd);
    Options named = options;
    named.jobTitle = QString::fromUtf8(title);
    if (!convertWithDriver(rasterFile, m_driver, paper, named, QString::fromLocal8Bit(outputName), &m_errorString)) {
        unlink(outputName);
        return 0;
    }

    // Already printer data: the queue must pass it on untouched.
    cups_option_t *rawOptions = nullptr;
    const int rawCount = cupsAddOption("raw", "true", 0, &rawOptions);
    const int job = cupsPrintFile2(CUPS_HTTP_DEFAULT, m_printerName.toUtf8().constData(), outputName, title.constData(), rawCount, rawOptions);
    cupsFreeOptions(rawCount, rawOptions);
    unlink(outputName);
    if (job == 0) {
        m_errorString = u"Printing on %1 failed: %2"_s.arg(m_printerName, QString::fromUtf8(cupsLastErrorString()));
    }
    return job;
}

} // namespace LabelCreator

#pragma GCC diagnostic pop
