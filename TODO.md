<!--
    SPDX-FileCopyrightText: 2026 nvaert1986 <25772781+nvaert1986@users.noreply.github.com>
    SPDX-License-Identifier: CC0-1.0
-->

# To do

What is done is described in the README; this lists what is left.

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

## Editor

- [ ] **Select several objects at once**, then distribute them and align
  them to each other, as in DLS.

## Code

- [ ] Split `mainwindow.cpp` and `printbar.cpp`; both have grown large.
- [ ] The desktop entry's description mentions only LabelWriter printers.
