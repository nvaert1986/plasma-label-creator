# SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
# SPDX-License-Identifier: BSD-2-Clause

# How the app fits into Plasma and other desktops. Each part is optional:
# the app works the same without any of them.

option(WITH_FILE_TYPES "Install file types for .label, .ABX and .rhino files, and open them with the app" ON)
option(WITH_ICONS "Install the app's own icons (else generic printer icons)" ON)
option(WITH_LAUNCHER_EXTRAS "Search keywords and right-click actions in the app's launchers" ON)
option(WITH_APPSTREAM "Install AppStream metadata, for Discover and other software centres" ON)
option(WITH_DOLPHIN_MENU "Print Label in Dolphin's right-click menu for label files" ON)
option(WITH_THUMBNAILS "Thumbnails of label files in Dolphin and file dialogs (needs KIO)" ON)

include(ECMInstallIcons)

# The parts that need the file types.
foreach(part WITH_DOLPHIN_MENU WITH_THUMBNAILS)
    if(${part} AND NOT WITH_FILE_TYPES)
        message(STATUS "${part} needs WITH_FILE_TYPES: left out")
        set(${part} OFF)
    endif()
endforeach()

# The launchers, as the options make them. They start the program by its
# full path, so an install in ~/.local works without ~/.local/bin in PATH.
set(APP_ICON "document-print")
set(PRINT_ICON "document-print-direct")
if(WITH_ICONS)
    # Named by the app id: a name with dashes, such as plasma-label-creator,
    # would find Breeze's "plasma" icon first when looked up.
    set(APP_ICON "io.github.nvaert1986.PlasmaLabelCreator")
    set(PRINT_ICON "io.github.nvaert1986.PlasmaLabelCreator-print")
endif()
set(MIME_TYPES "")
if(WITH_FILE_TYPES)
    set(MIME_TYPES "MimeType=application/x-dymo-label;application/x-dymo-address-book;application/x-dymo-rhino-template;\n")
endif()
set(LAUNCHER_EXTRAS "")
set(PRINT_LAUNCHER_EXTRAS "")
if(WITH_LAUNCHER_EXTRAS)
    set(LAUNCHER_EXTRAS "Keywords=label;labels;DYMO;LabelWriter;LabelManager;Zebra;Altec;ZPL;barcode;QR code;sticker;\nKeywords[nl]=label;labels;etiket;DYMO;LabelWriter;LabelManager;Zebra;Altec;ZPL;barcode;streepjescode;QR-code;sticker;\nActions=printOnly;\n\n[Desktop Action printOnly]\nName=Print-Only Mode\nName[nl]=Modus Alleen afdrukken\nIcon=${PRINT_ICON}\nExec=${KDE_INSTALL_FULL_BINDIR}/plasma-label-creator --print-only\n")
    set(PRINT_LAUNCHER_EXTRAS "Keywords=label;labels;print;DYMO;LabelWriter;Zebra;Altec;form;\nKeywords[nl]=label;labels;etiket;afdrukken;DYMO;LabelWriter;Zebra;Altec;formulier;\nActions=design;\n\n[Desktop Action design]\nName=Design Labels\nName[nl]=Labels ontwerpen\nIcon=${APP_ICON}\nExec=${KDE_INSTALL_FULL_BINDIR}/plasma-label-creator\n")
endif()
configure_file(plasma-label-creator.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/plasma-label-creator.desktop @ONLY)
configure_file(plasma-label-creator-print.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/plasma-label-creator-print.desktop @ONLY)
install(PROGRAMS
    ${CMAKE_CURRENT_BINARY_DIR}/plasma-label-creator.desktop
    ${CMAKE_CURRENT_BINARY_DIR}/plasma-label-creator-print.desktop
    DESTINATION ${KDE_INSTALL_APPDIR})

if(WITH_FILE_TYPES)
    install(FILES plasma-label-creator.xml DESTINATION ${KDE_INSTALL_MIMEDIR})
    # Packages update the database after installing; a plain install does it here.
    find_package(SharedMimeInfo QUIET)
    if(SharedMimeInfo_FOUND)
        update_xdg_mimetypes(${KDE_INSTALL_MIMEDIR})
    endif()
endif()

if(WITH_ICONS)
    ecm_install_icons(ICONS
        icons/sc-apps-io.github.nvaert1986.PlasmaLabelCreator.svg
        icons/sc-apps-io.github.nvaert1986.PlasmaLabelCreator-print.svg
        icons/sc-mimetypes-application-x-dymo-label.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}
        THEME hicolor)
endif()

if(WITH_APPSTREAM)
    install(FILES io.github.nvaert1986.PlasmaLabelCreator.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
endif()

if(WITH_DOLPHIN_MENU)
    configure_file(plasma-label-creator-dolphin.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/plasma-label-creator-dolphin.desktop @ONLY)
    # Executable: in ~/.local, Dolphin only reads service menus that are.
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/plasma-label-creator-dolphin.desktop DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus)
endif()

set(LABELCREATOR_THUMBNAILS OFF)
if(WITH_THUMBNAILS)
    find_package(KF6 ${KF_MIN_VERSION} QUIET COMPONENTS KIO)
    set_package_properties(KF6KIO PROPERTIES
        DESCRIPTION "KDE's file access framework"
        TYPE OPTIONAL
        PURPOSE "Thumbnails of label files in Dolphin")
    if(KF6KIO_FOUND)
        set(LABELCREATOR_THUMBNAILS ON)
        kcoreaddons_add_plugin(labelthumbnail SOURCES thumbnailer/labelthumbnail.cpp INSTALL_NAMESPACE "kf6/thumbcreator")
        target_link_libraries(labelthumbnail labelcreatorcore KF6::KIOGui)
        if(BUILD_TESTING)
            ecm_add_test(thumbnailer/labelthumbnailtest.cpp
                LINK_LIBRARIES Qt6::Test KF6::KIOGui KF6::CoreAddons
                TEST_NAME labelthumbnailtest)
            target_compile_definitions(labelthumbnailtest PRIVATE
                THUMBNAILER="$<TARGET_FILE:labelthumbnail>"
                TEST_DATA="${CMAKE_SOURCE_DIR}/autotests/data")
            add_dependencies(labelthumbnailtest labelthumbnail)
            set_tests_properties(labelthumbnailtest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        endif()
    endif()
endif()

add_feature_info("File types" WITH_FILE_TYPES ".label, .ABX and .rhino files open with the app")
add_feature_info("Icons" WITH_ICONS "the app's own icons")
add_feature_info("Launcher extras" WITH_LAUNCHER_EXTRAS "search keywords and right-click actions")
add_feature_info("AppStream" WITH_APPSTREAM "metadata for software centres")
add_feature_info("Dolphin menu" WITH_DOLPHIN_MENU "Print Label in Dolphin's right-click menu")
add_feature_info("Thumbnails" LABELCREATOR_THUMBNAILS "label thumbnails in Dolphin (KIO)")
