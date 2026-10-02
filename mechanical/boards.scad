// The two boards in KiCad's frame (y points down: the boards lie at x 0..180, y -115..-15).
// stacked = true : assembled as in the housing, front panel face up, main board under it -
//                  housing.scad's own boards(), so the stack here can never drift from the one
//                  the housing is built round (the gap, the turn-over, the dev board).
// stacked = false: side by side, each board as exported.
use <housing.scad>

stacked = true;

if (stacked) {
    boards();
} else {
    // class-board-2026 cloned beside this repository (see housing.scad, board_dir)
    import("../../class-board-2026/hardware/release/front-panel.stl");
    translate([200, 0, 0]) import("../../class-board-2026/hardware/release/class-board.stl");
}
