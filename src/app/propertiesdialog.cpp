/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "propertiesdialog.h"

#include "gs1dialog.h"

#include "units.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <functional>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

QComboBox *enumCombo(const QList<std::pair<QString, int>> &items, int current)
{
    auto *combo = new QComboBox;
    for (const auto &[text, value] : items) {
        combo->addItem(text, value);
    }
    combo->setCurrentIndex(std::max(0, combo->findData(current)));
    return combo;
}

template<typename Enum>
Enum comboValue(const QComboBox *combo)
{
    return Enum(combo->currentData().toInt());
}

// Positions may be negative: DLS lets objects stick out over the label's edge.
QDoubleSpinBox *millimetreBox(qreal twips, qreal minimum = -1000)
{
    auto *box = new QDoubleSpinBox;
    box->setRange(minimum, 1000);
    box->setDecimals(1);
    box->setSingleStep(0.5);
    box->setSuffix(i18nc("millimetres", " mm"));
    box->setValue(Units::twipsToMm(twips));
    return box;
}

QComboBox *horizontalAlignmentCombo(HorizontalAlignment alignment)
{
    return enumCombo({{i18n("Left"), int(HorizontalAlignment::Left)},
                      {i18n("Centre"), int(HorizontalAlignment::Center)},
                      {i18n("Right"), int(HorizontalAlignment::Right)}},
                     int(alignment));
}

QComboBox *verticalAlignmentCombo(VerticalAlignment alignment)
{
    return enumCombo({{i18n("Top"), int(VerticalAlignment::Top)}, {i18n("Middle"), int(VerticalAlignment::Middle)}, {i18n("Bottom"), int(VerticalAlignment::Bottom)}},
                     int(alignment));
}

// Font family, size and style in one row.
class FontEditor : public QWidget
{
public:
    explicit FontEditor(const LabelFont &font)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_family = new QFontComboBox;
        m_family->setCurrentFont(QFont(font.family));
        m_size = new QDoubleSpinBox;
        m_size->setRange(4, 200);
        m_size->setDecimals(1);
        m_size->setSuffix(i18nc("points", " pt"));
        m_size->setValue(font.size);
        m_bold = new QCheckBox(i18n("Bold"));
        m_bold->setChecked(font.bold);
        m_italic = new QCheckBox(i18n("Italic"));
        m_italic->setChecked(font.italic);
        m_underline = new QCheckBox(i18n("Underline"));
        m_underline->setChecked(font.underline);
        layout->addWidget(m_family, 1);
        layout->addWidget(m_size);
        layout->addWidget(m_bold);
        layout->addWidget(m_italic);
        layout->addWidget(m_underline);
        m_original = font;
    }

    LabelFont labelFont() const
    {
        LabelFont font = m_original;
        // Keep the stored family if the user did not pick another one, even
        // when it is not installed here.
        if (m_family->currentFont().family() != QFont(m_original.family).family()) {
            font.family = m_family->currentFont().family();
        }
        font.size = m_size->value();
        font.bold = m_bold->isChecked();
        font.italic = m_italic->isChecked();
        font.underline = m_underline->isChecked();
        return font;
    }

private:
    LabelFont m_original;
    QFontComboBox *m_family;
    QDoubleSpinBox *m_size;
    QCheckBox *m_bold;
    QCheckBox *m_italic;
    QCheckBox *m_underline;
};

// The barcode types DLS offers, under the names DLS shows, then the ones
// this program adds (DLS does not know those).
const QList<std::pair<QString, QString>> &barcodeTypes()
{
    static const QList<std::pair<QString, QString>> types = {
        {u"Code128Auto"_s, u"Code 128 Auto"_s},
        {u"Code128A"_s, u"Code 128-A"_s},
        {u"Code128B"_s, u"Code 128-B"_s},
        {u"Code128C"_s, u"Code 128-C"_s},
        {u"Code128Ean"_s, u"UCC/EAN 128"_s},
        {u"Code39"_s, u"Code 39 (Code 3 of 9)"_s},
        {u"Code39CS"_s, u"Code 39 w/Mod 43 Checksum"_s},
        {u"Code2of5"_s, u"Interleaved 2 of 5"_s},
        {u"Codabar"_s, u"Codabar"_s},
        {u"Ean8"_s, u"EAN 8"_s},
        {u"Ean13"_s, u"EAN 13"_s},
        {u"UpcA"_s, u"UPC-A"_s},
        {u"UpcE"_s, u"UPC-E"_s},
        {u"Itf14"_s, u"ITF-14"_s},
        {u"Pdf417"_s, u"PDF417"_s},
        {u"QRCode"_s, u"QR Code"_s},
        {u"Postnet"_s, u"POSTNET (US)"_s},
        {u"Planet"_s, u"PLANET (US)"_s},
        {u"IMail"_s, u"Intelligent Mail (US)"_s},
        {u"DataMatrix"_s, u"Data Matrix"_s},
        {u"Aztec"_s, u"Aztec Code"_s},
        {u"MaxiCode"_s, u"MaxiCode"_s},
        {u"MicroQRCode"_s, u"Micro QR Code"_s},
        {u"Code93"_s, u"Code 93"_s},
        {u"Code11"_s, u"Code 11"_s},
        {u"MsiPlessey"_s, u"MSI Plessey"_s},
        {u"Pharmacode"_s, u"Pharmacode"_s},
        {u"GS1_128"_s, u"GS1-128 ([AI] data)"_s},
        {u"GS1DataMatrix"_s, u"GS1 Data Matrix ([AI] data)"_s},
        {u"GS1DataBar"_s, u"GS1 DataBar Omnidirectional"_s},
        {u"GS1DataBarExpanded"_s, u"GS1 DataBar Expanded ([AI] data)"_s},
        {u"HIBC128"_s, u"HIBC Code 128"_s},
        {u"HIBC39"_s, u"HIBC Code 39"_s},
        {u"HIBCDataMatrix"_s, u"HIBC Data Matrix"_s},
        {u"HIBCQRCode"_s, u"HIBC QR Code"_s},
        {u"KIX"_s, u"KIX (PostNL)"_s},
        {u"RoyalMail"_s, u"Royal Mail 4-State (RM4SCC)"_s},
    };
    return types;
}

} // namespace

QByteArray chooseImage(QWidget *parent)
{
    QStringList patterns;
    for (const QByteArray &format : QImageReader::supportedImageFormats()) {
        patterns << u"*."_s + QString::fromLatin1(format);
    }
    const QString fileName =
        QFileDialog::getOpenFileName(parent, i18n("Choose Image"), QString(), i18n("Images (%1)", patterns.join(u' ')) + u";;"_s + i18n("All files (*)"));
    if (fileName.isEmpty()) {
        return {};
    }
    const QImage image(fileName);
    if (image.isNull()) {
        KMessageBox::error(parent, i18n("Could not read the image %1.", fileName));
        return {};
    }
    // DLS embeds images as PNG.
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return png;
}

bool editObjectProperties(QWidget *parent, LabelObject &object, TapeCell *cell)
{
    if (const auto *unknown = std::get_if<UnknownObject>(&object)) {
        KMessageBox::information(parent,
                                 i18n("Plasma Label Creator cannot edit \"%1\" objects yet. The object is kept as it is and saved unchanged.", unknown->elementName));
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(i18n("Properties of %1", objectBase(object).name));
    auto *layout = new QVBoxLayout(&dialog);

    // Common to all objects.
    ObjectBase &base = objectBase(object);
    auto *general = new QGroupBox(i18n("General"));
    auto *generalForm = new QFormLayout(general);
    auto *name = new QLineEdit(base.name);
    auto *x = millimetreBox(base.bounds.x());
    auto *y = millimetreBox(base.bounds.y());
    // Lines are thin boxes; a vertical line is less than a millimetre wide.
    auto *width = millimetreBox(base.bounds.width(), 0.1);
    auto *height = millimetreBox(base.bounds.height(), 0.1);
    auto *position = new QHBoxLayout;
    position->addWidget(x);
    position->addWidget(y);
    auto *size = new QHBoxLayout;
    size->addWidget(width);
    size->addWidget(height);
    auto *rotation = enumCombo({{i18n("None"), int(Rotation::Rotation0)},
                                {i18n("90° counter-clockwise"), int(Rotation::Rotation90)},
                                {i18n("180°"), int(Rotation::Rotation180)},
                                {i18n("90° clockwise"), int(Rotation::Rotation270)}},
                               int(base.rotation));
    // Any angle, turning the whole object; other label software shows the
    // object without it.
    auto *angle = new QDoubleSpinBox;
    angle->setRange(-180, 180);
    angle->setDecimals(1);
    angle->setSingleStep(5);
    angle->setSuffix(i18nc("degrees", "°"));
    angle->setToolTip(i18n("Turns the whole object about its centre, counter-clockwise; negative values turn it clockwise"));
    angle->setValue(base.angle);
    auto *mirrored = new QCheckBox(i18n("Mirror"));
    mirrored->setChecked(base.mirrored);
    generalForm->addRow(i18n("Name:"), name);
    generalForm->addRow(i18n("Position (x, y):"), position);
    generalForm->addRow(i18n("Size (w, h):"), size);
    generalForm->addRow(i18n("Rotation:"), rotation);
    generalForm->addRow(i18n("Angle:"), angle);
    generalForm->addRow(QString(), mirrored);

    // On tape, the object's cell places and sizes it; the cell is set here.
    QGroupBox *tapeGroup = nullptr;
    QComboBox *cellLength = nullptr;
    QDoubleSpinBox *fixedLength = nullptr;
    QDoubleSpinBox *border = nullptr;
    if (cell) {
        for (QWidget *box : std::initializer_list<QWidget *>{x, y, width, height}) {
            box->setEnabled(false);
            box->setToolTip(i18n("On tape, the object's cell places it; set its length below."));
        }
        tapeGroup = new QGroupBox(i18n("On the Tape"));
        auto *tapeForm = new QFormLayout(tapeGroup);
        cellLength = enumCombo({{i18n("As long as its content"), int(CellLengthMode::Auto)},
                                {i18n("Fixed length"), int(CellLengthMode::Fixed)},
                                {i18n("A share of a fixed-length label"), int(CellLengthMode::Star)}},
                               int(cell->lengthMode));
        fixedLength = millimetreBox(cell->length, 1);
        fixedLength->setEnabled(cell->lengthMode == CellLengthMode::Fixed);
        QObject::connect(cellLength, &QComboBox::currentIndexChanged, fixedLength, [cellLength, fixedLength]() {
            fixedLength->setEnabled(comboValue<CellLengthMode>(cellLength) == CellLengthMode::Fixed);
        });
        border = new QDoubleSpinBox;
        border->setRange(0, 10);
        border->setDecimals(1);
        border->setSingleStep(0.5);
        border->setSuffix(i18nc("points", " pt"));
        border->setSpecialValueText(i18n("None"));
        border->setValue(Units::twipsToPoints(cell->borderWidth));
        tapeForm->addRow(i18n("Length:"), cellLength);
        tapeForm->addRow(QString(), fixedLength);
        tapeForm->addRow(i18n("Border:"), border);
    }

    auto *specific = new QGroupBox;
    auto *form = new QFormLayout(specific);
    std::function<void()> applySpecific;

    TextObject *text = std::get_if<TextObject>(&object);
    AddressObject *address = std::get_if<AddressObject>(&object);
    if (!text) {
        text = address;
    }
    if (text) {
        specific->setTitle(std::holds_alternative<AddressObject>(object) ? i18n("Address") : i18n("Text"));
        const QString originalText = text->plainText();
        const LabelFont originalFont = text->spans.isEmpty() ? LabelFont() : text->spans.constFirst().font;
        auto *edit = new QPlainTextEdit(originalText);
        edit->setMinimumHeight(100);
        auto *font = new FontEditor(originalFont);
        auto *horizontal = horizontalAlignmentCombo(text->horizontalAlignment);
        auto *vertical = verticalAlignmentCombo(text->verticalAlignment);
        auto *fit = enumCombo({{i18n("Shrink to fit"), int(TextFitMode::ShrinkToFit)},
                               {i18n("Always fit"), int(TextFitMode::AlwaysFit)},
                               {i18n("Fixed size"), int(TextFitMode::None)}},
                              int(text->fitMode));
        auto *fullHeight = new QCheckBox(i18n("Use full font height"));
        fullHeight->setChecked(text->useFullFontHeight);
        auto *wrap = new QCheckBox(i18n("Wrap long lines to the box's width"));
        wrap->setChecked(text->wrap);
        form->addRow(edit);
        form->addRow(i18n("Font:"), font);
        form->addRow(i18n("Horizontal alignment:"), horizontal);
        form->addRow(i18n("Vertical alignment:"), vertical);
        form->addRow(i18n("Text size:"), fit);
        form->addRow(QString(), fullHeight);
        form->addRow(QString(), wrap);
        if (text->spans.size() > 1) {
            form->addRow(new QLabel(i18n("This text uses several fonts. Changing the text or font gives all of it the font above.")));
        }
        applySpecific = [=]() {
            text->horizontalAlignment = comboValue<HorizontalAlignment>(horizontal);
            text->verticalAlignment = comboValue<VerticalAlignment>(vertical);
            text->fitMode = comboValue<TextFitMode>(fit);
            text->useFullFontHeight = fullHeight->isChecked();
            text->wrap = wrap->isChecked();
            const QString newText = edit->toPlainText();
            const LabelFont newFont = font->labelFont();
            if (newText != originalText || !(newFont == originalFont)) {
                const QColor color = text->spans.isEmpty() ? QColor(Qt::black) : text->spans.constFirst().color;
                text->spans = {{newText, newFont, color}};
            }
            if (address) {
                address->fitLineFonts();
            }
        };
    } else if (auto *barcode = std::get_if<BarcodeObject>(&object)) {
        specific->setTitle(i18n("Barcode"));
        auto *type = new QComboBox;
        for (const auto &[value, display] : barcodeTypes()) {
            type->addItem(display, value);
        }
        if (type->findData(barcode->type) < 0) {
            type->addItem(barcode->type, barcode->type);
        }
        type->setCurrentIndex(type->findData(barcode->type));
        auto *data = new QLineEdit(barcode->text);
        // GS1 data built from application identifiers, for the types that take it.
        auto *buildGs1 = new QPushButton(QIcon::fromTheme(u"view-barcode"_s), i18n("Build…"));
        buildGs1->setToolTip(i18n("Build GS1 data from application identifiers, with each value checked"));
        const auto takesGs1 = [type]() {
            const QString chosen = type->currentData().toString();
            return chosen == u"GS1_128"_s || chosen == u"GS1DataMatrix"_s || chosen == u"GS1DataBarExpanded"_s;
        };
        buildGs1->setEnabled(takesGs1());
        QObject::connect(type, &QComboBox::currentIndexChanged, buildGs1, [buildGs1, takesGs1]() {
            buildGs1->setEnabled(takesGs1());
        });
        QObject::connect(buildGs1, &QPushButton::clicked, data, [data]() {
            Gs1Dialog builder(data->text(), data->window());
            if (builder.exec() == QDialog::Accepted) {
                data->setText(builder.data());
            }
        });
        auto *dataRow = new QHBoxLayout;
        dataRow->addWidget(data, 1);
        dataRow->addWidget(buildGs1);
        auto *barSize = enumCombo({{i18n("Small"), int(BarcodeSize::Small)}, {i18n("Medium"), int(BarcodeSize::Medium)}, {i18n("Large"), int(BarcodeSize::Large)}},
                                  int(barcode->size));
        auto *textPosition = enumCombo({{i18n("Below"), int(BarcodeTextPosition::Bottom)},
                                        {i18n("Above"), int(BarcodeTextPosition::Top)},
                                        {i18n("None"), int(BarcodeTextPosition::None)}},
                                       int(barcode->textPosition));
        auto *font = new FontEditor(barcode->textFont);
        auto *alignment = horizontalAlignmentCombo(barcode->horizontalAlignment);
        form->addRow(i18n("Type:"), type);
        form->addRow(i18n("Data:"), dataRow);
        form->addRow(i18n("Size:"), barSize);
        form->addRow(i18n("Text:"), textPosition);
        form->addRow(i18n("Text font:"), font);
        form->addRow(i18n("Alignment:"), alignment);
        applySpecific = [=]() {
            barcode->type = type->currentData().toString();
            barcode->text = data->text();
            barcode->size = comboValue<BarcodeSize>(barSize);
            barcode->textPosition = comboValue<BarcodeTextPosition>(textPosition);
            barcode->textFont = font->labelFont();
            barcode->horizontalAlignment = comboValue<HorizontalAlignment>(alignment);
        };
    } else if (auto *image = std::get_if<ImageObject>(&object)) {
        specific->setTitle(i18n("Image"));
        auto newData = std::make_shared<QByteArray>();
        auto *change = new QPushButton(i18n("Change Image…"));
        QObject::connect(change, &QPushButton::clicked, &dialog, [&dialog, newData]() {
            const QByteArray png = chooseImage(&dialog);
            if (!png.isEmpty()) {
                *newData = png;
            }
        });
        auto *scale = enumCombo({{i18n("Keep proportions"), int(ImageScaleMode::Uniform)},
                                 {i18n("Stretch to fill"), int(ImageScaleMode::Fill)},
                                 {i18n("Actual size"), int(ImageScaleMode::None)}},
                                int(image->scaleMode));
        auto *horizontal = horizontalAlignmentCombo(image->horizontalAlignment);
        auto *vertical = verticalAlignmentCombo(image->verticalAlignment);
        auto *border = new QDoubleSpinBox;
        border->setRange(0, 20);
        border->setDecimals(2);
        border->setSuffix(i18nc("points", " pt"));
        border->setValue(Units::twipsToPoints(image->borderWidth));
        // Linked instead of embedded: a file path, which may hold fields
        // such as {Photo} to take a picture per data row.
        auto *location = new QLineEdit(image->imageLocation.value_or(QString()));
        location->setPlaceholderText(i18n("Embedded in the label"));
        location->setToolTip(i18n("A picture file to show instead of the embedded one. It may use fields, such as /photos/{Serial}.png, for a picture per data row; "
                                  "a relative path is looked up next to the data file."));
        form->addRow(QString(), change);
        form->addRow(i18n("Picture file:"), location);
        form->addRow(i18n("Scaling:"), scale);
        form->addRow(i18n("Horizontal alignment:"), horizontal);
        form->addRow(i18n("Vertical alignment:"), vertical);
        form->addRow(i18n("Border:"), border);
        applySpecific = [=]() {
            if (!newData->isEmpty()) {
                image->imageData = *newData;
                image->imageLocation.reset();
            } else if (!location->text().trimmed().isEmpty()) {
                image->imageLocation = location->text().trimmed();
            } else if (image->imageLocation) {
                image->imageLocation.reset();
            }
            image->scaleMode = comboValue<ImageScaleMode>(scale);
            image->horizontalAlignment = comboValue<HorizontalAlignment>(horizontal);
            image->verticalAlignment = comboValue<VerticalAlignment>(vertical);
            image->borderWidth = Units::pointsToTwips(border->value());
        };
    } else if (auto *shape = std::get_if<ShapeObject>(&object)) {
        specific->setTitle(i18n("Shape"));
        auto *type = enumCombo({{i18n("Horizontal line"), int(ShapeType::HorizontalLine)},
                                {i18n("Vertical line"), int(ShapeType::VerticalLine)},
                                {i18n("Rectangle"), int(ShapeType::Rectangle)},
                                {i18n("Ellipse"), int(ShapeType::Ellipse)}},
                               int(shape->shapeType));
        auto *lineWidth = new QDoubleSpinBox;
        lineWidth->setRange(0.25, 20);
        lineWidth->setDecimals(2);
        lineWidth->setSingleStep(0.25);
        lineWidth->setSuffix(i18nc("points", " pt"));
        lineWidth->setValue(Units::twipsToPoints(shape->lineWidth));
        auto *filled = new QCheckBox(i18n("Fill"));
        filled->setChecked(shape->fillColor.alpha() > 0);
        form->addRow(i18n("Shape:"), type);
        form->addRow(i18n("Line width:"), lineWidth);
        form->addRow(QString(), filled);
        applySpecific = [=]() {
            shape->shapeType = comboValue<ShapeType>(type);
            shape->lineWidth = Units::pointsToTwips(lineWidth->value());
            shape->fillColor = filled->isChecked() ? QColor(Qt::black) : QColor(255, 255, 255, 0);
        };
    }

    if (auto *counter = std::get_if<CounterObject>(&object)) {
        specific->setTitle(i18n("Counter"));
        auto *preText = new QLineEdit(counter->preText);
        auto *postText = new QLineEdit(counter->postText);
        const auto spin = [](int value, int minimum) {
            auto *box = new QSpinBox;
            box->setRange(minimum, 999999999);
            box->setValue(value);
            return box;
        };
        auto *start = spin(counter->start, -999999999);
        auto *current = spin(counter->current, -999999999);
        auto *increment = spin(counter->increment, -999999);
        auto *width = spin(counter->formatWidth, 1);
        width->setMaximum(20);
        auto *zeros = new QCheckBox(i18n("Leading zeros"));
        zeros->setChecked(counter->useLeadingZeros);
        auto *reset = new QPushButton(i18n("Restart at Start Value"));
        QObject::connect(reset, &QPushButton::clicked, current, [start, current]() {
            current->setValue(start->value());
        });
        auto *font = new FontEditor(counter->font);
        auto *horizontal = horizontalAlignmentCombo(counter->layout.horizontalAlignment);
        // Counting on with other computers: the next value kept in a file.
        auto *sharedFile = new QLineEdit(counter->sharedFile);
        sharedFile->setPlaceholderText(i18n("Only on this label"));
        sharedFile->setToolTip(i18n("A file, on a network share say, that keeps the next value for every computer printing this label. "
                                    "It is locked while printing, so no two computers print the same number."));
        auto *browse = new QPushButton(QIcon::fromTheme(u"document-open"_s), QString());
        browse->setToolTip(i18n("Choose the file"));
        QObject::connect(browse, &QPushButton::clicked, sharedFile, [sharedFile]() {
            const QString file = QFileDialog::getSaveFileName(sharedFile,
                                                              i18n("Shared Counter File"),
                                                              sharedFile->text(),
                                                              i18n("Counter files (*.counter);;All files (*)"),
                                                              nullptr,
                                                              QFileDialog::DontConfirmOverwrite);
            if (!file.isEmpty()) {
                sharedFile->setText(file);
            }
        });
        auto *sharedRow = new QHBoxLayout;
        sharedRow->addWidget(sharedFile, 1);
        sharedRow->addWidget(browse);
        form->addRow(i18n("Text before:"), preText);
        form->addRow(i18n("Text after:"), postText);
        form->addRow(i18n("Start value:"), start);
        form->addRow(i18n("Next value:"), current);
        form->addRow(QString(), reset);
        form->addRow(i18n("Increment:"), increment);
        form->addRow(i18n("Minimum digits:"), width);
        form->addRow(QString(), zeros);
        form->addRow(i18n("Shared with other computers:"), sharedRow);
        form->addRow(i18n("Font:"), font);
        form->addRow(i18n("Alignment:"), horizontal);
        applySpecific = [=]() {
            counter->preText = preText->text();
            counter->postText = postText->text();
            counter->start = start->value();
            counter->current = current->value();
            counter->increment = increment->value();
            counter->formatWidth = width->value();
            counter->useLeadingZeros = zeros->isChecked();
            counter->sharedFile = sharedFile->text().trimmed();
            counter->font = font->labelFont();
            counter->layout.horizontalAlignment = comboValue<HorizontalAlignment>(horizontal);
        };
    } else if (auto *date = std::get_if<DateTimeObject>(&object)) {
        specific->setTitle(i18n("Date and Time"));
        const QDateTime now = QDateTime::currentDateTime();
        auto *format = new QComboBox;
        for (int value = int(DateFormat::None); value <= int(DateFormat::DayAbbrMonthYear); ++value) {
            DateTimeObject sample = *date;
            sample.format = DateFormat(value);
            sample.includeTime = false;
            sample.preText.clear();
            sample.postText.clear();
            format->addItem(value == int(DateFormat::None) ? i18n("No date") : sample.text(now), value);
        }
        format->setCurrentIndex(format->findData(int(date->format)));
        auto *time = new QCheckBox(i18n("Include the time"));
        time->setChecked(date->includeTime);
        auto *hours24 = new QCheckBox(i18n("24-hour clock"));
        hours24->setChecked(date->use24HourFormat);
        auto *culture = new QLineEdit(date->culture);
        culture->setPlaceholderText(i18n("System language"));
        culture->setToolTip(i18n("A language code such as nl-NL or en-US; empty uses the system's language."));
        auto *preText = new QLineEdit(date->preText);
        auto *postText = new QLineEdit(date->postText);
        auto *font = new FontEditor(date->font);
        auto *horizontal = horizontalAlignmentCombo(date->layout.horizontalAlignment);
        form->addRow(i18n("Format:"), format);
        form->addRow(QString(), time);
        form->addRow(QString(), hours24);
        form->addRow(i18n("Language:"), culture);
        form->addRow(i18n("Text before:"), preText);
        form->addRow(i18n("Text after:"), postText);
        form->addRow(i18n("Font:"), font);
        form->addRow(i18n("Alignment:"), horizontal);
        applySpecific = [=]() {
            date->format = DateFormat(format->currentData().toInt());
            date->includeTime = time->isChecked();
            date->use24HourFormat = hours24->isChecked();
            date->culture = culture->text().trimmed();
            date->preText = preText->text();
            date->postText = postText->text();
            date->font = font->labelFont();
            date->layout.horizontalAlignment = comboValue<HorizontalAlignment>(horizontal);
        };
    } else if (auto *curved = std::get_if<CircularTextObject>(&object)) {
        specific->setTitle(i18n("Curved Text"));
        auto *text = new QLineEdit(curved->text);
        auto *mode = enumCombo({{i18n("Arc, text on top"), int(CircularTextMode::ArcTop)},
                                {i18n("Arc, text along the bottom"), int(CircularTextMode::ArcBottom)},
                                {i18n("Full circle"), int(CircularTextMode::Circle)}},
                               int(curved->mode));
        auto *start = enumCombo({{i18n("Centred at the top"), int(CircleAlignment::CenterAtTop)},
                                 {i18n("Centred at the bottom"), int(CircleAlignment::CenterAtBottom)},
                                 {i18n("From the start angle"), int(CircleAlignment::StartAngle)}},
                                int(curved->circleAlignment));
        auto *angle = new QDoubleSpinBox;
        angle->setRange(-360, 360);
        angle->setSuffix(u"°"_s);
        angle->setValue(curved->startAngle);
        auto *alignment = horizontalAlignmentCombo(curved->textAlignment);
        auto *font = new FontEditor(curved->font);
        form->addRow(i18n("Text:"), text);
        form->addRow(i18n("Shape:"), mode);
        form->addRow(i18n("Circle position:"), start);
        form->addRow(i18n("Start angle:"), angle);
        form->addRow(i18n("Arc alignment:"), alignment);
        form->addRow(i18n("Font:"), font);
        applySpecific = [=]() {
            curved->text = text->text();
            curved->mode = comboValue<CircularTextMode>(mode);
            curved->circleAlignment = comboValue<CircleAlignment>(start);
            curved->startAngle = angle->value();
            curved->textAlignment = comboValue<HorizontalAlignment>(alignment);
            curved->font = font->labelFont();
        };
    }

    layout->addWidget(general);
    if (tapeGroup) {
        layout->addWidget(tapeGroup);
    }
    layout->addWidget(specific, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    base.name = name->text();
    // Only write back what was changed, so rounding to 0.1 mm does not move
    // objects the user did not touch.
    const auto changed = [](const QDoubleSpinBox *box, qreal twips) {
        return !qFuzzyCompare(box->value(), QString::number(Units::twipsToMm(twips), 'f', box->decimals()).toDouble());
    };
    const QRectF bounds = base.bounds;
    if (changed(x, bounds.x())) {
        base.bounds.moveLeft(Units::mmToTwips(x->value()));
    }
    if (changed(y, bounds.y())) {
        base.bounds.moveTop(Units::mmToTwips(y->value()));
    }
    if (changed(width, bounds.width())) {
        base.bounds.setWidth(Units::mmToTwips(width->value()));
    }
    if (changed(height, bounds.height())) {
        base.bounds.setHeight(Units::mmToTwips(height->value()));
    }
    if (cell) {
        cell->lengthMode = comboValue<CellLengthMode>(cellLength);
        if (cell->lengthMode == CellLengthMode::Fixed && changed(fixedLength, cell->length)) {
            cell->length = Units::mmToTwips(fixedLength->value());
        }
        cell->borderWidth = Units::pointsToTwips(border->value());
    }
    base.rotation = comboValue<Rotation>(rotation);
    if (std::abs(angle->value() - base.angle) > 0.05) { // shown to 0.1 degree
        base.angle = angle->value();
    }
    base.mirrored = mirrored->isChecked();
    if (applySpecific) {
        applySpecific();
    }
    return true;
}
