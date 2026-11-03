/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "appsettings.h"
#include "datareader.h"
#include "labelprinter.h"
#include "printrun.h"
#include "objectfactory.h"
#include "printhistory.h"
#include "recentlayouts.h"
#include "../plugin/plasmalabelcreatorplugin.h"

#include <KXmlGuiWindow>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class KActionMenu;
class KFontAction;
class KFontSizeAction;
class KRecentFilesAction;
class KToggleAction;
class LabelEditor;
class LabelView;
class PrintBar;
class PrintOnlyPanel;
class QDoubleSpinBox;
class QLabel;
class QSlider;
class QSplitter;
class Sidebar;
namespace LabelCreator
{
class SharedCounter;
}

class MainWindow : public KXmlGuiWindow, public LabelCreator::PluginHost
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // What add-ons may do (see plasmalabelcreatorplugin.h).
    QWidget *dialogParent() override;
    bool openLabel(const QString &fileName, const QString &origin) override;
    QString currentFileName() const override;
    QString currentOrigin() const override;
    bool saveLabelAs(const QString &fileName, const QString &origin, QString *errorString) override;
    void showMessage(const QString &text) override;

    bool openFile(const QString &fileName);
    /**
     * Print-only mode: choose a label from a folder, fill in its form and
     * print; the label cannot be edited. For people who only print.
     */
    void setPrintOnly(bool printOnly);
    bool isPrintOnly() const;
    /** The folder print-only mode lists labels from. */
    void setPrintOnlyFolder(const QString &folder);
    /** Shows tab @p index of the side panel; for screenshots while developing. */
    void showScreenshotTab(int index);
    /** Selects object @p index; for screenshots while developing. */
    void selectForScreenshot(int index);
    /** Starts editing object @p index's text on the label; for screenshots while developing. */
    void editTextForScreenshot(int index);
    /** Opens the label kinds dialog on kind @p row and saves a picture of it to @p file; for development. */
    void labelKindForScreenshot(int row, const QString &file);

protected:
    bool queryClose() override;

private:
    void setupActions();
    QWidget *createInfoBar();
    void setupFormatActions();
    void updateFormatActions();
    /** The font of the selected object, if it has one the toolbar can change. */
    std::optional<LabelCreator::LabelFont> selectedFont() const;
    void changeSelectedFont(const std::function<void(LabelCreator::LabelFont &)> &change);
    void setSelectedAlignment(std::optional<LabelCreator::HorizontalAlignment> horizontal, std::optional<LabelCreator::VerticalAlignment> vertical);
    void setSelectedFitMode(LabelCreator::TextFitMode mode);
    void setupLayoutActions();
    void updateCaption();
    void updateActions();
    void updateStatus();

    void newLabel();
    /**
     * The label type to start new labels with: the one used last if the
     * printer offers it, else one that fits the labels loaded in it.
     */
    LabelCreator::PaperSize startingPaper() const;
    /** Asks before printing a label that is not what the printer has loaded; false if cancelled. */
    bool confirmLabelFitsPrinter();
    /** Whether the label type is one the printer offers and fits what is loaded in it. */
    bool labelFitsPrinter() const;
    /**
     * Gives the label a type that suits the printer: always for a new,
     * untouched label, else after the user chose another printer.
     */
    void followPrinter();
    /** Shows the printable part of the label type on the canvas, if it has one. */
    void updatePrintArea();
    /** Asks whether the tape label is as long as its content or of a fixed length. */
    void editTapeLength();
    void open();
    bool save();
    bool saveAs();
    /** Asks to save unsaved changes; false if the user cancelled. */
    bool maybeSave();
    /**
     * Ends editing text on the label, keeping what was typed, before the
     * label is saved, printed or exported: a shortcut leaves the focus in
     * the editor, so it did not end by itself.
     */
    void finishEditing();

    /** Inserts a new object, centred on @p at (twips) if given, else on the label. */
    void insertObject(ObjectFactory::Kind kind, std::optional<QPointF> at = std::nullopt);
    void insertClipart(const QString &fileName, std::optional<QPointF> at = std::nullopt);
    void insert(LabelCreator::LabelObject object, std::optional<QPointF> at = std::nullopt);
    /** Puts @p address in the label's first address object, as DLS does. */
    void useAddress(const QString &address);
    /**
     * After printing, counters continue where the print run stopped; shared
     * counters (by object index) at the value written to their file.
     */
    void advanceCounters(int printedLabels, const QHash<int, int> &sharedNext = {});
    /**
     * Sets the shared counters in @p labels to the values in their files;
     * with @p locks, locks the files first and keeps them locked there.
     * False, with the reason shown, when a file cannot be locked or read.
     * The counters of an add-on's label that the add-on keeps come from it
     * instead: @p count values taken for printing (0 for a preview), their
     * names in @p byAddon.
     */
    bool useSharedCounters(QList<LabelCreator::LabelDocument> &labels,
                           std::vector<std::unique_ptr<LabelCreator::SharedCounter>> *locks,
                           QHash<QString, int> *values,
                           int count = 0,
                           QSet<QString> *byAddon = nullptr);
    void editObject(int index);
    void deleteObject();
    void duplicateObject();
    void copyObject();
    void cutObject();
    void paste();
    /**
     * Moves the selected objects, together, against an edge or to the middle
     * of the label.
     */
    void positionObject(Qt::Alignment alignment);
    /** Lines the selected objects up with each other. */
    void alignObjects(Qt::Alignment alignment);
    void distributeObjects(Qt::Orientation orientation);
    void groupObjects();
    void ungroupObjects();
    void setObjectsLocked(bool locked);
    void setObjectsPrinted(bool printed);
    /** Turns the selected objects that can move by @p degrees, counter-clockwise. */
    void turnObjects(qreal degrees);
    /** Sets the selected objects that can move to @p degrees. */
    void setObjectsAngle(qreal degrees);
    void editPrintSettings();
    void editLayers();
    void editRollLayout();
    /** Puts the selected objects on layer @p name; empty for the label itself. */
    void moveToLayer(const QString &name);
    void selectAllObjects();
    void setOrientation(LabelCreator::PaperOrientation orientation);
    void raiseObject();
    void lowerObject();
    /** Moves the selected objects in the drawing order with @p reorder (from Arrange). */
    void reorderSelected(const QString &description, QList<int> (*reorder)(LabelCreator::LabelDocument &, const QList<int> &));
    void changePaper(const LabelCreator::PaperSize &paper);
    void rotateLabel();
    void print();
    /** Sends @p run, made by labelsToPrint(), to the printer. */
    void printLabels(const LabelCreator::PrintRun::Run &run);
    /** Shows the pages print() would send, and prints them if asked. */
    void printPreview();
    /** Saves the label as a PDF (vector, for sharing) or a PNG at the printer's resolution. */
    void exportLabel();
    /**
     * The labels print() sends: the label, one per ticked data row, or one
     * per checked address, with a header and trailer label when it has them.
     * Empty, with the reason shown, when there is nothing to print.
     */
    std::optional<LabelCreator::PrintRun::Run> labelsToPrint();
    LabelCreator::LabelPrinter::Options printOptions(const LabelCreator::PrintRun::Run &run, const LabelCreator::PrintSettings &printing) const;
    /** Where pictures named without a folder are: next to the data file, else next to the label. */
    QString pictureFolder(const LabelCreator::LabelDocument &document) const;
    void setupViewActions();
    void setupDataActions();
    void setupArrangeActions();
    /** Chooses the data source and the rows to print. */
    void editDataSource();
    void editQuestions();
    void editCalculatedFields();
    /** Ticks the data rows to print, without changing where they come from. */
    void chooseRows();
    /** Earlier versions of the label: shown, and one restored if chosen. */
    void showVersions();
    void showPrintHistory();
    void showPrinterStatus();
    /** Settings > Configure Plasma Label Creator. */
    void configure();
    /** Uses @p settings everywhere they matter, from now on. */
    void applySettings(const AppSettings &settings);
    /** Warns when the printer or its queue reports a problem; false to stop. */
    bool confirmPrinterReady();
    /** Prints @p record's labels again, with its answers and rows. */
    void printAgain(const LabelCreator::PrintRecord &record);
    void setupPrintOnlyActions();
    /** In print-only mode, turns off every action that edits. */
    void keepEditingOff();
    /** The form in print-only mode follows the label shown. */
    void updatePrintOnlyForm();
    /** In print-only mode the label shows filled in with the form's answers. */
    void updatePrintOnlyPreview();
    /** Puts {name} in the selected object, or a new text object with it. */
    void insertField(const QString &name);
    /** The fields the label can use: the data's columns, then the questions. */
    QStringList availableFields();
    /** Loads the data source's rows if they are not loaded; false with the reason shown. */
    bool ensureDataLoaded(bool quiet = false);
    /** Offers printing from data in the print bar as the label's source allows. */
    void updateDataRows();
    /**
     * @p document with its fields filled from @p values, and image paths
     * that are relative made absolute: next to the data file, or the label.
     */
    LabelCreator::LabelDocument filled(const LabelCreator::LabelDocument &document, const QHash<QString, QString> &values) const;
    void setSelectedInverted(bool inverted);
    void setSelectedWrap(bool wrap);
    /** Asks for the selected printer's print offset, in on-screen terms for the current label. */
    void editPrintOffset();
    void openRecentLayout(const LabelCreator::RecentLayout &layout);
    /** Loads the add-ons that are installed and switched on, and puts their actions in the menus. */
    void loadAddons();
    /** Makes the label just opened an add-on's when one keeps its file. */
    void claimByAddon();
    /** The add-on the current label belongs to; nullptr for none. */
    LabelCreator::Plugin *currentAddon() const;
    /**
     * Tells the current label's add-on it was saved; false, with the
     * reason shown unless @p quiet, when that failed.
     */
    bool addonSaved(bool keepVersion, bool quiet = false);

    LabelCreator::RecentLayouts m_recentLayouts;
    // The add-ons loaded, and the add-on label open: its origin, as long as
    // the editor has its file.
    QList<LabelCreator::Plugin *> m_addons;
    QString m_origin;
    QString m_originFile;
    AppSettings m_settings;
    LabelEditor *m_editor;
    LabelView *m_view;
    Sidebar *m_sidebar;
    PrintBar *m_printBar;
    QLabel *m_paperName;
    QLabel *m_paperSize;
    QLabel *m_zoomLabel;
    QSlider *m_zoomSlider;
    KRecentFilesAction *m_recentFiles = nullptr;
    KFontAction *m_fontFamily = nullptr;
    KFontSizeAction *m_fontSize = nullptr;
    KToggleAction *m_bold = nullptr;
    KToggleAction *m_italic = nullptr;
    KToggleAction *m_underline = nullptr;
    KToggleAction *m_strikeout = nullptr;
    KToggleAction *m_inverted = nullptr;
    KToggleAction *m_wrap = nullptr;

    // Printing from data: the rows as loaded (and from which source), the
    // ticked ones, the last answers to the questions, and a database
    // password, kept in memory for this session only.
    std::optional<DataTable> m_dataTable;
    LabelCreator::DataSource m_dataTableSource;
    QHash<QString, QString> m_dataParameters; // the answers a query with fields was run with
    QList<int> m_dataRows;
    QHash<QString, QString> m_answers;
    // The data rows of the labels labelsToPrint() made last, for the print history.
    QList<QHash<QString, QString>> m_printedRows;
    // While printing again from the history: the label's name then.
    QString m_reprintTitle;
    // The database password given last, kept in memory only, for the
    // database (driver, host, database, user) it was given for.
    QString m_databasePassword;
    QStringList m_databasePasswordFor;
    QList<KToggleAction *> m_fitModes; // none, shrink to fit, always fit
    KToggleAction *m_portrait = nullptr;
    KToggleAction *m_landscape = nullptr;
    KToggleAction *m_lock = nullptr;
    KToggleAction *m_printOnlyAction = nullptr;
    PrintOnlyPanel *m_printOnlyPanel = nullptr;
    QSplitter *m_splitter = nullptr; // side panel or print-only panel, then the canvas
    bool m_printOnly = false;
    KToggleAction *m_noPrint = nullptr;
    QDoubleSpinBox *m_angle = nullptr; // in the arrange toolbar
    QAction *m_paste = nullptr;
    bool m_followPrinter = false; // the user chose another printer; the label type follows
    QList<KToggleAction *> m_horizontalAlignment; // left, centre, right
    // On the format toolbar: one button each, showing the alignment chosen.
    KActionMenu *m_horizontalMenu = nullptr;
    KActionMenu *m_verticalMenu = nullptr;
    QList<KToggleAction *> m_verticalAlignment; // top, middle, bottom
};
