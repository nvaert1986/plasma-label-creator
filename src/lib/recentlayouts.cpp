/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "recentlayouts.h"

#include "labelformat.h"
#include "labelrenderer.h"
#include "units.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace
{

constexpr QLatin1StringView IndexName("index.json");

QByteArray readAll(const QString &fileName)
{
    QFile file(fileName);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

RecentLayouts::RecentLayouts(const QString &directory)
    : m_directory(directory)
{
}

QString RecentLayouts::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/recent-layouts"_s;
}

QList<RecentLayout> RecentLayouts::readIndex() const
{
    QList<RecentLayout> layouts;
    const QDir directory(m_directory);
    const QJsonArray entries = QJsonDocument::fromJson(readAll(directory.filePath(IndexName))).array();
    for (const auto &value : entries) {
        const QJsonObject entry = value.toObject();
        RecentLayout layout;
        layout.title = entry.value("title"_L1).toString();
        layout.fileName = directory.filePath(entry.value("file"_L1).toString());
        layout.thumbnail = directory.filePath(entry.value("thumbnail"_L1).toString());
        layout.source = entry.value("source"_L1).toString();
        layout.used = QDateTime::fromString(entry.value("used"_L1).toString(), Qt::ISODateWithMs);
        // Files removed behind our back are dropped.
        if (QFileInfo::exists(layout.fileName)) {
            layouts << layout;
        }
    }
    std::stable_sort(layouts.begin(), layouts.end(), [](const RecentLayout &a, const RecentLayout &b) {
        return a.used > b.used;
    });
    return layouts;
}

bool RecentLayouts::writeIndex(const QList<RecentLayout> &layouts, QString *errorString) const
{
    QJsonArray entries;
    for (const RecentLayout &layout : layouts) {
        entries.append(QJsonObject{
            {"title"_L1, layout.title},
            {"file"_L1, QFileInfo(layout.fileName).fileName()},
            {"thumbnail"_L1, QFileInfo(layout.thumbnail).fileName()},
            {"source"_L1, layout.source},
            {"used"_L1, layout.used.toString(Qt::ISODateWithMs)},
        });
    }
    QSaveFile file(QDir(m_directory).filePath(IndexName));
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(entries).toJson()) < 0 || !file.commit()) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return false;
    }
    return true;
}

QList<RecentLayout> RecentLayouts::layouts() const
{
    return readIndex();
}

bool RecentLayouts::add(const LabelDocument &document, const QString &sourceFile, QString *errorString)
{
    if (!QDir().mkpath(m_directory)) {
        if (errorString) {
            *errorString = u"Could not create %1"_s.arg(m_directory);
        }
        return false;
    }
    const QByteArray content = LabelWriter::toByteArray(document);
    QList<RecentLayout> layouts = readIndex();

    // Printed before, from the same file or as the same label: reuse its place.
    RecentLayout layout;
    for (qsizetype i = 0; i < layouts.size(); ++i) {
        const RecentLayout &existing = layouts.at(i);
        if ((!sourceFile.isEmpty() && existing.source == sourceFile) || readAll(existing.fileName) == content) {
            layout = layouts.takeAt(i);
            break;
        }
    }
    if (layout.fileName.isEmpty()) {
        // Named by time; a counter keeps two prints in one millisecond apart.
        const QDir directory(m_directory);
        const QString stem = u"layout-"_s + QString::number(QDateTime::currentMSecsSinceEpoch());
        QString name = stem;
        for (int i = 2; QFileInfo::exists(directory.filePath(name + u".label"_s)); ++i) {
            name = stem + u'-' + QString::number(i);
        }
        layout.fileName = directory.filePath(name + u".label"_s);
        layout.thumbnail = directory.filePath(name + u".png"_s);
    }
    if (!sourceFile.isEmpty()) {
        layout.source = sourceFile;
    }
    layout.title = layout.source.isEmpty() ? document.paperName : QFileInfo(layout.source).completeBaseName();
    layout.used = QDateTime::currentDateTime();

    QSaveFile file(layout.fileName);
    if (!file.open(QIODevice::WriteOnly) || file.write(content) < 0 || !file.commit()) {
        if (errorString) {
            *errorString = file.errorString();
        }
        return false;
    }
    const QSizeF canvas = document.canvasSize();
    const qreal longest = std::max(canvas.width(), canvas.height());
    if (longest > 0) {
        LabelRenderer renderer(LabelRenderer::Mode::Preview);
        renderer.renderImage(document, ThumbnailSize * Units::TwipsPerInch / longest).save(layout.thumbnail, "PNG");
    }

    layouts.prepend(layout);
    while (layouts.size() > MaxLayouts) {
        const RecentLayout oldest = layouts.takeLast();
        QFile::remove(oldest.fileName);
        QFile::remove(oldest.thumbnail);
    }
    return writeIndex(layouts, errorString);
}

std::optional<LabelDocument> RecentLayouts::load(const RecentLayout &layout, QString *errorString) const
{
    LabelReader reader;
    const auto document = reader.readFile(layout.fileName);
    if (!document && errorString) {
        *errorString = reader.errorString();
    }
    return document;
}

void RecentLayouts::remove(const RecentLayout &layout)
{
    QList<RecentLayout> layouts = readIndex();
    layouts.removeIf([&layout](const RecentLayout &existing) {
        return existing.fileName == layout.fileName;
    });
    QFile::remove(layout.fileName);
    QFile::remove(layout.thumbnail);
    writeIndex(layouts, nullptr);
}

void RecentLayouts::clear()
{
    for (const RecentLayout &layout : readIndex()) {
        QFile::remove(layout.fileName);
        QFile::remove(layout.thumbnail);
    }
    writeIndex({}, nullptr);
}

} // namespace LabelCreator
