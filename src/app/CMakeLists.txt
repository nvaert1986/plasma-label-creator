# SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
# SPDX-License-Identifier: BSD-2-Clause

add_executable(plasma-label-creator
    main.cpp
    addressbookpanel.cpp
    addressbookpanel.h
    alteclabels.cpp
    alteclabels.h
    clipartpanel.cpp
    clipartpanel.h
    customlabels.cpp
    datadialogs.cpp
    datareader.cpp
    customlabels.h
    dragsupport.cpp
    dragsupport.h
    inlinetexteditor.cpp
    inlinetexteditor.h
    labelclipboard.cpp
    labelclipboard.h
    labelfit.cpp
    labelfit.h
    labelkindsdialog.cpp
    labelkindsdialog.h
    labeleditor.cpp
    labeleditor.h
    labelview.cpp
    labelview.h
    mainwindow.cpp
    mainwindow.h
    objectfactory.cpp
    objectfactory.h
    printbar.cpp
    printpreviewdialog.cpp
    printbar.h
    printersetup.cpp
    printersetup.h
    propertiesdialog.cpp
    propertiesdialog.h
    recentlayoutsmenu.cpp
    recentlayoutsmenu.h
    rulerwidget.cpp
    rulerwidget.h
    sidebar.cpp
    sidebar.h
    usbwatcher.cpp
    usbwatcher.h
    plasma-label-creator.qrc
)

target_include_directories(plasma-label-creator PRIVATE ${CMAKE_BINARY_DIR})

target_link_libraries(plasma-label-creator
    labelcreatorcore
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::DBus
    Qt6::Sql
    KF6::Archive
    PkgConfig::Udev
    KF6::ConfigWidgets
    KF6::CoreAddons
    KF6::Crash
    KF6::I18n
    KF6::IconThemes
    KF6::WidgetsAddons
    KF6::XmlGui
)

target_compile_definitions(plasma-label-creator PRIVATE TRANSLATION_DOMAIN="plasma-label-creator")

install(TARGETS plasma-label-creator ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS plasma-label-creator.desktop DESTINATION ${KDE_INSTALL_APPDIR})
