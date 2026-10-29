/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow.h"

#include "addressbookpanel.h"
#include "alteclabels.h"
#include "customlabels.h"
#include "datadialogs.h"
#include "fields.h"
#include "labelclipboard.h"
#include "labelfit.h"
#include "labelkinds.h"
#include "labelkindsdialog.h"
#include "tapelayout.h"
#include "labeleditor.h"
#include "labelview.h"
#include "printbar.h"
#include "printpreviewdialog.h"
#include "labelrenderer.h"
#include "propertiesdialog.h"
#include "recentlayoutsmenu.h"
#include "rulerwidget.h"
#include "sidebar.h"
#include "units.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KFontAction>
#include <KFontSizeAction>
#include <KToggleAction>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KRecentFilesAction>
#include <KSelectAction>
#include <KSharedConfig>
#include <KXMLGUIFactory>
#include <KStandardAction>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QFontDatabase>
#include <QBuffer>
#include <QClipboard>
#include <QFileDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QTimer>
#include <QListWidget>
#include <QDir>
#include <QMenu>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <QSplitter>
#include <QStatusBar>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

// Used for new labels until a label type has been chosen once.
const PaperSize FallbackPaper{u"w79h252"_s, u"30252 Address"_s, QSizeF(78.96, 252), QRectF(), QString(), QRectF(), 0};

QString labelFileFilter()
{
    return i18n("DYMO label files (*.label)") + u";;"_s + i18n("DYMO Rhino templates (*.rhino)") + u";;"_s + i18n("All files (*)");
}

KConfigGroup settings()
{
    return KSharedConfig::openConfig()->group(u"General"_s);
}

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

namespace
{

// The font the format toolbar works on, per object type: the text's, the
// barcode's human-readable text's.
template<typename Object>
LabelFont *editableFont(Object &object)
{
    using T = std::decay_t<Object>;
    if constexpr (std::is_same_v<T, TextObject> || std::is_same_v<T, AddressObject>) {
        return object.spans.isEmpty() ? nullptr : &object.spans.first().font;
    } else if constexpr (std::is_same_v<T, CounterObject> || std::is_same_v<T, DateTimeObject> || std::is_same_v<T, CircularTextObject>) {
        return &object.font;
    } else if constexpr (std::is_same_v<T, BarcodeObject>) {
        return &object.textFont;
    } else {
        return nullptr;
    }
}

} // namespace

void MainWindow::setupFormatActions()
{
    KActionCollection *actions = actionCollection();

    m_fontFamily = new KFontAction(i18n("Font"), this);
    actions->addAction(u"format_font_family"_s, m_fontFamily);
    connect(m_fontFamily, &KFontAction::textTriggered, this, [this](const QString &family) {
        changeSelectedFont([&family](LabelFont &font) {
            font.family = family;
        });
    });

    m_fontSize = new KFontSizeAction(i18n("Font Size"), this);
    actions->addAction(u"format_font_size"_s, m_fontSize);
    connect(m_fontSize, &KFontSizeAction::fontSizeChanged, this, [this](int size) {
        changeSelectedFont([size](LabelFont &font) {
            font.size = size;
        });
    });

    // Larger and smaller go through the usual point sizes.
    const auto stepSize = [this](int direction) {
        changeSelectedFont([direction](LabelFont &font) {
            const QList<int> sizes = QFontDatabase::standardSizes();
            int size = qRound(font.size);
            if (direction > 0) {
                const auto next = std::upper_bound(sizes.cbegin(), sizes.cend(), size);
                size = next == sizes.cend() ? size + 2 : *next;
            } else {
                const auto previous = std::lower_bound(sizes.cbegin(), sizes.cend(), size);
                size = previous == sizes.cbegin() ? std::max(4, size - 1) : *(previous - 1);
            }
            font.size = size;
        });
    };
    QAction *grow = actions->addAction(u"format_font_grow"_s, this, [stepSize]() {
        stepSize(1);
    });
    grow->setText(i18n("Larger Font"));
    grow->setIcon(QIcon::fromTheme(u"format-font-size-more"_s));
    actions->setDefaultShortcut(grow, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Greater));
    QAction *shrink = actions->addAction(u"format_font_shrink"_s, this, [stepSize]() {
        stepSize(-1);
    });
    shrink->setText(i18n("Smaller Font"));
    shrink->setIcon(QIcon::fromTheme(u"format-font-size-less"_s));
    actions->setDefaultShortcut(shrink, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Less));

    const auto addStyle = [&](const QString &name, const QString &text, const QString &icon, const QKeySequence &shortcut, auto member) {
        auto *action = new KToggleAction(QIcon::fromTheme(icon), text, this);
        actions->addAction(name, action);
        actions->setDefaultShortcut(action, shortcut);
        connect(action, &QAction::triggered, this, [this, member](bool on) {
            changeSelectedFont([member, on](LabelFont &font) {
                font.*member = on;
            });
        });
        return action;
    };
    m_bold = addStyle(u"format_bold"_s, i18n("Bold"), u"format-text-bold"_s, QKeySequence::Bold, &LabelFont::bold);
    m_italic = addStyle(u"format_italic"_s, i18n("Italic"), u"format-text-italic"_s, QKeySequence::Italic, &LabelFont::italic);
    m_underline = addStyle(u"format_underline"_s, i18n("Underline"), u"format-text-underline"_s, QKeySequence::Underline, &LabelFont::underline);
    m_strikeout = addStyle(u"format_strikeout"_s, i18n("Strikeout"), u"format-text-strikethrough"_s, QKeySequence(), &LabelFont::strikeout);

    // White text on a black box, which DLS shows the same way (its colours).
    m_inverted = new KToggleAction(QIcon::fromTheme(u"color-mode-invert-text"_s, QIcon::fromTheme(u"format-fill-color"_s)), i18n("White on Black"), this);
    actions->addAction(u"format_inverted"_s, m_inverted);
    connect(m_inverted, &QAction::triggered, this, &MainWindow::setSelectedInverted);

    const auto addAlignment = [&](const QString &name, const QString &text, const QString &icon, QActionGroup *group, auto apply) {
        auto *action = new KToggleAction(QIcon::fromTheme(icon), text, this);
        action->setActionGroup(group);
        actions->addAction(name, action);
        connect(action, &QAction::triggered, this, apply);
        return action;
    };
    auto *horizontal = new QActionGroup(this);
    m_horizontalAlignment = {
        addAlignment(u"format_align_left"_s, i18n("Align Left"), u"format-justify-left"_s, horizontal, [this]() {
            setSelectedAlignment(HorizontalAlignment::Left, std::nullopt);
        }),
        addAlignment(u"format_align_center"_s, i18n("Align Centre"), u"format-justify-center"_s, horizontal, [this]() {
            setSelectedAlignment(HorizontalAlignment::Center, std::nullopt);
        }),
        addAlignment(u"format_align_right"_s, i18n("Align Right"), u"format-justify-right"_s, horizontal, [this]() {
            setSelectedAlignment(HorizontalAlignment::Right, std::nullopt);
        }),
    };
    auto *vertical = new QActionGroup(this);
    m_verticalAlignment = {
        addAlignment(u"format_valign_top"_s, i18n("Align Top"), u"align-vertical-top"_s, vertical, [this]() {
            setSelectedAlignment(std::nullopt, VerticalAlignment::Top);
        }),
        addAlignment(u"format_valign_middle"_s, i18n("Align Middle"), u"align-vertical-center"_s, vertical, [this]() {
            setSelectedAlignment(std::nullopt, VerticalAlignment::Middle);
        }),
        addAlignment(u"format_valign_bottom"_s, i18n("Align Bottom"), u"align-vertical-bottom"_s, vertical, [this]() {
            setSelectedAlignment(std::nullopt, VerticalAlignment::Bottom);
        }),
    };

    // How text fills its box, DLS's "Text Fit".
    auto *fit = new QActionGroup(this);
    m_fitModes = {
        addAlignment(u"format_fit_none"_s, i18n("Fixed Font Size"), QString(), fit, [this]() {
            setSelectedFitMode(TextFitMode::None);
        }),
        addAlignment(u"format_fit_shrink"_s, i18n("Shrink to Fit"), QString(), fit, [this]() {
            setSelectedFitMode(TextFitMode::ShrinkToFit);
        }),
        addAlignment(u"format_fit_always"_s, i18n("Always Fit"), QString(), fit, [this]() {
            setSelectedFitMode(TextFitMode::AlwaysFit);
        }),
    };
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

void MainWindow::setupDataActions()
{
    KActionCollection *actions = actionCollection();
    QAction *source = actions->addAction(u"data_source"_s, this, &MainWindow::editDataSource);
    source->setText(i18n("&Data Source…"));
    source->setIcon(QIcon::fromTheme(u"table"_s, QIcon::fromTheme(u"x-office-spreadsheet"_s)));
    QAction *questions = actions->addAction(u"data_questions"_s, this, &MainWindow::editQuestions);
    questions->setText(i18n("Print-Time &Questions…"));
    questions->setIcon(QIcon::fromTheme(u"help-contents"_s));
    QAction *remove = actions->addAction(u"data_remove"_s, this, [this]() {
        LabelDocument document = m_editor->document();
        document.dataSource = DataSource();
        m_editor->apply(document, i18n("Remove Data Source"));
    });
    remove->setText(i18n("&Remove Data Source"));
    remove->setIcon(QIcon::fromTheme(u"edit-delete"_s));

    // Lists the fields when opened: the data's columns and the questions.
    auto *field = new KActionMenu(QIcon::fromTheme(u"insert-text"_s), i18n("Insert &Field"), this);
    field->setPopupMode(QToolButton::InstantPopup);
    actions->addAction(u"data_insert_field"_s, field);
    connect(field->menu(), &QMenu::aboutToShow, this, [this, field]() {
        QMenu *menu = field->menu();
        menu->clear();
        const QStringList fields = availableFields();
        for (const QString &name : fields) {
            menu->addAction(u'{' + name + u'}', this, [this, name]() {
                insertField(name);
            });
        }
        if (fields.isEmpty()) {
            menu->addAction(i18n("No fields yet: choose a data source or add a question"))->setEnabled(false);
        }
    });
}

QStringList MainWindow::availableFields()
{
    QStringList fields;
    ensureDataLoaded(true);
    if (m_dataTable) {
        fields << m_dataTable->columns;
    }
    for (const PrintQuestion &question : m_editor->document().questions) {
        if (!fields.contains(question.name)) {
            fields << question.name;
        }
    }
    return fields;
}

void MainWindow::insertField(const QString &name)
{
    const QString field = u'{' + name + u'}';
    const int index = m_editor->selection();
    if (index < 0) {
        LabelDocument document = m_editor->document();
        LabelObject object = ObjectFactory::create(ObjectFactory::Kind::Text, document);
        std::get_if<TextObject>(&object)->spans.first().text = field;
        insert(object);
        return;
    }
    LabelDocument document = m_editor->document();
    bool placed = true;
    std::visit(
        [&](auto &o) {
            using T = std::decay_t<decltype(o)>;
            if constexpr (std::is_same_v<T, TextObject> || std::is_same_v<T, AddressObject>) {
                if (o.spans.isEmpty()) {
                    o.spans = {{field, LabelFont(), Qt::black}};
                } else {
                    o.spans.last().text += field;
                }
                if constexpr (std::is_same_v<T, AddressObject>) {
                    o.fitLineFonts();
                }
            } else if constexpr (std::is_same_v<T, BarcodeObject> || std::is_same_v<T, CircularTextObject>) {
                o.text += field;
            } else if constexpr (std::is_same_v<T, CounterObject> || std::is_same_v<T, DateTimeObject>) {
                o.preText += field;
            } else if constexpr (std::is_same_v<T, ImageObject>) {
                // A picture per row: the field is (part of) its file path.
                o.imageLocation = o.imageLocation.value_or(QString()) + field;
            } else {
                placed = false;
            }
        },
        document.objects[index]);
    if (placed) {
        m_editor->apply(document, i18n("Insert Field"));
    }
}

bool MainWindow::ensureDataLoaded(bool quiet)
{
    const DataSource &source = m_editor->document().dataSource;
    if (source.kind == DataSource::Kind::None) {
        return false;
    }
    if (m_dataTable && m_dataTableSource == source) {
        return true;
    }
    QString password = m_databasePassword;
    if (source.kind == DataSource::Kind::Database && !source.user.isEmpty() && password.isEmpty()) {
        if (quiet) {
            return false;
        }
        bool ok = false;
        password = askDatabasePassword(source, this, &ok);
        if (!ok) {
            return false;
        }
    }
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto table = DataReader::read(source, password, &error);
    QApplication::restoreOverrideCursor();
    if (!table) {
        if (!quiet) {
            KMessageBox::error(this, i18n("The data could not be read:\n%1", error));
        }
        return false;
    }
    if (source.kind == DataSource::Kind::Database) {
        m_databasePassword = password;
    }
    m_dataTable = table;
    m_dataTableSource = source;
    m_dataRows.clear();
    for (int row = 0; row < table->rows.size(); ++row) {
        m_dataRows << row;
    }
    updateDataRows();
    return true;
}

void MainWindow::updateDataRows()
{
    const DataSource &source = m_editor->document().dataSource;
    if (m_dataTable && m_dataTableSource != source) {
        m_dataTable.reset();
        m_dataRows.clear();
    }
    m_printBar->setDataRows(source.kind != DataSource::Kind::None, m_dataTable ? int(m_dataRows.size()) : -1);
}

void MainWindow::editDataSource()
{
    ensureDataLoaded(true);
    DataSourceDialog dialog(m_editor->document().dataSource, m_dataTable, m_dataRows, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const DataSource source = dialog.source();
    if (dialog.table()) {
        m_dataTable = dialog.table();
        m_dataTableSource = source;
        m_dataRows = dialog.selectedRows();
    }
    if (source != m_editor->document().dataSource) {
        LabelDocument document = m_editor->document();
        document.dataSource = source;
        m_editor->apply(document, i18n("Change Data Source"));
    }
    updateDataRows();
    m_printBar->choosePrintRows();
}

void MainWindow::editQuestions()
{
    QuestionsDialog dialog(m_editor->document().questions, this);
    if (dialog.exec() != QDialog::Accepted || dialog.questions() == m_editor->document().questions) {
        return;
    }
    LabelDocument document = m_editor->document();
    document.questions = dialog.questions();
    m_editor->apply(document, i18n("Change Print-Time Questions"));
}

LabelDocument MainWindow::filled(const LabelDocument &document, const QHash<QString, QString> &values) const
{
    LabelDocument result = Fields::fill(document, values);
    // Relative picture paths: next to the data file, else next to the label.
    const DataSource &source = document.dataSource;
    const QString base = source.kind == DataSource::Kind::File ? QFileInfo(source.fileName).absolutePath()
        : m_editor->fileName().isEmpty()                       ? QDir::currentPath()
                                                               : QFileInfo(m_editor->fileName()).absolutePath();
    for (LabelObject &object : result.objects) {
        if (auto *image = std::get_if<ImageObject>(&object); image && image->imageLocation && !image->imageLocation->isEmpty()) {
            if (QFileInfo(*image->imageLocation).isRelative()) {
                image->imageLocation = QDir(base).absoluteFilePath(*image->imageLocation);
            }
        }
    }
    return result;
}

void MainWindow::setSelectedInverted(bool inverted)
{
    const int index = m_editor->selection();
    if (index < 0 || !ObjectFactory::canInvert(m_editor->document().objects.at(index))) {
        return;
    }
    LabelDocument document = m_editor->document();
    ObjectFactory::setInverted(document.objects[index], inverted);
    m_editor->apply(document, inverted ? i18n("White on Black") : i18n("Black on White"));
}

void MainWindow::setSelectedFitMode(TextFitMode mode)
{
    const int index = m_editor->selection();
    if (index < 0) {
        return;
    }
    LabelDocument document = m_editor->document();
    TextObject *text = std::get_if<TextObject>(&document.objects[index]);
    if (!text) {
        text = std::get_if<AddressObject>(&document.objects[index]);
    }
    if (text) {
        text->fitMode = mode;
        m_editor->apply(document, i18n("Change Text Fit"));
    }
}

std::optional<LabelFont> MainWindow::selectedFont() const
{
    const int index = m_editor->selection();
    if (index < 0) {
        return std::nullopt;
    }
    LabelObject object = m_editor->document().objects.at(index);
    return std::visit(
        [](auto &o) -> std::optional<LabelFont> {
            const LabelFont *font = editableFont(o);
            return font ? std::optional<LabelFont>(*font) : std::nullopt;
        },
        object);
}

void MainWindow::changeSelectedFont(const std::function<void(LabelFont &)> &change)
{
    const int index = m_editor->selection();
    if (index < 0) {
        return;
    }
    LabelDocument document = m_editor->document();
    std::visit(
        [&change](auto &o) {
            using T = std::decay_t<decltype(o)>;
            if constexpr (std::is_same_v<T, TextObject> || std::is_same_v<T, AddressObject>) {
                // Mixed fonts: the change applies to every run.
                for (TextSpan &span : o.spans) {
                    change(span.font);
                }
            } else if (LabelFont *font = editableFont(o)) {
                change(*font);
            }
        },
        document.objects[index]);
    m_editor->apply(document, i18n("Change Font"));
}

void MainWindow::setSelectedAlignment(std::optional<HorizontalAlignment> horizontal, std::optional<VerticalAlignment> vertical)
{
    const int index = m_editor->selection();
    if (index < 0) {
        return;
    }
    LabelDocument document = m_editor->document();
    std::visit(
        [&](auto &o) {
            using T = std::decay_t<decltype(o)>;
            if constexpr (std::is_same_v<T, TextObject> || std::is_same_v<T, AddressObject>) {
                o.horizontalAlignment = horizontal.value_or(o.horizontalAlignment);
                o.verticalAlignment = vertical.value_or(o.verticalAlignment);
            } else if constexpr (std::is_same_v<T, CounterObject> || std::is_same_v<T, DateTimeObject>) {
                o.layout.horizontalAlignment = horizontal.value_or(o.layout.horizontalAlignment);
                o.layout.verticalAlignment = vertical.value_or(o.layout.verticalAlignment);
            } else if constexpr (std::is_same_v<T, ImageObject>) {
                o.horizontalAlignment = horizontal.value_or(o.horizontalAlignment);
                o.verticalAlignment = vertical.value_or(o.verticalAlignment);
            } else if constexpr (std::is_same_v<T, BarcodeObject>) {
                o.horizontalAlignment = horizontal.value_or(o.horizontalAlignment);
            } else if constexpr (std::is_same_v<T, CircularTextObject>) {
                o.textAlignment = horizontal.value_or(o.textAlignment);
            }
        },
        document.objects[index]);
    m_editor->apply(document, i18n("Change Alignment"));
}

void MainWindow::updateFormatActions()
{
    // What the selected object has: a font, horizontal and/or vertical alignment.
    const int index = m_editor->selection();
    std::optional<HorizontalAlignment> horizontal;
    std::optional<VerticalAlignment> vertical;
    if (index >= 0) {
        std::visit(
            [&](const auto &o) {
                using T = std::decay_t<decltype(o)>;
                if constexpr (std::is_same_v<T, TextObject> || std::is_same_v<T, AddressObject> || std::is_same_v<T, ImageObject>) {
                    horizontal = o.horizontalAlignment;
                    vertical = o.verticalAlignment;
                } else if constexpr (std::is_same_v<T, CounterObject> || std::is_same_v<T, DateTimeObject>) {
                    horizontal = o.layout.horizontalAlignment;
                    vertical = o.layout.verticalAlignment;
                } else if constexpr (std::is_same_v<T, BarcodeObject>) {
                    horizontal = o.horizontalAlignment;
                } else if constexpr (std::is_same_v<T, CircularTextObject>) {
                    horizontal = o.textAlignment;
                }
            },
            m_editor->document().objects.at(index));
    }

    const std::optional<LabelFont> font = selectedFont();
    for (QAction *action : std::initializer_list<QAction *>{m_fontFamily, m_fontSize, m_bold, m_italic, m_underline, m_strikeout}) {
        action->setEnabled(font.has_value());
    }
    actionCollection()->action(u"format_font_grow"_s)->setEnabled(font.has_value());
    actionCollection()->action(u"format_font_shrink"_s)->setEnabled(font.has_value());
    if (font) {
        // Showing the font must not change it.
        const QSignalBlocker familyBlocker(m_fontFamily);
        const QSignalBlocker sizeBlocker(m_fontSize);
        m_fontFamily->setFont(font->family);
        m_fontSize->setFontSize(qRound(font->size));
        m_bold->setChecked(font->bold);
        m_italic->setChecked(font->italic);
        m_underline->setChecked(font->underline);
        m_strikeout->setChecked(font->strikeout);
    }

    const bool invertible = index >= 0 && ObjectFactory::canInvert(m_editor->document().objects.at(index));
    m_inverted->setEnabled(invertible);
    m_inverted->setChecked(invertible && ObjectFactory::isInverted(m_editor->document().objects.at(index)));

    std::optional<TextFitMode> fitMode;
    if (index >= 0) {
        const LabelObject &object = m_editor->document().objects.at(index);
        if (const auto *text = std::get_if<TextObject>(&object)) {
            fitMode = text->fitMode;
        } else if (const auto *address = std::get_if<AddressObject>(&object)) {
            fitMode = address->fitMode;
        }
    }
    const QList<TextFitMode> fitModes = {TextFitMode::None, TextFitMode::ShrinkToFit, TextFitMode::AlwaysFit};
    for (int i = 0; i < m_fitModes.size(); ++i) {
        m_fitModes.at(i)->setEnabled(fitMode.has_value());
        m_fitModes.at(i)->setChecked(fitMode == fitModes.at(i));
    }

    const QList<HorizontalAlignment> horizontals = {HorizontalAlignment::Left, HorizontalAlignment::Center, HorizontalAlignment::Right};
    for (int i = 0; i < m_horizontalAlignment.size(); ++i) {
        m_horizontalAlignment.at(i)->setEnabled(horizontal.has_value());
        m_horizontalAlignment.at(i)->setChecked(horizontal == horizontals.at(i));
    }
    const QList<VerticalAlignment> verticals = {VerticalAlignment::Top, VerticalAlignment::Middle, VerticalAlignment::Bottom};
    for (int i = 0; i < m_verticalAlignment.size(); ++i) {
        m_verticalAlignment.at(i)->setEnabled(vertical.has_value());
        m_verticalAlignment.at(i)->setChecked(vertical == verticals.at(i));
    }
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

bool MainWindow::labelFitsPrinter() const
{
    return LabelFit::check(m_editor->document(), m_printBar->paperSizes(), m_printBar->acceptsCustomSizes(), m_printBar->loadedLabelLengthMm())
        == LabelFit::Problem::None;
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

bool MainWindow::confirmLabelFitsPrinter()
{
    const LabelDocument &document = m_editor->document();
    const QSizeF size = document.paperSize();
    const qreal widthMm = Units::twipsToMm(size.width());
    const qreal lengthMm = Units::twipsToMm(size.height());
    const auto loaded = m_printBar->loadedLabelLengthMm();
    QString reason;
    switch (LabelFit::check(document, m_printBar->paperSizes(), m_printBar->acceptsCustomSizes(), loaded)) {
    case LabelFit::Problem::NotOffered:
        reason = i18n("This label is a %1 (%2 × %3 mm), which is not one of the label types of %4.",
                      document.paperName,
                      QString::number(widthMm, 'f', 1),
                      QString::number(lengthMm, 'f', 1),
                      m_printBar->printerName());
        break;
    case LabelFit::Problem::WrongLength:
        reason = i18n("This label is %1 mm long, but the labels in %2 are %3 mm long.",
                      QString::number(lengthMm, 'f', 1),
                      m_printBar->printerName(),
                      QString::number(*loaded, 'f', 1));
        break;
    case LabelFit::Problem::None:
        break;
    }
    if (reason.isEmpty()) {
        return true;
    }
    return KMessageBox::warningContinueCancel(this,
                                              reason + u"\n\n"_s + i18n("Choose the label type of the loaded labels in the Labels tab first, or print anyway."),
                                              i18n("Different Labels"),
                                              KGuiItem(i18n("Print Anyway"), u"document-print"_s))
        == KMessageBox::Continue;
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
    actionCollection()->action(u"tape_move_earlier"_s)->setEnabled(selected && tape);
    actionCollection()->action(u"tape_move_later"_s)->setEnabled(selected && tape);
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

LabelPrinter::Options MainWindow::printOptions() const
{
    LabelPrinter::Options options;
    // The printer's own calibration, in its directions, in points.
    options.offset = m_printBar->printOffsetMm() * 72.0 / Units::MmPerInch;
    options.copies = m_printBar->copies();
    options.printQuality = m_printBar->graphicsQuality() ? u"Graphics"_s : u"Text"_s;
    options.jobTitle = m_editor->fileName().isEmpty() ? i18n("Label") : QFileInfo(m_editor->fileName()).fileName();
    return options;
}

std::optional<QList<LabelDocument>> MainWindow::labelsToPrint()
{
    if (m_printBar->printerName().isEmpty()) {
        KMessageBox::error(this, i18n("No printer was found. Connect a DYMO LabelWriter or add one in the system's printer settings first."));
        return std::nullopt;
    }
    if (!confirmLabelFitsPrinter()) {
        return std::nullopt;
    }
    const LabelDocument document = m_editor->document();

    // The questions first: their answers go on every label of the run.
    QHash<QString, QString> answers;
    if (!document.questions.isEmpty()) {
        AnswersDialog dialog(document.questions, m_answers, this);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        answers = dialog.answers();
        m_answers = answers;
    }

    QList<LabelDocument> labels;
    if (m_printBar->printRows()) {
        // One label per ticked row, with the row's values and the answers.
        if (!ensureDataLoaded()) {
            return std::nullopt;
        }
        if (m_dataRows.isEmpty()) {
            KMessageBox::error(this, i18n("No rows are ticked. Choose them under Data > Data Source."));
            return std::nullopt;
        }
        for (const int row : std::as_const(m_dataRows)) {
            QHash<QString, QString> values = m_dataTable->values(row);
            values.insert(answers);
            labels << filled(document, values);
        }
        return labels;
    }
    if (!m_printBar->printAddresses()) {
        return QList<LabelDocument>{filled(document, answers)};
    }
    // One label per checked address, each with the address filled in.
    const bool hasAddress = std::any_of(document.objects.cbegin(), document.objects.cend(), [](const LabelObject &object) {
        return std::holds_alternative<AddressObject>(object);
    });
    if (!hasAddress) {
        KMessageBox::error(this, i18n("To print the checked addresses, the label needs an address object."));
        return std::nullopt;
    }
    for (const QString &address : m_sidebar->addressBook()->checkedAddresses()) {
        LabelDocument label = document;
        for (LabelObject &object : label.objects) {
            if (auto *target = std::get_if<AddressObject>(&object)) {
                const TextSpan first = target->spans.isEmpty() ? TextSpan{QString(), LabelFont(), Qt::black} : target->spans.constFirst();
                target->spans = {{address, first.font, first.color}};
                target->fitLineFonts();
                break;
            }
        }
        labels << filled(label, answers);
    }
    return labels;
}

void MainWindow::print()
{
    const auto labels = labelsToPrint();
    if (!labels) {
        return;
    }
    const QString printerName = m_printBar->printerName();
    LabelPrinter printer(printerName, m_printBar->printerDriver());

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const int job = printer.print(*labels, printOptions());
    QApplication::restoreOverrideCursor();

    if (job == 0) {
        KMessageBox::error(this, printer.errorString());
        return;
    }
    // Kept as it was printed, before counters move on.
    QString recentError;
    if (!m_recentLayouts.add(m_editor->document(), m_editor->fileName(), &recentError)) {
        qWarning("Could not keep the label in the recently used layouts: %s", qPrintable(recentError));
    }
    advanceCounters(printer.printedLabels());
    // What was printed is the best guess of the labels in the printer.
    settings().writeEntry("Paper", m_editor->document().paperName);
    QString message = i18np("Sent one label to %2 as job %3.", "Sent %1 labels to %2 as job %3.", printer.printedLabels(), printerName, job);
    if (!printer.warnings().isEmpty()) {
        message += u' ' + i18np("Note: %2", "%1 notes, first: %2", printer.warnings().size(), printer.warnings().constFirst());
    }
    statusBar()->showMessage(message, 10000);
}

void MainWindow::printPreview()
{
    const auto labels = labelsToPrint();
    if (!labels) {
        return;
    }
    LabelPrinter printer(m_printBar->printerName(), m_printBar->printerDriver());
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QList<QImage> pages = printer.pages(*labels, printOptions());
    QApplication::restoreOverrideCursor();
    if (pages.isEmpty()) {
        KMessageBox::error(this, printer.errorString());
        return;
    }
    PrintPreviewDialog dialog(pages, printer.resolution(), m_printBar->printerName(), printer.warnings(), this);
    if (dialog.exec() == QDialog::Accepted) {
        print();
    }
}

void MainWindow::exportLabel()
{
    const QString pdfFilter = i18n("PDF document (*.pdf)");
    const QString pngFilter = i18n("PNG image at the printer's resolution (*.png)");
    QString filter = pdfFilter;
    const QString base = m_editor->fileName().isEmpty() ? i18n("Label") : QFileInfo(m_editor->fileName()).completeBaseName();
    QString fileName = QFileDialog::getSaveFileName(this, i18n("Export Label"), base + u".pdf"_s, pdfFilter + u";;"_s + pngFilter, &filter);
    if (fileName.isEmpty()) {
        return;
    }
    const bool png = filter == pngFilter || fileName.endsWith(u".png"_s, Qt::CaseInsensitive);
    if (QFileInfo(fileName).suffix().isEmpty()) {
        fileName += png ? u".png"_s : u".pdf"_s;
    }

    // As on the canvas, tape laid out to its length; no outline, nothing
    // that only shows on screen.
    LabelDocument document = m_editor->document();
    TapeLayout::layout(document);
    LabelRenderer renderer(LabelRenderer::Mode::Print);
    qreal dpi = 300;
    if (!m_printBar->printerName().isEmpty()) {
        LabelPrinter printer(m_printBar->printerName(), m_printBar->printerDriver());
        if (printer.load()) {
            dpi = printer.resolution();
        }
    }
    renderer.setPrinterResolution(dpi);

    bool saved = false;
    if (png) {
        const QImage image = renderer.renderImage(document, dpi);
        saved = image.save(fileName, "PNG");
    } else {
        saved = renderer.writePdf(document, fileName, base);
    }
    if (!saved) {
        KMessageBox::error(this, i18n("Could not write %1.", fileName));
        return;
    }
    statusBar()->showMessage(i18n("Exported to %1.", fileName), 10000);
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

void MainWindow::editPrintOffset()
{
    // Asked for as seen on screen, for the label as it is laid out; kept
    // in the printer's own directions, so it holds for every label.
    const PaperOrientation orientation = m_editor->document().orientation;
    const QPointF shown = LabelPrinter::canvasShift(orientation, m_printBar->printOffsetMm());
    QDialog dialog(this);
    dialog.setWindowTitle(i18n("Print Offset of %1", m_printBar->printerName()));
    auto *form = new QFormLayout(&dialog);
    auto *intro = new QLabel(i18n("Print a label and measure how far everything must move to land where it is on the screen. "
                                  "The offset is kept for this printer and applies to all labels."));
    intro->setWordWrap(true);
    form->addRow(intro);
    const auto millimetreBox = [](qreal value) {
        auto *box = new QDoubleSpinBox;
        box->setRange(-20, 20);
        box->setDecimals(1);
        box->setSingleStep(0.1);
        box->setSuffix(i18nc("millimetres", " mm"));
        box->setValue(value);
        return box;
    };
    auto *right = millimetreBox(shown.x());
    auto *down = millimetreBox(shown.y());
    form->addRow(i18n("Move right by:"), right);
    form->addRow(i18n("Move down by:"), down);
    auto *hint = new QLabel(i18n("Negative values move left and up. The label itself does not move, so what comes within this distance of its edges may be cut off."));
    hint->setWordWrap(true);
    hint->setEnabled(false);
    form->addRow(hint);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, &dialog, [right, down]() {
        right->setValue(0);
        down->setValue(0);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() == QDialog::Accepted) {
        m_printBar->setPrintOffsetMm(LabelPrinter::paperShift(orientation, QPointF(right->value(), down->value())));
    }
}
