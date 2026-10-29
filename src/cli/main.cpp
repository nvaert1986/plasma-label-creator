/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "labelcreator_version.h"
#include "labelformat.h"
#include "labelkinds.h"
#include "labelprinter.h"
#include "labelrenderer.h"
#include "tapelayout.h"
#include "units.h"
#include "zplprinter.h"

#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTextStream>

#include <variant>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

QString objectKind(const LabelObject &object)
{
    return std::visit(
        [](const auto &o) -> QString {
            using T = std::decay_t<decltype(o)>;
            if constexpr (std::is_same_v<T, TextObject>) {
                return u"text"_s;
            } else if constexpr (std::is_same_v<T, AddressObject>) {
                return u"address"_s;
            } else if constexpr (std::is_same_v<T, BarcodeObject>) {
                return u"barcode (%1)"_s.arg(o.type);
            } else if constexpr (std::is_same_v<T, ImageObject>) {
                return u"image"_s;
            } else if constexpr (std::is_same_v<T, ShapeObject>) {
                return u"shape"_s;
            } else if constexpr (std::is_same_v<T, CounterObject>) {
                return u"counter (%1)"_s.arg(o.text());
            } else if constexpr (std::is_same_v<T, DateTimeObject>) {
                return u"date/time (%1)"_s.arg(o.text(QDateTime::currentDateTime()));
            } else if constexpr (std::is_same_v<T, CircularTextObject>) {
                return u"curved text"_s;
            } else {
                return u"unsupported <%1>"_s.arg(o.elementName);
            }
        },
        object);
}

void printInfo(QTextStream &out, const LabelDocument &document)
{
    const QSizeF canvas = document.canvasSize();
    out << "Paper: " << document.paperName << " (" << document.id << ")\n";
    out << "Canvas: " << Units::twipsToMm(canvas.width()) << " x " << Units::twipsToMm(canvas.height()) << " mm, "
        << (document.orientation == PaperOrientation::Landscape ? "landscape" : "portrait") << '\n';
    for (const LabelObject &object : document.objects) {
        const ObjectBase &base = objectBase(object);
        out << "  " << objectKind(object) << " \"" << base.name << "\" at " << base.bounds.x() << ',' << base.bounds.y() << ' '
            << base.bounds.width() << 'x' << base.bounds.height() << " twips\n";
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"plasma-label-creator-render"_s);
    QCoreApplication::setApplicationVersion(QStringLiteral(LABELCREATOR_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(u"Renders a DYMO Label Software .label file to an image."_s);
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption dpiOption(u"dpi"_s, u"Resolution of the image (default 300, the LabelWriter head)."_s, u"dpi"_s, u"300"_s);
    const QCommandLineOption printOption(u"print"_s, u"Render as for the printer: no antialiasing, no outline."_s);
    const QCommandLineOption infoOption(u"info"_s, u"List the paper and the objects."_s);
    const QCommandLineOption resaveOption(u"resave"_s, u"Write the label back out to <file>, to check round trips."_s, u"file"_s);
    const QCommandLineOption listPrintersOption(u"list-printers"_s, u"List the CUPS queues and their DYMO paper sizes."_s);
    const QCommandLineOption sendOption(u"send"_s, u"Print the label on CUPS queue <printer>."_s, u"printer"_s);
    const QCommandLineOption copiesOption(u"copies"_s, u"Number of copies to print (default 1)."_s, u"n"_s, u"1"_s);
    const QCommandLineOption qualityOption(u"quality"_s, u"DYMO print quality: Text or Graphics."_s, u"quality"_s);
    const QCommandLineOption densityOption(u"density"_s, u"DYMO print density: Light, Medium, Normal or Dark."_s, u"density"_s);
    const QCommandLineOption offsetOption(u"offset"_s, u"Extra print offset in mm, x,y (right and down positive)."_s, u"x,y"_s);
    const QCommandLineOption noMarginOption(u"no-margin-compensation"_s, u"Do not move the label up by the driver's top margin."_s);
    const QCommandLineOption paperImageOption(u"paper-image"_s, u"Write the 1-bit image that would be printed to <file>."_s, u"file"_s);
    const QCommandLineOption queueOption(u"queue"_s, u"The CUPS queue whose driver --paper-image uses, without printing."_s, u"printer"_s);
    const QCommandLineOption zplInfoOption(u"zpl-info"_s,
                                           u"Ask the ZPL label printer at <host[:port]> what it is and which labels are in it (prints nothing)."_s,
                                           u"host"_s);
    parser.addOptions({dpiOption,
                       printOption,
                       infoOption,
                       resaveOption,
                       listPrintersOption,
                       sendOption,
                       copiesOption,
                       qualityOption,
                       densityOption,
                       offsetOption,
                       noMarginOption,
                       paperImageOption,
                       queueOption,
                       zplInfoOption});
    parser.addPositionalArgument(u"input"_s, u"The .label file."_s);
    parser.addPositionalArgument(u"output"_s, u"The image to write, for example label.png, or label.pdf for a PDF."_s, u"[output]"_s);
    parser.process(app);

    QTextStream out(stdout);
    QTextStream err(stderr);

    if (parser.isSet(zplInfoOption)) {
        const QStringList address = parser.value(zplInfoOption).split(u':');
        QString error;
        const auto info = Zpl::query(address.constFirst(), address.size() > 1 ? quint16(address.at(1).toUInt()) : 9100, 3000, &error);
        if (!info) {
            err << error << Qt::endl;
            return 1;
        }
        const QSizeF size = info->labelSizeMm();
        out << "Model:        " << info->model << " (firmware " << info->firmware << ")" << Qt::endl;
        out << "Resolution:   " << qRound(info->dpi()) << " dpi" << Qt::endl;
        out << "Labels:       " << QString::number(size.width(), 'f', 1) << " mm print width, " << QString::number(size.height(), 'f', 1)
            << " mm long, " << (info->continuous() ? "continuous" : "separate labels") << ", sensor " << info->configuration.value(u"SENSOR TYPE"_s)
            << Qt::endl;
        out << "Printing:     " << (info->thermalTransfer ? "thermal transfer" : "direct thermal") << ", " << info->printMode().toLower() << Qt::endl;
        out << "Status:       " << (info->problems().isEmpty() ? u"ready"_s : info->problems().join(u", "_s)) << Qt::endl;
        return 0;
    }

    if (parser.isSet(listPrintersOption)) {
        for (const PrinterInfo &printer : LabelPrinter::printers()) {
            out << printer.name << (printer.isDefault ? " (default)" : "") << ": " << printer.makeAndModel << '\n';
            if (!printer.isLabelWriter()) {
                continue;
            }
            LabelPrinter labelPrinter(printer.name);
            if (!labelPrinter.load()) {
                err << "  " << labelPrinter.errorString() << '\n';
                continue;
            }
            for (const PaperSize &paper : labelPrinter.paperSizes()) {
                out << "  " << paper.keyword << ": " << paper.name << '\n';
            }
        }
        return 0;
    }

    const QStringList arguments = parser.positionalArguments();
    if (arguments.isEmpty() || arguments.size() > 2) {
        parser.showHelp(1);
    }

    bool dpiOk = false;
    const qreal dpi = parser.value(dpiOption).toDouble(&dpiOk);
    if (!dpiOk || dpi <= 0) {
        err << "Invalid --dpi value\n";
        return 1;
    }

    LabelReader reader;
    std::optional<LabelDocument> document;
    if (arguments.at(0).endsWith(u".rhino"_s, Qt::CaseInsensitive)) {
        // A DYMO Rhino template: its label, with the first texts it carries.
        QFile file(arguments.at(0));
        QString error;
        const auto rhino = file.open(QIODevice::ReadOnly) ? LabelKinds::readRhinoTemplate(file.readAll(), &error) : std::nullopt;
        if (!rhino) {
            err << arguments.at(0) << ": " << (error.isEmpty() ? file.errorString() : error) << '\n';
            return 2;
        }
        document = LabelKinds::fromRhinoTemplate(*rhino);
    } else {
        document = reader.readFile(arguments.at(0));
    }
    for (const QString &warning : reader.warnings()) {
        err << "warning: " << warning << '\n';
    }
    if (!document) {
        err << arguments.at(0) << ": " << reader.errorString() << '\n';
        return 2;
    }
    // A tape label's cells place its objects and set its length.
    TapeLayout::layout(*document);

    if (parser.isSet(infoOption)) {
        printInfo(out, *document);
    }

    if (parser.isSet(resaveOption)) {
        QString error;
        if (!LabelWriter::writeFile(*document, parser.value(resaveOption), &error)) {
            err << parser.value(resaveOption) << ": " << error << '\n';
            return 3;
        }
    }

    if (parser.isSet(sendOption) || parser.isSet(paperImageOption)) {
        const QString printerName = parser.value(sendOption);
        LabelPrinter printer(printerName.isEmpty() ? QString() : printerName);
        if (parser.isSet(paperImageOption)) {
            // Without a queue, use the driver of the first LabelWriter found.
            QString queue = parser.isSet(queueOption) ? parser.value(queueOption) : printerName;
            for (const PrinterInfo &info : LabelPrinter::printers()) {
                if (queue.isEmpty() && info.isLabelWriter()) {
                    queue = info.name;
                }
            }
            LabelPrinter driver(queue);
            if (!driver.load()) {
                err << driver.errorString() << '\n';
                return 4;
            }
            const auto paper = driver.paperFor(*document);
            if (!paper) {
                err << queue << " has no paper size for \"" << document->paperName << "\"\n";
                return 4;
            }
            QStringList warnings;
            const QImage page = LabelPrinter::paperImage(*document, *paper, driver.resolution(), &warnings);
            for (const QString &warning : warnings) {
                err << "warning: " << warning << '\n';
            }
            out << "Paper: " << paper->keyword << " (" << paper->name << "), " << page.width() << 'x' << page.height() << " dots\n";
            if (!page.save(parser.value(paperImageOption))) {
                err << parser.value(paperImageOption) << ": could not write the image\n";
                return 3;
            }
        }
        if (parser.isSet(sendOption)) {
            LabelPrinter::Options options;
            options.copies = std::max(1, parser.value(copiesOption).toInt());
            options.printQuality = parser.value(qualityOption);
            options.printDensity = parser.value(densityOption);
            options.jobTitle = QFileInfo(arguments.at(0)).fileName();
            options.compensateTopMargin = !parser.isSet(noMarginOption);
            if (parser.isSet(offsetOption)) {
                const QStringList parts = parser.value(offsetOption).split(u',');
                options.offset = QPointF(parts.value(0).toDouble(), parts.value(1).toDouble()) * 72.0 / 25.4;
            }
            const int job = printer.print(*document, options);
            for (const QString &warning : printer.warnings()) {
                err << "warning: " << warning << '\n';
            }
            if (job == 0) {
                err << printer.errorString() << '\n';
                return 4;
            }
            out << "Sent to " << printerName << " as job " << job << '\n';
        }
    }

    if (arguments.size() == 2 && arguments.at(1).endsWith(u".pdf"_s, Qt::CaseInsensitive)) {
        // A PDF is the label as it is, without the outline: always print mode.
        LabelRenderer renderer(LabelRenderer::Mode::Print);
        renderer.setPrinterResolution(dpi);
        if (!renderer.writePdf(*document, arguments.at(1), QFileInfo(arguments.at(0)).completeBaseName())) {
            err << arguments.at(1) << ": could not write the PDF\n";
            return 3;
        }
        for (const QString &warning : renderer.warnings()) {
            err << "warning: " << warning << '\n';
        }
    } else if (arguments.size() == 2) {
        LabelRenderer renderer(parser.isSet(printOption) ? LabelRenderer::Mode::Print : LabelRenderer::Mode::Preview);
        const QImage image = renderer.renderImage(*document, dpi);
        for (const QString &warning : renderer.warnings()) {
            err << "warning: " << warning << '\n';
        }
        if (!image.save(arguments.at(1))) {
            err << arguments.at(1) << ": could not write the image\n";
            return 3;
        }
    }

    return 0;
}
