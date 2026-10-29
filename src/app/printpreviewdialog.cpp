/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "printpreviewdialog.h"

#include "units.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

PrintPreviewDialog::PrintPreviewDialog(const QList<QImage> &pages, qreal dpi, const QString &printerName, const QStringList &notes, QWidget *parent)
    : QDialog(parent)
    , m_pages(pages)
    , m_dpi(dpi)
{
    setWindowTitle(i18n("Print Preview"));
    auto *layout = new QVBoxLayout(this);

    const QSize dots = pages.isEmpty() ? QSize() : pages.constFirst().size();
    auto *info = new QLabel(i18n("What %1 receives: %2 × %3 dots at %4 dpi (%5 × %6 mm), as the label feeds through the printer, in black and white.",
                                 printerName,
                                 dots.width(),
                                 dots.height(),
                                 qRound(dpi),
                                 QString::number(dots.width() / dpi * Units::MmPerInch, 'f', 1),
                                 QString::number(dots.height() / dpi * Units::MmPerInch, 'f', 1)),
                            this);
    info->setWordWrap(true);
    layout->addWidget(info);
    if (!notes.isEmpty()) {
        auto *warning = new QLabel(i18np("Note: %2", "%1 notes, first: %2", notes.size(), notes.constFirst()), this);
        warning->setWordWrap(true);
        layout->addWidget(warning);
    }

    m_image = new QLabel(this);
    m_image->setAlignment(Qt::AlignCenter);
    // White paper with a frame, on the window's background.
    m_image->setStyleSheet(u"QLabel { background: white; border: 1px solid palette(mid); }"_s);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidget(m_image);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_scroll->setMinimumSize(420, 320);
    layout->addWidget(m_scroll, 1);

    auto *controls = new QHBoxLayout;
    m_previous = new QPushButton(QIcon::fromTheme(u"go-previous"_s), i18n("Previous"), this);
    m_next = new QPushButton(QIcon::fromTheme(u"go-next"_s), i18n("Next"), this);
    m_position = new QLabel(this);
    connect(m_previous, &QPushButton::clicked, this, [this]() {
        showPage(m_index - 1);
    });
    connect(m_next, &QPushButton::clicked, this, [this]() {
        showPage(m_index + 1);
    });
    controls->addWidget(m_previous);
    controls->addWidget(m_position);
    controls->addWidget(m_next);
    controls->addStretch();
    m_zoomText = new QLabel(this);
    controls->addWidget(m_zoomText);
    // 0 fits the window; 1 shows every printer dot as one screen pixel, up
    // to 4 enlarges.
    m_zoom = new QSlider(Qt::Horizontal, this);
    m_zoom->setRange(0, 4);
    m_zoom->setValue(0);
    m_zoom->setMaximumWidth(120);
    connect(m_zoom, &QSlider::valueChanged, this, &PrintPreviewDialog::updateZoom);
    controls->addWidget(m_zoom);
    layout->addLayout(controls);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton *print = buttons->addButton(i18n("Print"), QDialogButtonBox::AcceptRole);
    print->setIcon(QIcon::fromTheme(u"document-print"_s));
    print->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    showPage(0);
    resize(720, 560);
}

void PrintPreviewDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (m_zoom && m_zoom->value() == 0) {
        updateZoom();
    }
}

void PrintPreviewDialog::showPage(int index)
{
    if (m_pages.isEmpty()) {
        m_position->setText(i18n("Nothing to print"));
        m_previous->setEnabled(false);
        m_next->setEnabled(false);
        return;
    }
    m_index = std::clamp(index, 0, int(m_pages.size() - 1));
    m_position->setText(i18n("Label %1 of %2", m_index + 1, m_pages.size()));
    m_previous->setEnabled(m_index > 0);
    m_next->setEnabled(m_index + 1 < m_pages.size());
    updateZoom();
}

void PrintPreviewDialog::updateZoom()
{
    if (m_pages.isEmpty()) {
        return;
    }
    const QImage &page = m_pages.at(m_index);
    const int zoom = m_zoom->value();
    if (zoom == 0) {
        // The whole label in the window, smoothed: an overview.
        m_zoomText->setText(i18n("Zoom: fit"));
        const QSize room = m_scroll->viewport()->size() - QSize(8, 8);
        m_image->setPixmap(QPixmap::fromImage(page.scaled(room, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    } else {
        // Whole dots, never smoothed: what a thermal head prints.
        m_zoomText->setText(i18n("Zoom: %1 × dots", zoom));
        m_image->setPixmap(QPixmap::fromImage(page.scaled(page.size() * zoom, Qt::IgnoreAspectRatio, Qt::FastTransformation)));
    }
    m_image->adjustSize();
}
