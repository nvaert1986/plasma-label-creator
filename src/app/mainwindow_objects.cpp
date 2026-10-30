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

void MainWindow::advanceCounters(int printedLabels, const QHash<int, int> &sharedNext)
{
    LabelDocument document = m_editor->document();
    bool counted = false;
    for (int i = 0; i < document.objects.size(); ++i) {
        if (auto *counter = std::get_if<CounterObject>(&document.objects[i])) {
            counter->current = sharedNext.value(i, counter->current + printedLabels * counter->increment);
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
    const QList<int> selected = m_editor->selectedObjects();
    if (selected.isEmpty()) {
        return;
    }
    LabelDocument document = m_editor->document();
    for (auto it = selected.crbegin(); it != selected.crend(); ++it) {
        document.removeObject(*it);
    }
    m_editor->apply(document, selected.size() > 1 ? i18n("Delete Objects") : i18n("Delete Object"), -1);
}

void MainWindow::duplicateObject()
{
    const QList<int> selected = m_editor->selectedObjects();
    if (selected.isEmpty()) {
        return;
    }
    // Through the clipboard's format, without touching the clipboard: copies
    // get names and groups of their own and land a little further on.
    LabelDocument document = m_editor->document();
    QList<LabelObject> originals;
    for (const int index : selected) {
        originals << document.objects.at(index);
    }
    const std::unique_ptr<QMimeData> data(LabelClipboard::mimeData(originals, document));
    QList<int> added;
    for (const LabelObject &copy : LabelClipboard::objectsToPaste(data.get(), document)) {
        added << document.addObject(copy);
    }
    m_editor->apply(document, selected.size() > 1 ? i18n("Duplicate Objects") : i18n("Duplicate Object"), added);
}

void MainWindow::copyObject()
{
    const QList<int> selected = m_editor->selectedObjects();
    if (selected.isEmpty()) {
        return;
    }
    QList<LabelObject> objects;
    for (const int index : selected) {
        objects << m_editor->document().objects.at(index);
    }
    QApplication::clipboard()->setMimeData(LabelClipboard::mimeData(objects, m_editor->document()));
}

void MainWindow::cutObject()
{
    const QList<int> selected = m_editor->selectedObjects();
    if (selected.isEmpty()) {
        return;
    }
    copyObject();
    LabelDocument document = m_editor->document();
    for (auto it = selected.crbegin(); it != selected.crend(); ++it) {
        document.removeObject(*it);
    }
    m_editor->apply(document, selected.size() > 1 ? i18n("Cut Objects") : i18n("Cut Object"), -1);
}

void MainWindow::paste()
{
    const QList<LabelObject> objects = LabelClipboard::objectsToPaste(QApplication::clipboard()->mimeData(), m_editor->document());
    if (objects.isEmpty()) {
        return;
    }
    LabelDocument document = m_editor->document();
    QList<int> added;
    for (const LabelObject &object : objects) {
        added << document.addObject(object);
    }
    m_editor->apply(document, i18n("Paste"), added);
    m_view->setFocus();
}

void MainWindow::positionObject(Qt::Alignment alignment)
{
    // The selection moves as one block; locked objects stay.
    LabelDocument document = m_editor->document();
    QList<int> movable;
    QRectF around;
    for (const int index : m_editor->selectedObjects()) {
        if (!document.isLocked(index)) {
            // What shows: an object at an angle reaches beyond its box.
            const QRectF bounds = objectOutline(objectBase(document.objects.at(index))).boundingRect();
            movable << index;
            around = around.isNull() ? bounds : around.united(bounds);
        }
    }
    if (movable.isEmpty()) {
        return;
    }
    LabelObject block = ShapeObject();
    objectBase(block).bounds = around;
    ObjectFactory::alignOnLabel(block, document, alignment);
    const QPointF delta = objectBase(block).bounds.topLeft() - around.topLeft();
    for (const int index : std::as_const(movable)) {
        objectBase(document.objects[index]).bounds.translate(delta);
    }
    m_editor->apply(document, i18n("Position Object"));
}

void MainWindow::alignObjects(Qt::Alignment alignment)
{
    LabelDocument document = m_editor->document();
    if (Arrange::align(document, m_editor->selectedObjects(), alignment)) {
        m_editor->apply(document, i18n("Align Objects"));
    }
}

void MainWindow::distributeObjects(Qt::Orientation orientation)
{
    LabelDocument document = m_editor->document();
    if (Arrange::distribute(document, m_editor->selectedObjects(), orientation)) {
        m_editor->apply(document, i18n("Distribute Objects"));
    }
}

void MainWindow::groupObjects()
{
    LabelDocument document = m_editor->document();
    Arrange::group(document, m_editor->selectedObjects());
    m_editor->apply(document, i18n("Group Objects"), Arrange::withGroups(document, m_editor->selectedObjects()));
}

void MainWindow::ungroupObjects()
{
    LabelDocument document = m_editor->document();
    Arrange::ungroup(document, m_editor->selectedObjects());
    m_editor->apply(document, i18n("Ungroup Objects"));
}

void MainWindow::setObjectsLocked(bool locked)
{
    LabelDocument document = m_editor->document();
    Arrange::setLocked(document, m_editor->selectedObjects(), locked);
    m_editor->apply(document, locked ? i18n("Lock Objects") : i18n("Unlock Objects"));
}

void MainWindow::setObjectsPrinted(bool printed)
{
    LabelDocument document = m_editor->document();
    for (const int index : m_editor->selectedObjects()) {
        objectBase(document.objects[index]).printed = printed;
    }
    m_editor->apply(document, printed ? i18n("Print Objects") : i18n("Do Not Print Objects"));
}

void MainWindow::editLayers()
{
    LayersDialog dialog(m_editor->document().layers, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    LabelDocument document = m_editor->document();
    document.changeLayers(dialog.layers(), dialog.renamed());
    // Objects on a layer that no longer shows or can be edited are not selected.
    QList<int> selection;
    for (const int index : m_editor->selectedObjects()) {
        if (document.isSelectable(index)) {
            selection << index;
        }
    }
    m_editor->apply(document, i18n("Change Layers"), selection);
}

void MainWindow::moveToLayer(const QString &name)
{
    LabelDocument document = m_editor->document();
    for (const int index : m_editor->selectedObjects()) {
        objectBase(document.objects[index]).layer = name;
    }
    QList<int> selection;
    for (const int index : m_editor->selectedObjects()) {
        if (document.isSelectable(index)) {
            selection << index;
        }
    }
    m_editor->apply(document, i18n("Move to Layer"), selection);
}

void MainWindow::selectAllObjects()
{
    QList<int> all;
    for (int i = 0; i < m_editor->document().objects.size(); ++i) {
        if (m_editor->document().isSelectable(i)) {
            all << i;
        }
    }
    m_editor->setSelectedObjects(all, m_editor->selection());
}

void MainWindow::setOrientation(PaperOrientation orientation)
{
    if (m_editor->document().orientation != orientation) {
        rotateLabel();
    }
}

void MainWindow::raiseObject()
{
    reorderSelected(i18n("Bring to Front"), &Arrange::bringToFront);
}

void MainWindow::lowerObject()
{
    reorderSelected(i18n("Send to Back"), &Arrange::sendToBack);
}

void MainWindow::reorderSelected(const QString &description, QList<int> (*reorder)(LabelDocument &, const QList<int> &))
{
    // The selected objects keep their order among themselves; the current
    // object is followed to where it went.
    QList<int> selected = m_editor->selectedObjects();
    if (selected.isEmpty()) {
        return;
    }
    selected << m_editor->selection();
    LabelDocument document = m_editor->document();
    QList<int> moved = reorder(document, selected);
    const int current = moved.takeLast();
    m_editor->apply(document, description, moved, current);
}

void MainWindow::setupArrangeActions()
{
    KActionCollection *actions = actionCollection();

    QAction *selectAll = KStandardAction::selectAll(this, &MainWindow::selectAllObjects, actions);
    // On the label only: Ctrl+A in the address book selects its entries.
    selectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_view->addAction(selectAll);

    // Lining the selected objects up with each other.
    const auto addAlign = [&](const QString &name, const QString &text, const QString &icon, Qt::Alignment alignment) {
        QAction *action = actions->addAction(name, this, [this, alignment]() {
            alignObjects(alignment);
        });
        action->setText(text);
        action->setIcon(QIcon::fromTheme(icon));
    };
    addAlign(u"arrange_align_left"_s, i18n("Left Edges"), u"align-horizontal-left"_s, Qt::AlignLeft);
    addAlign(u"arrange_align_hcenter"_s, i18n("Centres Horizontally"), u"align-horizontal-center"_s, Qt::AlignHCenter);
    addAlign(u"arrange_align_right"_s, i18n("Right Edges"), u"align-horizontal-right"_s, Qt::AlignRight);
    addAlign(u"arrange_align_top"_s, i18n("Top Edges"), u"align-vertical-top"_s, Qt::AlignTop);
    addAlign(u"arrange_align_vcenter"_s, i18n("Centres Vertically"), u"align-vertical-center"_s, Qt::AlignVCenter);
    addAlign(u"arrange_align_bottom"_s, i18n("Bottom Edges"), u"align-vertical-bottom"_s, Qt::AlignBottom);

    QAction *horizontal = actions->addAction(u"arrange_distribute_horizontal"_s, this, [this]() {
        distributeObjects(Qt::Horizontal);
    });
    horizontal->setText(i18n("Distribute Horizontally"));
    horizontal->setIcon(QIcon::fromTheme(u"distribute-horizontal-gaps"_s));
    QAction *vertical = actions->addAction(u"arrange_distribute_vertical"_s, this, [this]() {
        distributeObjects(Qt::Vertical);
    });
    vertical->setText(i18n("Distribute Vertically"));
    vertical->setIcon(QIcon::fromTheme(u"distribute-vertical-gaps"_s));

    QAction *group = actions->addAction(u"arrange_group"_s, this, &MainWindow::groupObjects);
    group->setText(i18n("Group"));
    group->setIcon(QIcon::fromTheme(u"object-group"_s));
    actions->setDefaultShortcut(group, QKeySequence(Qt::CTRL | Qt::Key_G));
    QAction *ungroup = actions->addAction(u"arrange_ungroup"_s, this, &MainWindow::ungroupObjects);
    ungroup->setText(i18n("Ungroup"));
    ungroup->setIcon(QIcon::fromTheme(u"object-ungroup"_s));
    actions->setDefaultShortcut(ungroup, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));

    // Layers, and objects that only help to design.
    QAction *layers = actions->addAction(u"layout_layers"_s, this, &MainWindow::editLayers);
    layers->setText(i18n("La&yers…"));
    layers->setIcon(QIcon::fromTheme(u"layer-visible-on"_s));
    auto *moveTo = new KActionMenu(QIcon::fromTheme(u"layer-next"_s), i18n("Move to Layer"), this);
    moveTo->setPopupMode(QToolButton::InstantPopup);
    actions->addAction(u"layout_move_to_layer"_s, moveTo);
    connect(moveTo->menu(), &QMenu::aboutToShow, this, [this, moveTo]() {
        QMenu *menu = moveTo->menu();
        menu->clear();
        const QList<int> selected = m_editor->selectedObjects();
        const auto addLayer = [&](const QString &text, const QString &name) {
            QAction *action = menu->addAction(text, this, [this, name]() {
                moveToLayer(name);
            });
            action->setCheckable(true);
            action->setChecked(!selected.isEmpty() && std::all_of(selected.cbegin(), selected.cend(), [this, &name](int index) {
                return objectBase(m_editor->document().objects.at(index)).layer == name;
            }));
        };
        addLayer(i18n("The Label Itself"), QString());
        for (const Layer &layer : m_editor->document().layers) {
            addLayer(layer.name, layer.name);
        }
        menu->addSeparator();
        menu->addAction(QIcon::fromTheme(u"layer-visible-on"_s), i18n("Edit Layers…"), this, &MainWindow::editLayers);
    });
    m_noPrint = new KToggleAction(QIcon::fromTheme(u"document-print-direct"_s), i18n("Do Not Print"), this);
    m_noPrint->setToolTip(i18n("Shown faded while designing, left out when printing"));
    actions->addAction(u"object_no_print"_s, m_noPrint);
    connect(m_noPrint, &QAction::triggered, this, [this](bool noPrint) {
        setObjectsPrinted(!noPrint);
    });

    m_lock = new KToggleAction(QIcon::fromTheme(u"object-locked"_s), i18n("Lock Position and Size"), this);
    m_lock->setToolTip(i18n("Locked objects cannot be moved or resized"));
    actions->addAction(u"arrange_lock"_s, m_lock);
    connect(m_lock, &QAction::triggered, this, &MainWindow::setObjectsLocked);
}
