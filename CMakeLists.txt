# SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.22)

project(plasma-label-creator VERSION 0.3.0 LANGUAGES CXX)

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

# The core library only needs QtCore, QtGui and zint, so a future headless
# service can link it without pulling in widgets or KDE Frameworks.
option(BUILD_APP "Build the Plasma Label Creator desktop application (needs KDE Frameworks)" ON)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Network)
find_package(zint REQUIRED CONFIG)
set_package_properties(zint PROPERTIES
    DESCRIPTION "Barcode encoding library"
    URL "https://zint.org.uk/"
    TYPE REQUIRED)
find_package(Cups REQUIRED)
set_package_properties(Cups PROPERTIES
    DESCRIPTION "Printing to DYMO LabelWriter queues"
    URL "https://openprinting.github.io/cups/"
    TYPE REQUIRED)

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

if(BUILD_APP)
    add_subdirectory(src/app)
endif()

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
