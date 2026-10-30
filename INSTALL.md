<!--
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: CC0-1.0
-->

# Installing Plasma Label Creator

There are no binary packages yet: you build it from source. That takes
four commands once the requirements are installed.

## Requirements

### Versions

"Minimum" is what the build asks for. "Tested" is what the program was
developed and tested with. Versions in between should work but have not
been tried; if one does not, please report it.

| Requirement | Minimum | Tested |
|---|---|---|
| CMake | 3.22 | 4.3 |
| Ninja (or make) | any | 1.13 |
| C++ compiler with C++20 support | GCC or Clang with C++20 | GCC 15.3 |
| Extra CMake Modules (ECM) | 6.0 | 6.29 |
| Qt 6: Core, Gui, Network | 6.7 | 6.11 |
| Qt 6: Widgets, PrintSupport, DBus, Sql (application) | 6.7 | 6.11 |
| Qt 6: Test (only for the tests) | 6.7 | 6.11 |
| KDE Frameworks 6: Archive, ConfigWidgets, CoreAddons, Crash, I18n, IconThemes, WidgetsAddons, XmlGui | 6.0 | 6.29 |
| zint, with its CMake package files | any 2.x that installs them | 2.16 |
| CUPS library and headers | 2.x | 2.4 |
| libudev (application) | any | systemd 257 |
| pkg-config or pkgconf | any | 3.0 |

zint must be installed with its CMake package files (`zint-config.cmake`).
zint's own CMake build installs them; if your distribution leaves them
out, the build stops with "Could not find a package configuration file
provided by zint".

### Packages per distribution

Only Gentoo has been tried. The names for the other distributions are
their usual package names and may need adjusting.

**Gentoo**

```sh
emerge --ask --noreplace dev-build/cmake dev-build/ninja \
    kde-frameworks/extra-cmake-modules dev-qt/qtbase media-libs/zint \
    net-print/cups virtual/libudev dev-util/pkgconf \
    kde-frameworks/karchive kde-frameworks/kconfigwidgets \
    kde-frameworks/kcoreaddons kde-frameworks/kcrash kde-frameworks/ki18n \
    kde-frameworks/kiconthemes kde-frameworks/kwidgetsaddons \
    kde-frameworks/kxmlgui
```

`dev-qt/qtbase` needs the USE flags `gui`, `widgets`, `network`, `dbus` and
`sql` (a KDE Plasma system has them), plus `sqlite` for SQLite databases and
`odbc` for SQL Server, Access and other databases through ODBC. Printing from
MySQL or PostgreSQL needs the `mysql` or `postgres` flag.

**Debian / Ubuntu** (with KDE Frameworks 6, e.g. Debian 13)

```sh
apt install build-essential cmake ninja-build extra-cmake-modules pkgconf \
    qt6-base-dev libkf6archive-dev libkf6configwidgets-dev libkf6coreaddons-dev \
    libkf6crash-dev libkf6i18n-dev libkf6iconthemes-dev \
    libkf6widgetsaddons-dev libkf6xmlgui-dev libzint-dev libcups2-dev \
    libudev-dev
```

**Fedora**

```sh
dnf install gcc-c++ cmake ninja-build extra-cmake-modules pkgconf \
    qt6-qtbase-devel kf6-karchive-devel kf6-kconfigwidgets-devel kf6-kcoreaddons-devel \
    kf6-kcrash-devel kf6-ki18n-devel kf6-kiconthemes-devel \
    kf6-kwidgetsaddons-devel kf6-kxmlgui-devel zint-devel cups-devel \
    systemd-devel
```

**Arch Linux**

```sh
pacman -S --needed base-devel cmake ninja extra-cmake-modules qt6-base \
    karchive kconfigwidgets kcoreaddons kcrash ki18n kiconthemes kwidgetsaddons \
    kxmlgui zint cups systemd-libs
```

### For printing

These are needed at run time, not to build.

| What | For | Gentoo | Debian / Ubuntu | Fedora | Arch |
|---|---|---|---|---|---|
| CUPS, running | all printing | `net-print/cups` | `cups` | `cups` | `cups` |
| DYMO's CUPS driver | LabelWriter 300 to 450 series, LabelManagers | `net-print/dymo-cups-drivers` | `printer-driver-dymo` | `dymo-cups-drivers` | `dymo-cups-drivers` (AUR) |
| DYMO's LW5xx driver | LabelWriter 550 series | `net-print/dymo-lw5xx-cups-drivers` from [dymo-overlay](https://github.com/nvaert1986/dymo-overlay) | build from [dymosoftware/Drivers](https://github.com/dymosoftware/Drivers) | as Debian | as Debian |
| cups-pk-helper | the "Set Up Printer" button | `net-print/cups-pk-helper` | `cups-pk-helper` | `cups-pk-helper` | `cups-pk-helper` |

ZPL printers (Altec ATP, Zebra) need nothing extra: CUPS's own "Zebra ZPL
Label Printer" driver is used (its filter, `rastertolabel`, comes with
CUPS). Labels across the roll and continuous rolls need that driver: it
takes any label size, and its "Media Tracking" option is set to Continuous
for continuous rolls.

## Building

From the source folder:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

The programs are then in `build/bin/`: `plasma-label-creator` (the
application) and `plasma-label-creator-render` (the command-line tool). You
can run them from there without installing.

The tests need no display and no printers. A few that compare against a
real DYMO Label Software installation or real labels skip themselves when
those are not there.

### For development

Optional, to check the code as it was checked here:

| Tool | Tested | Gentoo |
|---|---|---|
| clang-tidy and run-clang-tidy | LLVM 22 | `llvm-core/clang` |

The build's compile commands carry a GCC-only flag
(`-mno-direct-extern-access`) that clang stops on: run clang-tidy on a copy
of `build/compile_commands.json` without it, in parallel:

```sh
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
mkdir -p /tmp/tidy
sed 's/ -mno-direct-extern-access//' build/compile_commands.json > /tmp/tidy/compile_commands.json
run-clang-tidy -j "$(nproc)" -quiet -p /tmp/tidy \
    -checks='-*,bugprone-*,performance-*,clang-analyzer-*' "$PWD/src/"
ctest --test-dir build -j "$(nproc)" --output-on-failure
```

### Build options

| Option | Default | Effect |
|---|---|---|
| `-DCMAKE_BUILD_TYPE=` | none | `RelWithDebInfo` or `Release` for normal use, `Debug` for development |
| `-DBUILD_APP=OFF` | `ON` | Build only the core library and the command-line tool: no KDE Frameworks, Qt Widgets or libudev needed |
| `-DBUILD_TESTING=OFF` | `ON` | Skip the tests: Qt Test is not needed |
| `-DCMAKE_INSTALL_PREFIX=` | `/usr/local` | Where `cmake --install` puts the files, e.g. `~/.local` |

Without Ninja, leave out `-G Ninja` and CMake uses make.

## Installing

System-wide (as root):

```sh
cmake --install build
```

Or for your user only, without root:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build
cmake --install build
```

This installs both programs and a desktop entry, so Plasma Label Creator
appears in the application launcher. For a `~/.local` install, make sure
`~/.local/bin` is in your `PATH`.

To uninstall, remove the files listed in `build/install_manifest.txt`.

## Setting up a printer

- **DYMO on USB:** plug it in. CUPS usually creates a queue by itself; if
  not, the application offers "Set Up Printer" and creates one with the
  right DYMO driver (it asks for the admin password).
- **DYMO shared from Windows or on the network:** add it in System
  Settings > Printers with the DYMO driver for that model. A queue without
  the DYMO driver also works: choose it in the application and say which
  LabelWriter it is.
- **Altec ATP or Zebra:** add it in System Settings > Printers with the
  driver "Zebra ZPL Label Printer" at the printer's resolution (203, 300 or
  600 dpi). On the network, use the address `socket://<printer>:9100`; the
  application then asks the printer which labels are loaded. If prints are
  slightly off, adjust them with Print Offset... in the print bar.
