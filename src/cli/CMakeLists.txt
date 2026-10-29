# SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
# SPDX-License-Identifier: BSD-2-Clause

add_executable(plasma-label-creator-render main.cpp)

target_include_directories(plasma-label-creator-render PRIVATE ${CMAKE_BINARY_DIR})
target_link_libraries(plasma-label-creator-render labelcreatorcore)

install(TARGETS plasma-label-creator-render ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
