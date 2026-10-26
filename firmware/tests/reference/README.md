# Kaomoji expression reference

`kaomoji-expression-reference.png` is the user-supplied visual reference used
for the procedural expression study. It is intentionally retained unchanged so
future expression changes can be checked against the original art direction.

SHA-256: `a1586bd6d26edfb87cb9d886b844c30dadba879e57f6d94ef78b38dd5eac2377`

## Individual faces

`individual/` contains the 24 source faces, cropped unchanged into the same
3-column × 8-row order as the reference. Files use one-based coordinates:
`row-1-col-1.png` is the calm default face at the upper left, and
`row-8-col-3.png` is the lower-right face. These crops are for close
shape-by-shape approximation; the original image above remains authoritative.

## Local tracing editor

Open `face_editor.html` in a modern desktop browser. Select a crop, add a
Bézier curve, ellipse, or polygon, then drag its on-canvas control points over
the semi-transparent reference. Every untouched face begins with the
user-authored R1 C1 base, retained exactly at `templates/r1-c1-base.svg`, so
new work starts from the approved stroke weight, eye placement, and mouth
rather than an empty canvas. Download the clean SVG or save it directly into a
folder (Chrome/Edge). Each face has a 100-step undo/redo history during its
editing session (Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z). Shape drafts and completion
marks are kept in browser local storage, so face work can proceed one at a
time.
