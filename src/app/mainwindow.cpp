/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow_p.h"

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;
using MainWindowPrivate::labelFileFilter;
using MainWindowPrivate::settings;

namespace
{

// Used for new labels until a label type has been chosen once.
const PaperSize FallbackPaper{u"w79h252"_s, u"30252 Address"_s, QSizeF(78.96, 252), QRectF(), QString(), QRectF(), 0, {}};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent)
    , m_editor(new LabelEditor(this))
{
    m_sidebar = new Sidebar;
    m_view = new LabelView(m_editor);
    auto *scroll = new QScrollArea;
    scroll->setWidget(m_view);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_printBar = new PrintBar;

    // The canvas with rulers along the top and left, as in DLS.
    auto *canvas = new QWidget;
    auto *canvasLayout = new QGridLayout(canvas);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    canvasLayout->setSpacing(0);
    auto *corner = new QWidget;
    corner->setFixedSize(RulerWidget::Thickness, RulerWidget::Thickness);
    canvasLayout->addWidget(corner, 0, 0);
    canvasLayout->addWidget(new RulerWidget(Qt::Horizontal, m_view), 0, 1);
    canvasLayout->addWidget(new RulerWidget(Qt::Vertical, m_view), 1, 0);
    canvasLayout->addWidget(scroll, 1, 1);

    // Label information and zoom under the canvas, printing below that.
    auto *centre = new QWidget;
    auto *centreLayout = new QVBoxLayout(centre);
    centreLayout->setContentsMargins(0, 0, 0, 0);
    centreLayout->setSpacing(0);
    centreLayout->addWidget(canvas, 1);
    centreLayout->addWidget(createInfoBar());
    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    centreLayout->addWidget(line);
    centreLayout->addWidget(m_printBar);

    auto *splitter = new QSplitter;
    splitter->addWidget(m_sidebar);
    splitter->addWidget(centre);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({300, 750});
    setCentralWidget(splitter);

    setupActions();
    // First-run size; setupGUI() restores the user's own size after that.
    resize(1000, 650);
    setupGUI(Default, QStringLiteral("plasma-label-creatorui.rc"));

    connect(m_editor, &LabelEditor::documentChanged, this, &MainWindow::updateStatus);
    connect(m_editor, &LabelEditor::documentChanged, this, &MainWindow::updateActions);
    connect(m_editor, &LabelEditor::documentChanged, this, &MainWindow::updateDataRows);
    connect(m_editor, &LabelEditor::selectionChanged, this, &MainWindow::updateActions);
    connect(m_editor, &LabelEditor::documentChanged, this, &MainWindow::updateFormatActions);
    connect(m_editor, &LabelEditor::selectionChanged, this, &MainWindow::updateFormatActions);
    connect(m_editor, &LabelEditor::modifiedChanged, this, &MainWindow::updateCaption);
    connect(m_editor, &LabelEditor::fileNameChanged, this, &MainWindow::updateCaption);
    connect(m_view, &LabelView::editObjectRequested, this, &MainWindow::editObject);
    connect(m_view, &LabelView::zoomChanged, this, &MainWindow::updateStatus);
    connect(m_view, &LabelView::contextMenuRequested, this, [this](const QPoint &pos) {
        if (auto *menu = qobject_cast<QMenu *>(factory()->container(u"object_popup"_s, this))) {
            menu->popup(pos);
        }
    });
    connect(m_sidebar, &Sidebar::paperChosen, this, &MainWindow::changePaper);
    connect(m_sidebar, &Sidebar::insertRequested, this, [this](ObjectFactory::Kind kind) {
        insertObject(kind);
    });
    connect(m_sidebar, &Sidebar::clipartRequested, this, [this](const QString &fileName) {
        insertClipart(fileName);
    });
    connect(m_view, &LabelView::objectDropped, this, &MainWindow::insertObject);
    connect(m_view, &LabelView::imagesDropped, this, [this](const QStringList &fileNames, const QPointF &twips) {
        // Several images fan out a little, so none hides another.
        const qreal step = Units::mmToTwips(3);
        for (int i = 0; i < fileNames.size(); ++i) {
            insertClipart(fileNames.at(i), twips + QPointF(i * step, i * step));
        }
    });
    connect(m_sidebar->addressBook(), &AddressBookPanel::addressChosen, this, &MainWindow::useAddress);
    connect(m_printBar, &PrintBar::printRequested, this, &MainWindow::print);
    connect(m_printBar, &PrintBar::printerChanged, this, [](const QString &name) {
        settings().writeEntry("Printer", name);
    });
    connect(m_printBar, &PrintBar::paperSizesChanged, m_sidebar, &Sidebar::setPaperSizes);
    // The label type follows the printer: for a new, untouched label always,
    // else when the user chooses another printer.
    connect(m_printBar, &PrintBar::paperSizesChanged, this, &MainWindow::followPrinter);
    connect(m_printBar, &PrintBar::printOffsetRequested, this, &MainWindow::editPrintOffset);
    // Custom sizes only for printers that print them (not a DYMO).
    const auto updateCustomSize = [this]() {
        actionCollection()->action(u"label_custom_size"_s)->setEnabled(m_printBar->acceptsCustomSizes());
    };
    connect(m_printBar, &PrintBar::paperSizesChanged, this, updateCustomSize);
    updateCustomSize();
    connect(m_editor, &LabelEditor::documentChanged, this, &MainWindow::updatePrintArea);
    connect(m_printBar, &PrintBar::paperSizesChanged, this, &MainWindow::updatePrintArea);
    connect(m_sidebar, &Sidebar::customSizeEditRequested, this, [this](const QString &name) {
        const auto paper = CustomLabels::create(this, CustomLabels::find(name));
        if (!paper) {
            return;
        }
        m_printBar->reloadPaperSizes();
        // The label made with it takes the new size, name and area.
        if (m_editor->document().paperName == name) {
            changePaper(*paper);
        }
        updatePrintArea();
    });
    connect(m_sidebar, &Sidebar::customSizeRemovalRequested, this, [this](const QString &name) {
        if (KMessageBox::warningContinueCancel(this, i18n("Remove the label size \"%1\"? Labels made with it keep their size.", name), i18n("Remove Label Size"), KStandardGuiItem::remove())
            == KMessageBox::Continue) {
            CustomLabels::remove(name);
            m_printBar->reloadPaperSizes();
        }
    });
    connect(m_printBar, &PrintBar::printerChosen, this, [this]() {
        m_followPrinter = true;
        followPrinter();
    });
    connect(m_sidebar->addressBook(), &AddressBookPanel::checkedChanged, m_printBar, &PrintBar::setCheckedAddresses);
    m_sidebar->setPaperSizes(m_printBar->paperSizes());
    m_printBar->setCheckedAddresses(int(m_sidebar->addressBook()->checkedAddresses().size()));

    m_printBar->selectStartupPrinter(settings().readEntry("Printer", QString()));
    newLabel();
}

QWidget *MainWindow::createInfoBar()
{
    auto *bar = new QWidget;
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 2, 8, 2);
    m_paperName = new QLabel;
    QFont bold = m_paperName->font();
    bold.setBold(true);
    m_paperName->setFont(bold);
    m_paperSize = new QLabel;
    m_zoomLabel = new QLabel;
    m_zoomLabel->setMinimumWidth(m_zoomLabel->fontMetrics().horizontalAdvance(u"8888%"_s));
    m_zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    // The slider is logarithmic: every 100 steps doubles the zoom.
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(-200, 300);
    m_zoomSlider->setFixedWidth(160);
    m_zoomSlider->setToolTip(i18n("Zoom"));
    auto *fit = new QToolButton;
    fit->setIcon(QIcon::fromTheme(u"zoom-fit-best"_s));
    fit->setToolTip(i18n("Fit the label in the window"));
    fit->setAutoRaise(true);
    layout->addWidget(m_paperName);
    layout->addWidget(m_paperSize);
    layout->addStretch();
    layout->addWidget(m_zoomLabel);
    layout->addWidget(m_zoomSlider);
    layout->addWidget(fit);
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int value) {
        m_view->setZoom(std::exp2(value / 100.0));
    });
    connect(fit, &QToolButton::clicked, m_view, &LabelView::zoomToFit);
    return bar;
}

void MainWindow::setupActions()
{
    KActionCollection *actions = actionCollection();

    KStandardAction::openNew(this, &MainWindow::newLabel, actions);
    KStandardAction::open(this, &MainWindow::open, actions);
    m_recentFiles = KStandardAction::openRecent(
        this,
        [this](const QUrl &url) {
            openFile(url.toLocalFile());
        },
        actions);
    m_recentFiles->loadEntries(KSharedConfig::openConfig()->group(u"Recent Files"_s));
    // DLS's "Recently used layouts": the labels printed last.
    auto *recentLayouts = new KActionMenu(QIcon::fromTheme(u"document-open-recent"_s), i18n("Recently Used Layouts"), this);
    recentLayouts->setPopupMode(QToolButton::InstantPopup);
    recentLayouts->setToolTip(i18n("Open one of the labels printed last"));
    auto *recentMenu = new RecentLayoutsMenu(&m_recentLayouts, this);
    recentLayouts->setMenu(recentMenu);
    connect(recentMenu, &RecentLayoutsMenu::layoutChosen, this, &MainWindow::openRecentLayout);
    actions->addAction(u"layout_recent"_s, recentLayouts);

    KStandardAction::save(this, &MainWindow::save, actions);
    KStandardAction::saveAs(this, &MainWindow::saveAs, actions);
    KStandardAction::print(this, &MainWindow::print, actions);
    KStandardAction::printPreview(this, &MainWindow::printPreview, actions);
    QAction *exportAction = actions->addAction(u"file_export"_s, this, &MainWindow::exportLabel);
    exportAction->setText(i18n("&Export as PDF or Image…"));
    exportAction->setIcon(QIcon::fromTheme(u"document-export"_s));
    KStandardAction::quit(qApp, &QApplication::closeAllWindows, actions);

    QUndoStack *undoStack = m_editor->undoStack();
    QAction *undo = KStandardAction::undo(undoStack, &QUndoStack::undo, actions);
    QAction *redo = KStandardAction::redo(undoStack, &QUndoStack::redo, actions);
    undo->setEnabled(false);
    redo->setEnabled(false);
    connect(undoStack, &QUndoStack::canUndoChanged, undo, &QAction::setEnabled);
    connect(undoStack, &QUndoStack::canRedoChanged, redo, &QAction::setEnabled);

    QAction *remove = actions->addAction(u"edit_delete"_s, this, &MainWindow::deleteObject);
    remove->setText(i18n("Delete Object"));
    remove->setIcon(QIcon::fromTheme(u"edit-delete"_s));
    actions->setDefaultShortcut(remove, QKeySequence::Delete);
    // Delete means the label's object only while working on the label, not
    // while in the address book or clip art lists.
    remove->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_view->addAction(remove);

    QAction *cut = KStandardAction::cut(this, &MainWindow::cutObject, actions);
    cut->setShortcutContext(Qt::WidgetWithChildrenShortcut); // as Delete below
    m_view->addAction(cut);
    KStandardAction::copy(this, &MainWindow::copyObject, actions);
    m_paste = KStandardAction::paste(this, &MainWindow::paste, actions);
    // On the label only: Ctrl+V in the address book or clip art lists does
    // not paste onto the label behind them.
    m_paste->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_view->addAction(m_paste);
    const auto updatePaste = [this]() {
        m_paste->setEnabled(LabelClipboard::canPaste(QApplication::clipboard()->mimeData()));
    };
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, updatePaste);
    updatePaste();

    QAction *duplicate = actions->addAction(u"edit_duplicate"_s, this, &MainWindow::duplicateObject);
    duplicate->setText(i18n("Duplicate Object"));
    duplicate->setIcon(QIcon::fromTheme(u"edit-copy"_s));
    actions->setDefaultShortcut(duplicate, QKeySequence(Qt::CTRL | Qt::Key_D));

    QAction *properties = actions->addAction(u"edit_properties"_s, this, [this]() {
        editObject(m_editor->selection());
    });
    properties->setText(i18n("Object Properties…"));
    properties->setIcon(QIcon::fromTheme(u"document-properties"_s));
    actions->setDefaultShortcut(properties, QKeySequence(Qt::ALT | Qt::Key_Return));

    QAction *raise = actions->addAction(u"edit_raise"_s, this, &MainWindow::raiseObject);
    raise->setText(i18n("Bring to Front"));
    raise->setIcon(QIcon::fromTheme(u"object-order-front"_s));
    QAction *lower = actions->addAction(u"edit_lower"_s, this, &MainWindow::lowerObject);
    lower->setText(i18n("Send to Back"));
    lower->setIcon(QIcon::fromTheme(u"object-order-back"_s));

    KStandardAction::zoomIn(m_view, &LabelView::zoomIn, actions);
    KStandardAction::zoomOut(m_view, &LabelView::zoomOut, actions);
    KStandardAction::fitToPage(m_view, &LabelView::zoomToFit, actions);
    KStandardAction::actualSize(
        m_view,
        [this]() {
            m_view->setZoom(1.0);
        },
        actions);
    setupViewActions();
    setupDataActions();
    setupArrangeActions();

    const auto addInsert = [&](const QString &name, const QString &text, const QString &icon, ObjectFactory::Kind kind) {
        QAction *action = actions->addAction(name, this, [this, kind]() {
            insertObject(kind);
        });
        action->setText(text);
        action->setIcon(QIcon::fromTheme(icon));
    };
    addInsert(u"insert_address"_s, i18n("Address"), u"view-pim-contacts"_s, ObjectFactory::Kind::Address);
    addInsert(u"insert_text"_s, i18n("Text"), u"draw-text"_s, ObjectFactory::Kind::Text);
    addInsert(u"insert_barcode"_s, i18n("Barcode"), u"view-barcode"_s, ObjectFactory::Kind::Barcode);
    addInsert(u"insert_image"_s, i18n("Image…"), u"insert-image"_s, ObjectFactory::Kind::Image);
    addInsert(u"insert_rectangle"_s, i18n("Rectangle"), u"draw-rectangle"_s, ObjectFactory::Kind::Rectangle);
    addInsert(u"insert_ellipse"_s, i18n("Ellipse"), u"draw-ellipse"_s, ObjectFactory::Kind::Ellipse);
    addInsert(u"insert_hline"_s, i18n("Horizontal Line"), u"draw-line"_s, ObjectFactory::Kind::HorizontalLine);
    addInsert(u"insert_vline"_s, i18n("Vertical Line"), u"draw-line"_s, ObjectFactory::Kind::VerticalLine);
    addInsert(u"insert_counter"_s, i18n("Counter"), u"accessories-calculator"_s, ObjectFactory::Kind::Counter);
    addInsert(u"insert_datetime"_s, i18n("Date and Time"), u"view-calendar-day"_s, ObjectFactory::Kind::DateTime);
    addInsert(u"insert_curvedtext"_s, i18n("Curved Text"), u"draw-text"_s, ObjectFactory::Kind::CurvedText);

    // Labels of a kind: cable wraps and flags, patch panels, rack markers.
    QAction *labelKind = actions->addAction(u"insert_label_kind"_s, this, [this]() {
        LabelKindsDialog dialog(m_editor->document(), this);
        if (dialog.exec() == QDialog::Accepted && maybeSave()) {
            m_editor->reset(dialog.label());
            updateCaption();
        }
    });
    labelKind->setText(i18n("Label of a Kind…"));
    labelKind->setIcon(QIcon::fromTheme(u"view-list-icons"_s));

    AddressBookPanel *book = m_sidebar->addressBook();
    const auto addBookAction = [&](const QString &name, const QString &text, const QString &icon, auto slot) {
        QAction *action = actions->addAction(name, book, slot);
        action->setText(text);
        action->setIcon(QIcon::fromTheme(icon));
        connect(action, &QAction::triggered, m_sidebar, &Sidebar::showAddressBook);
    };
    addBookAction(u"book_new"_s, i18n("New Address Book"), u"document-new"_s, &AddressBookPanel::newBook);
    addBookAction(u"book_open"_s, i18n("Open Address Book…"), u"document-open"_s, &AddressBookPanel::open);
    addBookAction(u"book_import"_s, i18n("Import Addresses from CSV…"), u"document-import"_s, &AddressBookPanel::importCsv);
    addBookAction(u"book_save"_s, i18n("Save Address Book"), u"document-save"_s, &AddressBookPanel::save);
    addBookAction(u"book_saveas"_s, i18n("Save Address Book As…"), u"document-save-as"_s, &AddressBookPanel::saveAs);

    setupFormatActions();
    setupLayoutActions();
}

void MainWindow::setupLayoutActions()
{
    KActionCollection *actions = actionCollection();

    QAction *labelType = actions->addAction(u"label_type"_s, this, [this]() {
        // Dragged shut, the side panel opens again.
        if (auto *splitter = qobject_cast<QSplitter *>(m_sidebar->parentWidget()); splitter && splitter->sizes().constFirst() == 0) {
            splitter->setSizes({300, std::max(300, splitter->width() - 300)});
        }
        m_sidebar->showLabelTypes();
    });
    labelType->setText(i18n("Label Type…"));
    labelType->setIcon(QIcon::fromTheme(u"document-page-setup"_s));
    QAction *customSize = actions->addAction(u"label_custom_size"_s, this, [this]() {
        if (const auto paper = CustomLabels::create(this)) {
            m_printBar->reloadPaperSizes();
            changePaper(*paper);
        }
    });
    customSize->setText(i18n("Custom Label Size…"));
    customSize->setIcon(QIcon::fromTheme(u"transform-scale"_s));
    // Tape labels: their length, and the order of their cells.
    QAction *tapeLength = actions->addAction(u"tape_length"_s, this, &MainWindow::editTapeLength);
    tapeLength->setText(i18n("Tape Length…"));
    tapeLength->setIcon(QIcon::fromTheme(u"measure"_s));
    const auto addMoveCell = [&](const QString &name, const QString &text, const QString &icon, int steps) {
        QAction *action = actions->addAction(name, this, [this, steps]() {
            LabelDocument document = m_editor->document();
            if (document.moveCell(m_editor->selection(), steps)) {
                m_editor->apply(document, i18n("Move on the Tape"));
            }
        });
        action->setText(text);
        action->setIcon(QIcon::fromTheme(icon));
    };
    addMoveCell(u"tape_move_earlier"_s, i18n("Move Earlier on the Tape"), u"go-previous"_s, -1);
    addMoveCell(u"tape_move_later"_s, i18n("Move Later on the Tape"), u"go-next"_s, 1);
    QAction *rotate = actions->addAction(u"label_rotate"_s, this, &MainWindow::rotateLabel);
    rotate->setText(i18n("Rotate Label"));
    rotate->setIcon(QIcon::fromTheme(u"object-rotate-right"_s));

    // Label orientation, as DLS's Layout menu has it.
    auto *orientation = new QActionGroup(this);
    m_portrait = new KToggleAction(QIcon::fromTheme(u"view-portrait"_s), i18n("Portrait"), this);
    m_landscape = new KToggleAction(QIcon::fromTheme(u"view-landscape"_s), i18n("Landscape"), this);
    for (KToggleAction *action : {m_portrait, m_landscape}) {
        action->setActionGroup(orientation);
    }
    actions->addAction(u"layout_portrait"_s, m_portrait);
    actions->addAction(u"layout_landscape"_s, m_landscape);
    connect(m_portrait, &QAction::triggered, this, [this]() {
        setOrientation(PaperOrientation::Portrait);
    });
    connect(m_landscape, &QAction::triggered, this, [this]() {
        setOrientation(PaperOrientation::Landscape);
    });

    // Placing the selected object on the label.
    const auto addPosition = [&](const QString &name, const QString &text, const QString &icon, Qt::Alignment alignment) {
        QAction *action = actions->addAction(name, this, [this, alignment]() {
            positionObject(alignment);
        });
        action->setText(text);
        action->setIcon(QIcon::fromTheme(icon));
    };
    addPosition(u"layout_position_left"_s, i18n("Against the Left Edge"), u"align-horizontal-left"_s, Qt::AlignLeft);
    addPosition(u"layout_position_hcenter"_s, i18n("Centred Horizontally"), u"align-horizontal-center"_s, Qt::AlignHCenter);
    addPosition(u"layout_position_right"_s, i18n("Against the Right Edge"), u"align-horizontal-right"_s, Qt::AlignRight);
    addPosition(u"layout_position_top"_s, i18n("Against the Top Edge"), u"align-vertical-top"_s, Qt::AlignTop);
    addPosition(u"layout_position_vcenter"_s, i18n("Centred Vertically"), u"align-vertical-center"_s, Qt::AlignVCenter);
    addPosition(u"layout_position_bottom"_s, i18n("Against the Bottom Edge"), u"align-vertical-bottom"_s, Qt::AlignBottom);
    addPosition(u"layout_position_center"_s, i18n("In the Middle of the Label"), u"align-horizontal-center"_s, Qt::AlignCenter);
}

void MainWindow::setupViewActions()
{
    KActionCollection *actions = actionCollection();
    KConfigGroup view = KSharedConfig::openConfig()->group(u"View"_s);

    auto *showGrid = new KToggleAction(QIcon::fromTheme(u"view-grid"_s), i18n("Show &Grid"), this);
    actions->addAction(u"view_show_grid"_s, showGrid);
    actions->setDefaultShortcut(showGrid, QKeySequence(Qt::CTRL | Qt::Key_Apostrophe));
    showGrid->setChecked(view.readEntry("ShowGrid", false));
    m_view->setGridVisible(showGrid->isChecked());
    connect(showGrid, &QAction::toggled, this, [this](bool on) {
        m_view->setGridVisible(on);
        KSharedConfig::openConfig()->group(u"View"_s).writeEntry("ShowGrid", on);
    });

    auto *snap = new KToggleAction(QIcon::fromTheme(u"snap-bounding-box"_s), i18n("&Snap to Grid"), this);
    snap->setToolTip(i18n("Moving and resizing put edges on the grid. Hold Alt to place freely."));
    actions->addAction(u"view_snap_grid"_s, snap);
    actions->setDefaultShortcut(snap, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Apostrophe));
    snap->setChecked(view.readEntry("SnapToGrid", false));
    m_view->setSnapToGrid(snap->isChecked());
    connect(snap, &QAction::toggled, this, [this](bool on) {
        m_view->setSnapToGrid(on);
        KSharedConfig::openConfig()->group(u"View"_s).writeEntry("SnapToGrid", on);
    });

    // In millimetres, like the rulers.
    const QList<qreal> spacings = {0.5, 1, 2, 2.5, 5, 10};
    auto *spacing = new KSelectAction(i18n("Grid S&pacing"), this);
    actions->addAction(u"view_grid_spacing"_s, spacing);
    for (const qreal mm : spacings) {
        spacing->addAction(i18nc("grid spacing", "%1 mm", QLocale().toString(mm)));
    }
    const qreal chosen = view.readEntry("GridSpacingMm", 2.0);
    const qsizetype index = spacings.indexOf(chosen);
    spacing->setCurrentItem(int(index < 0 ? spacings.indexOf(2.0) : index));
    m_view->setGridSpacing(Units::mmToTwips(spacings.at(spacing->currentItem())));
    connect(spacing, &KSelectAction::indexTriggered, this, [this, spacings](int i) {
        m_view->setGridSpacing(Units::mmToTwips(spacings.at(i)));
        KSharedConfig::openConfig()->group(u"View"_s).writeEntry("GridSpacingMm", spacings.at(i));
    });
}

bool MainWindow::openFile(const QString &name)
{
    // Relative paths from the command line would end up in the recent files
    // as broken entries.
    const QString fileName = QFileInfo(name).absoluteFilePath();
    if (!maybeSave()) {
        return false;
    }
    // A DYMO Rhino template opens as a new label made from it.
    if (fileName.endsWith(u".rhino"_s, Qt::CaseInsensitive)) {
        QFile file(fileName);
        QString error;
        const auto rhino = file.open(QIODevice::ReadOnly) ? LabelKinds::readRhinoTemplate(file.readAll(), &error) : std::nullopt;
        if (!rhino) {
            KMessageBox::error(this, i18n("Could not open %1:\n%2", fileName, error.isEmpty() ? file.errorString() : error));
            return false;
        }
        m_editor->reset(LabelKinds::fromRhinoTemplate(*rhino));
        updateCaption();
        if (rhino->labels.size() > 1) {
            statusBar()->showMessage(i18n("The template has %1 labels; this is the first.", rhino->labels.size()), 10000);
        }
        return true;
    }
    QString error;
    QStringList warnings;
    if (!m_editor->load(fileName, &error, &warnings)) {
        KMessageBox::error(this, i18n("Could not open %1:\n%2", fileName, error));
        return false;
    }
    m_recentFiles->addUrl(QUrl::fromLocalFile(fileName));
    m_recentFiles->saveEntries(KSharedConfig::openConfig()->group(u"Recent Files"_s));
    // A LabelWriter cannot tell which labels are loaded; like DLS, new labels
    // start with the type used last.
    settings().writeEntry("Paper", m_editor->document().paperName);
    m_view->setZoom(1.0); // labels open at their real size, as in DLS
    if (!warnings.isEmpty()) {
        statusBar()->showMessage(i18np("Opened with one note: %2", "Opened with %1 notes, first: %2", warnings.size(), warnings.constFirst()), 10000);
    }
    return true;
}

void MainWindow::showScreenshotTab(int index)
{
    switch (index) {
    case 1:
        m_sidebar->showInsert();
        break;
    case 2:
        m_sidebar->showAddressBook();
        break;
    default:
        m_sidebar->showLabelTypes();
        break;
    }
}

void MainWindow::selectForScreenshot(int index)
{
    m_editor->setSelection(index);
}

void MainWindow::editTextForScreenshot(int index)
{
    m_view->editText(index);
}

void MainWindow::labelKindForScreenshot(int row, const QString &file)
{
    auto *dialog = new LabelKindsDialog(m_editor->document(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(720, 560);
    dialog->show();
    if (auto *kinds = dialog->findChild<QListWidget *>()) {
        kinds->setCurrentRow(row);
    }
    QTimer::singleShot(400, dialog, [dialog, file]() {
        dialog->grab().save(file);
    });
}

void MainWindow::newLabel()
{
    if (!maybeSave()) {
        return;
    }
    const PaperSize paper = startingPaper();
    m_editor->reset(ObjectFactory::newLabel(paper));
    m_view->setZoom(1.0);
    updateCaption();
}

PaperSize MainWindow::startingPaper() const
{
    return LabelFit::startingPaper(m_printBar->paperSizes(), settings().readEntry("Paper", FallbackPaper.name), m_printBar->loadedLabelLengthMm(), FallbackPaper);
}

void MainWindow::editTapeLength()
{
    LabelDocument document = m_editor->document();
    if (!document.tape) {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(i18n("Tape Length"));
    auto *form = new QFormLayout(&dialog);
    auto *automatic = new QRadioButton(i18n("As long as the content"));
    auto *fixed = new QRadioButton(i18n("Fixed length:"));
    auto *length = new QDoubleSpinBox;
    // The LabelManager 450D cuts fixed labels from 40 to 400 mm; the driver allows less.
    length->setRange(10, 1000);
    length->setDecimals(1);
    length->setSuffix(i18nc("millimetres", " mm"));
    length->setValue(Units::twipsToMm(document.tape->length));
    (document.tape->lengthMode == CellLengthMode::Fixed ? fixed : automatic)->setChecked(true);
    length->setEnabled(fixed->isChecked());
    connect(fixed, &QRadioButton::toggled, length, &QWidget::setEnabled);
    auto *fixedRow = new QHBoxLayout;
    fixedRow->addWidget(fixed);
    fixedRow->addWidget(length);
    form->addRow(automatic);
    form->addRow(fixedRow);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    document.tape->lengthMode = fixed->isChecked() ? CellLengthMode::Fixed : CellLengthMode::Auto;
    if (fixed->isChecked()) {
        document.tape->length = Units::mmToTwips(length->value());
    }
    m_editor->apply(document, i18n("Change Tape Length"));
}

void MainWindow::updatePrintArea()
{
    // A tape label: the band the print head reaches.
    if (m_editor->document().tape) {
        m_view->setPrintArea(TapeLayout::printableBand(m_editor->document()));
        return;
    }
    // The label type's printable part: from the printer's list (an Altec
    // or own size), else from the user's own sizes.
    const LabelDocument &document = m_editor->document();
    std::optional<PaperSize> paper;
    for (const PaperSize &candidate : m_printBar->paperSizes()) {
        if (candidate.name == document.paperName) {
            paper = candidate;
            break;
        }
    }
    if (!paper) {
        paper = CustomLabels::find(document.paperName);
    }
    if (!paper || paper->printArea.isEmpty()) {
        m_view->setPrintArea({});
        return;
    }
    const QRectF twips(paper->printArea.topLeft() * Units::TwipsPerPoint, paper->printArea.size() * Units::TwipsPerPoint);
    m_view->setPrintArea(LabelPrinter::canvasRect(document.orientation, document.paperSize(), twips));
}

void MainWindow::followPrinter()
{
    const LabelDocument &document = m_editor->document();
    const bool untouched = document.objects.isEmpty() && !m_editor->isModified() && m_editor->fileName().isEmpty();
    if (m_printBar->paperSizes().isEmpty() || (!untouched && !m_followPrinter)) {
        return;
    }
    if (!labelFitsPrinter()) {
        const PaperSize paper = startingPaper();
        if (untouched) {
            m_editor->reset(ObjectFactory::newLabel(paper));
        } else if (paper.name != document.paperName) {
            changePaper(paper);
            statusBar()->showMessage(i18n("The label type is now %1, to suit %2. Undo to go back.", paper.name, m_printBar->printerName()), 10000);
        }
    }
    // Once the printer has said which labels it has (or will not be asked),
    // later changes to the list leave the label alone again.
    if (!m_printBar->isDetecting()) {
        m_followPrinter = false;
    }
}

void MainWindow::open()
{
    const QString fileName = QFileDialog::getOpenFileName(this, i18n("Open Label"), QString(), labelFileFilter());
    if (!fileName.isEmpty()) {
        openFile(fileName);
    }
}

bool MainWindow::save()
{
    if (m_editor->fileName().isEmpty()) {
        return saveAs();
    }
    QString error;
    if (!m_editor->save(m_editor->fileName(), &error)) {
        KMessageBox::error(this, i18n("Could not save %1:\n%2", m_editor->fileName(), error));
        return false;
    }
    return true;
}

bool MainWindow::saveAs()
{
    // The dialog adds .label itself, so it also asks before overwriting that file.
    QFileDialog dialog(this, i18n("Save Label"), m_editor->fileName(), labelFileFilter());
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(u"label"_s);
    if (!m_editor->fileName().isEmpty()) {
        dialog.selectFile(m_editor->fileName());
    }
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return false;
    }
    const QString fileName = dialog.selectedFiles().constFirst();
    QString error;
    if (!m_editor->save(fileName, &error)) {
        KMessageBox::error(this, i18n("Could not save %1:\n%2", fileName, error));
        return false;
    }
    m_recentFiles->addUrl(QUrl::fromLocalFile(fileName));
    m_recentFiles->saveEntries(KSharedConfig::openConfig()->group(u"Recent Files"_s));
    return true;
}

bool MainWindow::maybeSave()
{
    if (!m_editor->isModified()) {
        return true;
    }
    const QString name = m_editor->fileName().isEmpty() ? i18n("Untitled") : QFileInfo(m_editor->fileName()).fileName();
    switch (KMessageBox::warningTwoActionsCancel(this,
                                                 i18n("The label \"%1\" has been changed. Do you want to save it?", name),
                                                 i18n("Unsaved Changes"),
                                                 KStandardGuiItem::save(),
                                                 KStandardGuiItem::discard())) {
    case KMessageBox::PrimaryAction:
        return save();
    case KMessageBox::SecondaryAction:
        return true;
    default:
        return false;
    }
}

bool MainWindow::queryClose()
{
    return maybeSave() && m_sidebar->addressBook()->maybeSave();
}

void MainWindow::updateCaption()
{
    const QString name = m_editor->fileName().isEmpty() ? i18n("Untitled") : QFileInfo(m_editor->fileName()).fileName();
    setCaption(name, m_editor->isModified());
}

void MainWindow::updateActions()
{
    const bool selected = m_editor->selection() >= 0;
    for (const QString &name : {u"edit_delete"_s,
                                u"edit_duplicate"_s,
                                u"edit_properties"_s,
                                u"edit_raise"_s,
                                u"edit_lower"_s,
                                u"edit_cut"_s,
                                u"edit_copy"_s,
                                u"layout_position_left"_s,
                                u"layout_position_hcenter"_s,
                                u"layout_position_right"_s,
                                u"layout_position_top"_s,
                                u"layout_position_vcenter"_s,
                                u"layout_position_bottom"_s,
                                u"layout_position_center"_s}) {
        actionCollection()->action(name)->setEnabled(selected);
    }
    // On tape, cells place objects: no positioning; the tape's own actions instead.
    const bool tape = m_editor->document().tape.has_value();
    for (const QString &name : {u"layout_position_left"_s,
                                u"layout_position_hcenter"_s,
                                u"layout_position_right"_s,
                                u"layout_position_top"_s,
                                u"layout_position_vcenter"_s,
                                u"layout_position_bottom"_s,
                                u"layout_position_center"_s}) {
        actionCollection()->action(name)->setEnabled(selected && !tape);
    }
    actionCollection()->action(u"tape_length"_s)->setEnabled(tape);
    // Cells move one at a time.
    const QList<int> selection = m_editor->selectedObjects();
    actionCollection()->action(u"tape_move_earlier"_s)->setEnabled(selection.size() == 1 && tape);
    actionCollection()->action(u"tape_move_later"_s)->setEnabled(selection.size() == 1 && tape);

    // Lining up and spreading out: what moves are groups and objects on their own.
    const LabelDocument &document = m_editor->document();
    const int units = Arrange::unitCount(document, selection);
    const bool grouped = std::any_of(selection.cbegin(), selection.cend(), [&document](int index) {
        return Arrange::isGrouped(document, index);
    });
    const bool movable = !tape && Arrange::canMove(document, selection);
    for (const QString &name : {u"arrange_align_left"_s,
                                u"arrange_align_hcenter"_s,
                                u"arrange_align_right"_s,
                                u"arrange_align_top"_s,
                                u"arrange_align_vcenter"_s,
                                u"arrange_align_bottom"_s}) {
        actionCollection()->action(name)->setEnabled(movable && units >= 2);
    }
    actionCollection()->action(u"arrange_distribute_horizontal"_s)->setEnabled(movable && units >= 3);
    actionCollection()->action(u"arrange_distribute_vertical"_s)->setEnabled(movable && units >= 3);
    actionCollection()->action(u"arrange_group"_s)->setEnabled(units >= 2);
    const bool several = selection.size() > 1;
    actionCollection()->action(u"edit_delete"_s)->setText(several ? i18n("Delete Objects") : i18n("Delete Object"));
    actionCollection()->action(u"edit_duplicate"_s)->setText(several ? i18n("Duplicate Objects") : i18n("Duplicate Object"));
    actionCollection()->action(u"arrange_ungroup"_s)->setEnabled(grouped);
    actionCollection()->action(u"edit_select_all"_s)->setEnabled(!document.objects.isEmpty());
    m_lock->setEnabled(selected && !tape);
    m_noPrint->setEnabled(selected);
    m_noPrint->setChecked(selected && std::none_of(selection.cbegin(), selection.cend(), [&document](int index) {
                              return objectBase(document.objects.at(index)).printed;
                          }));
    actionCollection()->action(u"layout_move_to_layer"_s)->setEnabled(selected);
    m_lock->setChecked(selected && std::all_of(selection.cbegin(), selection.cend(), [&document](int index) {
                           return objectBase(document.objects.at(index)).locked;
                       }));
    const bool landscape = m_editor->document().orientation == PaperOrientation::Landscape;
    m_landscape->setChecked(landscape);
    m_portrait->setChecked(!landscape);
}

void MainWindow::updateStatus()
{
    const LabelDocument &document = m_editor->document();
    const QSizeF canvas = document.canvasSize();
    m_paperName->setText(document.paperName);
    m_paperSize->setText(i18nc("label width x height", "%1 mm × %2 mm", qRound(Units::twipsToMm(canvas.width())), qRound(Units::twipsToMm(canvas.height()))));
    m_zoomLabel->setText(i18nc("zoom level", "%1%", qRound(m_view->zoom() * 100)));
    const QSignalBlocker blocker(m_zoomSlider);
    m_zoomSlider->setValue(qRound(std::log2(m_view->zoom()) * 100));
    m_sidebar->setCurrentPaper(document.paperName);
}

void MainWindow::changePaper(const PaperSize &paper)
{
    LabelDocument document = m_editor->document();
    if (paper.tape > 0) {
        // To tape: the tape's width; die-cut objects each get a cell.
        LabelDocument tape = ObjectFactory::newLabel(paper);
        if (document.tape) {
            tape.tape->root = document.tape->root;
            tape.tape->lengthMode = document.tape->lengthMode;
            tape.tape->length = document.tape->length;
            tape.objects = document.objects;
        } else {
            for (const LabelObject &object : std::as_const(document.objects)) {
                tape.addObject(object);
            }
        }
        m_editor->apply(tape, i18n("Change Label Type"));
        settings().writeEntry("Paper", paper.name);
        return;
    }
    // To a die-cut label: from tape, objects stay where the layout put them.
    document.tape.reset();
    const LabelDocument blank = ObjectFactory::newLabel(paper.name, paper.keyword, paper.size);
    document.paperName = blank.paperName;
    document.id = blank.id;
    document.paperOutline = blank.paperOutline;
    document.drawCommandsXml.clear();
    document.orientation = blank.orientation;
    document.roll = paper.roll;
    if (paper.roll.continuous) {
        // Cut from the roll: square corners.
        document.paperOutline = QPainterPath();
        document.paperOutline.addRect(QRectF(QPointF(0, 0), paper.size * Units::TwipsPerPoint));
    }
    m_editor->apply(document, i18n("Change Label Type"));
    settings().writeEntry("Paper", paper.name);
}

void MainWindow::rotateLabel()
{
    // The objects turn with the label, so it prints as before.
    LabelDocument document = m_editor->document();
    document.rotateQuarter();
    m_editor->apply(document, i18n("Rotate Label"));
}

void MainWindow::openRecentLayout(const RecentLayout &layout)
{
    if (!maybeSave()) {
        return;
    }
    // Still the same as its file: open the file, so Save goes back to it.
    if (!layout.source.isEmpty() && QFileInfo::exists(layout.source)) {
        QFile source(layout.source);
        QFile copy(layout.fileName);
        if (source.open(QIODevice::ReadOnly) && copy.open(QIODevice::ReadOnly) && source.readAll() == copy.readAll()) {
            openFile(layout.source);
            return;
        }
    }
    QString error;
    const auto document = m_recentLayouts.load(layout, &error);
    if (!document) {
        KMessageBox::error(this, i18n("Could not open the layout %1:\n%2", layout.title, error));
        return;
    }
    m_editor->reset(*document);
    settings().writeEntry("Paper", document->paperName);
    m_view->setZoom(1.0);
    updateCaption();
    statusBar()->showMessage(i18n("Opened \"%1\" as it was printed. Save it to keep it as a file.", layout.title), 10000);
}
