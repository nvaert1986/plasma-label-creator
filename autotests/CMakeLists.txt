# SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
# SPDX-License-Identifier: BSD-2-Clause

set(tests
    addressbooktest
    arrangetest
    barcodetest
    clipartlibrarytest
    fieldstest
    gs1test
    labelformattest
    labelkindstest
    labelprintertest
    labelrenderertest
    recentlayoutstest
    sharedcountertest
    zplprintertest
    unitstest
)

foreach(test ${tests})
    ecm_add_test(${test}.cpp LINK_LIBRARIES labelcreatorcore Qt6::Test TEST_NAME ${test})
    target_compile_definitions(${test} PRIVATE LABELCREATOR_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
endforeach()

# Compares Plasma Label Creator's own Code 39 encoder with zint's.
target_link_libraries(barcodetest zint::zint)
# Plays a ZPL printer on a local TCP port.
target_link_libraries(zplprintertest Qt6::Network)
# Reads the printed raster back.
target_link_libraries(labelprintertest Cups::Cups)

# Text rendering needs a QGuiApplication; run it without a display.
set_tests_properties(${tests} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# The editor's document, undo and canvas, built from the app's sources.
if(BUILD_APP)
    ecm_add_test(labeleditortest.cpp
        ${CMAKE_SOURCE_DIR}/src/app/alteclabels.cpp
        ${CMAKE_SOURCE_DIR}/src/app/dragsupport.cpp
        ${CMAKE_SOURCE_DIR}/src/app/inlinetexteditor.cpp
        ${CMAKE_SOURCE_DIR}/src/app/labelclipboard.cpp
        ${CMAKE_SOURCE_DIR}/src/app/labeleditor.cpp
        ${CMAKE_SOURCE_DIR}/src/app/labelview.cpp
        ${CMAKE_SOURCE_DIR}/src/app/objectfactory.cpp
        ${CMAKE_SOURCE_DIR}/src/app/printpreviewdialog.cpp
        ${CMAKE_SOURCE_DIR}/src/app/plasma-label-creator.qrc # the Altec catalogue
        LINK_LIBRARIES labelcreatorcore Qt6::Test Qt6::Widgets KF6::I18n
        TEST_NAME labeleditortest)
    target_include_directories(labeleditortest PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
    set_tests_properties(labeleditortest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    ecm_add_test(printerdetectiontest.cpp
        ${CMAKE_SOURCE_DIR}/src/app/printersetup.cpp
        ${CMAKE_SOURCE_DIR}/src/app/usbwatcher.cpp
        LINK_LIBRARIES labelcreatorcore Qt6::Test Qt6::DBus KF6::I18n PkgConfig::Udev
        TEST_NAME printerdetectiontest)
    target_include_directories(printerdetectiontest PRIVATE ${CMAKE_SOURCE_DIR}/src/app)

    # Reading data to print from: CSV, Excel, OpenDocument, databases.
    ecm_add_test(datareadertest.cpp
        ${CMAKE_SOURCE_DIR}/src/app/datareader.cpp
        LINK_LIBRARIES labelcreatorcore Qt6::Test Qt6::Sql KF6::Archive KF6::I18n
        TEST_NAME datareadertest)
    target_include_directories(datareadertest PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
    target_compile_definitions(datareadertest PRIVATE LABELCREATOR_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")

    # The data source, questions and answers dialogs.
    ecm_add_test(datadialogstest.cpp
        ${CMAKE_SOURCE_DIR}/src/app/datadialogs.cpp
        ${CMAKE_SOURCE_DIR}/src/app/datareader.cpp
        ${CMAKE_SOURCE_DIR}/src/app/layersdialog.cpp
        ${CMAKE_SOURCE_DIR}/src/app/gs1dialog.cpp
        LINK_LIBRARIES labelcreatorcore Qt6::Test Qt6::Widgets Qt6::Sql KF6::Archive KF6::I18n
        TEST_NAME datadialogstest)
    target_include_directories(datadialogstest PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
    set_tests_properties(datadialogstest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # The address book tab: check marks, searching, sorting.
    ecm_add_test(addressbookpaneltest.cpp
        ${CMAKE_SOURCE_DIR}/src/app/addressbookpanel.cpp
        LINK_LIBRARIES labelcreatorcore Qt6::Test Qt6::Widgets KF6::ConfigCore KF6::I18n KF6::WidgetsAddons
        TEST_NAME addressbookpaneltest)

    # Pressing OK in the properties dialog must leave untouched objects alone.
    ecm_add_test(propertiesdialogtest.cpp
        ${CMAKE_SOURCE_DIR}/src/app/objectfactory.cpp
        ${CMAKE_SOURCE_DIR}/src/app/propertiesdialog.cpp
        ${CMAKE_SOURCE_DIR}/src/app/gs1dialog.cpp
        LINK_LIBRARIES labelcreatorcore Qt6::Test Qt6::Widgets KF6::I18n KF6::WidgetsAddons
        TEST_NAME propertiesdialogtest)

    # Which label suits which printer, and the user's own label sizes.
    ecm_add_test(labeltypestest.cpp
        ${CMAKE_SOURCE_DIR}/src/app/alteclabels.cpp
        ${CMAKE_SOURCE_DIR}/src/app/customlabels.cpp
        ${CMAKE_SOURCE_DIR}/src/app/labelfit.cpp
        ${CMAKE_SOURCE_DIR}/src/app/objectfactory.cpp
        LINK_LIBRARIES labelcreatorcore Qt6::Test Qt6::Widgets KF6::ConfigCore KF6::I18n
        TEST_NAME labeltypestest)

    foreach(test addressbookpaneltest propertiesdialogtest labeltypestest)
        target_include_directories(${test} PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
        set_tests_properties(${test} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endforeach()
endif()
