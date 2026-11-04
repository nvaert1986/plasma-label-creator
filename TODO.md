<!--
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: CC0-1.0
-->

# To do

What is done is described in the README; this lists what is left. The
designer comes first; the section "Later" at the end is low priority.

## Done in 0.3.0

See `CHANGELOG.md` under "Unreleased" for the details.

- [x] Select several objects at once: Ctrl/Shift-click, a band over empty
  space, Select All.
- [x] Align them to each other and distribute them evenly.
- [x] Group and ungroup; lock position and size.
- [x] Text that wraps within its box.
- [x] Printable strips for Altec's catalogue wrap labels (which end is not
  yet confirmed, see Testing on real printers).
- [x] The desktop entry mentions LabelManager and ZPL printers.
- [x] `mainwindow.cpp` split by topic.
- [x] Layers (shown, printed, locked) and objects that do not print.
- [x] Objects turned to any angle.
- [x] Calculated fields, date offsets and filters in fields, counters
  shared between computers.
- [x] GS1 builder for GS1-128, GS1 Data Matrix and GS1 DataBar Expanded.
- [x] Several labels across a roll, and continuous rolls with a set length
  (printers that take any label size: Altec, Zebra).
- [x] Turning objects with the mouse (rotate handle, Shift for 15° steps,
  resize handles on turned objects), Turn Left/Right 15°, an angle box.
- [x] Toolbars rearranged into two rows with new Insert and Arrange bars.

## Done in 0.4.0

- [x] Print-only mode, with a "Label Printing" launcher entry.
- [x] Data-entry forms: drop-downs, numbers, dates, check boxes, lookups.
- [x] Versions of saved labels, and the print history.
- [x] Printer warnings in the print bar and a Printer Status window.
- [x] Settings > Configure Plasma Label Creator (designing, printing,
  print-only mode with a lock, history and versions).
- [x] Print preview fitted on opening, landscape labels as on the canvas.
- [x] Export with the label's outline (rounded corners) as a proof.
- [x] `printbar.cpp` split by topic (bar, printers, status).
- [x] DYMO's own label ids in saved labels.
- [x] Automation service: watched folders, and print requests over HTTP
  with Qt HTTP Server (optional, greyed out in the settings without it).
- [x] Automation: network ZPL printers asked before printing; Recent Jobs.

## Done in 0.5.0

Features found in other label software, the Easy to Medium ones:

- [x] Print conditions on objects.
- [x] Copies per row from a data column.
- [x] Header and trailer labels around a run.
- [x] Query parameters from the answers.
- [x] Anchoring, and text that grows to fit.
- [x] SVG drawings.
- [x] Pictures from a database column.
- [x] Polygons, stars and arrows.
- [x] Guidelines.
- [x] A different label per row, named in a data column.
- [x] Print density, darkness and speed per label.

## Done in 0.6.0

- [x] Add-ons: optional Qt plugins (header-only interface in `src/plugin`),
  with File menu actions, a status bar indicator, their own label folders,
  and hooks for saving, printing and shared counters. Settings > Add-ons.

## Next session

Version 0.6.0: add-ons. Version 0.5.0: print conditions, growing text and anchoring, guidelines,
polygons, stars, arrows and SVG, copies and a label per row, header and
trailer labels, density, darkness and speed per label, query parameters
and pictures from a database (tested). Version 0.4.0 before it: print-only
mode, forms, versions, print history, printer status, settings, export
with the label's outline, DYMO's label ids and the automation service.
Next: the checks under "Bug review" (with a first real test print through
the automation service), then the printer tests below. Without a printer:
sheets of labels for office printers and the automation extras under
"Later". After
that, bringing the web version up to date (see "Web
version"). Still to decide: whether RFID encoding stays on the list.

## Third bug review (2026-10-30, 0.3.0)

Done for turning with the mouse and the new toolbars: the angle shown
while turning could stay behind after an undo during the drag and show up
in the next move; the pointer is a closed hand while turning. clang-tidy
over all sources: the same 24 checked findings as before, nothing new. 22
test suites pass.

## Second bug review (2026-10-30, evening)

Done for layers, angles, calculated fields, shared counters, the GS1
builder and roll layouts: two-digit years in field dates (read as 1926),
`{Time}` offsets (hours added), stale layer names from files and pastes,
no wait cursor while waiting for a shared counter; tests added for Rotate
Label with objects at an angle. Clean rebuild without warnings. clang-tidy
(bugprone, performance, clang-analyzer) over all sources: 49 findings, 25
fixed; the 24 left are checked: optionals guarded by an earlier test,
positive-only rounding of bar widths, and Qt-parented objects.

## Bug review (2026-10-30)

Done: the code since 0.2.0 and the 0.2.0 data and printing code were
reviewed, and the bugs found are fixed with tests (see `CHANGELOG.md`,
"Fixed"). Checked: labels from other label software write GroupID -1, so
they never group by accident; groups, locks and wrapping survive saving,
reopening, copy and paste, also on tape labels with reordered cells.

Still to do by hand, in the running app:

- [ ] Try multi-select, align, distribute, group, lock and wrapping text
  on real labels, with undo and redo in between.
- [ ] Print a label with wrapped text and grouped objects, and compare it
  with the screen and the print preview.
- [ ] Print from a database with a password (SQL Server over ODBC, say):
  the password is asked once per database and again after a wrong one.
- [ ] Try layers, Do Not Print, turning with the mouse and the angle box,
  calculated fields, the GS1 builder; print a label with a turned object
  and a non-printing one.
- [ ] The new toolbars on the real desktop (Breeze, your font size): both
  rows fit at 1280 px offscreen; at 1000 px Fit to Page runs over.
- [ ] Print-only mode on real labels: a folder of labels, forms with each
  kind of field, a lookup from data, Choose Rows, counters saved; the
  Label Printing launcher entry after `cmake --install`.
- [ ] Versions: save a label a few times and restore one. Print history:
  Print Again with data rows.
- [ ] Printer warnings: a paused CUPS queue, an Altec with the head open or
  out of labels; the Printer Status window.
- [ ] Settings: each page, Restore Defaults; print-only locked (and fixed
  with `LockPrintOnly[$i]=true` in /etc/xdg/plasma-label-creatorrc);
  versions next to the label on a share.
- [ ] Shared counter: two computers printing the same label with one
  counter file on a share count on without repeats.
- [ ] Export: PDF and PNG with and without the outline; a name without
  folder or extension; a folder that no longer exists.
- [ ] DYMO's label ids: a new 30252/99010 label saved has `<Id>Address`;
  an older file saved again gets it too; DYMO's software opens both.
- [ ] Automation service: start and stop it from Settings > Automation,
  start at log-in; a CSV dropped in a watched folder prints a label per
  row (counters count on in the label, the print shows in the history); a
  bad job lands in `failed/` with the reason; a request over HTTP with
  `curl` (and without the token: refused); a stopped queue is refused
  unless `printAnyway`; the Altec with its head open or other labels loaded
  is refused; Recent Jobs lists it all. Needs a real test print.
- [x] The features of 0.5.0 (tested): a condition on an object (preview and
  print), text that grows with an object anchored to it, guidelines from
  both rulers, a star and an arrow in DYMO's software (a picture), an SVG
  logo printed sharp, copies and a label per row from a CSV, header and
  trailer labels with {Count} (counters skip them), density on the
  LabelWriter and darkness and speed on the Altec, a query with
  {Serial} in it, a picture stored in a database.

## Designer

The Medium designer items are done (see "Done in 0.3.0"). Left, all
Hard:

- [ ] **RFID encoding**: write RFID tags in RFID labels on printers that
  have an encoder. (Hard; the LabelWriter 550's RFID label check stays out.)
  Needs an RFID printer, matching labels and a UHF reader to test: the
  encoding (EPC, SGTIN-96) and the printer's answers can be tested without
  one, a tag really written only with one. Printing would go to the
  printer as raw ZPL (the label as a picture plus ^RS/^RF), since CUPS's
  Zebra driver cannot add RFID commands.
- [ ] **Scripting** in labels: fields calculated by a small script. (Hard)
- [ ] **Read NiceLabel `.nlbl` files**: see Files below. (Hard)

## Testing on real printers

- [ ] **Landscape print offset.** Prints are moved up by the driver's
  unprintable top margin (`ImageableArea`), confirmed on a 30334 (1.5 mm).
  For 30252 Address the margin is 5.8 mm: print a frame label on a 30252 to
  confirm the margin is right for every paper, not a coincidence for 30334.
- [ ] **LabelWriter 550 series.** Supported through DYMO's LW5xx driver on a
  queue with its own driver (USB or network); never tried on a real 550.
- [ ] **Raw CUPS queues.** Labels for a queue without the DYMO driver are
  converted on this computer and sent raw; still to try on a real raw
  network queue.
- [ ] **Labels across and continuous rolls on a real Altec.** The gap
  between labels across is not on Altec's site: 2 mm is assumed for the
  catalogue's rolls (adjust in Layout > Roll Layout). Print a frame label
  on a 2-across roll to find the real gap and the left edge, and one on a
  continuous roll (the job switches the printer to continuous media, `^MNN`;
  the next normal job switches back to gap sensing, `^MNY`).
- [ ] **Altec wrap label strips.** The catalogue's wrap labels take print
  on the strip at the end that leaves the printer first; Altec's site does
  not say which end it is. Print a frame label on a wrap label to confirm.
  Also: Wikkellabels 12.7 × 19.05 mm and some Twistlabels sizes have no
  strip length in the catalogue.
- [ ] **LabelManager tape widths.** 12 mm is tested; 6, 9, 19 and 24 mm and
  the printable band on each are not.
- [ ] **Zebra printers.** Should work like the Altec (ZPL); not tested on one.

## Printers

- [ ] **DYMO Rhino 6000 / 6500.** DYMO has no Linux driver for them. DLS
  treats them as tape printers like the LabelManager 450, so its driver may
  work; needs their USB IDs and a test on a real Rhino.
- [ ] **Altec:** round label outlines, and label detection over USB (only
  over the network now).

## Files

- [ ] **Read NiceLabel `.nlbl` files**, for interoperability: open the
  label (size, objects, text, barcodes) and convert it to a `.label`.

## Code

Nothing open.

## Plasma integration

Done after 0.6.0 in `src/integration`, each part a build option (see
INSTALL.md). One change from the plan: the desktop files keep their names;
the AppStream id points at them with `<launchable>`, so pinned launchers stay.

- [x] **File types** for `.label` (DieCutLabel, ContinuousLabel), `.ABX`
  (AddressBook) and `.rhino` files, as shared MIME info recognised by the
  XML root, so other `.label` files are not claimed. The desktop entries
  list them (`MimeType=`): Dolphin then names and opens them as labels.
  (Easy)
- [x] **Launcher entry extras**: actions on right-click ("Print-Only Mode",
  "New Label") and search keywords (label, DYMO, Zebra, Altec, barcode) for
  KRunner and the launcher. (Easy)
- [x] **Its own icons**: an original SVG for the app and one for Label
  Printing, installed in the hicolor theme, instead of the generic printer
  icons. (Easy-Medium)
- [x] **AppStream metadata** (description, screenshots, releases from
  0.4.0 on) for Discover and other software centres, checked with
  `appstreamcli validate`, with a reverse-DNS app id
  (`io.github.nvaert1986.PlasmaLabelCreator`) for the desktop entries and
  `setDesktopFileName`. Launchers pinned under the old name are pinned
  again once. (Easy)
- [x] **"Print Label" in Dolphin's right-click menu** for `.label` files: open
  it in print-only mode, or print it on the default printer. (Easy)
- [x] **Thumbnails in Dolphin**: a picture of each label instead of an
  icon, drawn by the app's own renderer, as a KIO thumbnail plugin that is
  built only when KIO is found. (Medium)

## Web version: the desktop app's features

The web version has most of what the first release had (designing, label
types per printer, labels of a kind, `.label` and `.rhino` files, printing
through the browser), plus sign-in, users, and printers, label sizes and
labels kept per user. The aim is to bring it close to the desktop app, one
phase at a time, each with its own tests. Not everything can follow: a
browser cannot reach CUPS or talk to a printer directly.

- [ ] **Phase 0, the rest of 0.1.0**, first:
  - Rulers along the canvas.
  - Editing text on the label itself (double-click), and a format toolbar:
    font, size, larger and smaller, bold, italic, underline, strikeout.
  - Cut, copy, paste and duplicate objects, also between labels.
  - Bring to Front and Send to Back.
  - Position Object: against an edge, centred, in the middle, moved by a
    distance.
  - Dragging objects in from the sidebar, and dropping pictures and label
    files on the canvas.
  - Address books: open and save `.ABX`, import addresses from CSV, check
    addresses and print a label for each.
  - Before printing, check that the label suits the printer's labels
    ("Print Anyway").
  - Recently used layouts, with thumbnails.
  - Advance Counters.
  - Clip art: DYMO's cannot be shipped; instead each user's own pictures
    kept on the server to pick from.

  (Medium; mostly Easy pieces. Not possible in a browser: noticing
  printers plugged in over USB and setting them up, see phase 5. The
  command-line tool stays with the desktop app.)
- [ ] **Phase 1, the 0.2.0 features**: a label for each row of a CSV, text,
  Excel or ODS file; fields; pictures from data; print-time questions; grid
  and snapping; white on black; Rotate Label turning the objects; the
  barcodes added in 0.2.0; print preview; PDF and PNG export (with the
  label's outline). (Medium; the barcodes are cheap, zint
  already runs in the browser; Excel and ODS need a reader, PDF a library.)
- [ ] **Phase 2, the 0.3.0 designer**: selecting several objects, align
  and distribute, group, lock, wrapped text, layers and Do Not Print, free
  angles and turning with the mouse, calculated fields, the GS1 builder,
  labels across and continuous rolls. (Medium-Hard; working with several
  objects at once on the canvas is most of it.)
- [ ] **Phase 3, for operators**: print-only page, data-entry forms,
  versions of labels, print history, shared counters, settings. (Medium;
  simpler than on the desktop, as the server already knows the users: the
  versions and history can be per user and shared, see "Later".)
- [ ] **Phase 4, databases** as a data source (SQL Server and others),
  queried by the server: read-only, with care for who may use which
  database and where its password is kept. (Medium)
- [ ] **Phase 5, printers**, only where it can be done: the server asking a
  ZPL printer on the network about its labels and status, if it can reach
  it. A CUPS queue's state, LabelWriters plugged in over USB and printing
  without the browser's print dialog would need a small helper program on
  each computer. (Hard; left out unless needed.)

## Later (low priority)

Beyond the designer: printing for operators, automation, and managing
labels in a team. Shared features (versions, history, roles, approvals)
belong in one place both apps use: the plan is to build them in the web
version's server and let the desktop app open and save labels there,
rather than build them twice.

In the desktop app:

- [ ] **Open and save labels in the web version's library**, signed in with
  the same account. (Medium)
- [ ] **Sheets of labels on office printers**: A4 and Letter sheets (Avery,
  Herma and the like) with a catalogue of sheet layouts, printing the
  labels of a run in rows and columns, starting at any label of a sheet
  that was used before. (Medium-Hard)
- [ ] **Automation: a raw TCP port**, so other systems print to the
  service as if to a printer on port 9100, with the data as CSV or JSON.
  (Medium)
- [ ] **Automation: XML and fixed-width text jobs**, and a data filter
  that maps their fields to the label's. (Medium to Hard)
- [ ] **Automation: a database trigger** (print when rows appear) and
  **scheduled jobs**. (Medium)

In the web version's server, used by both apps:

- [ ] **Versions** of saved labels, shared by everyone using them, to compare
  and restore (the desktop app keeps them per computer). (Easy-Medium)
- [ ] **Print history** for everyone (the desktop app keeps one per
  computer): who printed which label, version and how many.
  (Easy-Medium)
- [ ] **Roles and permissions**: designers, approvers, operators; labels per
  group. (Medium)
- [ ] **Approval**: labels are drafts until released; operators print only
  released versions. (Medium)

Big projects, only when someone needs them:

- [ ] **Full automation**: many triggers, a filter designer for any data
  format, connections to ERP systems. (Hard)
- [ ] **Full form designer** with its own logic and runtime. (Hard)
- [ ] **Compliance-grade audit trail and electronic signatures** for
  regulated industries. (Hard)
