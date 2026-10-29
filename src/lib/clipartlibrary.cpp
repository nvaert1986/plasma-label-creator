/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "clipartlibrary.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLocale>
#include <QSaveFile>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <QtEndian>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace
{

constexpr char CatalogName[] = "Clipart.xml";
constexpr char DlsCatalogResource[] = "resources/clipart/clipart.xml";
constexpr char DlsSubdirectory[] = "dls";
constexpr char UserSubdirectory[] = "user";

QString languageKey(const QLocale &locale)
{
    return locale.name().replace(u'_', u'-');
}

} // namespace

QString ClipartCategory::displayName() const
{
    const QLocale locale;
    for (const QString &key : {languageKey(locale), QLocale::languageToCode(locale.language()), u"en"_s}) {
        const auto it = names.constFind(key);
        if (it != names.constEnd() && !it->isEmpty()) {
            return *it;
        }
    }
    return id;
}

namespace DotNetResources
{

namespace
{

bool readCompressedInt(const QByteArray &data, qsizetype &pos, quint32 *value)
{
    quint32 result = 0;
    for (int shift = 0; shift < 35; shift += 7) {
        if (pos >= data.size()) {
            return false;
        }
        const quint8 byte = quint8(data.at(pos++));
        result |= quint32(byte & 0x7f) << shift;
        if (!(byte & 0x80)) {
            *value = result;
            return true;
        }
    }
    return false;
}

bool readUInt32(const QByteArray &data, qsizetype pos, quint32 *value)
{
    if (pos < 0 || pos + 4 > data.size()) {
        return false;
    }
    *value = qFromLittleEndian<quint32>(data.constData() + pos);
    return true;
}

} // namespace

QMap<QString, QByteArray> parse(const QByteArray &data, qsizetype offset)
{
    // System.Resources.ResourceReader's format: a header, the type names,
    // name hashes and positions, then names and a data section. WPF stores
    // files as streams (type code 0x21), a length followed by the bytes.
    QMap<QString, QByteArray> entries;
    quint32 magic = 0, headerVersion = 0, skip = 0;
    if (!readUInt32(data, offset, &magic) || magic != 0xBEEFCACE || !readUInt32(data, offset + 4, &headerVersion)
        || !readUInt32(data, offset + 8, &skip)) {
        return entries;
    }
    qsizetype pos = offset + 12 + skip;
    quint32 version = 0, count = 0, typeCount = 0;
    if (!readUInt32(data, pos, &version) || !readUInt32(data, pos + 4, &count) || !readUInt32(data, pos + 8, &typeCount) || count > 100000
        || typeCount > 1000) {
        return entries;
    }
    pos += 12;
    for (quint32 i = 0; i < typeCount; ++i) {
        quint32 length = 0;
        if (!readCompressedInt(data, pos, &length)) {
            return entries;
        }
        pos += length;
    }
    pos += (8 - (pos - offset) % 8) % 8; // padding to 8 bytes
    pos += 4 * qsizetype(count); // name hashes
    const qsizetype positions = pos;
    pos += 4 * qsizetype(count);
    quint32 dataSection = 0;
    if (!readUInt32(data, pos, &dataSection)) {
        return entries;
    }
    const qsizetype names = pos + 4;

    for (quint32 i = 0; i < count; ++i) {
        quint32 namePosition = 0;
        if (!readUInt32(data, positions + 4 * qsizetype(i), &namePosition)) {
            return entries;
        }
        qsizetype p = names + namePosition;
        quint32 nameLength = 0;
        if (!readCompressedInt(data, p, &nameLength) || p + nameLength + 4 > data.size()) {
            return entries;
        }
        const QString name = QString::fromUtf16(reinterpret_cast<const char16_t *>(data.constData() + p), nameLength / 2);
        quint32 dataOffset = 0;
        readUInt32(data, p + nameLength, &dataOffset);
        qsizetype d = offset + dataSection + dataOffset;
        quint32 typeCode = 0;
        if (!readCompressedInt(data, d, &typeCode)) {
            continue;
        }
        if (typeCode == 0x20 || typeCode == 0x21) { // byte array or stream
            quint32 length = 0;
            if (readUInt32(data, d, &length) && d + 4 + length <= data.size()) {
                entries.insert(name.toLower(), data.mid(d + 4, length));
            }
        }
    }
    return entries;
}

QMap<QString, QByteArray> read(const QByteArray &assembly)
{
    // Manifest resources sit in the assembly as length-prefixed blobs;
    // finding them by their magic number avoids parsing the metadata.
    QMap<QString, QByteArray> entries;
    static const QByteArray magic("\xCE\xCA\xEF\xBE", 4);
    for (qsizetype pos = assembly.indexOf(magic); pos >= 0; pos = assembly.indexOf(magic, pos + 4)) {
        entries.insert(parse(assembly, pos));
    }
    return entries;
}

} // namespace DotNetResources

ClipartLibrary::ClipartLibrary(const QString &directory)
    : m_directory(directory)
{
}

QString ClipartLibrary::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/clipart"_s;
}

QString ClipartLibrary::directory() const
{
    return m_directory;
}

QList<ClipartCategory> ClipartLibrary::categories() const
{
    return m_categories;
}

QList<ClipartItem> ClipartLibrary::items() const
{
    return m_items;
}

QString ClipartLibrary::path(const ClipartItem &item) const
{
    return QDir(m_directory).filePath(item.fileName);
}

bool ClipartLibrary::load(QString *errorString)
{
    m_categories.clear();
    m_items.clear();
    QFile file(QDir(m_directory).filePath(QLatin1String(CatalogName)));
    if (!file.exists()) {
        addCategory({UserCategory, {{u"en"_s, u"My Clip Art"_s}}});
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return false;
    }
    if (!readCatalog(file.readAll(), &m_categories, &m_items, errorString)) {
        return false;
    }
    addCategory({UserCategory, {{u"en"_s, u"My Clip Art"_s}}});
    return true;
}

bool ClipartLibrary::save(QString *errorString) const
{
    QDir().mkpath(m_directory);
    QSaveFile file(QDir(m_directory).filePath(QLatin1String(CatalogName)));
    if (!file.open(QIODevice::WriteOnly) || file.write(writeCatalog(m_categories, m_items)) < 0 || !file.commit()) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return false;
    }
    return true;
}

void ClipartLibrary::addCategory(const ClipartCategory &category)
{
    for (ClipartCategory &existing : m_categories) {
        if (existing.id == category.id) {
            existing.names.insert(category.names);
            return;
        }
    }
    m_categories << category;
}

bool ClipartLibrary::readCatalog(const QByteArray &data, QList<ClipartCategory> *categories, QList<ClipartItem> *items, QString *errorString) const
{
    QXmlStreamReader xml(data);
    if (!xml.readNextStartElement() || xml.name() != "Clipart"_L1) {
        if (errorString) {
            *errorString = u"Not a clip art catalogue"_s;
        }
        return false;
    }
    while (xml.readNextStartElement()) {
        if (xml.name() == "Categories"_L1) {
            while (xml.readNextStartElement()) {
                ClipartCategory category;
                while (xml.readNextStartElement()) {
                    if (xml.name() == "CategoryId"_L1) {
                        category.id = xml.readElementText().trimmed();
                    } else if (xml.name() == "Names"_L1) {
                        while (xml.readNextStartElement()) {
                            category.names.insert(xml.name().toString(), xml.readElementText().trimmed());
                        }
                    } else {
                        xml.skipCurrentElement();
                    }
                }
                if (!category.id.isEmpty()) {
                    categories->append(category);
                }
            }
        } else if (xml.name() == "Arts"_L1) {
            while (xml.readNextStartElement()) {
                const QXmlStreamAttributes attributes = xml.attributes();
                ClipartItem item;
                item.categoryId = attributes.value("CategoryId"_L1).toString();
                item.fileName = attributes.value("FileName"_L1).toString();
                item.builtIn = attributes.value("BuiltIn"_L1).compare("True"_L1, Qt::CaseInsensitive) == 0;
                item.unicode = attributes.value("Unicode"_L1).toString();
                xml.skipCurrentElement();
                if (!item.fileName.isEmpty()) {
                    items->append(item);
                }
            }
        } else {
            xml.skipCurrentElement();
        }
    }
    if (xml.hasError()) {
        if (errorString) {
            *errorString = xml.errorString();
        }
        return false;
    }
    return true;
}

QByteArray ClipartLibrary::writeCatalog(const QList<ClipartCategory> &categories, const QList<ClipartItem> &items) const
{
    QByteArray data;
    QXmlStreamWriter xml(&data);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);
    xml.writeStartDocument();
    xml.writeStartElement("Clipart"_L1);
    xml.writeStartElement("Categories"_L1);
    for (const ClipartCategory &category : categories) {
        xml.writeStartElement("Category"_L1);
        xml.writeTextElement("CategoryId"_L1, category.id);
        xml.writeStartElement("Names"_L1);
        for (auto it = category.names.cbegin(); it != category.names.cend(); ++it) {
            xml.writeTextElement(it.key(), it.value());
        }
        xml.writeEndElement();
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeStartElement("Arts"_L1);
    for (const ClipartItem &item : items) {
        xml.writeEmptyElement("Art"_L1);
        xml.writeAttribute("CategoryId"_L1, item.categoryId);
        xml.writeAttribute("FileName"_L1, item.fileName);
        xml.writeAttribute("BuiltIn"_L1, item.builtIn ? "True"_L1 : "False"_L1);
        if (!item.unicode.isEmpty()) {
            xml.writeAttribute("Unicode"_L1, item.unicode);
        }
    }
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    return data;
}

QString ClipartLibrary::uniqueFileName(const QString &subdirectory, const QString &baseName) const
{
    const QDir dir(QDir(m_directory).filePath(subdirectory));
    QString name = baseName;
    for (int i = 1; dir.exists(name + u".png"_s); ++i) {
        name = baseName + u'-' + QString::number(i);
    }
    return subdirectory + u'/' + name + u".png"_s;
}

int ClipartLibrary::importDlsProgram(const QString &dllFileName, QString *errorString)
{
    QFile file(dllFileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return 0;
    }
    const QMap<QString, QByteArray> resources = DotNetResources::read(file.readAll());
    const QByteArray catalog = resources.value(QLatin1String(DlsCatalogResource));
    if (catalog.isEmpty()) {
        if (errorString) {
            *errorString = u"%1 contains no DYMO Label clip art; choose DYMO.DLS.dll from the DYMO Label Software folder."_s.arg(QFileInfo(dllFileName).fileName());
        }
        return 0;
    }

    QList<ClipartCategory> categories;
    QList<ClipartItem> items;
    if (!readCatalog(catalog, &categories, &items, errorString)) {
        return 0;
    }
    QDir().mkpath(QDir(m_directory).filePath(QLatin1String(DlsSubdirectory)));

    int imported = 0;
    for (const ClipartCategory &category : std::as_const(categories)) {
        addCategory(category);
    }
    for (ClipartItem item : std::as_const(items)) {
        const QByteArray png = resources.value(item.fileName.toLower());
        if (png.isEmpty()) {
            continue;
        }
        const QString fileName = QLatin1String(DlsSubdirectory) + u'/' + QFileInfo(item.fileName).fileName();
        // Importing again replaces the images rather than duplicating them.
        QFile out(QDir(m_directory).filePath(fileName));
        if (!out.open(QIODevice::WriteOnly) || out.write(png) != png.size()) {
            continue;
        }
        item.fileName = fileName;
        m_items.removeIf([&fileName](const ClipartItem &existing) {
            return existing.fileName == fileName;
        });
        m_items << item;
        ++imported;
    }
    if (!save(errorString)) {
        return 0;
    }
    return imported;
}

int ClipartLibrary::importDlsCatalog(const QString &xmlFileName, QString *errorString)
{
    QFile file(xmlFileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return 0;
    }
    QList<ClipartCategory> categories;
    QList<ClipartItem> items;
    if (!readCatalog(file.readAll(), &categories, &items, errorString)) {
        return 0;
    }
    for (const ClipartCategory &category : std::as_const(categories)) {
        addCategory(category);
    }

    // DLS records Windows paths; look for the images next to the catalogue,
    // by name, also when the path is relative.
    const QDir source = QFileInfo(xmlFileName).dir();
    int imported = 0;
    for (const ClipartItem &item : std::as_const(items)) {
        // Entries whose image is not there are skipped; in a DLS user list
        // the built-in ones point into the program, which importDlsProgram()
        // reads instead.
        QString name = item.fileName;
        name.replace(u'\\', u'/');
        QString path = source.filePath(name);
        if (!QFile::exists(path)) {
            path = source.filePath(QFileInfo(name).fileName());
        }
        if (!QFile::exists(path)) {
            continue;
        }
        if (importImages({path}, item.categoryId.isEmpty() ? UserCategory : item.categoryId) == 1) {
            ++imported;
        }
    }
    if (!save(errorString)) {
        return 0;
    }
    return imported;
}

int ClipartLibrary::importImages(const QStringList &fileNames, const QString &categoryId, QString *errorString)
{
    QDir().mkpath(QDir(m_directory).filePath(QLatin1String(UserSubdirectory)));
    int imported = 0;
    for (const QString &source : fileNames) {
        const QImage image(source);
        if (image.isNull()) {
            if (errorString) {
                *errorString = u"Could not read the image %1"_s.arg(source);
            }
            continue;
        }
        const QString fileName = uniqueFileName(QLatin1String(UserSubdirectory), QFileInfo(source).completeBaseName());
        if (!image.save(QDir(m_directory).filePath(fileName), "PNG")) {
            continue;
        }
        ClipartItem item;
        item.categoryId = categoryId;
        item.fileName = fileName;
        m_items << item;
        ++imported;
    }
    if (!categoryId.isEmpty() && std::none_of(m_categories.cbegin(), m_categories.cend(), [&categoryId](const ClipartCategory &c) {
            return c.id == categoryId;
        })) {
        addCategory({categoryId, {{u"en"_s, categoryId}}});
    }
    if (imported > 0 && !save(errorString)) {
        return 0;
    }
    return imported;
}

bool ClipartLibrary::exportTo(const QString &directory, const QStringList &categoryIds, QString *errorString) const
{
    if (!QDir().mkpath(directory)) {
        if (errorString) {
            *errorString = u"Could not create %1"_s.arg(directory);
        }
        return false;
    }
    QList<ClipartItem> exported;
    for (const ClipartItem &item : m_items) {
        if (!categoryIds.isEmpty() && !categoryIds.contains(item.categoryId)) {
            continue;
        }
        // Flat, with names DLS can find next to its Clipart.xml.
        QString name = QFileInfo(item.fileName).fileName();
        for (int i = 1; QFile::exists(QDir(directory).filePath(name)); ++i) {
            name = QFileInfo(item.fileName).completeBaseName() + u'-' + QString::number(i) + u".png"_s;
        }
        if (!QFile::copy(path(item), QDir(directory).filePath(name))) {
            continue;
        }
        ClipartItem copy = item;
        copy.fileName = name;
        // In DLS, anything that does not come with DLS is the user's own.
        copy.builtIn = false;
        exported << copy;
    }
    QList<ClipartCategory> categories;
    for (const ClipartCategory &category : m_categories) {
        if (categoryIds.isEmpty() || categoryIds.contains(category.id)) {
            categories << category;
        }
    }
    QSaveFile file(QDir(directory).filePath(QLatin1String(CatalogName)));
    if (!file.open(QIODevice::WriteOnly) || file.write(writeCatalog(categories, exported)) < 0 || !file.commit()) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return false;
    }
    return true;
}

bool ClipartLibrary::remove(const ClipartItem &item)
{
    const qsizetype removed = m_items.removeIf([&item](const ClipartItem &existing) {
        return existing.fileName == item.fileName;
    });
    if (removed == 0) {
        return false;
    }
    QFile::remove(path(item));
    return save();
}

} // namespace LabelCreator
