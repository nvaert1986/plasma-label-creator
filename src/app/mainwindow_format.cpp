/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow_p.h"

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;
using MainWindowPrivate::labelFileFilter;
using MainWindowPrivate::settings;

// The Format menu and toolbar: fonts, styles, alignment, text fit, white on black.

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

    // Long lines break at spaces to the box's width.
    m_wrap = new KToggleAction(QIcon::fromTheme(u"text-wrap"_s), i18n("Wrap Text"), this);
    m_wrap->setToolTip(i18n("Break long lines at spaces to fit the box's width"));
    actions->addAction(u"format_wrap"_s, m_wrap);
    connect(m_wrap, &QAction::triggered, this, &MainWindow::setSelectedWrap);
}

// The format actions change every selected object they apply to; their
// state shows the current object's.

void MainWindow::setSelectedInverted(bool inverted)
{
    LabelDocument document = m_editor->document();
    for (const int index : m_editor->selectedObjects()) {
        if (ObjectFactory::canInvert(document.objects.at(index))) {
            ObjectFactory::setInverted(document.objects[index], inverted);
        }
    }
    m_editor->apply(document, inverted ? i18n("White on Black") : i18n("Black on White"));
}

void MainWindow::setSelectedWrap(bool wrap)
{
    LabelDocument document = m_editor->document();
    for (const int index : m_editor->selectedObjects()) {
        TextObject *text = std::get_if<TextObject>(&document.objects[index]);
        if (!text) {
            text = std::get_if<AddressObject>(&document.objects[index]);
        }
        if (text) {
            text->wrap = wrap;
        }
    }
    m_editor->apply(document, wrap ? i18n("Wrap Text") : i18n("Do Not Wrap Text"));
}

void MainWindow::setSelectedFitMode(TextFitMode mode)
{
    LabelDocument document = m_editor->document();
    for (const int index : m_editor->selectedObjects()) {
        TextObject *text = std::get_if<TextObject>(&document.objects[index]);
        if (!text) {
            text = std::get_if<AddressObject>(&document.objects[index]);
        }
        if (text) {
            text->fitMode = mode;
        }
    }
    m_editor->apply(document, i18n("Change Text Fit"));
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
    LabelDocument document = m_editor->document();
    for (const int index : m_editor->selectedObjects()) {
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
    }
    m_editor->apply(document, i18n("Change Font"));
}

void MainWindow::setSelectedAlignment(std::optional<HorizontalAlignment> horizontal, std::optional<VerticalAlignment> vertical)
{
    LabelDocument document = m_editor->document();
    for (const int index : m_editor->selectedObjects()) {
        std::visit(
            [&](auto &o) {
                using T = std::decay_t<decltype(o)>;
                if constexpr (std::is_same_v<T, TextObject> || std::is_same_v<T, AddressObject> || std::is_same_v<T, ImageObject>) {
                    o.horizontalAlignment = horizontal.value_or(o.horizontalAlignment);
                    o.verticalAlignment = vertical.value_or(o.verticalAlignment);
                } else if constexpr (std::is_same_v<T, CounterObject> || std::is_same_v<T, DateTimeObject>) {
                    o.layout.horizontalAlignment = horizontal.value_or(o.layout.horizontalAlignment);
                    o.layout.verticalAlignment = vertical.value_or(o.layout.verticalAlignment);
                } else if constexpr (std::is_same_v<T, BarcodeObject>) {
                    o.horizontalAlignment = horizontal.value_or(o.horizontalAlignment);
                } else if constexpr (std::is_same_v<T, CircularTextObject>) {
                    o.textAlignment = horizontal.value_or(o.textAlignment);
                }
            },
            document.objects[index]);
    }
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
        bool wraps = false;
        if (index >= 0) {
            const LabelObject &object = m_editor->document().objects.at(index);
            if (const auto *text = std::get_if<TextObject>(&object)) {
                fitMode = text->fitMode;
                wraps = text->wrap;
            } else if (const auto *address = std::get_if<AddressObject>(&object)) {
                fitMode = address->fitMode;
                wraps = address->wrap;
            }
        }
        m_wrap->setEnabled(fitMode.has_value());
        m_wrap->setChecked(wraps);
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
