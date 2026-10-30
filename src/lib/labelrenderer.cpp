/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "labelrenderer.h"

#include "barcode.h"
#include "tapelayout.h"
#include "units.h"

#include <QFontMetricsF>
#include <QHash>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

#include <functional>

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>

#include <QtMath>

using namespace Qt::Literals::StringLiterals;

namespace LabelCreator
{

namespace
{

// Resolution assumed for images that do not record one, for ScaleMode None.
constexpr qreal DefaultImageDpi = 300.0;

// How DLS 8 sizes each barcode type: the narrow element width in inches for
// Small and Large (Medium is their mean), the wide:narrow ratio of
// two-width symbologies, and a per-size correction applied at 300 dpi and
// up. Values from DLS 8's barcode engine.
struct BarcodeType {
    Barcode::Symbology symbology;
    qreal smallWidth;
    qreal largeWidth;
    qreal wideRatio;
    qreal highResolutionFactor[3]; // Small, Medium, Large
};

const BarcodeType *barcodeType(const QString &type)
{
    // The "Lib" variants are library-binding layouts of Code 39 and Codabar.
    // Code128Ean is DLS's "UCC/EAN 128": Code 128 with a leading FNC1.
    // Postnet, Planet and IMail are DLS's names for the US postal codes; the
    // types after them are this program's own, beyond what DLS offers, and
    // their widths follow the nearest DLS type.
    static const QHash<QString, BarcodeType> types = {
        {u"code39"_s, {Barcode::Symbology::Code39, 0.011, 0.022, 2.0, {0.9, 0.95, 0.9}}},
        {u"code39cs"_s, {Barcode::Symbology::Code39WithCheckDigit, 0.011, 0.022, 2.0, {0.9, 0.95, 0.9}}},
        {u"code39libr"_s, {Barcode::Symbology::Code39, 0.011, 0.022, 2.0, {0.9, 0.95, 0.9}}},
        {u"code39libl"_s, {Barcode::Symbology::Code39, 0.011, 0.022, 2.0, {0.9, 0.95, 0.9}}},
        {u"code128auto"_s, {Barcode::Symbology::Code128, 0.015, 0.025, 1.0, {0.75, 0.9, 0.8}}},
        {u"code128a"_s, {Barcode::Symbology::Code128A, 0.015, 0.025, 1.0, {0.75, 0.9, 0.8}}},
        {u"code128b"_s, {Barcode::Symbology::Code128B, 0.015, 0.025, 1.0, {0.75, 0.85, 0.75}}},
        {u"code128c"_s, {Barcode::Symbology::Code128C, 0.015, 0.025, 1.0, {0.75, 0.9, 0.8}}},
        {u"code128ean"_s, {Barcode::Symbology::Code128Fnc1, 0.015, 0.025, 1.0, {0.6, 0.8, 0.8}}},
        {u"code2of5"_s, {Barcode::Symbology::Interleaved2of5, 0.0125, 0.025, 2.2, {0.7, 0.78, 0.75}}},
        {u"upca"_s, {Barcode::Symbology::UpcA, 0.017, 0.024, 1.0, {0.6, 0.75, 0.9}}},
        {u"upce"_s, {Barcode::Symbology::UpcE, 0.017, 0.024, 1.0, {0.6, 0.75, 0.9}}},
        {u"ean8"_s, {Barcode::Symbology::Ean8, 0.017, 0.024, 1.0, {0.6, 0.75, 0.9}}},
        {u"ean13"_s, {Barcode::Symbology::Ean13, 0.017, 0.024, 1.0, {0.6, 0.75, 0.9}}},
        {u"codabar"_s, {Barcode::Symbology::Codabar, 0.01, 0.02, 2.2, {1.0, 1.0, 1.0}}},
        {u"codabarlibr"_s, {Barcode::Symbology::Codabar, 0.01, 0.02, 2.2, {1.0, 1.0, 1.0}}},
        {u"codabarlibl"_s, {Barcode::Symbology::Codabar, 0.01, 0.02, 2.2, {1.0, 1.0, 1.0}}},
        {u"itf14"_s, {Barcode::Symbology::Itf14, 0.025, 0.05, 2.2, {0.75, 0.8, 0.75}}},
        {u"pdf417"_s, {Barcode::Symbology::Pdf417, 0.012, 0.0215, 1.0, {1.0, 1.0, 1.0}}},
        {u"qrcode"_s, {Barcode::Symbology::QrCode, 0.012, 0.0215, 1.0, {1.0, 1.0, 1.0}}},
        {u"postnet"_s, {Barcode::Symbology::Postnet, 0.02, 0.025, 1.0, {1.0, 1.0, 1.0}}},
        {u"planet"_s, {Barcode::Symbology::Planet, 0.02, 0.025, 1.0, {1.0, 1.0, 1.0}}},
        {u"imail"_s, {Barcode::Symbology::UspsIntelligentMail, 0.02, 0.025, 1.0, {1.0, 1.0, 1.0}}},
        {u"datamatrix"_s, {Barcode::Symbology::DataMatrix, 0.012, 0.0215, 1.0, {1.0, 1.0, 1.0}}},
        {u"aztec"_s, {Barcode::Symbology::Aztec, 0.012, 0.0215, 1.0, {1.0, 1.0, 1.0}}},
        {u"maxicode"_s, {Barcode::Symbology::MaxiCode, 0.012, 0.0215, 1.0, {1.0, 1.0, 1.0}}},
        {u"microqrcode"_s, {Barcode::Symbology::MicroQr, 0.012, 0.0215, 1.0, {1.0, 1.0, 1.0}}},
        {u"code93"_s, {Barcode::Symbology::Code93, 0.015, 0.025, 1.0, {0.75, 0.9, 0.8}}},
        {u"code11"_s, {Barcode::Symbology::Code11, 0.015, 0.025, 1.0, {0.75, 0.9, 0.8}}},
        {u"msiplessey"_s, {Barcode::Symbology::MsiPlessey, 0.015, 0.025, 1.0, {0.75, 0.9, 0.8}}},
        {u"pharmacode"_s, {Barcode::Symbology::Pharmacode, 0.015, 0.025, 1.0, {0.75, 0.9, 0.8}}},
        {u"gs1_128"_s, {Barcode::Symbology::Gs1_128, 0.015, 0.025, 1.0, {0.6, 0.8, 0.8}}},
        {u"gs1datamatrix"_s, {Barcode::Symbology::Gs1DataMatrix, 0.012, 0.0215, 1.0, {1.0, 1.0, 1.0}}},
        {u"gs1databar"_s, {Barcode::Symbology::Gs1DataBarOmni, 0.015, 0.025, 1.0, {0.75, 0.9, 0.8}}},
        {u"gs1databarexpanded"_s, {Barcode::Symbology::Gs1DataBarExpanded, 0.015, 0.025, 1.0, {0.75, 0.9, 0.8}}},
        {u"hibc128"_s, {Barcode::Symbology::Hibc128, 0.015, 0.025, 1.0, {0.75, 0.9, 0.8}}},
        {u"hibc39"_s, {Barcode::Symbology::Hibc39, 0.011, 0.022, 1.0, {0.9, 0.95, 0.9}}},
        {u"hibcdatamatrix"_s, {Barcode::Symbology::HibcDataMatrix, 0.012, 0.0215, 1.0, {1.0, 1.0, 1.0}}},
        {u"hibcqrcode"_s, {Barcode::Symbology::HibcQrCode, 0.012, 0.0215, 1.0, {1.0, 1.0, 1.0}}},
        {u"kix"_s, {Barcode::Symbology::Kix, 0.02, 0.025, 1.0, {1.0, 1.0, 1.0}}},
        {u"royalmail"_s, {Barcode::Symbology::RoyalMail4State, 0.02, 0.025, 1.0, {1.0, 1.0, 1.0}}},
    };
    const auto it = types.constFind(type.toLower());
    return it == types.constEnd() ? nullptr : &*it;
}

// Narrow and wide element widths in twips, rounded to whole dots at the
// printer's resolution the way DLS does it.
Barcode::BarWidths barWidths(const BarcodeType &type, BarcodeSize size, qreal dpi)
{
    const qreal medium = (type.smallWidth + type.largeWidth) / 2.0;
    qreal inches = 0;
    int factor = 0;
    switch (size) {
    case BarcodeSize::Small:
        inches = type.smallWidth;
        factor = 0;
        break;
    case BarcodeSize::Medium:
        inches = medium;
        factor = 1;
        break;
    case BarcodeSize::Large:
        inches = type.largeWidth;
        factor = 2;
        break;
    case BarcodeSize::SmallMedium:
        inches = (type.smallWidth + medium) / 2.0;
        factor = 1;
        break;
    }
    if (dpi >= 300) {
        inches *= type.highResolutionFactor[factor];
    }
    const int narrowDots = std::max(1, int(dpi * inches + 0.5));
    const int wideDots = std::max(narrowDots, int(type.wideRatio * inches * dpi + 0.5));
    const qreal twipsPerDot = Units::TwipsPerInch / dpi;
    return {narrowDots * twipsPerDot, wideDots * twipsPerDot};
}

Qt::Alignment toQt(HorizontalAlignment alignment)
{
    switch (alignment) {
    case HorizontalAlignment::Left:
        return Qt::AlignLeft;
    case HorizontalAlignment::Center:
        return Qt::AlignHCenter;
    case HorizontalAlignment::Right:
        return Qt::AlignRight;
    }
    Q_UNREACHABLE();
}

Qt::Alignment toQt(VerticalAlignment alignment)
{
    switch (alignment) {
    case VerticalAlignment::Top:
        return Qt::AlignTop;
    case VerticalAlignment::Middle:
        return Qt::AlignVCenter;
    case VerticalAlignment::Bottom:
        return Qt::AlignBottom;
    }
    Q_UNREACHABLE();
}

QRectF aligned(const QSizeF &size, const QRectF &box, Qt::Alignment alignment)
{
    QPointF topLeft = box.topLeft();
    if (alignment & Qt::AlignHCenter) {
        topLeft.rx() += (box.width() - size.width()) / 2.0;
    } else if (alignment & Qt::AlignRight) {
        topLeft.rx() += box.width() - size.width();
    }
    if (alignment & Qt::AlignVCenter) {
        topLeft.ry() += (box.height() - size.height()) / 2.0;
    } else if (alignment & Qt::AlignBottom) {
        topLeft.ry() += box.height() - size.height();
    }
    return QRectF(topLeft, size);
}

qreal rotationDegrees(Rotation rotation)
{
    // As DLS 8 does it: Rotation90 turns counter-clockwise, so text reads
    // bottom to top.
    switch (rotation) {
    case Rotation::Rotation0:
        return 0;
    case Rotation::Rotation90:
        return -90;
    case Rotation::Rotation180:
        return 180;
    case Rotation::Rotation270:
        return 90;
    }
    Q_UNREACHABLE();
}

struct TextRun {
    QString text;
    QFont font;
    QColor color;
    qreal width = 0;
};

struct TextLine {
    QList<TextRun> runs;
    qreal width = 0;
    // Ink that sticks out left of the first glyph's origin.
    qreal leftOverhang = 0;
    // Full font metrics; the descent includes the line gap, as WPF's line
    // spacing (which DLS 8 uses) does.
    qreal ascent = 0;
    qreal descent = 0;
    // The ink actually drawn, relative to the baseline.
    qreal inkAscent = 0;
    qreal inkDescent = 0;
};

// Splits the spans into lines at their line breaks. Also gives the last
// font, which empty lines take their height from.
std::pair<QList<TextLine>, QFont> splitLines(const TextObject &object)
{
    QList<TextLine> lines(1);
    QFont lastFont = toQFont(object.spans.isEmpty() ? LabelFont() : object.spans.constFirst().font);
    for (const TextSpan &span : object.spans) {
        const QFont font = toQFont(span.font);
        lastFont = font;
        const QStringList parts = span.text.split(u'\n');
        for (qsizetype i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                lines.append(TextLine());
            }
            QString part = parts.at(i);
            part.remove(u'\r');
            if (!part.isEmpty()) {
                lines.last().runs.append({part, font, span.color});
            }
        }
    }
    return {lines, lastFont};
}

// Measures @p lines at the nominal font sizes.
void measureLines(QList<TextLine> &lines, const QFont &lastFont)
{
    for (TextLine &line : lines) {
        if (line.runs.isEmpty()) {
            // An empty line still takes the height of the font around it.
            const QFontMetricsF metrics(lastFont);
            line.ascent = metrics.ascent();
            line.descent = metrics.descent() + metrics.leading();
            continue;
        }
        for (TextRun &run : line.runs) {
            const QFontMetricsF metrics(run.font);
            // Includes trailing spaces, like WPF's WidthIncludingTrailingWhitespace.
            run.width = metrics.horizontalAdvance(run.text);
            line.width += run.width;
            line.ascent = std::max(line.ascent, metrics.ascent());
            line.descent = std::max(line.descent, metrics.descent() + metrics.leading());
            const QRectF ink = metrics.tightBoundingRect(run.text);
            line.inkAscent = std::max(line.inkAscent, -ink.top());
            line.inkDescent = std::max(line.inkDescent, ink.bottom());
        }
        const TextRun &first = line.runs.constFirst();
        const TextRun &last = line.runs.constLast();
        line.leftOverhang = std::max(0.0, -QFontMetricsF(first.font).leftBearing(first.text.front()));
        const qreal rightOverhang = std::max(0.0, -QFontMetricsF(last.font).rightBearing(last.text.back()));
        line.width += line.leftOverhang + rightOverhang;
    }
}

// Splits the spans into lines and measures them at the nominal font sizes.
QList<TextLine> layoutLines(const TextObject &object)
{
    auto [lines, lastFont] = splitLines(object);
    measureLines(lines, lastFont);
    return lines;
}

// The size of a block of lines; see paintText().
struct TextBlock {
    qreal width = 0;
    qreal top = 0;
    qreal height = 0;
};

TextBlock measure(const QList<TextLine> &lines, bool useFullFontHeight)
{
    // Lines always stack at full line spacing. Without "full font height" the
    // block is measured by its ink instead, from the highest to the lowest
    // glyph, which is what DLS 8 fits and aligns.
    TextBlock block;
    qreal baseline = 0;
    qreal inkTop = std::numeric_limits<qreal>::max();
    qreal inkBottom = std::numeric_limits<qreal>::lowest();
    for (const TextLine &line : lines) {
        block.width = std::max(block.width, line.width);
        baseline += line.ascent;
        if (!line.runs.isEmpty()) {
            inkTop = std::min(inkTop, baseline - line.inkAscent);
            inkBottom = std::max(inkBottom, baseline + line.inkDescent);
        }
        baseline += line.descent;
    }
    block.height = baseline;
    if (!useFullFontHeight && inkBottom > inkTop) {
        block.top = inkTop;
        block.height = inkBottom - inkTop;
    }
    return block;
}

/*
 * Wraps a text object's lines at any width: after spaces, or inside a word
 * that is wider than a line on its own. Every word is measured once, so
 * trying many widths, as fitting the text to its box does, costs little.
 */
class TextWrapper
{
public:
    explicit TextWrapper(const TextObject &object)
    {
        std::tie(m_lines, m_lastFont) = splitLines(object);
        const QFontMetricsF last(m_lastFont);
        m_emptyAscent = last.ascent();
        m_emptyDescent = last.descent() + last.leading();
        for (int l = 0; l < m_lines.size(); ++l) {
            m_firstPiece << int(m_pieces.size());
            const QList<TextRun> &runs = m_lines.at(l).runs;
            for (int r = 0; r < runs.size(); ++r) {
                const QString &text = runs.at(r).text;
                const QFontMetricsF metrics(runs.at(r).font);
                qsizetype start = 0;
                for (qsizetype i = 0; i <= text.size(); ++i) {
                    // A piece ends after the last space before a word, or at the end.
                    if (i < text.size() && !(text.at(i).isSpace() && (i + 1 == text.size() || !text.at(i + 1).isSpace()))) {
                        continue;
                    }
                    const qsizetype end = i < text.size() ? i + 1 : text.size();
                    if (end == start) {
                        break;
                    }
                    Piece piece;
                    piece.line = l;
                    piece.run = r;
                    piece.from = start;
                    piece.to = end;
                    piece.inkTo = end;
                    while (piece.inkTo > start && text.at(piece.inkTo - 1).isSpace()) {
                        --piece.inkTo;
                    }
                    piece.width = metrics.horizontalAdvance(text.mid(start, end - start));
                    piece.inkWidth = metrics.horizontalAdvance(text.mid(start, piece.inkTo - start));
                    piece.ascent = metrics.ascent();
                    piece.descent = metrics.descent() + metrics.leading();
                    const QRectF ink = metrics.tightBoundingRect(text.mid(start, end - start));
                    piece.inkAscent = -ink.top();
                    piece.inkDescent = ink.bottom();
                    m_pieces << piece;
                    start = end;
                }
            }
        }
        m_firstPiece << int(m_pieces.size());
    }

    // The size of the block wrapped at @p width, from the words' measures:
    // without the kerning between words and the overhang of the outer glyphs.
    TextBlock blockAt(qreal width, bool useFullFontHeight) const
    {
        QList<TextLine> lines;
        for (const Line &line : breakAt(width)) {
            TextLine measured;
            if (line.segments.isEmpty()) {
                measured.ascent = m_emptyAscent;
                measured.descent = m_emptyDescent;
            }
            for (qsizetype i = 0; i < line.segments.size(); ++i) {
                const Segment &segment = line.segments.at(i);
                const Piece &piece = m_pieces.at(segment.piece);
                const bool trimmed = line.broken && i + 1 == line.segments.size();
                measured.width += segmentWidth(segment, trimmed);
                measured.ascent = std::max(measured.ascent, piece.ascent);
                measured.descent = std::max(measured.descent, piece.descent);
                measured.inkAscent = std::max(measured.inkAscent, piece.inkAscent);
                measured.inkDescent = std::max(measured.inkDescent, piece.inkDescent);
                measured.runs.append(TextRun()); // marks the line as having ink
            }
            lines << measured;
        }
        return measure(lines, useFullFontHeight);
    }

    // The lines wrapped at @p width, measured as they will be drawn.
    QList<TextLine> linesAt(qreal width) const
    {
        QList<TextLine> lines;
        for (const Line &line : breakAt(width)) {
            TextLine built;
            for (qsizetype i = 0; i < line.segments.size(); ++i) {
                const Segment &segment = line.segments.at(i);
                const Piece &piece = m_pieces.at(segment.piece);
                const TextRun &run = m_lines.at(line.hardLine).runs.at(piece.run);
                QString text = run.text.mid(segment.from, segment.to - segment.from);
                // Spaces where a line breaks take no room.
                if (line.broken && i + 1 == line.segments.size()) {
                    while (!text.isEmpty() && text.back().isSpace()) {
                        text.chop(1);
                    }
                }
                if (text.isEmpty()) {
                    continue;
                }
                if (i > 0 && !built.runs.isEmpty() && m_pieces.at(line.segments.at(i - 1).piece).run == piece.run) {
                    built.runs.last().text += text;
                } else {
                    built.runs.append({text, run.font, run.color});
                }
            }
            lines << built;
        }
        measureLines(lines, m_lastFont);
        return lines;
    }

private:
    // A word and the spaces after it, in one run of one line.
    struct Piece {
        int line = 0;
        int run = 0;
        qsizetype from = 0;
        qsizetype to = 0;
        qsizetype inkTo = 0; // without the spaces
        qreal width = 0;
        qreal inkWidth = 0;
        qreal ascent = 0;
        qreal descent = 0;
        qreal inkAscent = 0;
        qreal inkDescent = 0;
        // Advances of its first 0, 1, 2... characters, measured when the
        // piece has to break inside.
        mutable QList<qreal> prefix;
    };
    // A piece, or part of one, on a wrapped line.
    struct Segment {
        int piece = 0;
        qsizetype from = 0;
        qsizetype to = 0;
    };
    struct Line {
        int hardLine = 0;
        QList<Segment> segments;
        bool broken = false; // ends where the text wraps, not at its own line break
    };

    const QList<qreal> &prefixOf(const Piece &piece) const
    {
        if (piece.prefix.isEmpty()) {
            const TextRun &run = m_lines.at(piece.line).runs.at(piece.run);
            const QString &text = run.text;
            const QFontMetricsF metrics(run.font);
            for (qsizetype n = 0; n <= piece.to - piece.from; ++n) {
                piece.prefix << metrics.horizontalAdvance(text.mid(piece.from, n));
            }
        }
        return piece.prefix;
    }

    qreal segmentWidth(const Segment &segment, bool trimmed) const
    {
        const Piece &piece = m_pieces.at(segment.piece);
        const qsizetype to = trimmed ? std::min(segment.to, piece.inkTo) : segment.to;
        if (segment.from == piece.from && segment.to == piece.to) {
            return trimmed ? piece.inkWidth : piece.width;
        }
        const QList<qreal> &prefix = prefixOf(piece);
        return prefix.at(std::max(to, segment.from) - piece.from) - prefix.at(segment.from - piece.from);
    }

    QList<Line> breakAt(qreal width) const
    {
        QList<Line> lines;
        for (int l = 0; l < m_lines.size(); ++l) {
            Line line;
            line.hardLine = l;
            qreal used = 0;
            bool inked = false; // the line has more than spaces
            const auto wrap = [&]() {
                line.broken = true;
                lines << line;
                line = Line();
                line.hardLine = l;
                used = 0;
                inked = false;
            };
            for (int p = m_firstPiece.at(l); p < m_firstPiece.at(l + 1); ++p) {
                const Piece &piece = m_pieces.at(p);
                if (inked && used + piece.inkWidth > width) {
                    wrap();
                }
                if (!inked && piece.inkWidth > width && piece.inkTo - piece.from > 1) {
                    // Too wide on its own: as many characters as fit on each line.
                    const QList<qreal> &prefix = prefixOf(piece);
                    const qsizetype length = piece.inkTo - piece.from;
                    qsizetype at = 0;
                    while (true) {
                        qsizetype count = 1;
                        while (at + count < length && prefix.at(at + count + 1) - prefix.at(at) <= width) {
                            ++count;
                        }
                        if (at + count >= length) {
                            line.segments.append({p, piece.from + at, piece.to});
                            used += prefix.constLast() - prefix.at(at);
                            inked = true;
                            break;
                        }
                        line.segments.append({p, piece.from + at, piece.from + at + count});
                        wrap();
                        at += count;
                    }
                    continue;
                }
                line.segments.append({p, piece.from, piece.to});
                used += piece.width;
                inked = inked || piece.inkTo > piece.from;
            }
            lines << line;
        }
        return lines;
    }

    QList<TextLine> m_lines; // as the text breaks them
    QFont m_lastFont;
    qreal m_emptyAscent = 0;
    qreal m_emptyDescent = 0;
    QList<Piece> m_pieces;
    QList<int> m_firstPiece; // per line, and one past the last
};

// Counters and dates lay out their text like a text object.
template<typename Generated>
TextObject generatedText(const Generated &object, const QString &text)
{
    TextObject result;
    result.horizontalAlignment = object.layout.horizontalAlignment;
    result.verticalAlignment = object.layout.verticalAlignment;
    result.fitMode = object.layout.fitMode;
    result.useFullFontHeight = object.layout.useFullFontHeight;
    result.verticalized = object.layout.verticalized;
    result.spans = {{text, object.font, object.foreColor}};
    return result;
}

} // namespace

LabelRenderer::LabelRenderer(Mode mode)
    : m_mode(mode)
{
}

QFont LabelRenderer::printFont(QFont font) const
{
    // The render hint alone does not stop glyphs drawn through a scaled
    // painter from being antialiased; a thermal head has no grey.
    if (m_mode == Mode::Print) {
        font.setStyleStrategy(QFont::NoAntialias);
    }
    return font;
}

LabelRenderer::Mode LabelRenderer::mode() const
{
    return m_mode;
}

qreal LabelRenderer::printerResolution() const
{
    return m_printerResolution;
}

void LabelRenderer::setPrinterResolution(qreal dpi)
{
    m_printerResolution = dpi;
}

void LabelRenderer::setDateTime(const QDateTime &when)
{
    m_dateTime = when;
}

void LabelRenderer::setCounterStep(int step)
{
    m_counterStep = step;
}

QStringList LabelRenderer::warnings() const
{
    return m_warnings;
}

QImage LabelRenderer::renderImage(const LabelDocument &original, qreal dpi)
{
    // A tape label's length follows its content: lay it out to size the image.
    LabelDocument laidOut = original;
    TapeLayout::layout(laidOut);
    const LabelDocument &document = laidOut;
    const QSizeF canvas = document.canvasSize();
    const QSize size(std::max(1, qRound(Units::twipsToDots(canvas.width(), dpi))), std::max(1, qRound(Units::twipsToDots(canvas.height(), dpi))));

    QImage image(size, m_mode == Mode::Print ? QImage::Format_RGB32 : QImage::Format_ARGB32_Premultiplied);
    image.fill(m_mode == Mode::Print ? Qt::white : Qt::transparent);
    image.setDotsPerMeterX(qRound(dpi / Units::MmPerInch * 1000.0));
    image.setDotsPerMeterY(image.dotsPerMeterX());

    QPainter painter(&image);
    painter.scale(Units::twipsToDots(1.0, dpi), Units::twipsToDots(1.0, dpi));
    paint(&painter, document);
    return image;
}

bool LabelRenderer::writePdf(const LabelDocument &original, const QString &fileName, const QString &title)
{
    // A tape label's length follows its content: lay it out to size the page.
    LabelDocument document = original;
    TapeLayout::layout(document);
    const QSizeF canvas = document.canvasSize();
    QPdfWriter writer(fileName);
    writer.setTitle(title);
    writer.setCreator(u"Plasma Label Creator"_s);
    writer.setResolution(1200);
    writer.setPageMargins(QMarginsF());
    writer.setPageSize(QPageSize(QSizeF(Units::twipsToMm(canvas.width()), Units::twipsToMm(canvas.height())), QPageSize::Millimeter, title, QPageSize::ExactMatch));
    QPainter painter;
    if (!painter.begin(&writer)) {
        return false;
    }
    const qreal scale = writer.resolution() / Units::TwipsPerInch;
    painter.scale(scale, scale);
    paint(&painter, document);
    return painter.end();
}

void LabelRenderer::paint(QPainter *painter, const LabelDocument &laidOut)
{
    m_warnings.clear();
    // Tape labels are placed by their cells: lay out a copy, as read from a file.
    LabelDocument tapeCopy;
    if (laidOut.tape) {
        tapeCopy = laidOut;
        TapeLayout::layout(tapeCopy);
    }
    const LabelDocument &document = laidOut.tape ? tapeCopy : laidOut;

    painter->save();
    const bool preview = m_mode == Mode::Preview;
    painter->setRenderHint(QPainter::Antialiasing, preview);
    painter->setRenderHint(QPainter::TextAntialiasing, preview);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, preview);

    if (preview) {
        // The outline is stored as the paper feeds; DLS lays landscape labels
        // out with the leading edge on the left: paper (x, y) is canvas
        // (y, width - x).
        QTransform toCanvas;
        if (document.orientation == PaperOrientation::Landscape) {
            toCanvas.translate(0, document.paperSize().width());
            toCanvas.rotate(-90);
        }
        painter->setPen(QPen(QColor(160, 160, 160), 0));
        painter->setBrush(Qt::white);
        painter->drawPath(toCanvas.map(document.paperOutline));
    }

    for (const LabelObject &object : document.objects) {
        paintObject(painter, object);
    }

    // A tape label's cell borders, which print too.
    if (document.tape) {
        const std::function<void(const TapeCell &)> borders = [&](const TapeCell &cell) {
            if (cell.borderWidth > 0 && !cell.bounds.isEmpty()) {
                painter->setPen(QPen(cell.borderColor, cell.borderWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
                painter->setBrush(Qt::NoBrush);
                // Inside the cell, so neighbours share a line rather than doubling it.
                const qreal half = cell.borderWidth / 2;
                painter->drawRect(cell.bounds.adjusted(half, half, -half, -half));
            }
            for (const TapeCell &child : cell.subcells) {
                borders(child);
            }
        };
        borders(document.tape->root);
    }
    painter->restore();
}

qreal LabelRenderer::naturalLength(const LabelObject &object, qreal across, bool horizontal) const
{
    // The content's size before fitting, width then height, unturned.
    const auto textBlock = [](const QString &text, const LabelFont &font) {
        const QFontMetricsF metrics(toQFont(font));
        qreal width = 0;
        const QStringList lines = text.isEmpty() ? QStringList{u" "_s} : text.split(u'\n');
        for (const QString &line : lines) {
            width = std::max(width, metrics.horizontalAdvance(line));
        }
        return QSizeF(width, metrics.lineSpacing() * lines.size());
    };
    // Turned a quarter, an object's width runs the other way.
    const auto quarterTurned = [&object]() {
        const Rotation rotation = objectBase(object).rotation;
        return rotation == Rotation::Rotation90 || rotation == Rotation::Rotation270;
    };
    // Scales @p size so it fits @p across as @p fitMode says, and gives its length.
    const auto fitted = [&](QSizeF size, TextFitMode fitMode) {
        if (quarterTurned()) {
            size.transpose();
        }
        if (!horizontal) {
            size.transpose();
        }
        if (size.height() <= 0) {
            return size.width();
        }
        qreal factor = 1;
        if (fitMode == TextFitMode::AlwaysFit) {
            factor = across / size.height();
        } else if (fitMode == TextFitMode::ShrinkToFit) {
            factor = std::min<qreal>(1, across / size.height());
        }
        return size.width() * factor;
    };

    return std::visit(
        [&](const auto &o) -> qreal {
            using T = std::decay_t<decltype(o)>;
            if constexpr (std::is_same_v<T, TextObject> || std::is_same_v<T, AddressObject>) {
                return fitted(textBlock(o.plainText(), o.spans.isEmpty() ? LabelFont() : o.spans.constFirst().font), o.fitMode);
            } else if constexpr (std::is_same_v<T, CounterObject>) {
                return fitted(textBlock(o.text(0), o.font), o.layout.fitMode);
            } else if constexpr (std::is_same_v<T, DateTimeObject>) {
                return fitted(textBlock(o.text(QDateTime::currentDateTime()), o.font), o.layout.fitMode);
            } else if constexpr (std::is_same_v<T, ImageObject>) {
                QImage image;
                image.loadFromData(o.imageData);
                return image.isNull() ? across : fitted(QSizeF(image.size()), TextFitMode::AlwaysFit);
            } else if constexpr (std::is_same_v<T, BarcodeObject>) {
                const BarcodeType *type = barcodeType(o.type);
                if (!type) {
                    return across * 3;
                }
                Barcode barcode(type->symbology, o.text);
                if (!barcode.encode()) {
                    return across * 3;
                }
                const QSizeF symbol = barcode.naturalSize();
                if (Barcode::isTwoDimensional(type->symbology)) {
                    return fitted(symbol, TextFitMode::AlwaysFit);
                }
                // Modules at their bar width, plus quiet zones of ten modules.
                const qreal narrow = barWidths(*type, o.size, m_printerResolution).narrow;
                const qreal length = (symbol.width() + 20) * narrow;
                return quarterTurned() == horizontal ? across : length;
            } else if constexpr (std::is_same_v<T, CircularTextObject>) {
                return across;
            } else if constexpr (std::is_same_v<T, ShapeObject>) {
                return o.shapeType == ShapeType::VerticalLine ? 3 * o.lineWidth : across;
            } else {
                return across * 2;
            }
        },
        object);
}

void LabelRenderer::paintObject(QPainter *painter, const LabelObject &object)
{
    const ObjectBase &base = objectBase(object);
    if (base.bounds.isEmpty() && !std::holds_alternative<ShapeObject>(object)) {
        return;
    }

    painter->save();

    // Work in a box centred on the origin, turned and mirrored as the object
    // is; a quarter turn swaps its width and height.
    painter->translate(base.bounds.center());
    painter->rotate(rotationDegrees(base.rotation));
    if (base.mirrored) {
        painter->scale(-1, 1);
    }
    const bool quarterTurn = base.rotation == Rotation::Rotation90 || base.rotation == Rotation::Rotation270;
    const QSizeF size = quarterTurn ? base.bounds.size().transposed() : base.bounds.size();
    const QRectF box(QPointF(-size.width() / 2.0, -size.height() / 2.0), size);

    if (base.backColor.alpha() > 0) {
        painter->fillRect(box, base.backColor);
    }

    std::visit(
        [&](const auto &o) {
            using T = std::decay_t<decltype(o)>;
            if constexpr (std::is_same_v<T, TextObject> || std::is_same_v<T, AddressObject>) {
                paintText(painter, o, box);
            } else if constexpr (std::is_same_v<T, BarcodeObject>) {
                paintBarcode(painter, o, box);
            } else if constexpr (std::is_same_v<T, ImageObject>) {
                paintImage(painter, o, box);
            } else if constexpr (std::is_same_v<T, ShapeObject>) {
                paintShape(painter, o, box);
            } else if constexpr (std::is_same_v<T, CounterObject>) {
                paintText(painter, generatedText(o, o.text(m_counterStep)), box);
            } else if constexpr (std::is_same_v<T, DateTimeObject>) {
                paintText(painter, generatedText(o, o.text(m_dateTime.isValid() ? m_dateTime : QDateTime::currentDateTime())), box);
            } else if constexpr (std::is_same_v<T, CircularTextObject>) {
                paintCircularText(painter, o, box);
            } else {
                paintPlaceholder(painter, box, o.elementName);
            }
        },
        object);

    painter->restore();
}


void LabelRenderer::paintText(QPainter *painter, const TextObject &object, const QRectF &box)
{
    QList<TextLine> lines;
    TextBlock block;
    qreal scale = 1.0;
    if (!object.wrap) {
        lines = layoutLines(object);
        block = measure(lines, object.useFullFontHeight);
        if (block.width <= 0 || block.height <= 0) {
            return;
        }
        const qreal fit = std::min(box.width() / block.width, box.height() / block.height);
        if (object.fitMode == TextFitMode::AlwaysFit || (object.fitMode == TextFitMode::ShrinkToFit && fit < 1.0)) {
            scale = fit;
        }
    } else {
        // Wrapping text fills the box's width at the size it is drawn, so
        // smaller text wraps less: find the largest size whose lines fit.
        const TextWrapper wrapper(object);
        const auto fitsAt = [&](qreal s) {
            const TextBlock wrapped = wrapper.blockAt(box.width() / s, object.useFullFontHeight);
            return wrapped.height * s <= box.height() && wrapped.width * s <= box.width() * 1.0001;
        };
        if (object.fitMode != TextFitMode::None) {
            // Wrapping only adds lines, so the text unwrapped bounds the size.
            const TextBlock unwrapped = wrapper.blockAt(std::numeric_limits<qreal>::max(), object.useFullFontHeight);
            if (unwrapped.height <= 0) {
                return;
            }
            const qreal largest = object.fitMode == TextFitMode::AlwaysFit ? box.height() / unwrapped.height : 1.0;
            if (fitsAt(largest)) {
                scale = largest;
            } else {
                qreal low = 0;
                qreal high = largest;
                for (int i = 0; i < 24; ++i) {
                    const qreal middle = (low + high) / 2;
                    (fitsAt(middle) ? low : high) = middle;
                }
                scale = low > 0 ? low : high;
            }
        }
        lines = wrapper.linesAt(box.width() / scale);
        block = measure(lines, object.useFullFontHeight);
        if (block.width <= 0 || block.height <= 0) {
            return;
        }
        // The words' measures leave out kerning between them and the
        // outer glyphs' overhang: the lines as drawn must fit too.
        if (object.fitMode != TextFitMode::None) {
            scale /= std::max(1.0, std::max(block.width * scale / box.width(), block.height * scale / box.height()));
        }
    }

    painter->save();
    if (object.fitMode == TextFitMode::None) {
        painter->setClipRect(box, Qt::IntersectClip);
    }

    // Lay out in nominal font units inside a box scaled by the fit factor,
    // so every line keeps its proportions.
    const QRectF layoutBox(box.topLeft() / scale, box.size() / scale);
    const QRectF area = aligned(QSizeF(layoutBox.width(), block.height), layoutBox, toQt(object.verticalAlignment));
    painter->scale(scale, scale);

    qreal y = area.top() - block.top;
    for (const TextLine &line : lines) {
        y += line.ascent;
        qreal x = aligned(QSizeF(line.width, 0), QRectF(area.left(), 0, area.width(), 0), toQt(object.horizontalAlignment)).left();
        x += line.leftOverhang;
        for (const TextRun &run : line.runs) {
            painter->setFont(printFont(run.font));
            painter->setPen(run.color);
            painter->drawText(QPointF(x, y), run.text);
            x += run.width;
        }
        y += line.descent;
    }
    painter->restore();
}

void LabelRenderer::paintBarcode(QPainter *painter, const BarcodeObject &object, const QRectF &box)
{
    const BarcodeType *type = barcodeType(object.type);
    if (!type) {
        m_warnings << u"%1: barcode type \"%2\" is not supported"_s.arg(object.name, object.type);
        paintPlaceholder(painter, box, object.type);
        return;
    }

    // EAN and UPC set their digits between the guard bars, which zint lays
    // out; for the others DLS draws the text itself, in the object's font
    // and size, centred above or below the bars.
    const bool zintText = type->symbology == Barcode::Symbology::Ean8 || type->symbology == Barcode::Symbology::Ean13
        || type->symbology == Barcode::Symbology::UpcA || type->symbology == Barcode::Symbology::UpcE;
    const bool showText = object.textPosition != BarcodeTextPosition::None;

    Barcode barcode(type->symbology, object.text);
    barcode.setShowText(showText && zintText);
    barcode.setTextUsesFontSize(true);
    barcode.setErrorCorrectionLevel(object.errorCorrectionLevel);
    if (!barcode.encode()) {
        m_warnings << u"%1: %2"_s.arg(object.name, barcode.errorString());
        paintPlaceholder(painter, box, barcode.errorString());
        return;
    }

    // The native barcode library draws its text through GDI, where Windows
    // substitutes Arial for Helvetica.
    LabelFont textFont = object.textFont;
    if (textFont.family.compare(u"Helvetica"_s, Qt::CaseInsensitive) == 0) {
        textFont.family = u"Arial"_s;
    }
    const QFont font = printFont(toQFont(textFont));

    QRectF target = box.marginsRemoved(object.quietZonesPadding);
    const qreal textHeight = showText && !zintText ? QFontMetricsF(font).height() : 0;
    if (object.textPosition == BarcodeTextPosition::Top) {
        target.setTop(target.top() + textHeight);
    } else {
        target.setBottom(target.bottom() - textHeight);
    }

    painter->save();
    painter->setFont(font);
    const QRectF symbol = barcode.paint(painter, target, barWidths(*type, object.size, m_printerResolution), toQt(object.horizontalAlignment) | Qt::AlignVCenter);
    if (textHeight > 0 && !symbol.isEmpty()) {
        const qreal y = object.textPosition == BarcodeTextPosition::Top ? symbol.top() - textHeight : symbol.bottom();
        painter->setPen(object.foreColor);
        // GS1 data is typed with its application identifiers in square
        // brackets; the human-readable line shows them in round ones.
        QString text = object.text;
        if (Barcode::takesGs1Data(type->symbology)) {
            text.replace(u'[', u'(').replace(u']', u')');
        }
        painter->drawText(QRectF(symbol.left(), y, symbol.width(), textHeight), Qt::AlignHCenter | Qt::AlignTop | Qt::TextDontClip, text);
    }
    painter->restore();
}

void LabelRenderer::paintImage(QPainter *painter, const ImageObject &object, const QRectF &box)
{
    QImage image;
    if (object.imageLocation) {
        if (!object.imageLocation->isEmpty()) {
            image.load(*object.imageLocation);
        }
    } else {
        image.loadFromData(object.imageData);
    }

    if (!image.isNull()) {
        QSizeF size;
        switch (object.scaleMode) {
        case ImageScaleMode::Fill:
            size = box.size();
            break;
        case ImageScaleMode::Uniform:
            size = QSizeF(image.size()).scaled(box.size(), Qt::KeepAspectRatio);
            break;
        case ImageScaleMode::None: {
            const qreal dpi = image.dotsPerMeterX() > 0 ? image.dotsPerMeterX() * Units::MmPerInch / 1000.0 : DefaultImageDpi;
            size = QSizeF(Units::TwipsPerInch * image.width() / dpi, Units::TwipsPerInch * image.height() / dpi);
            break;
        }
        }

        painter->save();
        painter->setClipRect(box, Qt::IntersectClip);
        painter->drawImage(aligned(size, box, toQt(object.horizontalAlignment) | toQt(object.verticalAlignment)), image);
        painter->restore();
    } else if (!object.imageLocation || !object.imageLocation->isEmpty()) {
        m_warnings << u"%1: the image could not be loaded"_s.arg(object.name);
        paintPlaceholder(painter, box, object.name);
    }

    if (object.borderWidth > 0) {
        const qreal half = object.borderWidth / 2.0;
        painter->setPen(QPen(object.borderColor, object.borderWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(box.adjusted(half, half, -half, -half));
    }
}

void LabelRenderer::paintShape(QPainter *painter, const ShapeObject &object, const QRectF &box)
{
    const qreal width = object.lineWidth;
    const qreal half = width / 2.0;
    painter->setPen(QPen(object.foreColor, width, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    painter->setBrush(object.fillColor.alpha() > 0 ? QBrush(object.fillColor) : Qt::NoBrush);

    const auto offset = [&](qreal start, qreal end) {
        switch (object.lineAlignment) {
        case LineAlignment::LeftOrTop:
            return start + half;
        case LineAlignment::RightOrBottom:
            return end - half;
        case LineAlignment::Center:
            break;
        }
        return (start + end) / 2.0;
    };

    switch (object.shapeType) {
    case ShapeType::HorizontalLine: {
        const qreal y = offset(box.top(), box.bottom());
        painter->drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
        break;
    }
    case ShapeType::VerticalLine: {
        const qreal x = offset(box.left(), box.right());
        painter->drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));
        break;
    }
    case ShapeType::Rectangle:
        painter->drawRect(box.adjusted(half, half, -half, -half));
        break;
    case ShapeType::Ellipse:
        painter->drawEllipse(box.adjusted(half, half, -half, -half));
        break;
    }
}

void LabelRenderer::paintCircularText(QPainter *painter, const CircularTextObject &object, const QRectF &box)
{
    if (object.text.isEmpty() || box.isEmpty()) {
        return;
    }
    QFont font = printFont(toQFont(object.font));
    QFontMetricsF metrics(font);

    // The circle the baseline follows. A full circle sits inside the box; an
    // arc is the part of a circle whose chord is the box's width and whose
    // height is what the text leaves of the box's height.
    const bool circle = object.mode == CircularTextMode::Circle;
    const bool bottom = object.mode == CircularTextMode::ArcBottom;
    QPointF centre;
    qreal radius = 0;
    qreal available = 0; // arc length the text may use
    qreal middle = 0; // angle of the arc's middle, degrees, counter-clockwise from 3 o'clock
    const qreal textHeight = metrics.ascent() + metrics.descent();
    if (circle) {
        radius = std::max<qreal>(1, std::min(box.width(), box.height()) / 2.0 - metrics.ascent());
        centre = box.center();
        available = 2 * M_PI * radius;
        switch (object.circleAlignment) {
        case CircleAlignment::CenterAtTop:
            middle = 90;
            break;
        case CircleAlignment::CenterAtBottom:
            middle = 270;
            break;
        case CircleAlignment::StartAngle:
            middle = 90 - object.startAngle; // clockwise from the top
            break;
        }
    } else {
        const qreal chord = box.width() - textHeight;
        const qreal sagitta = std::max<qreal>(1, box.height() - textHeight);
        radius = (chord * chord / 4 + sagitta * sagitta) / (2 * sagitta);
        const qreal halfAngle = std::asin(std::min<qreal>(1, chord / 2 / radius));
        available = 2 * halfAngle * radius;
        if (bottom) {
            centre = QPointF(box.center().x(), box.bottom() - metrics.descent() - radius);
            middle = 270;
        } else {
            centre = QPointF(box.center().x(), box.top() + metrics.ascent() + radius);
            middle = 90;
        }
    }

    // Shrink the text until it fits the arc.
    qreal length = metrics.horizontalAdvance(object.text);
    if (length > available && length > 0) {
        font.setPixelSize(std::max(1, int(font.pixelSize() * available / length)));
        metrics = QFontMetricsF(font);
        length = metrics.horizontalAdvance(object.text);
    }

    // Text on the top runs clockwise (left to right over the top); on the
    // bottom it runs counter-clockwise so it still reads left to right.
    const bool clockwise = !(bottom || (circle && object.circleAlignment == CircleAlignment::CenterAtBottom));
    const qreal direction = clockwise ? -1 : 1;
    qreal offset = -length / 2; // along the arc, from its middle
    if (circle && object.circleAlignment == CircleAlignment::StartAngle) {
        // The text starts at the angle rather than being centred on it.
        // DLS stores the angle but its renderer does not show how it is
        // read; clockwise from the top is assumed.
        offset = 0;
    } else if (!circle && object.textAlignment == HorizontalAlignment::Left) {
        offset = -available / 2;
    } else if (!circle && object.textAlignment == HorizontalAlignment::Right) {
        offset = available / 2 - length;
    }

    painter->save();
    painter->setFont(font);
    painter->setPen(object.foreColor);
    for (const QChar c : object.text) {
        const qreal advance = metrics.horizontalAdvance(c);
        const qreal angle = middle + direction * qRadiansToDegrees((offset + advance / 2) / radius);
        const qreal radians = qDegreesToRadians(angle);
        const QPointF point(centre.x() + radius * std::cos(radians), centre.y() - radius * std::sin(radians));
        painter->save();
        painter->translate(point);
        // Upright on the outside of the circle on top, on the inside below.
        painter->rotate(clockwise ? 90 - angle : 270 - angle);
        painter->drawText(QPointF(-advance / 2, 0), QString(c));
        painter->restore();
        offset += advance;
    }
    painter->restore();
}

void LabelRenderer::paintPlaceholder(QPainter *painter, const QRectF &box, const QString &text)
{
    // Only the preview shows what could not be drawn; a print leaves it blank.
    if (m_mode == Mode::Print) {
        return;
    }
    painter->save();
    painter->setPen(QPen(QColor(200, 0, 0), 0, Qt::DashLine));
    painter->setBrush(QBrush(QColor(200, 0, 0, 40), Qt::BDiagPattern));
    painter->drawRect(box);
    QFont font = painter->font();
    font.setPixelSize(int(std::min(box.height() / 3.0, Units::pointsToTwips(8))));
    painter->setFont(font);
    painter->drawText(box, Qt::AlignCenter | Qt::TextWordWrap, text);
    painter->restore();
}

} // namespace LabelCreator
