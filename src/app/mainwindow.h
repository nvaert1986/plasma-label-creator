/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "datareader.h"
#include "labelprinter.h"
#include "objectfactory.h"
#include "recentlayouts.h"

#include <KXmlGuiWindow>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class KFontAction;
class KFontSizeAction;
class KRecentFilesAction;
class KToggleAction;
class LabelEditor;
class LabelView;
class PrintBar;
class QLabel;
class QSlider;
class Sidebar;
namespace LabelCreator
{
class SharedCounter;
}

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    bool openFile(const QString &fileName);
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
     */
    bool useSharedCounters(QList<LabelCreator::LabelDocument> &labels, std::vector<std::unique_ptr<LabelCreator::SharedCounter>> *locks);
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
    void editLayers();
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
    /** Sends @p labels, made by labelsToPrint(), to the printer. */
    void printLabels(const QList<LabelCreator::LabelDocument> &labels);
    /** Shows the pages print() would send, and prints them if asked. */
    void printPreview();
    /** Saves the label as a PDF (vector, for sharing) or a PNG at the printer's resolution. */
    void exportLabel();
    /**
     * The labels print() sends: the label, or one per checked address.
     * Empty, with the reason shown, when there is nothing to print.
     */
    std::optional<QList<LabelCreator::LabelDocument>> labelsToPrint();
    LabelCreator::LabelPrinter::Options printOptions() const;
    void setupViewActions();
    void setupDataActions();
    void setupArrangeActions();
    /** Chooses the data source and the rows to print. */
    void editDataSource();
    void editQuestions();
    void editCalculatedFields();
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

    LabelCreator::RecentLayouts m_recentLayouts;
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
    QList<int> m_dataRows;
    QHash<QString, QString> m_answers;
    // The database password given last, kept in memory only, for the
    // database (driver, host, database, user) it was given for.
    QString m_databasePassword;
    QStringList m_databasePasswordFor;
    QList<KToggleAction *> m_fitModes; // none, shrink to fit, always fit
    KToggleAction *m_portrait = nullptr;
    KToggleAction *m_landscape = nullptr;
    KToggleAction *m_lock = nullptr;
    KToggleAction *m_noPrint = nullptr;
    QAction *m_paste = nullptr;
    bool m_followPrinter = false; // the user chose another printer; the label type follows
    QList<KToggleAction *> m_horizontalAlignment; // left, centre, right
    QList<KToggleAction *> m_verticalAlignment; // top, middle, bottom
};
