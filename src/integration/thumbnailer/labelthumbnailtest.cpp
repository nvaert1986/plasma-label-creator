/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The thumbnailer, loaded as KIO loads it, drawing the test labels.

#include <KIO/ThumbnailCreator>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <QTest>

using namespace Qt::Literals::StringLiterals;

class LabelThumbnailTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void drawsLabels_data()
    {
        QTest::addColumn<QString>("file");
        QTest::newRow("die-cut") << u"address.label"_s;
        QTest::newRow("continuous") << u"continuous.label"_s;
        QTest::newRow("tape") << u"tape.label"_s;
    }

    void drawsLabels()
    {
        QFETCH(QString, file);
        const KPluginMetaData metaData(QStringLiteral(THUMBNAILER));
        QVERIFY(metaData.isValid());
        QVERIFY(metaData.mimeTypes().contains(u"application/x-dymo-label"_s));
        auto result = KPluginFactory::instantiatePlugin<KIO::ThumbnailCreator>(metaData);
        QVERIFY2(result, qPrintable(result.errorString));
        const QUrl url = QUrl::fromLocalFile(QStringLiteral(TEST_DATA) + u'/' + file);
        const KIO::ThumbnailResult thumbnail = result.plugin->create(KIO::ThumbnailRequest(url, QSize(256, 256), u"application/x-dymo-label"_s, 1.0, 0));
        QVERIFY(thumbnail.isValid());
        const QImage image = thumbnail.image();
        // The label's longer side is about the size asked for.
        QVERIFY2(std::max(image.width(), image.height()) >= 200 && std::max(image.width(), image.height()) <= 300,
                 qPrintable(u"%1x%2"_s.arg(image.width()).arg(image.height())));
        delete result.plugin;
    }

    void failsOnOtherFiles()
    {
        auto result = KPluginFactory::instantiatePlugin<KIO::ThumbnailCreator>(KPluginMetaData(QStringLiteral(THUMBNAILER)));
        QVERIFY(result);
        const QUrl url = QUrl::fromLocalFile(QStringLiteral(TEST_DATA) + u"/addresses.ABX"_s);
        QVERIFY(!result.plugin->create(KIO::ThumbnailRequest(url, QSize(256, 256), u"application/x-dymo-label"_s, 1.0, 0)).isValid());
        delete result.plugin;
    }
};

QTEST_MAIN(LabelThumbnailTest)
#include "labelthumbnailtest.moc"
