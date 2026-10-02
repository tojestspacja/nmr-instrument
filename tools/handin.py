"""Copy the course hand-in for the housing into class-board-2026, as a snapshot of this repository.

The course collects every student's housing from class-board-2026, docs/students/<name>/housing/
(workbook/housing-brief.md: housing.scad, the STL of each printed part, the DXF of the laser-cut
plate, housing.png). The design itself lives here, in mechanical/; that folder only holds a copy.
This script writes the copy and stamps it with this repository's commit, so it is never edited by
hand and never becomes a second source of truth.

    py tools/handin.py                  # class-board-2026 cloned beside this repository
    py tools/handin.py --to <path>      # or say where its housing folder is

Then commit and push the copy from class-board-2026 (branch w1-yi-tsai).
"""

import argparse
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "mechanical"
DEFAULT = ROOT.parent / "class-board-2026" / "docs" / "students" / "yi-tsai" / "housing"
# housing.scad needs nmr-params.scad to open; the rest is the brief's list
FILES = ["housing.scad", "nmr-params.scad", "base-shell.stl", "hood.stl", "coupon.stl",
         "bottom-plate.dxf", "housing.png"]
# from inside class-board-2026 the board meshes are four levels up, not in a sibling checkout
BOARD_HERE = 'board_dir = "../../class-board-2026/hardware/release/";'
BOARD_THERE = 'board_dir = "../../../../hardware/release/";'
SITE = "https://tojestspacja.github.io/nmr-instrument/"
REPO = "https://github.com/tojestspacja/nmr-instrument"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--to", type=Path, default=DEFAULT)
    a = ap.parse_args()
    dst = a.to
    if not dst.parent.exists():
        raise SystemExit(f"{dst.parent} does not exist - clone class-board-2026 beside this repository or pass --to")
    commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    dirty = subprocess.run(["git", "status", "--porcelain", "--", "mechanical"], cwd=ROOT,
                           capture_output=True, text=True, check=True).stdout.strip()
    if dirty:
        print("warning: mechanical/ has uncommitted changes; the stamp names the last commit, not them")
    dst.mkdir(parents=True, exist_ok=True)
    stamp = (f"// HAND-IN SNAPSHOT - do not edit here. Source: {REPO}, mechanical/, commit {commit[:7]}.\n"
             f"// Regenerate with `py tools/handin.py` in that repository.\n")
    for name in FILES:
        if name.endswith(".scad"):
            s = (SRC / name).read_text(encoding="utf-8")
            if name == "housing.scad":
                if BOARD_HERE not in s:
                    raise SystemExit("housing.scad: board_dir line not found; update tools/handin.py")
                s = s.replace(BOARD_HERE, BOARD_THERE)
            (dst / name).write_text(stamp + s, encoding="utf-8", newline="\n")
        else:
            shutil.copyfile(SRC / name, dst / name)
    (dst / "README.md").write_text(
        "# Project 3 housing: course hand-in\n\n"
        f"This folder is the **hand-in copy** of my housing, the files the course collects here "
        f"(`housing.scad`, `base-shell.stl`, `hood.stl`, `coupon.stl`, `bottom-plate.dxf`, `housing.png`; "
        f"`nmr-params.scad` is there because `housing.scad` includes it).\n\n"
        f"It is a snapshot, generated from [{REPO}]({REPO}) (`mechanical/`, commit `{commit[:7]}`) by "
        f"`tools/handin.py`. **Do not edit it here**; change the design there and regenerate.\n\n"
        f"The whole project (housing, probe, B0 coils, simulator, data, fabrication files, and how the real "
        f"instrument works) is at **{SITE}**.\n\n"
        "Print orientation and parts: `housing.scad` with `part = \"shell\"`, `\"hood\"`, `\"coupon\"`, `\"plate\"` (DXF).\n",
        encoding="utf-8", newline="\n")
    print(f"wrote {len(FILES)} files + README.md to {dst} (snapshot of {commit[:7]})")


if __name__ == "__main__":
    main()
