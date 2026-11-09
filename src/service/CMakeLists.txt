# SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
# SPDX-License-Identifier: BSD-2-Clause

# The automation service: Qt and KI18n only, like the core library, so it
# also runs on a computer without a desktop.
add_library(automationservice STATIC
    automationservice.cpp
    automationservice.h
)
if(LABELCREATOR_HTTP_SERVER)
    target_sources(automationservice PRIVATE httpapi.cpp httpapi.h)
    target_link_libraries(automationservice PUBLIC Qt6::HttpServer)
endif()
target_include_directories(automationservice PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR})
target_link_libraries(automationservice PUBLIC labelcreatorcore Qt6::Network PRIVATE KF6::I18n)
target_compile_definitions(automationservice PRIVATE TRANSLATION_DOMAIN="plasma-label-creator")

add_executable(plasma-label-creator-automation main.cpp)
target_link_libraries(plasma-label-creator-automation automationservice KF6::I18n)
target_compile_definitions(plasma-label-creator-automation PRIVATE TRANSLATION_DOMAIN="plasma-label-creator")

install(TARGETS plasma-label-creator-automation ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# For computers where it runs without anyone logged in to a desktop:
# systemctl --user enable --now plasma-label-creator-automation
configure_file(plasma-label-creator-automation.service.in ${CMAKE_CURRENT_BINARY_DIR}/plasma-label-creator-automation.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plasma-label-creator-automation.service DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})
