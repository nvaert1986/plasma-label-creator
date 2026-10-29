/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace LabelCreator
{

struct ClipartCategory {
    QString id;
    QMap<QString, QString> names; // by language, e.g. "nl" or "pt-BR"

    /** The name in the user's language, falling back to English or the id. */
    QString displayName() const;
};

struct ClipartItem {
    QString categoryId;
    QString fileName; // relative to the library's directory
    bool builtIn = false;
    QString unicode;
};

/**
 * The user's clip art: images in a directory, listed in a Clipart.xml in
 * the format DLS 8 uses for its own clip art, so libraries move between
 * Plasma Label Creator and DLS as they are.
 */
class ClipartLibrary
{
public:
    /** The category DLS calls "My Clip Art". */
    static inline const QString UserCategory = QStringLiteral("UserDefined");

    explicit ClipartLibrary(const QString &directory = defaultDirectory());

    static QString defaultDirectory();
    QString directory() const;

    bool load(QString *errorString = nullptr);
    bool save(QString *errorString = nullptr) const;

    QList<ClipartCategory> categories() const;
    QList<ClipartItem> items() const;
    QString path(const ClipartItem &item) const;

    /**
     * Imports DLS's own clip art from its DYMO.DLS.dll: the images and the
     * categories embedded in it. For a user's own copy of DLS; Plasma Label Creator
     * itself ships none of it. Returns the number of images imported.
     */
    int importDlsProgram(const QString &dllFileName, QString *errorString = nullptr);
    /**
     * Imports a DLS Clipart.xml (DLS's settings folder holds the user's),
     * with images found by name next to it. Returns the number imported.
     */
    int importDlsCatalog(const QString &xmlFileName, QString *errorString = nullptr);
    /** Copies images into @p categoryId, as PNG. Returns the number imported. */
    int importImages(const QStringList &fileNames, const QString &categoryId = UserCategory, QString *errorString = nullptr);

    /** Writes the images of @p categoryIds (all if empty) and a Clipart.xml to @p directory. */
    bool exportTo(const QString &directory, const QStringList &categoryIds = {}, QString *errorString = nullptr) const;

    /** Removes an item and its image. */
    bool remove(const ClipartItem &item);

private:
    void addCategory(const ClipartCategory &category);
    bool readCatalog(const QByteArray &xml, QList<ClipartCategory> *categories, QList<ClipartItem> *items, QString *errorString) const;
    QByteArray writeCatalog(const QList<ClipartCategory> &categories, const QList<ClipartItem> &items) const;
    QString uniqueFileName(const QString &subdirectory, const QString &baseName) const;

    QString m_directory;
    QList<ClipartCategory> m_categories;
    QList<ClipartItem> m_items;
};

/**
 * Reads the embedded resources of .NET assemblies: the ".resources" blobs
 * that hold, among others, the files of a WPF application.
 */
namespace DotNetResources
{
/** All file entries of every .resources blob in @p assembly, by lowercase name. */
QMap<QString, QByteArray> read(const QByteArray &assembly);
/** The entries of one .resources blob starting at @p offset. */
QMap<QString, QByteArray> parse(const QByteArray &data, qsizetype offset);
}

} // namespace LabelCreator
