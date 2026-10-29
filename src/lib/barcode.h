/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <QList>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <memory>

class QPainter;

struct zint_symbol;

namespace LabelCreator
{

/** Element widths in painter units; zero means "fit the target". */
struct BarWidths {
    /** The narrowest bar or space, i.e. one module. */
    qreal narrow = 0;
    /**
     * The wide bar or space of symbologies that use two widths (Code 39,
     * Codabar, 2 of 5). Zero keeps zint's own wide:narrow ratio.
     */
    qreal wide = 0;
};

/**
 * A barcode encoded by zint and kept as vector output, so it can be painted
 * sharply at any resolution: screen preview, PNG export or a 300 dpi printer.
 */
class Barcode
{
public:
    enum class Symbology {
        Code128,
        Code128A, // starts in code set A
        Code128B,
        Code128C,
        /**
         * Code 128 starting with FNC1, which makes it read as GS1-128. Like
         * DLS's "UCC/EAN 128", the data is not checked for application
         * identifiers.
         */
        Code128Fnc1,
        Code39,
        Code39WithCheckDigit,
        Codabar,
        Ean8,
        Ean13,
        UpcA,
        UpcE,
        Itf14,
        Interleaved2of5,
        QrCode,
        DataMatrix,
        Pdf417,
    };

    Barcode(Symbology symbology, const QString &data);
    ~Barcode();

    Barcode(Barcode &&) noexcept;
    Barcode &operator=(Barcode &&) noexcept;

    Symbology symbology() const;
    QString data() const;

    bool showText() const;
    void setShowText(bool show);

    /**
     * Draws the human-readable text at the size of the painter's font, as
     * label software does, instead of scaling it with the bars. The bars get
     * the height that leaves; the guard bars of EAN and UPC reach down into
     * the line of text.
     */
    bool textUsesFontSize() const;
    void setTextUsesFontSize(bool enabled);

    /** QR error correction, 0 (L) to 3 (H). Ignored by other symbologies. */
    int errorCorrectionLevel() const;
    void setErrorCorrectionLevel(int level);

    /**
     * Encodes the data. Returns false when zint rejects it, for example
     * letters in an EAN-13; errorString() then holds zint's message.
     */
    bool encode();

    bool isValid() const;
    QString errorString() const;

    /** Size of the encoded symbol in zint's own units (X-dimensions). */
    QSizeF naturalSize() const;

    using BarWidths = LabelCreator::BarWidths;

    /**
     * Paints the encoded symbol into @p target. With bar widths set, modules
     * get that width (shrunk if the symbol would not fit); otherwise 1D
     * symbols stretch to the full width and 2D symbols fill as much as their
     * aspect ratio allows. 1D symbols always fill the height. @p alignment places the symbol when it is
     * narrower than the target. The caller owns the render hints and the
     * font family of the human-readable text, so print paths can switch
     * antialiasing off. Returns the area the symbol was drawn in.
     */
    QRectF paint(QPainter *painter, const QRectF &target, BarWidths widths = {}, Qt::Alignment alignment = Qt::AlignCenter) const;

    static bool isTwoDimensional(Symbology symbology);
    static bool hasTwoWidths(Symbology symbology);

private:
    // A bar or space of a symbology Plasma Label Creator encodes itself.
    struct Element {
        bool bar;
        bool wide;
    };

    bool encodeCode39();
    QRectF paintTwoWidths(QPainter *painter, const QRectF &target, BarWidths widths, Qt::Alignment alignment) const;
    QRectF paintElements(QPainter *painter, const QRectF &target, BarWidths widths, Qt::Alignment alignment) const;

    struct SymbolDeleter {
        void operator()(zint_symbol *symbol) const;
    };

    Symbology m_symbology;
    QString m_data;
    bool m_showText = true;
    bool m_textUsesFontSize = false;
    int m_errorCorrectionLevel = 0;
    QString m_errorString;
    std::unique_ptr<zint_symbol, SymbolDeleter> m_symbol;
    // Code 39 is encoded here rather than by zint, which refuses "*" in the
    // data where DLS encodes it like any other character.
    QList<Element> m_elements;
};

} // namespace LabelCreator
