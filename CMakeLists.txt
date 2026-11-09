# SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.22)

project(plasma-label-creator VERSION 0.6.1 LANGUAGES CXX)

set(QT_MIN_VERSION "6.7.0") # QLocale::toDate with a base year for two-digit years
set(KF_MIN_VERSION "6.0.0")
set(KDE_COMPILERSETTINGS_LEVEL "6.0")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMAddTests)
include(FeatureSummary)

# The core library only needs QtCore, QtGui, zint and KI18n, so the
# automation service can link it without pulling in widgets or the rest of
# KDE Frameworks.
option(BUILD_APP "Build the Plasma Label Creator desktop application (needs KDE Frameworks)" ON)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Network Svg)
find_package(zint REQUIRED CONFIG)
set_package_properties(zint PROPERTIES
    DESCRIPTION "Barcode encoding library"
    URL "https://zint.org.uk/"
    TYPE REQUIRED)
find_package(Cups REQUIRED)
# Messages are translated in the core library, the command-line tool and the
# automation service too. KI18n needs only QtCore and gettext.
find_package(KF6I18n ${KF_MIN_VERSION} REQUIRED)
set_package_properties(Cups PROPERTIES
    DESCRIPTION "Printing to DYMO LabelWriter queues"
    URL "https://openprinting.github.io/cups/"
    TYPE REQUIRED)

# The automation service takes print requests over HTTP when Qt HTTP
# Server is installed; without it, it watches folders only.
option(WITH_HTTP_SERVER "Print requests over HTTP in the automation service (needs Qt HTTP Server)" ON)
set(LABELCREATOR_HTTP_SERVER OFF)
if(WITH_HTTP_SERVER)
    find_package(Qt6HttpServer 6.8 QUIET CONFIG)
    set_package_properties(Qt6HttpServer PROPERTIES
        DESCRIPTION "Qt HTTP Server"
        URL "https://doc.qt.io/qt-6/qthttpserver-index.html"
        TYPE OPTIONAL
        PURPOSE "Print requests over HTTP in the automation service")
    if(Qt6HttpServer_FOUND)
        set(LABELCREATOR_HTTP_SERVER ON)
    endif()
endif()
add_feature_info("Print requests over HTTP" LABELCREATOR_HTTP_SERVER "the automation service takes print requests over HTTP (Qt HTTP Server)")
configure_file(src/labelcreator_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/labelcreator_config.h)

if(BUILD_APP)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Widgets PrintSupport DBus Sql)
    # Noticing LabelWriters being plugged in.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Udev REQUIRED IMPORTED_TARGET libudev)
    find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
        Archive
        ConfigWidgets
        CoreAddons
        Crash
        I18n
        IconThemes
        WidgetsAddons
        XmlGui
    )
    # The handbook (Help > Plasma Label Creator Handbook, F1), in KHelpCenter.
    option(WITH_HANDBOOK "Install the handbook (needs KDocTools)" ON)
    if(WITH_HANDBOOK)
        find_package(KF6DocTools ${KF_MIN_VERSION} REQUIRED)
    endif()
endif()

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
endif()

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX LABELCREATOR
    VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/labelcreator_version.h
)

add_subdirectory(src/lib)
add_subdirectory(src/cli)
add_subdirectory(src/service)

if(BUILD_APP)
    add_subdirectory(src/app)
    add_subdirectory(src/integration)
    if(WITH_HANDBOOK)
        add_subdirectory(doc)
    endif()
endif()

# The translations in po/: the messages, and the handbook's.
ki18n_install(po)
if(WITH_HANDBOOK)
    kdoctools_install(po)
endif()

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
