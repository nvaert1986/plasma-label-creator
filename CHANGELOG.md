<!--
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: CC0-1.0
-->

# Changelog

## Unreleased

### Printing

- **Print-only mode** (File > Print-Only Mode, `plasma-label-creator
  --print-only [folder]`, and a "Label Printing" launcher entry): the labels
  in a folder with pictures to choose from, the label shown read-only and
  filled in with the form's answers, the form beside it, Choose Rows for
  data (without changing where it comes from), and the print bar. Nothing
  can be edited. Counters that moved on are saved in the label after
  printing. The designer's toolbars come back as they were.
- **Data-entry forms**: print-time questions can be text (with rules, and
  a lookup in a data column that fills the other fields from the row
  found), a drop-down list (fixed choices or a column of the data), a
  number (range, decimals), a date (format, today by default) or a check
  box (an answer for ticked and for not). Data > Print-Time Questions...
  chooses the kind.
- **Versions** (File > Versions...): before a label is saved over, the
  file as it was is kept, 20 per label, in the app's data folder; restore
  one onto the canvas (undoable).
- **Print history** (File > Print History...): every print, with when,
  which label, printer, how many, job, answers and data rows, and the
  label as printed; search it (data values too), open a label as printed,
  or print the same labels again.
- **Printer status**: the print bar shows a stopped or paused queue, jobs
  waiting, and what a network ZPL printer reports (out of labels, head
  open, paused, out of ribbon), asked again every 30 seconds; before
  printing it asks "Print Anyway?" when something is wrong. Printer
  Status... shows every label printer's queue and state, refreshed every
  ten seconds. `plasma-label-creator-render --list-printers` shows them too.

### Settings

- **Settings > Configure Plasma Label Creator…**:
  - Designing: the font of new text, the arrow keys' steps (and with
    Shift), the turning step (Shift while turning, Turn Left/Right), the
    tape a LabelManager label starts with, rulers in millimetres or inches.
  - Printing: whether and how often network printers are asked, the
    warning before printing to a printer that is not ready, the copies a
    print starts with, the gap between Altec labels across, the length
    continuous rolls start at, how long to wait for a shared counter.
  - Print-only mode: the folder it starts in, saving counters in the
    label, and locking the app to print-only mode with no way back.
  - History: versions kept per label (or none), versions next to the label
    in a `.versions` folder instead of the app's data folder, prints kept
    in the history (or none), recently used layouts kept.
- Only settings that differ from the defaults are written. An administrator
  can fix any of them with KDE's Kiosk (`Key[$i]=value` in the `[Settings]`
  group of `plasma-label-creatorrc`); the window then shows them locked.

### Fixed

- Print Again checks that the label suits the printer and the printer is
  ready, as any print, and keeps the label's name in the history.
- A network printer that keeps not answering no longer reloads the label
  types every 30 seconds.
- Print-only mode no longer offers printing the hidden address book's
  checked addresses, and redraws a label's picture only when it changed.
- Print Preview showed only a thin white rectangle until zoomed in: the
  label is now fitted to the window when it opens. A landscape label is
  shown as on the canvas; "As it feeds through the printer" shows it turned,
  as the printer receives it.
- File > Versions... looks where the settings keep versions (also next to
  the label); a font for new text that is not installed keeps its name
  when the settings are saved; print-only mode never asks about saving,
  and switching to it asks about unsaved changes first.

### Other

- The about text names all supported printers.

## 0.3.0 (2026-10-30)

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
- **Layers** (Layout > Layers…): each shown or hidden on screen, printed
  or not, locked or not; Layout > Move to Layer. Objects not on a layer
  are on the label itself.
- **Do Not Print** (Layout menu): an object only to design with, such as a
  picture of the product; drawn faded, left out when printing and
  exporting.
- **Any angle** (Angle in an object's properties): turns the whole object
  about its centre, on top of its quarter-turn Rotation. Other label
  software shows it without the angle.
- **Turning with the mouse**: a selected object has a round handle above
  its top edge; dragging it turns the object, with the angle shown by the
  pointer, and Shift turns in steps of 15°. A turned object keeps its
  resize handles, which work along its own sides.
- **Turn Left 15° / Turn Right 15°** (Layout menu, Ctrl+[ and Ctrl+]) and
  an angle box on the new Arrange toolbar.
- **Toolbars rearranged** to fit a smaller window: the main toolbar and a
  new Insert toolbar on the first row; the Format toolbar (alignment now
  one drop-down button each, a narrower font list) and a new Arrange
  toolbar (Align Objects, Group, Lock, turning, order, Rotate Label, Fit
  to Page) on the second. A layout saved by an earlier version is
  replaced once. The window starts at 1280 × 800.
- **Calculated fields** (Data > Calculated Fields…): fields worked out when
  printing, such as `{Prefix}-{Serial|pad:6}`, used like columns.
- **More in fields**: `{Date}`, `{Time}` and `{Now}`; date and time
  offsets (`{Date+30}`, `{Installed+1y}`, `{Time+2h}`, also w and m);
  dates with two-digit years count as this century; `{Serial+1}`; filters
  after a `|`: upper, lower, trim, left, right, mid, pad, date:format,
  replace, default. `{Date+30|date:dd-MM-yyyy}` gives a best-before date.
- **Shared counters**: a counter can keep its next value in a file, on a
  network share say, so computers printing the same label count on from
  each other; the file is locked while printing.
- **GS1 builder** (Build… next to a GS1 barcode's data): application
  identifiers and values in a table, each checked (length, digits, dates,
  check digits; a missing check digit is added).
- Locks, wrapping, layers, angles, calculated fields and shared counter
  files are kept in the `.label` file's comment, which other label
  software ignores.

### Printers

- **Altec wrap labels** (Wikkellabels, Gekleurde Wikkellabels,
  Twistlabels) from the catalogue show their printable white strip: the
  rest of the label is shaded on the canvas. The strip's length comes from
  Altec's article codes; it is placed at the end that leaves the printer
  first, which is still to be confirmed on paper.
- **Labels across the roll** (Layout > Roll Layout…): rolls with 2 to 10
  labels side by side print a row at a time, with the next labels (data
  rows, copies, counters going on) next to each other, on printers that
  take any label size (Altec, Zebra). The gap between them is set there.
- **Continuous rolls**: a label of the length set in Roll Layout; the job
  tells CUPS's Zebra driver there are no gaps to find.
- The Altec catalogue now has its rolls with 2 to 6 labels across laid out
  as such (2 mm apart until measured) and its 36 continuous rolls.
- Labels across or continuous rolls on a printer that takes only its own
  label sizes (a LabelWriter) are warned about before printing.

### Fixed

- The toolbar's Fit to Page button was missing (a wrong action name).
- Print Preview followed by Print asked the print-time questions again and
  re-read the data; it now prints what was previewed.
- A database password given for one database was tried on another one,
  and a wrong password was not asked for again.
- Database queries now run in a transaction that is rolled back, read-only
  where the database supports it (PostgreSQL, MySQL/MariaDB, ODBC), so even
  a function called by a `SELECT` cannot change anything.
- Comments on cells in OpenDocument spreadsheets, and phonetic hints in
  Excel texts, no longer end up in the printed value.
- With no rows ticked, Data > Data Source… showed every row ticked again.
- Copying several objects of a tape label pasted the wrong objects.
- After Bring to Front or Send to Back with several objects selected, the
  properties and the format toolbar could follow the wrong object.
- Escape now ends a selection band.
- Wrapping text was slow to draw for long texts (about 35 ms per paint, now
  about 5 ms), which made dragging stutter.

### Requirements

- Qt 6.7 or newer (was 6.6), for reading two-digit years in dates.
- INSTALL.md describes checking the code with clang-tidy.

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
