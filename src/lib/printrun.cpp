/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "printrun.h"

#include "fields.h"
#include "labelformat.h"
#include "tapelayout.h"
#include "units.h"

#include <QDir>
#include <QFileInfo>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator::PrintRun
{

namespace
{

bool fail(QString *errorString, const QString &message)
{
    if (errorString) {
        *errorString = message;
    }
    return false;
}

// More of one row than this is a mistake in the data, not a print run.
constexpr int MaxRowCopies = 999;

// Another label of the run, read once however many rows name it.
std::optional<LabelDocument> otherLabel(const QString &name, const Request &request, QHash<QString, LabelDocument> &cache, QString *errorString)
{
    const QString file = labelFileFor(name, request.labelFile);
    if (file.isEmpty()) {
        fail(errorString,
             request.labelFile.isEmpty() ? u"Save the label first: the label %1 is looked for next to it."_s.arg(name)
                                         : u"There is no label %1 next to this one."_s.arg(name));
        return std::nullopt;
    }
    if (const auto it = cache.constFind(file); it != cache.cend()) {
        return *it;
    }
    LabelReader reader;
    auto document = reader.readFile(file);
    if (!document) {
        fail(errorString, u"%1 could not be read: %2"_s.arg(QFileInfo(file).fileName(), reader.errorString()));
        return std::nullopt;
    }
    TapeLayout::layout(*document);
    if (!sameLabelType(*document, request.document)) {
        fail(errorString,
             u"%1 is for %2 labels, this one for %3: one run prints on one label type."_s.arg(QFileInfo(file).fileName(),
                                                                                            document->paperName,
                                                                                            request.document.paperName));
        return std::nullopt;
    }
    cache.insert(file, *document);
    return document;
}

// Another label's own questions are not asked: their defaults fill them.
QHash<QString, QString> withDefaults(const LabelDocument &document, QHash<QString, QString> values)
{
    for (const PrintQuestion &question : document.questions) {
        if (!values.contains(question.name)) {
            values.insert(question.name, question.defaultValue);
        }
    }
    return values;
}

} // namespace

QString labelFileFor(const QString &name, const QString &labelFile)
{
    QString fileName = name.trimmed();
    if (fileName.isEmpty()) {
        return {};
    }
    if (!fileName.endsWith(u".label"_s, Qt::CaseInsensitive)) {
        fileName += u".label"_s;
    }
    if (QFileInfo(fileName).isRelative()) {
        if (labelFile.isEmpty()) {
            return {};
        }
        fileName = QFileInfo(labelFile).absoluteDir().absoluteFilePath(fileName);
    }
    return QFileInfo(fileName).isFile() ? QFileInfo(fileName).absoluteFilePath() : QString();
}

bool sameLabelType(const LabelDocument &a, const LabelDocument &b)
{
    // Tape: the same tape, whatever its length.
    if (a.tape || b.tape) {
        return a.tape && b.tape && a.tape->width == b.tape->width;
    }
    // The same size, give or take a millimetre, and the same roll.
    const QSizeF sizeA = a.paperSize();
    const QSizeF sizeB = b.paperSize();
    const qreal tolerance = Units::mmToTwips(1.0);
    return std::abs(sizeA.width() - sizeB.width()) <= tolerance && std::abs(sizeA.height() - sizeB.height()) <= tolerance && a.roll == b.roll;
}

LabelDocument filled(const LabelDocument &document, const QHash<QString, QString> &values, const QString &pictureFolder)
{
    LabelDocument result = Fields::fill(document, values);
    for (LabelObject &object : result.objects) {
        auto *image = std::get_if<ImageObject>(&object);
        // A picture in the field itself (from a database) has no folder.
        if (image && image->imageLocation && !image->imageLocation->isEmpty() && !image->imageLocation->startsWith(u"data:"_s)
            && QFileInfo(*image->imageLocation).isRelative()) {
            image->imageLocation = QDir(pictureFolder).absoluteFilePath(*image->imageLocation);
        }
    }
    return result;
}

QStringList runColumns(const LabelDocument &document)
{
    QStringList columns;
    for (const QString &column : {document.dataSource.copiesColumn, document.dataSource.labelColumn}) {
        if (!column.isEmpty()) {
            columns << column;
        }
    }
    return columns;
}

std::optional<Run> make(const Request &request, QString *errorString)
{
    const LabelDocument &document = request.document;
    const QString pictures =
        !request.pictureFolder.isEmpty() ? request.pictureFolder : request.labelFile.isEmpty() ? QDir::currentPath() : QFileInfo(request.labelFile).absolutePath();
    QHash<QString, QString> everyLabel = request.values;
    everyLabel.insert(request.answers);

    Run run;
    run.copies = std::clamp(request.copies, 1, MaxRowCopies);
    QHash<QString, LabelDocument> others;
    if (!request.prepared.isEmpty()) {
        run.labels = request.prepared;
    } else if (request.rows.isEmpty()) {
        run.labels << filled(document, everyLabel, pictures);
    } else {
        const DataSource &source = document.dataSource;
        for (qsizetype i = 0; i < request.rows.size(); ++i) {
            const QHash<QString, QString> &row = request.rows.at(i);
            // How many of this row: its copies column, else one.
            int count = 1;
            if (!source.copiesColumn.isEmpty()) {
                const QString copies = row.value(source.copiesColumn).trimmed();
                bool ok = true;
                count = copies.isEmpty() ? 1 : copies.toInt(&ok);
                if (!ok || count < 0) {
                    fail(errorString, u"Row %1: \"%2\" in %3 is not a number of copies."_s.arg(i + 1).arg(copies, source.copiesColumn));
                    return std::nullopt;
                }
                count = std::min(count, MaxRowCopies);
            }
            if (count == 0) {
                continue;
            }
            // Which label: the one its label column names, else this one.
            LabelDocument label = document;
            if (const QString name = source.labelColumn.isEmpty() ? QString() : row.value(source.labelColumn).trimmed(); !name.isEmpty()) {
                QString error;
                const auto other = otherLabel(name, request, others, &error);
                if (!other) {
                    fail(errorString, u"Row %1: %2"_s.arg(i + 1).arg(error));
                    return std::nullopt;
                }
                label = *other;
            }
            // The run's values, the row's over them, the answers over both.
            QHash<QString, QString> values = request.values;
            values.insert(row);
            values.insert(request.answers);
            const LabelDocument one = filled(label, withDefaults(label, values), pictures);
            for (int copy = 0; copy < count; ++copy) {
                run.labels << one;
            }
        }
        if (run.labels.isEmpty()) {
            fail(errorString, u"Nothing to print: every row asks for no copies."_s);
            return std::nullopt;
        }
    }

    // A header and trailer label come once: the copies are laid out here.
    if (document.headerLabel.isEmpty() && document.trailerLabel.isEmpty()) {
        return run;
    }
    if (run.copies > 1) {
        QList<LabelDocument> copied;
        for (const LabelDocument &label : std::as_const(run.labels)) {
            for (int copy = 0; copy < run.copies; ++copy) {
                copied << label;
            }
        }
        run.labels = copied;
        run.copies = 1;
    }
    // {Count}: the labels between them.
    QHash<QString, QString> values = everyLabel;
    values.insert(u"Count"_s, QString::number(run.labels.size()));
    for (const auto &[name, header] : {std::pair{document.headerLabel, true}, std::pair{document.trailerLabel, false}}) {
        if (name.isEmpty()) {
            continue;
        }
        QString error;
        const auto label = otherLabel(name, request, others, &error);
        if (!label) {
            fail(errorString, (header ? u"Header label: %1"_s : u"Trailer label: %1"_s).arg(error));
            return std::nullopt;
        }
        const LabelDocument one = filled(*label, withDefaults(*label, values), pictures);
        if (header) {
            run.labels.prepend(one);
            run.headerLabels = 1;
        } else {
            run.labels.append(one);
            run.trailerLabels = 1;
        }
    }
    return run;
}

} // namespace LabelCreator::PrintRun
