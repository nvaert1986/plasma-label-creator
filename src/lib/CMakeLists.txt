# SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
# SPDX-License-Identifier: BSD-2-Clause

# Static on purpose: it is shared by the app, the tests and (later) the web
# service, but it has no stable API to version yet.
add_library(labelcreatorcore STATIC
    addressbook.cpp
    addressbook.h
    barcode.cpp
    barcode.h
    clipartlibrary.cpp
    clipartlibrary.h
    labeldocument.cpp
    labeldocument.h
    labelformat.cpp
    labelformat.h
    labelkinds.cpp
    labelkinds.h
    labelprinter.cpp
    labelprinter.h
    labelrenderer.cpp
    labelrenderer.h
    recentlayouts.cpp
    recentlayouts.h
    tapelayout.cpp
    tapelayout.h
    units.h
    zplprinter.cpp
    zplprinter.h
)

target_include_directories(labelcreatorcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(labelcreatorcore
    PUBLIC
        Qt6::Core
        Qt6::Gui
    PRIVATE
        Qt6::Network
        Cups::Cups
        zint::zint
)
