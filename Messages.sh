#! /usr/bin/env bash
# SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
# SPDX-License-Identifier: CC0-1.0

# Extracts the messages to translate into $podir/plasma-label-creator.pot, the
# way KDE's translation tools run it (they set $EXTRACTRC, $XGETTEXT and
# $podir). po/update.sh runs it here too.
$EXTRACTRC $(find src -name '*.rc') >> rc.cpp
$XGETTEXT rc.cpp $(find src -name '*.cpp' -o -name '*.h' | grep -v '/plugin/' | sort) -o $podir/plasma-label-creator.pot
rm -f rc.cpp
