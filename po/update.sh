#! /usr/bin/env bash
# SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
# SPDX-License-Identifier: CC0-1.0

# Brings the translations up to date with the source: extracts the messages
# (Messages.sh) into po/plasma-label-creator.pot and merges them into each
# language's po/<lang>/plasma-label-creator.po, marking what changed as
# fuzzy; then does the same for the handbook's paragraphs with po4a
# (doc/po4a.cfg) and writes each language's handbook to po/<lang>/docs/.
# Needs gettext and po4a.
#
#   po/update.sh            update every language
#   po/update.sh nl         update Dutch only
#   po/update.sh --new fr   start a French translation
#
# Translate po/<lang>/plasma-label-creator.po and doc/po/<lang>.po, then run
# it again to write the translated handbook.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
domain=plasma-label-creator
version=$(sed -n 's/^project([^ ]* VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")
bugs=https://github.com/nvaert1986/plasma-label-creator/issues

# The same keywords as KDE's own extraction.
XGETTEXT="xgettext --from-code=UTF-8 -C --kde -ci18n --package-name=$domain --package-version=$version
    --msgid-bugs-address=$bugs
    -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3
    -kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3
    -kkli18n:1 -kkli18nc:1c,2 -kkli18np:1,2 -kkli18ncp:1c,2,3
    -kxi18n:1 -kxi18nc:1c,2 -kxi18np:1,2 -kxi18ncp:1c,2,3
    -kI18N_NOOP:1 -kI18NC_NOOP:1c,2"

# KDE's extractrc where it is installed; else the <text> elements of the
# XMLGUI files as i18nc() calls, which is all this project's .rc files hold.
extractrc() {
    if type -P extractrc >/dev/null; then
        command extractrc "$@"
        return
    fi
    local file context text
    for file in "$@"; do
        sed -n 's#.*<text\( context="\([^"]*\)"\)\{0,1\}>\([^<]*\)</text>.*#\2|\3#p' "$file" |
            sed 's/&amp;/\&/g; s/&lt;/</g; s/&gt;/>/g; s/&quot;/\\"/g' |
            while IFS='|' read -r context text; do
                echo "// i18n: file: $file"
                if [ -n "$context" ]; then
                    echo "i18nc(\"$context\", \"$text\");"
                else
                    echo "i18n(\"$text\");"
                fi
            done
    done
}

# The language's name as KDE's DocBook tools know it.
language_entity() {
    case "$1" in
        nl) echo Dutch ;;
        de) echo German ;;
        fr) echo French ;;
        es) echo Spanish ;;
        it) echo Italian ;;
        pt) echo Portuguese ;;
        pt_BR) echo Brazilian-Portuguese ;;
        sv) echo Swedish ;;
        da) echo Danish ;;
        pl) echo Polish ;;
        *) echo "po/update.sh: add the DocBook name of language $1 to language_entity" >&2; exit 1 ;;
    esac
}

cd "$root"
new=
if [ "${1:-}" = --new ]; then
    new=${2:?the language code, such as fr}
    languages=("$new")
elif [ $# -gt 0 ]; then
    languages=("$@")
else
    languages=()
    for dir in po/*/; do
        languages+=("$(basename "$dir")")
    done
fi

export -f extractrc
EXTRACTRC=extractrc XGETTEXT="$XGETTEXT" podir="$root/po" bash Messages.sh

for lang in "${languages[@]}"; do
    messages=po/$lang/$domain.po
    if [ -n "$new" ]; then
        mkdir -p "po/$lang"
        msginit --no-translator --locale="$lang" --input="po/$domain.pot" --output-file="$messages"
        sed -i "s/^\[po4a_langs\] .*/& $lang/" doc/po4a.cfg
    else
        msgmerge --quiet --update --backup=none --previous "$messages" "po/$domain.pot"
    fi
    echo "$lang:"
    msgfmt --check --statistics -o /dev/null "$messages"
done

# The handbook: updates doc/po/handbook.pot and doc/po/<lang>.po, and writes
# po/<lang>/docs/ for each language at least 80% translated (untranslated
# paragraphs stay English).
po4a doc/po4a.cfg
for lang in $(sed -n 's/^\[po4a_langs\] //p' doc/po4a.cfg); do
    echo "$lang handbook:"
    msgfmt --check --statistics -o /dev/null "doc/po/$lang.po"
    handbook=po/$lang/docs/$domain/index.docbook
    if [ -f "$handbook" ]; then
        sed -i "s/<!ENTITY % English \"INCLUDE\">/<!ENTITY % $(language_entity "$lang") \"INCLUDE\">/" "$handbook"
    fi
done
