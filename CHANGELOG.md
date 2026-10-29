<!--
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: CC0-1.0
-->

# Changelog

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
  comment, which DLS ignores: DLS still opens the label.

### Designing

- **Grid and snapping** (View > Show Grid, Snap to Grid, Grid Spacing):
  moving and resizing put edges on the grid; hold Alt to place freely.
- **White on black** text (Format > White on Black), using the object's own
  colours, so DLS shows it the same way.
- **More barcodes**: Data Matrix, Aztec, MaxiCode, Micro QR, Code 93,
  Code 11, MSI Plessey, Pharmacode, GS1-128, GS1 Data Matrix, GS1 DataBar,
  HIBC (Code 128, Code 39, Data Matrix, QR), KIX (PostNL) and Royal Mail.
  GS1 data is typed with `[AI]` brackets and shows `(AI)` under the bars.
  Data Matrix symbols are always square. DLS's own POSTNET, PLANET and
  Intelligent Mail now draw too. DLS cannot show the new types.

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

The first published version: a DYMO Label Software 8 style label designer
for DYMO LabelWriter and LabelManager printers and ZPL printers (Altec ATP,
Zebra), reading and writing DLS `.label` files.
