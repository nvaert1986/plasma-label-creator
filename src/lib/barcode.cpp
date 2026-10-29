/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "barcode.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPolygonF>
#include <QRectF>

#include <zint.h>

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

int zintSymbology(Barcode::Symbology symbology)
{
    switch (symbology) {
    case Barcode::Symbology::Code128:
    case Barcode::Symbology::Code128A:
    case Barcode::Symbology::Code128B:
    case Barcode::Symbology::Code128C:
    case Barcode::Symbology::Code128Fnc1:
        return BARCODE_CODE128;
    case Barcode::Symbology::Code39:
    case Barcode::Symbology::Code39WithCheckDigit:
        return BARCODE_CODE39;
    case Barcode::Symbology::Codabar:
        return BARCODE_CODABAR;
    case Barcode::Symbology::Ean8:
        return BARCODE_EAN8;
    case Barcode::Symbology::Ean13:
        return BARCODE_EAN13;
    case Barcode::Symbology::UpcA:
        return BARCODE_UPCA;
    case Barcode::Symbology::UpcE:
        return BARCODE_UPCE;
    case Barcode::Symbology::Itf14:
        return BARCODE_ITF14;
    case Barcode::Symbology::Interleaved2of5:
        return BARCODE_C25INTER;
    case Barcode::Symbology::QrCode:
        return BARCODE_QRCODE;
    case Barcode::Symbology::DataMatrix:
        return BARCODE_DATAMATRIX;
    case Barcode::Symbology::Pdf417:
        return BARCODE_PDF417;
    case Barcode::Symbology::Aztec:
        return BARCODE_AZTEC;
    case Barcode::Symbology::MaxiCode:
        return BARCODE_MAXICODE;
    case Barcode::Symbology::MicroQr:
        return BARCODE_MICROQR;
    case Barcode::Symbology::Code93:
        return BARCODE_CODE93;
    case Barcode::Symbology::Code11:
        return BARCODE_CODE11;
    case Barcode::Symbology::MsiPlessey:
        return BARCODE_MSI_PLESSEY;
    case Barcode::Symbology::Pharmacode:
        return BARCODE_PHARMA;
    case Barcode::Symbology::Gs1_128:
        return BARCODE_GS1_128;
    case Barcode::Symbology::Gs1DataMatrix:
        return BARCODE_DATAMATRIX;
    case Barcode::Symbology::Gs1DataBarOmni:
        return BARCODE_DBAR_OMN;
    case Barcode::Symbology::Gs1DataBarExpanded:
        return BARCODE_DBAR_EXP;
    case Barcode::Symbology::Hibc128:
        return BARCODE_HIBC_128;
    case Barcode::Symbology::Hibc39:
        return BARCODE_HIBC_39;
    case Barcode::Symbology::HibcDataMatrix:
        return BARCODE_HIBC_DM;
    case Barcode::Symbology::HibcQrCode:
        return BARCODE_HIBC_QR;
    case Barcode::Symbology::Postnet:
        return BARCODE_POSTNET;
    case Barcode::Symbology::Planet:
        return BARCODE_PLANET;
    case Barcode::Symbology::UspsIntelligentMail:
        return BARCODE_USPS_IMAIL;
    case Barcode::Symbology::Kix:
        return BARCODE_KIX;
    case Barcode::Symbology::RoyalMail4State:
        return BARCODE_RM4SCC;
    }
    Q_UNREACHABLE();
}

// zint escape sequence that starts a Code 128 variant, if it needs one.
QByteArray code128Prefix(Barcode::Symbology symbology)
{
    switch (symbology) {
    case Barcode::Symbology::Code128A:
        return QByteArrayLiteral("\\^A");
    case Barcode::Symbology::Code128B:
        return QByteArrayLiteral("\\^B");
    case Barcode::Symbology::Code128C:
        return QByteArrayLiteral("\\^C");
    case Barcode::Symbology::Code128Fnc1:
        return QByteArrayLiteral("\\^1");
    default:
        return {};
    }
}

// zint hexagons (MaxiCode) are given by centre and inscribed diameter.
QPolygonF hexagon(const zint_vector_hexagon *hex)
{
    const qreal radius = hex->diameter / std::sqrt(3.0);
    const qreal startDegrees = 90.0 + hex->rotation;
    QPolygonF polygon;
    for (int i = 0; i < 6; ++i) {
        const qreal angle = qDegreesToRadians(startDegrees + i * 60.0);
        polygon << QPointF(hex->x + radius * std::cos(angle), hex->y - radius * std::sin(angle));
    }
    return polygon;
}

QPointF alignedOrigin(const QSizeF &size, const QRectF &target, Qt::Alignment alignment)
{
    QPointF origin = target.topLeft();
    if (alignment & Qt::AlignHCenter) {
        origin.rx() += (target.width() - size.width()) / 2.0;
    } else if (alignment & Qt::AlignRight) {
        origin.rx() += target.width() - size.width();
    }
    if (alignment & Qt::AlignVCenter) {
        origin.ry() += (target.height() - size.height()) / 2.0;
    } else if (alignment & Qt::AlignBottom) {
        origin.ry() += target.height() - size.height();
    }
    return origin;
}

// Draws the human-readable text unscaled at positions mapped from zint's
// coordinates, so stretched bars do not stretch the digits. With a zero
// text scale the painter's font is used as it is.
template<typename MapPoint>
void paintStrings(QPainter *painter, const zint_vector *vector, MapPoint map, qreal textScale)
{
    painter->save();
    painter->setPen(Qt::black);
    for (const zint_vector_string *string = vector->strings; string; string = string->next) {
        if (textScale > 0) {
            QFont font = painter->font();
            font.setPixelSize(std::max(1, qRound(string->fsize * textScale)));
            painter->setFont(font);
        }

        const QString text = QString::fromUtf8(reinterpret_cast<const char *>(string->text), string->length);
        const qreal width = painter->fontMetrics().horizontalAdvance(text);

        painter->save();
        painter->translate(map(QPointF(string->x, string->y)));
        painter->rotate(string->rotation);
        // halign: 0 centre, 1 left, 2 right; y is the baseline.
        qreal x = 0;
        if (string->halign == 0) {
            x = -width / 2.0;
        } else if (string->halign == 2) {
            x = -width;
        }
        painter->drawText(QPointF(x, 0), text);
        painter->restore();
    }
    painter->restore();
}

} // namespace

void Barcode::SymbolDeleter::operator()(zint_symbol *symbol) const
{
    ZBarcode_Delete(symbol);
}

Barcode::Barcode(Symbology symbology, const QString &data)
    : m_symbology(symbology)
    , m_data(data)
{
}

Barcode::~Barcode() = default;
Barcode::Barcode(Barcode &&) noexcept = default;
Barcode &Barcode::operator=(Barcode &&) noexcept = default;

Barcode::Symbology Barcode::symbology() const
{
    return m_symbology;
}

QString Barcode::data() const
{
    return m_data;
}

bool Barcode::showText() const
{
    return m_showText;
}

void Barcode::setShowText(bool show)
{
    if (m_showText != show) {
        m_showText = show;
        m_symbol.reset();
        m_elements.clear();
    }
}

bool Barcode::textUsesFontSize() const
{
    return m_textUsesFontSize;
}

void Barcode::setTextUsesFontSize(bool enabled)
{
    m_textUsesFontSize = enabled;
}

int Barcode::errorCorrectionLevel() const
{
    return m_errorCorrectionLevel;
}

void Barcode::setErrorCorrectionLevel(int level)
{
    level = std::clamp(level, 0, 3);
    if (m_errorCorrectionLevel != level) {
        m_errorCorrectionLevel = level;
        m_symbol.reset();
        m_elements.clear();
    }
}

bool Barcode::encode()
{
    m_elements.clear();
    m_errorString.clear();
    if (m_symbology == Symbology::Code39 || m_symbology == Symbology::Code39WithCheckDigit) {
        m_symbol.reset();
        return encodeCode39();
    }

    m_symbol.reset(ZBarcode_Create());
    if (!m_symbol) {
        m_errorString = QStringLiteral("Out of memory");
        return false;
    }

    m_symbol->symbology = zintSymbology(m_symbology);
    m_symbol->input_mode = takesGs1Data(m_symbology) ? GS1_MODE : UNICODE_MODE;
    m_symbol->show_hrt = m_showText ? 1 : 0;
    // Label files carry their own quiet-zone padding.
    m_symbol->output_options |= BARCODE_NO_QUIET_ZONES;
    // zint's vector output uses two units per module at scale 1; at 0.5 one
    // unit is one module, so element widths map directly.
    m_symbol->scale = 0.5f;
    if (m_symbology == Symbology::QrCode) {
        m_symbol->option_1 = m_errorCorrectionLevel + 1;
    }
    // Square Data Matrix symbols only, which every scanner reads; zint would
    // otherwise pick a rectangular one when it is smaller.
    if (m_symbology == Symbology::DataMatrix || m_symbology == Symbology::Gs1DataMatrix || m_symbology == Symbology::HibcDataMatrix) {
        m_symbol->option_3 = DM_SQUARE;
    }

    QByteArray utf8 = m_data.toUtf8();
    const QByteArray prefix = code128Prefix(m_symbology);
    if (!prefix.isEmpty()) {
        // Forced code sets and FNC1 are zint escape sequences, so backslashes
        // in the data itself must be escaped. zint reads its "\^" codes
        // first, so a literal "\^" is written "\^^" rather than "\\^".
        m_symbol->input_mode |= ESCAPE_MODE | EXTRA_ESCAPE_MODE;
        QByteArray escaped = prefix;
        for (qsizetype i = 0; i < utf8.size(); ++i) {
            if (utf8.at(i) != '\\') {
                escaped += utf8.at(i);
                continue;
            }
            const bool caret = i + 1 < utf8.size() && utf8.at(i + 1) == '^';
            escaped += caret ? QByteArrayLiteral("\\^^") : QByteArrayLiteral("\\\\");
            if (caret) {
                ++i;
            }
        }
        utf8 = escaped;
    }
    const int result = ZBarcode_Encode_and_Buffer_Vector(m_symbol.get(),
                                                         reinterpret_cast<const unsigned char *>(utf8.constData()),
                                                         int(utf8.size()),
                                                         0);
    if (result >= ZINT_ERROR) {
        m_errorString = QString::fromUtf8(m_symbol->errtxt);
        m_symbol.reset();
        return false;
    }
    return true;
}

bool Barcode::encodeCode39()
{
    // The characters in check-digit order, then the start/stop character.
    static constexpr char characters[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%*";
    // Nine elements each, bar first; a set bit is a wide element.
    static constexpr quint16 patterns[] = {
        0b000110100, 0b100100001, 0b001100001, 0b101100000, 0b000110001, 0b100110000, 0b001110000, 0b000100101, 0b100100100,
        0b001100100, 0b100001001, 0b001001001, 0b101001000, 0b000011001, 0b100011000, 0b001011000, 0b000001101, 0b100001100,
        0b001001100, 0b000011100, 0b100000011, 0b001000011, 0b101000010, 0b000010011, 0b100010010, 0b001010010, 0b000000111,
        0b100000110, 0b001000110, 0b000010110, 0b110000001, 0b011000001, 0b111000000, 0b010010001, 0b110010000, 0b011010000,
        0b010000101, 0b110000100, 0b011000100, 0b010101000, 0b010100010, 0b010001010, 0b000101010, 0b010010100,
    };
    constexpr int startStop = 43;

    QList<int> symbols{startStop};
    int checkSum = 0;
    for (qsizetype i = 0; i < m_data.size(); ++i) {
        const QChar c = m_data.at(i);
        const char *found = c.unicode() < 128 ? std::strchr(characters, char(c.unicode())) : nullptr;
        if (!found || c.unicode() == 0) {
            m_errorString = c.isLower() ? u"Lowercase letters cannot be encoded in Code 39"_s
                                        : u"Invalid character at position %1 for Code 39"_s.arg(i + 1);
            return false;
        }
        const int index = int(found - characters);
        symbols << index;
        if (index != startStop) {
            checkSum += index;
        }
    }
    if (m_data.isEmpty()) {
        m_errorString = u"No data to encode"_s;
        return false;
    }
    if (m_symbology == Symbology::Code39WithCheckDigit) {
        symbols << checkSum % 43;
    }
    symbols << startStop;

    for (qsizetype i = 0; i < symbols.size(); ++i) {
        if (i > 0) {
            m_elements.append({false, false}); // narrow gap between characters
        }
        const quint16 pattern = patterns[symbols.at(i)];
        for (int element = 0; element < 9; ++element) {
            m_elements.append({element % 2 == 0, bool(pattern & (1 << (8 - element)))});
        }
    }
    return true;
}

bool Barcode::isValid() const
{
    return (m_symbol && m_symbol->vector) || !m_elements.isEmpty();
}

QString Barcode::errorString() const
{
    return m_errorString;
}

QSizeF Barcode::naturalSize() const
{
    if (!isValid()) {
        return {};
    }
    if (!m_elements.isEmpty()) {
        // In modules at a 2:1 ratio, as zint would draw it, 50 modules high.
        qreal width = 0;
        for (const Element &element : m_elements) {
            width += element.wide ? 2 : 1;
        }
        return QSizeF(width, 50);
    }
    return QSizeF(m_symbol->vector->width, m_symbol->vector->height);
}

bool Barcode::hasTwoWidths(Symbology symbology)
{
    switch (symbology) {
    case Symbology::Code39:
    case Symbology::Code39WithCheckDigit:
    case Symbology::Codabar:
    case Symbology::Itf14:
    case Symbology::Interleaved2of5:
        return true;
    default:
        return false;
    }
}

bool Barcode::isTwoDimensional(Symbology symbology)
{
    switch (symbology) {
    case Symbology::QrCode:
    case Symbology::DataMatrix:
    case Symbology::Pdf417:
    case Symbology::Aztec:
    case Symbology::MaxiCode:
    case Symbology::MicroQr:
    case Symbology::Gs1DataMatrix:
    case Symbology::HibcDataMatrix:
    case Symbology::HibcQrCode:
        return true;
    default:
        return false;
    }
}

bool Barcode::takesGs1Data(Symbology symbology)
{
    return symbology == Symbology::Gs1_128 || symbology == Symbology::Gs1DataMatrix || symbology == Symbology::Gs1DataBarExpanded;
}

QRectF Barcode::paint(QPainter *painter, const QRectF &target, BarWidths widths, Qt::Alignment alignment) const
{
    if (!isValid() || target.isEmpty()) {
        return {};
    }
    if (!m_elements.isEmpty()) {
        return paintElements(painter, target, widths, alignment);
    }
    if (widths.narrow > 0 && widths.wide > 0 && hasTwoWidths(m_symbology)) {
        return paintTwoWidths(painter, target, widths, alignment);
    }

    const zint_vector *vector = m_symbol->vector;
    qreal scaleX = target.width() / vector->width;
    qreal scaleY = target.height() / vector->height;

    if (isTwoDimensional(m_symbology)) {
        scaleX = scaleY = std::min(scaleX, scaleY);
        if (widths.narrow > 0) {
            scaleX = scaleY = std::min(scaleX, widths.narrow);
        }
    } else if (widths.narrow > 0) {
        scaleX = std::min(scaleX, widths.narrow);
    }

    const bool fontSizedText = m_textUsesFontSize && vector->strings && !isTwoDimensional(m_symbology);
    const QSizeF size(vector->width * scaleX, fontSizedText ? target.height() : vector->height * scaleY);
    const QPointF origin = alignedOrigin(size, target, alignment);

    // Vertical placement. Normally zint's layout is scaled as a whole. With
    // font-sized text the bars take what the line of text leaves, and
    // whatever zint draws below the shortest bars (EAN/UPC guard bars) is
    // stretched over that line.
    const QFontMetricsF metrics(painter->font());
    qreal barsBottom = vector->height;
    qreal longestBottom = 0;
    for (const zint_vector_rect *rect = vector->rectangles; rect; rect = rect->next) {
        barsBottom = std::min<qreal>(barsBottom, rect->y + rect->height);
        longestBottom = std::max<qreal>(longestBottom, rect->y + rect->height);
    }
    const qreal textTop = size.height() - metrics.height();
    const auto mapY = [&](qreal y) {
        if (!fontSizedText) {
            return origin.y() + y * scaleY;
        }
        if (y <= barsBottom) {
            return origin.y() + y / barsBottom * textTop;
        }
        const qreal below = longestBottom > barsBottom ? (y - barsBottom) / (longestBottom - barsBottom) : 1.0;
        return origin.y() + textTop + std::min(1.0, below) * metrics.height();
    };

    painter->save();
    for (const zint_vector_rect *rect = vector->rectangles; rect; rect = rect->next) {
        // Positive colours are only used for the colour-capable symbologies;
        // a thermal printer has black and nothing else.
        const bool background = rect->colour == 8;
        const qreal top = mapY(rect->y);
        painter->fillRect(QRectF(origin.x() + rect->x * scaleX, top, rect->width * scaleX, mapY(rect->y + rect->height) - top),
                          background ? Qt::white : Qt::black);
    }

    painter->translate(origin);
    painter->scale(scaleX, scaleY);
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::black);

    for (const zint_vector_hexagon *hex = vector->hexagons; hex; hex = hex->next) {
        painter->drawPolygon(hexagon(hex));
    }

    for (const zint_vector_circle *circle = vector->circles; circle; circle = circle->next) {
        const QRectF bounds(circle->x - circle->diameter / 2.0, circle->y - circle->diameter / 2.0, circle->diameter, circle->diameter);
        const QColor colour = circle->colour ? Qt::white : Qt::black;
        if (circle->width > 0) {
            painter->setPen(QPen(colour, circle->width));
            painter->setBrush(Qt::NoBrush);
        } else {
            painter->setPen(Qt::NoPen);
            painter->setBrush(colour);
        }
        painter->drawEllipse(bounds);
    }

    painter->restore();

    if (fontSizedText) {
        const qreal baseline = origin.y() + size.height() - metrics.descent();
        paintStrings(
            painter,
            vector,
            [&](const QPointF &point) {
                return QPointF(origin.x() + point.x() * scaleX, baseline);
            },
            0);
    } else {
        paintStrings(
            painter,
            vector,
            [&](const QPointF &point) {
                return origin + QPointF(point.x() * scaleX, point.y() * scaleY);
            },
            std::min(scaleX, scaleY));
    }
    return QRectF(origin, size);
}

QRectF Barcode::paintTwoWidths(QPainter *painter, const QRectF &target, BarWidths widths, Qt::Alignment alignment) const
{
    // zint draws wide elements as runs of two or three modules; label
    // software uses its own wide:narrow ratio, so lay the elements out again
    // from the module pattern.
    struct Element {
        bool bar;
        bool wide;
    };
    const auto moduleIsSet = [this](int x) {
        return (m_symbol->encoded_data[0][x >> 3] >> (x & 7)) & 1;
    };
    QList<Element> elements;
    qreal natural = 0;
    for (int x = 0; x < m_symbol->width;) {
        const bool bar = moduleIsSet(x);
        int run = 1;
        while (x + run < m_symbol->width && bool(moduleIsSet(x + run)) == bar) {
            ++run;
        }
        elements.append({bar, run > 1});
        natural += run > 1 ? widths.wide : widths.narrow;
        x += run;
    }

    // The bars' vertical extent and horizontal span in zint's layout; wider
    // rectangles are bearer bars or boxes, which are not drawn.
    const zint_vector *vector = m_symbol->vector;
    qreal top = vector->height;
    qreal bottom = 0;
    qreal left = vector->width;
    qreal right = 0;
    for (const zint_vector_rect *rect = vector->rectangles; rect; rect = rect->next) {
        if (rect->width <= 3) {
            top = std::min<qreal>(top, rect->y);
            bottom = std::max<qreal>(bottom, rect->y + rect->height);
            left = std::min<qreal>(left, rect->x);
            right = std::max<qreal>(right, rect->x + rect->width);
        }
    }
    if (natural <= 0 || right <= left || bottom <= top) {
        return {};
    }

    const qreal scaleX = std::min(1.0, target.width() / natural);
    const qreal scaleY = target.height() / vector->height;
    const qreal width = natural * scaleX;
    const QPointF origin = alignedOrigin(QSizeF(width, vector->height * scaleY), target, alignment);

    qreal x = origin.x();
    for (const Element &element : elements) {
        const qreal elementWidth = (element.wide ? widths.wide : widths.narrow) * scaleX;
        if (element.bar) {
            painter->fillRect(QRectF(x, origin.y() + top * scaleY, elementWidth, (bottom - top) * scaleY), Qt::black);
        }
        x += elementWidth;
    }

    const qreal spanScale = width / (right - left);
    paintStrings(
        painter,
        vector,
        [&](const QPointF &point) {
            return QPointF(origin.x() + (point.x() - left) * spanScale, origin.y() + point.y() * scaleY);
        },
        std::min(spanScale, scaleY));
    return QRectF(origin, QSizeF(width, vector->height * scaleY));
}

QRectF Barcode::paintElements(QPainter *painter, const QRectF &target, BarWidths widths, Qt::Alignment alignment) const
{
    // Without widths the symbol stretches to the target at a 2:1 ratio. The
    // bars fill the height; the caller draws any text.
    const bool stretch = widths.narrow <= 0;
    if (stretch) {
        widths = {1, 2};
    } else if (widths.wide <= 0) {
        widths.wide = 2 * widths.narrow;
    }
    qreal natural = 0;
    for (const Element &element : m_elements) {
        natural += element.wide ? widths.wide : widths.narrow;
    }
    const qreal scale = stretch ? target.width() / natural : std::min(1.0, target.width() / natural);
    const QSizeF size(natural * scale, target.height());
    const QPointF origin = alignedOrigin(size, target, alignment);

    qreal x = origin.x();
    for (const Element &element : m_elements) {
        const qreal width = (element.wide ? widths.wide : widths.narrow) * scale;
        if (element.bar) {
            painter->fillRect(QRectF(x, origin.y(), width, size.height()), Qt::black);
        }
        x += width;
    }
    return QRectF(origin, size);
}
