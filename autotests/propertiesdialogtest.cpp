/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "objectfactory.h"
#include "propertiesdialog.h"

#include <QApplication>
#include <QBuffer>
#include <QDialog>
#include <QImage>
#include <QTest>
#include <QTimer>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

QByteArray png()
{
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::black);
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return data;
}

// Presses OK in the properties dialog as soon as it is open.
void acceptSoon()
{
    QTimer::singleShot(0, []() {
        if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
            dialog->accept();
        }
    });
}

} // namespace

class PropertiesDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void okWithoutChangesChangesNothing_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<QPointF>("position");
        for (int kind = int(ObjectFactory::Kind::Text); kind <= int(ObjectFactory::Kind::CurvedText); ++kind) {
            QTest::addRow("kind %d on the label", kind) << kind << QPointF(150, 150);
            // DLS lets objects stick out over the label's top-left edge.
            QTest::addRow("kind %d over the edge", kind) << kind << QPointF(-150, -75);
        }
    }

    void okWithoutChangesChangesNothing()
    {
        QFETCH(int, kind);
        QFETCH(QPointF, position);
        const LabelDocument document = ObjectFactory::newLabel(u"30252 Address"_s, u"w79h252"_s, QSizeF(78.96, 252));
        LabelObject object = ObjectFactory::create(ObjectFactory::Kind(kind), document, png());
        objectBase(object).bounds.moveTopLeft(position);
        const LabelObject before = object;

        acceptSoon();
        QVERIFY(editObjectProperties(nullptr, object));
        QCOMPARE(objectBase(object).bounds, objectBase(before).bounds);
        QCOMPARE(objectBase(object).name, objectBase(before).name);
        QVERIFY2(object == before, "an object property changed although nothing was edited");
    }
};

QTEST_MAIN(PropertiesDialogTest)

#include "propertiesdialogtest.moc"
