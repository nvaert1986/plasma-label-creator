/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow_p.h"

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;
using MainWindowPrivate::labelFileFilter;
using MainWindowPrivate::settings;

// Editing objects: inserting, deleting, copying, placing and ordering them.

void MainWindow::insertObject(ObjectFactory::Kind kind, std::optional<QPointF> at)
{
    QByteArray imageData;
    if (kind == ObjectFactory::Kind::Image) {
        imageData = chooseImage(this);
        if (imageData.isEmpty()) {
            return;
        }
    }
    insert(ObjectFactory::create(kind, m_editor->document(), imageData), at);
}

void MainWindow::insertClipart(const QString &fileName, std::optional<QPointF> at)
{
    const QImage image(fileName);
    if (image.isNull()) {
        KMessageBox::error(this, i18n("Could not read the clip art %1.", fileName));
        return;
    }
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    insert(ObjectFactory::create(ObjectFactory::Kind::Image, m_editor->document(), png), at);
}

void MainWindow::insert(LabelObject object, std::optional<QPointF> at)
{
    LabelDocument document = m_editor->document();
    if (at) {
        ObjectFactory::centreOn(object, document, *at);
    }
    const int index = document.addObject(object);
    m_editor->apply(document, i18n("Insert Object"), index);
    m_view->setFocus();
}

void MainWindow::useAddress(const QString &address)
{
    LabelDocument document = m_editor->document();
    for (LabelObject &object : document.objects) {
        if (auto *target = std::get_if<AddressObject>(&object)) {
            const TextSpan first = target->spans.isEmpty() ? TextSpan{QString(), LabelFont(), Qt::black} : target->spans.constFirst();
            target->spans = {{address, first.font, first.color}};
            target->fitLineFonts();
            m_editor->apply(document, i18n("Use Address"));
            return;
        }
    }
    statusBar()->showMessage(i18n("Add an address object to the label to put the address on it."), 5000);
}

void MainWindow::advanceCounters(int printedLabels)
{
    LabelDocument document = m_editor->document();
    bool counted = false;
    for (LabelObject &object : document.objects) {
        if (auto *counter = std::get_if<CounterObject>(&object)) {
            counter->current += printedLabels * counter->increment;
            counted = true;
        }
    }
    if (counted) {
        m_editor->apply(document, i18n("Advance Counters"));
    }
}

void MainWindow::editObject(int index)
{
    if (index < 0 || index >= m_editor->document().objects.size()) {
        return;
    }
    LabelDocument document = m_editor->document();
    if (editObjectProperties(this, document.objects[index], document.cellOf(index))) {
        m_editor->apply(document, i18n("Change Object Properties"));
    }
}

void MainWindow::deleteObject()
{
    const int index = m_editor->selection();
    if (index < 0) {
        return;
    }
    LabelDocument document = m_editor->document();
    document.removeObject(index);
    m_editor->apply(document, i18n("Delete Object"), -1);
}

void MainWindow::duplicateObject()
{
    const int index = m_editor->selection();
    if (index < 0) {
        return;
    }
    LabelDocument document = m_editor->document();
    LabelObject copy = document.objects.at(index);
    ObjectBase &base = objectBase(copy);
    base.name = ObjectFactory::copyName(document, base.name);
    base.bounds.translate(144, 144);
    const int added = document.addObject(copy);
    m_editor->apply(document, i18n("Duplicate Object"), added);
}

void MainWindow::copyObject()
{
    const int index = m_editor->selection();
    if (index >= 0) {
        QApplication::clipboard()->setMimeData(LabelClipboard::mimeData(m_editor->document().objects.at(index), m_editor->document()));
    }
}

void MainWindow::cutObject()
{
    const int index = m_editor->selection();
    if (index < 0) {
        return;
    }
    copyObject();
    LabelDocument document = m_editor->document();
    document.removeObject(index);
    m_editor->apply(document, i18n("Cut Object"), -1);
}

void MainWindow::paste()
{
    const QList<LabelObject> objects = LabelClipboard::objectsToPaste(QApplication::clipboard()->mimeData(), m_editor->document());
    if (objects.isEmpty()) {
        return;
    }
    LabelDocument document = m_editor->document();
    int last = -1;
    for (const LabelObject &object : objects) {
        last = document.addObject(object);
    }
    m_editor->apply(document, i18n("Paste"), last);
    m_view->setFocus();
}

void MainWindow::positionObject(Qt::Alignment alignment)
{
    const int index = m_editor->selection();
    if (index < 0) {
        return;
    }
    LabelDocument document = m_editor->document();
    ObjectFactory::alignOnLabel(document.objects[index], document, alignment);
    m_editor->apply(document, i18n("Position Object"));
}

void MainWindow::setOrientation(PaperOrientation orientation)
{
    if (m_editor->document().orientation != orientation) {
        rotateLabel();
    }
}

void MainWindow::raiseObject()
{
    const int index = m_editor->selection();
    if (index < 0) {
        return;
    }
    LabelDocument document = m_editor->document();
    document.moveObject(index, int(document.objects.size() - 1));
    m_editor->apply(document, i18n("Bring to Front"), int(document.objects.size() - 1));
}

void MainWindow::lowerObject()
{
    const int index = m_editor->selection();
    if (index < 0) {
        return;
    }
    LabelDocument document = m_editor->document();
    document.moveObject(index, 0);
    m_editor->apply(document, i18n("Send to Back"), 0);
}
