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

## Next session

Version 0.3.0 is on GitHub (Niels tested it). Start from what Niels
finds next; then the hands-on checks
under "Bug review" and the printer tests below.

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
- [ ] Shared counter: two computers printing the same label with one
  counter file on a share count on without repeats.

## Designer

The Medium designer items are done (see "Done in 0.3.0"). Left, all
Hard:

- [ ] **The printer's own fonts and barcodes, and labels stored on the
  printer**: send ZPL text and barcode commands instead of an image, and
  keep label formats in the printer's memory to recall with new data.
  (Hard)
- [ ] **RFID encoding**: write RFID tags in RFID labels on printers that
  have an encoder. (Hard; the LabelWriter 550's RFID label check stays out.)
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
- [ ] **DLS label ids.** Labels made with File → New get the driver's page
  keyword (e.g. `w79h252`) as `<Id>` where DLS writes its own (`Address`,
  `Small30334`). DLS 8 opens them fine so far; if that changes, use DLS's
  own ids.

## Code

- [ ] Split `printbar.cpp`; it has grown large (`mainwindow.cpp` is split).

## Later (low priority)

Beyond the designer: printing for operators, automation, and managing
labels in a team. Shared features (versions, history, roles, approvals)
belong in one place both apps use: the plan is to build them in the web
version's server and let the desktop app open and save labels there,
rather than build them twice.

In the desktop app:

- [ ] **Print-only mode**: pick a label from a folder, answer its questions,
  choose rows, print; no editing. (Easy)
- [ ] **Versions and print history, for one user**: earlier versions kept
  next to a label to restore; a log of what was printed, when, where, how
  many and with which answers. (Easy)
- [ ] **Automation service**: a background service that prints when a CSV
  or JSON file lands in a folder, or on an HTTP request, through CUPS.
  (Medium)
- [ ] **Simple forms**: a data-entry screen per label with fields,
  drop-downs, defaults, lookups and a Print button, built on the
  print-time questions. (Medium)
- [ ] **Printer status** for this computer's printers on one screen, from
  CUPS and the ZPL network queries. (Medium)
- [ ] **Open and save labels in the web version's library**, signed in with
  the same account. (Medium)

In the web version's server, used by both apps:

- [ ] **Versions** of saved labels, to compare and restore. (Easy-Medium)
- [ ] **Print history**: who printed which label, version and how many.
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
