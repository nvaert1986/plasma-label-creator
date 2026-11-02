/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "automation.h"

#include "fields.h"
#include "labelformat.h"
#include "labelprinter.h"
#include "printhistory.h"
#include "sharedcounter.h"
#include "tapelayout.h"
#include "units.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QLockFile>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QSysInfo>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>


using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
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

// More rows than this in one job is a mistake, not a print run.
constexpr int MaxRows = 10000;
constexpr int MaxCopies = 999;

QString text(const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::String:
        return value.toString();
    case QJsonValue::Bool:
        return value.toBool() ? u"true"_s : u"false"_s;
    case QJsonValue::Double: {
        const double number = value.toDouble();
        return number == std::floor(number) && std::abs(number) < 1e15 ? QString::number(qint64(number)) : QString::number(number, 'g', 15);
    }
    default:
        return {};
    }
}

std::optional<QHash<QString, QString>> fieldValues(const QJsonValue &value, const QString &what, QString *errorString)
{
    QHash<QString, QString> values;
    if (value.isUndefined() || value.isNull()) {
        return values;
    }
    if (!value.isObject()) {
        fail(errorString, u"\"%1\" is not an object of field names and values."_s.arg(what));
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it->isArray() || it->isObject()) {
            fail(errorString, u"\"%1\": the value of %2 is not a text, number or true/false."_s.arg(what, it.key()));
            return std::nullopt;
        }
        if (!it->isNull()) {
            values.insert(it.key(), text(*it));
        }
    }
    return values;
}

// The app's settings file, as KDE writes it: [Group], Key=value, and [$i]
// for what an administrator fixed. Only the plain top-level groups.
using ConfigGroups = QHash<QString, QHash<QString, QString>>;

QString unescape(const QString &value)
{
    QString result;
    result.reserve(value.size());
    for (qsizetype i = 0; i < value.size(); ++i) {
        const QChar c = value.at(i);
        if (c != u'\\' || i + 1 == value.size()) {
            result += c;
            continue;
        }
        const QChar next = value.at(++i);
        result += next == u's' ? QChar(u' ') : next == u'n' ? QChar(u'\n') : next == u't' ? QChar(u'\t') : next == u'r' ? QChar(u'\r') : next;
    }
    return result;
}

void readConfig(const QString &fileName, ConfigGroups *groups, QSet<QString> *fixed)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QString group;
    bool groupFixed = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(u'#')) {
            continue;
        }
        if (line.startsWith(u'[')) {
            groupFixed = line.endsWith(u"[$i]"_s);
            const QString header = groupFixed ? line.chopped(4) : line;
            // Nested groups ([A][B]) are not read.
            group = header.count(u'[') == 1 && header.endsWith(u']') ? header.mid(1, header.size() - 2) : QString();
            continue;
        }
        const qsizetype equals = line.indexOf(u'=');
        if (group.isEmpty() || equals <= 0) {
            continue;
        }
        QString key = line.left(equals).trimmed();
        bool keyFixed = groupFixed;
        if (key.endsWith(u"[$i]"_s)) {
            keyFixed = true;
            key.chop(4);
        }
        if (key.contains(u'[')) {
            continue; // translated or otherwise marked: not ours
        }
        const QString id = group + u'\x1f' + key;
        if (fixed->contains(id)) {
            continue;
        }
        (*groups)[group].insert(key, unescape(line.mid(equals + 1).trimmed()));
        if (keyFixed) {
            fixed->insert(id);
        }
    }
}

QString writeDate(const PrintQuestion &question, const QDate &date)
{
    return question.dateFormat.isEmpty() ? QLocale().toString(date, QLocale::ShortFormat) : QLocale().toString(date, question.dateFormat);
}

QDate readDate(const PrintQuestion &question, const QString &text)
{
    const QDate date = question.dateFormat.isEmpty() ? QLocale().toDate(text, QLocale::ShortFormat) : QLocale().toDate(text, question.dateFormat);
    // Also as programs write dates: 2026-11-02.
    return date.isValid() ? date : QDate::fromString(text, Qt::ISODate);
}

QString checkedText(const PrintQuestion &question)
{
    return question.checkedText.isEmpty() ? u"Yes"_s : question.checkedText;
}

QString uncheckedText(const PrintQuestion &question)
{
    return question.uncheckedText.isEmpty() ? u"No"_s : question.uncheckedText;
}

} // namespace

QString AutomationSettings::defaultFileName()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/plasma-label-creator/automation.json"_s;
}

AutomationSettings AutomationSettings::load(const QString &fileName, QString *errorString)
{
    AutomationSettings settings;
    QFile file(fileName);
    if (!file.exists()) {
        return settings;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        fail(errorString, u"%1: %2"_s.arg(fileName, file.errorString()));
        return settings;
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!json.isObject()) {
        fail(errorString, u"%1: %2"_s.arg(fileName, parseError.errorString()));
        return settings;
    }
    const QJsonObject root = json.object();
    const QJsonArray folderArray = root.value("folders"_L1).toArray();
    for (const QJsonValue value : folderArray) {
        const QJsonObject object = value.toObject();
        Folder folder;
        folder.folder = object.value("folder"_L1).toString();
        folder.label = object.value("label"_L1).toString();
        folder.printer = object.value("printer"_L1).toString();
        folder.copies = std::clamp(object.value("copies"_L1).toInt(1), 1, MaxCopies);
        if (!folder.folder.isEmpty()) {
            settings.folders << folder;
        }
    }
    settings.labelsFolder = root.value("labelsFolder"_L1).toString();
    settings.printer = root.value("printer"_L1).toString();
    settings.saveCounters = root.value("saveCounters"_L1).toBool(true);
    const QJsonObject http = root.value("http"_L1).toObject();
    settings.http = http.value("enabled"_L1).toBool(false);
    settings.httpNetwork = http.value("network"_L1).toBool(false);
    settings.httpPort = std::clamp(http.value("port"_L1).toInt(8631), 0, 65535); // 0: any free port
    settings.httpToken = http.value("token"_L1).toString();
    return settings;
}

bool AutomationSettings::save(const QString &fileName, QString *errorString) const
{
    QJsonArray folderList;
    for (const Folder &folder : folders) {
        folderList.append(QJsonObject{{"folder"_L1, folder.folder}, {"label"_L1, folder.label}, {"printer"_L1, folder.printer}, {"copies"_L1, folder.copies}});
    }
    const QJsonObject root{
        {"folders"_L1, folderList},
        {"labelsFolder"_L1, labelsFolder},
        {"printer"_L1, printer},
        {"saveCounters"_L1, saveCounters},
        {"http"_L1, QJsonObject{{"enabled"_L1, http}, {"network"_L1, httpNetwork}, {"port"_L1, httpPort}, {"token"_L1, httpToken}}},
    };
    if (!QDir().mkpath(QFileInfo(fileName).absolutePath())) {
        return fail(errorString, u"Could not create the folder for %1."_s.arg(fileName));
    }
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(errorString, u"%1: %2"_s.arg(fileName, file.errorString()));
    }
    // The token lets anyone print: for this user's eyes only.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QJsonDocument(root).toJson());
    if (!file.commit()) {
        return fail(errorString, u"%1: %2"_s.arg(fileName, file.errorString()));
    }
    return true;
}

QString AutomationSettings::newToken()
{
    QByteArray bytes(32, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(bytes.data()), bytes.size() / int(sizeof(quint32)));
    return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

std::optional<PrintRequest> PrintRequest::fromJson(const QByteArray &json, QString *errorString)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (!document.isObject()) {
        fail(errorString, document.isNull() ? u"Not JSON: %1."_s.arg(parseError.errorString()) : u"Not a JSON object."_s);
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    PrintRequest request;
    for (const char *name : {"label", "printer"}) {
        const QJsonValue value = root.value(QLatin1StringView(name));
        if (!value.isUndefined() && !value.isNull() && !value.isString()) {
            fail(errorString, u"\"%1\" is not a text."_s.arg(QLatin1StringView(name)));
            return std::nullopt;
        }
    }
    request.label = root.value("label"_L1).toString();
    request.printer = root.value("printer"_L1).toString();
    const QJsonValue copies = root.value("copies"_L1);
    if (!copies.isUndefined()) {
        if (!copies.isDouble() || copies.toDouble() != std::floor(copies.toDouble()) || copies.toInt() < 1 || copies.toInt() > MaxCopies) {
            fail(errorString, u"\"copies\" is not a whole number from 1 to %1."_s.arg(MaxCopies));
            return std::nullopt;
        }
        request.copies = copies.toInt();
    }
    const auto values = fieldValues(root.value("values"_L1), u"values"_s, errorString);
    if (!values) {
        return std::nullopt;
    }
    request.values = *values;
    const QJsonValue rows = root.value("rows"_L1);
    if (!rows.isUndefined() && !rows.isNull()) {
        if (!rows.isArray()) {
            fail(errorString, u"\"rows\" is not a list of rows."_s);
            return std::nullopt;
        }
        const QJsonArray list = rows.toArray();
        if (list.size() > MaxRows) {
            fail(errorString, u"More than %1 rows in one job."_s.arg(MaxRows));
            return std::nullopt;
        }
        for (const QJsonValue row : list) {
            const auto rowValues = fieldValues(row, u"rows"_s, errorString);
            if (!rowValues) {
                return std::nullopt;
            }
            request.rows << *rowValues;
        }
    }
    const QJsonValue anyway = root.value("printAnyway"_L1);
    if (!anyway.isUndefined() && !anyway.isBool()) {
        fail(errorString, u"\"printAnyway\" is not true or false."_s);
        return std::nullopt;
    }
    request.printAnyway = anyway.toBool(false);
    return request;
}

PrintEnvironment PrintEnvironment::fromAppConfig()
{
    // locateAll lists the user's file first; the system's are read first so
    // the user's override them, except where they are fixed.
    QStringList files = QStandardPaths::locateAll(QStandardPaths::GenericConfigLocation, u"plasma-label-creatorrc"_s);
    std::reverse(files.begin(), files.end());
    ConfigGroups groups;
    QSet<QString> fixed;
    for (const QString &file : std::as_const(files)) {
        readConfig(file, &groups, &fixed);
    }
    PrintEnvironment environment;
    environment.drivers = groups.value(u"Printer Drivers"_s);
    const QHash<QString, QString> offsets = groups.value(u"Print Offsets"_s);
    for (auto it = offsets.cbegin(); it != offsets.cend(); ++it) {
        const QStringList parts = it.value().split(u',');
        if (parts.size() == 2) {
            environment.offsetsMm.insert(it.key(), QPointF(parts.at(0).toDouble(), parts.at(1).toDouble()));
        }
    }
    const QHash<QString, QString> settings = groups.value(u"Settings"_s);
    bool ok = false;
    if (const int wait = settings.value(u"CounterWaitSeconds"_s).toInt(&ok); ok) {
        environment.counterWaitSeconds = std::max(0, wait);
    }
    if (const int kept = settings.value(u"HistoryKept"_s).toInt(&ok); ok) {
        environment.historyKept = std::max(0, kept);
    }
    return environment;
}

namespace Automation
{

QString lockFileName()
{
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        runtime = QDir::tempPath();
    }
    return runtime + u"/plasma-label-creator-automation.lock"_s;
}

qint64 servicePid()
{
    qint64 pid = 0;
    QString host;
    QString application;
    QLockFile lock(lockFileName());
    if (!lock.getLockInfo(&pid, &host, &application) || pid <= 0 || host != QSysInfo::machineHostName()) {
        return 0;
    }
    // Still running, and still the service, not another program that got
    // its process id (the app's short name is the same: the program's).
    QFile commandLine(u"/proc/%1/cmdline"_s.arg(pid));
    if (!commandLine.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QByteArray program = commandLine.readAll().split('\0').value(0);
    return QFileInfo(QString::fromLocal8Bit(program)).fileName() == u"plasma-label-creator-automation"_s ? pid : 0;
}

QStringList labelNames(const QString &labelsFolder)
{
    QStringList names;
    const QString root = QFileInfo(labelsFolder).canonicalFilePath();
    if (root.isEmpty()) {
        return names;
    }
    QDirIterator it(root, {u"*.label"_s}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext() && names.size() < 1000) {
        const QString file = QFileInfo(it.next()).canonicalFilePath();
        if (file.startsWith(root + u'/')) {
            names << file.mid(root.size() + 1);
        }
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

std::optional<QString> labelInFolder(const QString &labelsFolder, const QString &name)
{
    if (labelsFolder.isEmpty() || name.trimmed().isEmpty()) {
        return std::nullopt;
    }
    QString fileName = name.trimmed();
    if (!fileName.endsWith(u".label"_s, Qt::CaseInsensitive)) {
        fileName += u".label"_s;
    }
    // Resolved, links and .. included, it must still be inside the folder.
    const QString root = QFileInfo(labelsFolder).canonicalFilePath();
    QString file = QFileInfo(QDir(labelsFolder).absoluteFilePath(fileName)).canonicalFilePath();
    if (root.isEmpty() || file.isEmpty() || !file.startsWith(root + u'/') || !QFileInfo(file).isFile()) {
        return std::nullopt;
    }
    return file;
}

std::optional<QHash<QString, QString>> answers(const LabelDocument &document, const QHash<QString, QString> &values, QString *errorString)
{
    QHash<QString, QString> result;
    for (const PrintQuestion &question : document.questions) {
        const auto given = values.constFind(question.name);
        const bool answered = given != values.cend();
        QString answer = answered ? *given : question.defaultValue;
        const QString label = question.prompt.isEmpty() ? question.name : question.prompt;
        switch (question.kind) {
        case PrintQuestion::Kind::Text:
            break;
        case PrintQuestion::Kind::Choice:
            // Not given: what the form starts on, the default if it is one
            // of the choices, else the first (an empty one when optional).
            if (!answered && question.choicesColumn.isEmpty() && !question.choices.contains(answer)) {
                answer = question.required ? question.choices.value(0) : QString();
            }
            break;
        case PrintQuestion::Kind::Number: {
            bool ok = false;
            double number = 0;
            if (answered) {
                // Programs write 2.5; people may write 2,5 as their language does.
                number = QLocale::c().toDouble(answer.trimmed(), &ok);
                if (!ok) {
                    QLocale local;
                    local.setNumberOptions(QLocale::RejectGroupSeparator);
                    number = local.toDouble(answer.trimmed(), &ok);
                }
                if (!ok && !answer.trimmed().isEmpty()) {
                    fail(errorString, u"%1: not a number."_s.arg(label));
                    return std::nullopt;
                }
            } else {
                // As the form starts: the default, kept within the range.
                number = QLocale().toDouble(answer);
                if (question.minimum != 0 || question.maximum != 0) {
                    number = std::clamp(number, question.minimum, std::max(question.minimum, question.maximum));
                }
                ok = true;
            }
            answer = ok ? QLocale().toString(number, 'f', question.decimals) : QString();
            break;
        }
        case PrintQuestion::Kind::Date: {
            // Today unless set, as the form does; a date given must be one.
            QDate date = answer.trimmed().isEmpty() ? QDate() : readDate(question, answer.trimmed());
            if (!date.isValid() && answered && !answer.trimmed().isEmpty()) {
                fail(errorString, u"%1: not a date (%2, or yyyy-MM-dd)."_s.arg(label, question.dateFormat.isEmpty() ? QLocale().dateFormat(QLocale::ShortFormat) : question.dateFormat));
                return std::nullopt;
            }
            if (!date.isValid()) {
                date = QDate::currentDate();
            }
            answer = writeDate(question, date);
            break;
        }
        case PrintQuestion::Kind::Check: {
            if (!answered) {
                // The form ticks it only for the ticked text.
                answer = answer == checkedText(question) ? checkedText(question) : uncheckedText(question);
                break;
            }
            const QString lower = answer.trimmed().toLower();
            if (answer == checkedText(question) || lower == u"true"_s || lower == u"1"_s || lower == u"yes"_s) {
                answer = checkedText(question);
            } else if (answer == uncheckedText(question) || lower.isEmpty() || lower == u"false"_s || lower == u"0"_s || lower == u"no"_s) {
                answer = uncheckedText(question);
            } else {
                fail(errorString, u"%1: true or false, or \"%2\" or \"%3\"."_s.arg(label, checkedText(question), uncheckedText(question)));
                return std::nullopt;
            }
            break;
        }
        }
        if (const auto problem = Fields::problem(question, answer)) {
            fail(errorString, *problem);
            return std::nullopt;
        }
        result.insert(question.name, answer);
    }
    return result;
}

std::optional<QList<LabelDocument>> prepare(const LabelDocument &document, const QString &labelFile, const PrintRequest &request, QString *errorString)
{
    const auto given = answers(document, request.values, errorString);
    if (!given) {
        return std::nullopt;
    }
    // Relative picture paths: next to the label, as the app finds them.
    const QDir base = QFileInfo(labelFile).absoluteDir();
    const auto filled = [&base](const LabelDocument &label, const QHash<QString, QString> &values) {
        LabelDocument result = Fields::fill(label, values);
        for (LabelObject &object : result.objects) {
            if (auto *image = std::get_if<ImageObject>(&object); image && image->imageLocation && !image->imageLocation->isEmpty()) {
                if (QFileInfo(*image->imageLocation).isRelative()) {
                    image->imageLocation = base.absoluteFilePath(*image->imageLocation);
                }
            }
        }
        return result;
    };
    // The values on every label, then each row's, then the answers, which
    // win as in the app.
    QList<LabelDocument> labels;
    if (request.rows.isEmpty()) {
        QHash<QString, QString> values = request.values;
        values.insert(*given);
        labels << filled(document, values);
        return labels;
    }
    for (const QHash<QString, QString> &row : request.rows) {
        QHash<QString, QString> values = request.values;
        values.insert(row);
        values.insert(*given);
        labels << filled(document, values);
    }
    return labels;
}

PrintOutcome print(const QString &labelFile, const PrintRequest &request, const PrintEnvironment &environment)
{
    PrintOutcome outcome;
    const auto refuse = [&outcome](PrintOutcome::Status status, const QString &message) {
        outcome.status = status;
        outcome.message = message;
        return outcome;
    };

    LabelReader reader;
    auto read = reader.readFile(labelFile);
    if (!read) {
        return refuse(PrintOutcome::Status::NotFound, u"%1 could not be read: %2"_s.arg(QFileInfo(labelFile).fileName(), reader.errorString()));
    }
    // A tape label's cells place its objects and set its length.
    TapeLayout::layout(*read);
    LabelDocument document = *read;

    const QString printerName = request.printer;
    if (printerName.isEmpty()) {
        return refuse(PrintOutcome::Status::BadRequest, u"No printer: name one, or choose a default printer in the automation settings."_s);
    }
    const QList<PrinterInfo> printers = LabelPrinter::printers();
    const auto info = std::find_if(printers.cbegin(), printers.cend(), [&printerName](const PrinterInfo &printer) {
        return printer.name == printerName;
    });
    if (info == printers.cend()) {
        return refuse(PrintOutcome::Status::NotFound, u"There is no printer called %1."_s.arg(printerName));
    }
    // A queue without a label printer driver prints as the LabelWriter it
    // was told it is, in the app's print bar.
    const QString driver = info->hasLabelDriver() ? QString() : environment.drivers.value(printerName);
    if (!info->hasLabelDriver() && driver.isEmpty()) {
        return refuse(PrintOutcome::Status::NotReady,
                      u"%1 is not set up as a label printer: choose which LabelWriter it is in Plasma Label Creator's print bar first."_s.arg(printerName));
    }
    LabelPrinter printer(printerName, driver);
    if (!printer.load()) {
        return refuse(PrintOutcome::Status::Failed, printer.errorString());
    }
    if (!printer.paperFor(document)) {
        return refuse(PrintOutcome::Status::NotReady,
                      u"%1 cannot print this label: a %2 is not one of its label types."_s.arg(printerName, document.paperName));
    }
    const QStringList problems = LabelPrinter::queueStatus(printerName).problems();
    if (!problems.isEmpty() && !request.printAnyway) {
        return refuse(PrintOutcome::Status::NotReady,
                      u"%1 reports: %2. Ask with printAnyway to print anyway: the labels then wait in the queue."_s.arg(printerName, problems.join(u", "_s)));
    }

    QString error;
    auto prepared = prepare(document, labelFile, request, &error);
    if (!prepared) {
        return refuse(PrintOutcome::Status::BadRequest, error);
    }
    QList<LabelDocument> labels = *prepared;

    // Shared counters stay locked from reading their files until the next
    // values are written, so two computers never print the same numbers.
    std::vector<std::unique_ptr<SharedCounter>> locks;
    QHash<QString, int> sharedValues;
    for (int i = 0; i < document.objects.size(); ++i) {
        const auto *counter = std::get_if<CounterObject>(&document.objects.at(i));
        if (!counter || counter->sharedFile.isEmpty()) {
            continue;
        }
        if (!sharedValues.contains(counter->sharedFile)) {
            auto shared = std::make_unique<SharedCounter>(counter->sharedFile);
            if (!shared->lock(environment.counterWaitSeconds * 1000)) {
                return refuse(PrintOutcome::Status::Failed, shared->errorString());
            }
            const auto value = shared->read(counter->current);
            if (!value) {
                return refuse(PrintOutcome::Status::Failed, shared->errorString());
            }
            sharedValues.insert(counter->sharedFile, *value);
            locks.push_back(std::move(shared));
        }
        for (LabelDocument &label : labels) {
            if (auto *printed = std::get_if<CounterObject>(&label.objects[i])) {
                printed->current = sharedValues.value(counter->sharedFile);
            }
        }
    }

    LabelPrinter::Options options;
    options.copies = std::clamp(request.copies, 1, MaxCopies);
    options.offset = environment.offsetsMm.value(printerName) * 72.0 / Units::MmPerInch;
    options.jobTitle = QFileInfo(labelFile).fileName();
    const int job = printer.print(labels, options);
    if (job == 0) {
        return refuse(PrintOutcome::Status::Failed, printer.errorString());
    }
    outcome.status = PrintOutcome::Status::Printed;
    outcome.job = job;
    outcome.labels = printer.printedLabels();
    outcome.warnings = printer.warnings();
    outcome.message = u"Sent %1 label(s) to %2 as job %3."_s.arg(outcome.labels).arg(printerName).arg(job);

    // Counters count on: shared ones in their files, the others in the label.
    QStringList written;
    bool counted = false;
    for (int i = 0; i < document.objects.size(); ++i) {
        auto *counter = std::get_if<CounterObject>(&document.objects[i]);
        if (!counter) {
            continue;
        }
        counted = true;
        if (counter->sharedFile.isEmpty()) {
            counter->current += outcome.labels * counter->increment;
            continue;
        }
        const int next = sharedValues.value(counter->sharedFile) + outcome.labels * counter->increment;
        counter->current = next;
        if (!written.contains(counter->sharedFile)) {
            written << counter->sharedFile;
            SharedCounter file(counter->sharedFile);
            if (!file.write(next)) {
                outcome.warnings << u"The next counter value could not be saved: %1"_s.arg(file.errorString());
            }
        }
    }
    locks.clear();

    if (environment.historyKept > 0) {
        PrintRecord record;
        record.when = QDateTime::currentDateTime();
        record.source = labelFile;
        record.title = QFileInfo(labelFile).fileName();
        record.printer = printerName;
        record.labels = outcome.labels;
        record.job = job;
        const auto given = answers(*read, request.values);
        for (const PrintQuestion &question : read->questions) {
            record.answers.insert(question.name, given ? given->value(question.name) : QString());
        }
        record.rows = request.rows;
        PrintHistory history(environment.historyDirectory.isEmpty() ? PrintHistory::defaultDirectory() : environment.historyDirectory);
        history.setMaxRecords(environment.historyKept);
        QString historyError;
        // Kept as it was printed, before the counters moved on.
        if (!history.add(record, *read, &historyError)) {
            outcome.warnings << u"The print history could not be written: %1"_s.arg(historyError);
        }
    }
    if (counted && environment.saveCounters) {
        QString saveError;
        if (!LabelWriter::writeFile(document, labelFile, &saveError)) {
            outcome.warnings << u"The counters could not be saved in the label: %1"_s.arg(saveError);
        }
    }
    return outcome;
}

} // namespace Automation

} // namespace LabelCreator
