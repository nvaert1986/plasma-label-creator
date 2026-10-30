<!--
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: CC0-1.0
-->

# Changelog

## Unreleased

### Designing

- **Several objects at once**: Ctrl or Shift and a click adds an object to
  the selection or takes it out again; dragging over empty space selects
  the objects inside the band; Edit > Select All. Dragging, the arrow keys,
  Delete, Cut, Copy, Paste, Duplicate, Bring to Front, Send to Back,
  Position on Label and the format toolbar then work on all of them. Undo
  and redo bring the selection back with the change.
- **Copy and paste several objects**, also between windows: they keep
  their places to each other, and other programs get their texts, one per
  line. Pasted or duplicated groups become new groups.
- **Align and distribute** (Layout > Align Objects): line the selected
  objects up along their left, right, top or bottom edges or their centres,
  or space three or more evenly across or down.
- **Groups** (Layout > Group, Ctrl+G; Ungroup, Ctrl+Shift+G): grouped
  objects select, move and line up as one. Groups are saved in the label
  file's own group field.
- **Lock Position and Size** (Layout menu): a locked object cannot be
  dragged, resized or moved by the arrow keys or Align; it shows a padlock.
  Other objects line up with it.
- **Text that wraps** (Format > Wrap Text, or "Wrap long lines" in the
  properties): long lines break at spaces to the box's width, and words too
  long for a line break inside. With Shrink to Fit or Always Fit the text
  takes the largest size at which its wrapped lines fit the box. Editing
  the text on the label wraps the same way.
- Locks and wrapping are kept in the `.label` file's comment, which other
  label software ignores.

### Printers

- **Altec wrap labels** (Wikkellabels, Gekleurde Wikkellabels,
  Twistlabels) from the catalogue show their printable white strip: the
  rest of the label is shaded on the canvas. The strip's length comes from
  Altec's article codes; it is placed at the end that leaves the printer
  first, which is still to be confirmed on paper.

### Other

- The desktop entry now says the program supports LabelManager and ZPL
  printers too.
- The main window's code is split into files by topic (files, format,
  data, objects, printing); no change in use.
- New tests for arranging objects, selecting several on the canvas,
  wrapping text and the Altec wrap labels.

## 0.2.0 (2026-10-29)

### Printing from data

- **A label for each row** of a CSV or text file, an Excel (`.xlsx`) or
  LibreOffice (`.ods`) spreadsheet, or a database query. Choose the source
  and tick the rows under Data > Data Source…, then print "Each row of the
  data source" from the print bar.
  - CSV: quoted fields with line breaks, the separator detected (`;`, `,`,
    tab or `|`), UTF-8 or Windows-1252 text.
  - Spreadsheets: any sheet; dates show as dates, in your format.
  - Databases through Qt SQL: SQLite, ODBC (SQL Server, Access and others),
    MySQL, PostgreSQL, as installed. Only a query that reads is run (one
    `SELECT` or `WITH ... SELECT`), so a label file can never change a
    database. Passwords are asked for and never saved.
- **Fields**: `{Column}` in a text, an address, a barcode's data, a
  counter's or date's text around it, or an image's file path. Data >
  Insert Field puts one in the selected object. `{{` and `}}` are braces.
- **Pictures from data**: an image can show a file instead of an embedded
  picture (Properties > Picture file), and the path can use fields, such as
  `photos/{Serial}.png`. Relative paths are looked up next to the data file.
- **Questions at print time** (Data > Print-Time Questions…), such as a
  batch number, with rules: an answer needed, a length, digits or letters
  only, or a pattern. Asked before every print and print preview; the
  answers fill `{Name}` on every label of the run, and are offered again
  the next time.
- The data source and the questions are kept in the `.label` file as a
  comment, which other label software ignores, so the file stays
  compatible.

### Designing

- **Grid and snapping** (View > Show Grid, Snap to Grid, Grid Spacing):
  moving and resizing put edges on the grid; hold Alt to place freely.
- **White on black** text (Format > White on Black), using the object's own
  colours, so other label software that opens the file shows it the same
  way.
- **Rotate Label turns the objects with it**: positions move round and each
  object's rotation follows, tape cells included. The label looks the same,
  just turned, and prints exactly as before (tested dot for dot). Before,
  only the label turned and objects could end up off it.
- **More barcodes**: Data Matrix, Aztec, MaxiCode, Micro QR, Code 93,
  Code 11, MSI Plessey, Pharmacode, GS1-128, GS1 Data Matrix, GS1 DataBar,
  HIBC (Code 128, Code 39, Data Matrix, QR), KIX (PostNL) and Royal Mail.
  GS1 data is typed with `[AI]` brackets and shows `(AI)` under the bars.
  Data Matrix symbols are always square. POSTNET, PLANET and Intelligent
  Mail, which `.label` files could already name, now draw too. Other label
  software that opens the file may not show the new types.

### Printing and output

- **Print preview** (File > Print Preview…): the pages exactly as the
  printer receives them, dot for dot, stepping through a print run.
- **Export** a label as a PDF of its own size, with the text kept as text,
  or as a PNG at the printer's resolution (File > Export as PDF or Image…).
  `plasma-label-creator-render label.label out.pdf` writes a PDF too.

### Requirements

- KDE Frameworks Archive (for spreadsheets) and Qt SQL (for databases) are
  now needed. See INSTALL.md.

## 0.1.0 (2026-10-29)

A Plasma label creator for Linux that supports DYMO LabelWriter and
LabelManager printers and ZPL label printers (such as Altec ATP and Zebra):
die-cut and tape labels, text, addresses, barcodes and QR codes, images,
shapes, counters, dates and curved text, address books, labels of a kind
(cable wraps and flags, patch panels, terminal blocks, rack markers), and
opening and saving `.label` files.
