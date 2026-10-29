<!--
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: CC0-1.0
-->

# To do

What is done is described in the README; this lists what is left. The
designer comes first; the section "Later" at the end is low priority.

## Designer

- [ ] **Select several objects at once**, then align them to each other,
  distribute them, group and lock them.
- [ ] **Text that wraps** into paragraphs within its box, besides the
  current line breaks and shrink-to-fit.
- [ ] **Calculated fields**: joining fields and text, parts of a field,
  date offsets (best before = today + 30 days), counters shared between
  computers.
- [ ] **GS1 builder**: compose GS1-128 and GS1 Data Matrix data from a list
  of application identifiers, with their formats checked, instead of
  typing `[AI]` brackets by hand.
- [ ] **Free rotation** of objects at any angle (the `.label` format only
  has quarter turns: needs an extension, kept in the settings comment).
- [ ] **Several labels across a roll**, and continuous rolls with a set
  length (Altec and other ZPL printers).
- [ ] **Layers and non-printing objects**, such as a picture of the product
  to design on that does not print.

Harder, after the items above:

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
- [ ] **LabelManager tape widths.** 12 mm is tested; 6, 9, 19 and 24 mm and
  the printable band on each are not.
- [ ] **Zebra printers.** Should work like the Altec (ZPL); not tested on one.

## Printers

- [ ] **DYMO Rhino 6000 / 6500.** DYMO has no Linux driver for them. DLS
  treats them as tape printers like the LabelManager 450, so its driver may
  work; needs their USB IDs and a test on a real Rhino.
- [ ] **Altec:** continuous rolls, several labels across the roll, round
  label outlines, and label detection over USB (only over the network now).
- [ ] **Printable areas for Altec's catalogue wrap labels.** Custom label
  sizes can have one; the catalogue's wrap labels do not yet.

## Files

- [ ] **Read NiceLabel `.nlbl` files**, for interoperability: open the
  label (size, objects, text, barcodes) and convert it to a `.label`.
- [ ] **DLS label ids.** Labels made with File → New get the driver's page
  keyword (e.g. `w79h252`) as `<Id>` where DLS writes its own (`Address`,
  `Small30334`). DLS 8 opens them fine so far; if that changes, use DLS's
  own ids.

## Code

- [ ] Split `mainwindow.cpp` and `printbar.cpp`; both have grown large.
- [ ] The desktop entry's description mentions only LabelWriter printers.

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
