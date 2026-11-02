/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "printonlypanel.h"

#include "answersform.h"
#include "labelformat.h"
#include "labelrenderer.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

using namespace LabelCreator;
using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr int ThumbnailSize = 120; // pixels, the longer side

// A small picture of the label in @p fileName; a blank one if it cannot be read.
QIcon thumbnail(const QString &fileName)
{
    LabelReader reader;
    const auto document = reader.readFile(fileName);
    if (!document) {
        return QIcon::fromTheme(u"document-close"_s);
    }
    const QSizeF canvas = document->canvasSize();
    const qreal scale = ThumbnailSize / std::max(canvas.width(), canvas.height());
    QImage image((canvas * scale).toSize().expandedTo(QSize(1, 1)), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.scale(scale, scale);
    LabelRenderer(LabelRenderer::Mode::Preview).paint(&painter, *document);
    painter.end();
    return QIcon(QPixmap::fromImage(image));
}

} // namespace

PrintOnlyPanel::PrintOnlyPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *folderRow = new QHBoxLayout;
    m_folderName = new QLabel(this);
    m_folderName->setTextFormat(Qt::PlainText);
    m_folderName->setMinimumWidth(80);
    auto *choose = new QPushButton(QIcon::fromTheme(u"folder-open"_s), i18n("Folder…"), this);
    choose->setToolTip(i18n("Choose the folder with the labels to print"));
    connect(choose, &QPushButton::clicked, this, &PrintOnlyPanel::chooseFolder);
    folderRow->addWidget(m_folderName, 1);
    folderRow->addWidget(choose);
    layout->addLayout(folderRow);

    m_labels = new QListWidget(this);
    m_labels->setViewMode(QListView::IconMode);
    m_labels->setIconSize(QSize(ThumbnailSize, ThumbnailSize));
    m_labels->setResizeMode(QListView::Adjust);
    m_labels->setMovement(QListView::Static);
    m_labels->setWordWrap(true);
    m_labels->setSpacing(6);
    connect(m_labels, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        Q_EMIT labelChosen(item->data(Qt::UserRole).toString());
    });
    connect(m_labels, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        Q_EMIT labelChosen(item->data(Qt::UserRole).toString());
    });
    layout->addWidget(m_labels, 1);

    m_answersBox = new QGroupBox(i18n("Print With"), this);
    m_answersLayout = new QVBoxLayout(m_answersBox);
    m_problem = new QLabel(m_answersBox);
    m_problem->setWordWrap(true);
    m_answersLayout->addWidget(m_problem);
    m_answersBox->hide();
    layout->addWidget(m_answersBox);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &PrintOnlyPanel::listLabels);
}

QString PrintOnlyPanel::folder() const
{
    return m_folder;
}

void PrintOnlyPanel::setFolder(const QString &folder)
{
    if (!m_watcher->directories().isEmpty()) {
        m_watcher->removePaths(m_watcher->directories());
    }
    m_folder = folder;
    m_folderName->setText(folder.isEmpty() ? i18n("No folder chosen") : QDir(folder).dirName());
    m_folderName->setToolTip(folder);
    if (!folder.isEmpty() && QFileInfo(folder).isDir()) {
        m_watcher->addPath(folder);
    }
    listLabels();
}

void PrintOnlyPanel::chooseFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, i18n("Folder With Labels"), m_folder);
    if (!folder.isEmpty()) {
        setFolder(folder);
        Q_EMIT folderChosen(folder);
    }
}

void PrintOnlyPanel::listLabels()
{
    const QString current = m_labels->currentItem() ? m_labels->currentItem()->data(Qt::UserRole).toString() : QString();
    m_labels->clear();
    if (m_folder.isEmpty()) {
        return;
    }
    const QFileInfoList files = QDir(m_folder).entryInfoList({u"*.label"_s}, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &file : files) {
        // Drawn again only when the file changed (counters saved after printing).
        auto cached = m_thumbnails.constFind(file.absoluteFilePath());
        if (cached == m_thumbnails.constEnd() || cached->first != file.lastModified()) {
            cached = m_thumbnails.insert(file.absoluteFilePath(), {file.lastModified(), thumbnail(file.absoluteFilePath())});
        }
        auto *item = new QListWidgetItem(cached->second, file.completeBaseName(), m_labels);
        item->setData(Qt::UserRole, file.absoluteFilePath());
        item->setToolTip(file.absoluteFilePath());
    }
    setCurrentLabel(current);
}

void PrintOnlyPanel::setCurrentLabel(const QString &fileName)
{
    for (int i = 0; i < m_labels->count(); ++i) {
        if (m_labels->item(i)->data(Qt::UserRole).toString() == fileName) {
            m_labels->setCurrentRow(i);
            return;
        }
    }
    m_labels->setCurrentRow(-1);
}

void PrintOnlyPanel::setQuestions(const QList<PrintQuestion> &questions, const QHash<QString, QString> &previous, const std::optional<DataTable> &data)
{
    delete m_form;
    m_answersBox->setVisible(!questions.isEmpty());
    if (questions.isEmpty()) {
        m_problem->clear();
        return;
    }
    m_form = new AnswersForm(questions, previous, data, m_answersBox);
    m_answersLayout->insertWidget(0, m_form);
    const auto showProblem = [this]() {
        m_problem->setText(problem().value_or(QString()));
        Q_EMIT answersChanged();
    };
    connect(m_form, &AnswersForm::changed, this, showProblem);
    showProblem();
}

QHash<QString, QString> PrintOnlyPanel::answers() const
{
    return m_form ? m_form->answers() : QHash<QString, QString>();
}

std::optional<QString> PrintOnlyPanel::problem() const
{
    return m_form ? m_form->problem() : std::nullopt;
}
