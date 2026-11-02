/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "datareader.h"
#include "labeldocument.h"

#include <QDateTime>
#include <QHash>
#include <QIcon>
#include <QPointer>
#include <QWidget>

#include <optional>

class AnswersForm;
class QFileSystemWatcher;
class QGroupBox;
class QLabel;
class QListWidget;
class QVBoxLayout;

/**
 * The side panel of print-only mode: the labels in a folder, with pictures
 * to choose them by, and the form for the chosen label's questions.
 */
class PrintOnlyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PrintOnlyPanel(QWidget *parent = nullptr);

    QString folder() const;
    /** Lists the labels in @p folder, and follows it as files come and go. */
    void setFolder(const QString &folder);
    /** Marks @p fileName as the label shown, if it is in the folder. */
    void setCurrentLabel(const QString &fileName);

    /** The form for @p questions, with @p previous answers and the label's data. */
    void setQuestions(const QList<LabelCreator::PrintQuestion> &questions,
                      const QHash<QString, QString> &previous,
                      const std::optional<DataTable> &data);
    QHash<QString, QString> answers() const;
    /** What keeps the answers from printing; nullopt when they will do. */
    std::optional<QString> problem() const;

Q_SIGNALS:
    void labelChosen(const QString &fileName);
    void folderChosen(const QString &folder);
    void answersChanged();

private:
    void chooseFolder();
    void listLabels();

    QString m_folder;
    QLabel *m_folderName;
    QListWidget *m_labels;
    QFileSystemWatcher *m_watcher;
    QGroupBox *m_answersBox;
    QVBoxLayout *m_answersLayout;
    QPointer<AnswersForm> m_form;
    // Pictures already drawn, by file, with the time the file had then.
    QHash<QString, std::pair<QDateTime, QIcon>> m_thumbnails;
    QLabel *m_problem;
};
