/*
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Shared by the parts of MainWindow: mainwindow.cpp, mainwindow_format.cpp,
// mainwindow_data.cpp, mainwindow_objects.cpp, mainwindow_print.cpp.

#pragma once

#include "addressbookpanel.h"
#include "alteclabels.h"
#include "customlabels.h"
#include "datadialogs.h"
#include "fields.h"
#include "labelclipboard.h"
#include "labeleditor.h"
#include "labelfit.h"
#include "labelkinds.h"
#include "labelkindsdialog.h"
#include "labelrenderer.h"
#include "labelview.h"
#include "mainwindow.h"
#include "printbar.h"
#include "printpreviewdialog.h"
#include "propertiesdialog.h"
#include "recentlayoutsmenu.h"
#include "rulerwidget.h"
#include "sidebar.h"
#include "tapelayout.h"
#include "units.h"
#include <KActionCollection>
#include <KActionMenu>
#include <KConfigGroup>
#include <KFontAction>
#include <KFontSizeAction>
#include <KLocalizedString>
#include <KMessageBox>
#include <KRecentFilesAction>
#include <KSelectAction>
#include <KSharedConfig>
#include <KStandardAction>
#include <KToggleAction>
#include <KXMLGUIFactory>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace MainWindowPrivate
{

inline QString labelFileFilter()
{
    using namespace Qt::Literals::StringLiterals;
    return i18n("DYMO label files (*.label)") + u";;"_s + i18n("DYMO Rhino templates (*.rhino)") + u";;"_s + i18n("All files (*)");
}

inline KConfigGroup settings()
{
    using namespace Qt::Literals::StringLiterals;
    return KSharedConfig::openConfig()->group(u"General"_s);
}

} // namespace MainWindowPrivate
