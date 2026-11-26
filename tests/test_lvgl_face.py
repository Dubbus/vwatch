"""End-to-end: the LVGL example boots in QEMU, renders, and reacts to buttons.

Run from the repo root: python3 tests/test_lvgl_face.py
Needs the LVGL submodule (git submodule update --init).
"""
import os, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "host"))
from vwhost import Watch   # noqa: E402

EX = os.path.join(ROOT, "examples", "lvgl_face")
BLACK, WHITE = 0x00, 0x3F


def colours(w, rows):
    return {px for row in w.fb[rows] for px in row}


def main():
    subprocess.run(["make", "-s", "-C", EX], check=True, capture_output=True)
    w = Watch(os.path.join(EX, "build", "lvgl_face.elf"))
    try:
        info = w.hello()
        assert info["proto"] == 2 and info["app"] == "lvgl_face", info
        r = w.tick(1000, hr=120)
        assert r["frame_bytes"] > 0, "LVGL should render on the first tick"
        assert WHITE in colours(w, slice(95, 140)), "clock digits should render white"
        # anti-aliasing: the big digits' edges use intermediate greys, not just black/white
        greys = {c for c in colours(w, slice(95, 140)) if c in (0x15, 0x2A)}
        assert greys, "expected anti-aliased grey edge pixels around the clock digits"
        assert w.fb[120][2] != BLACK, "seconds ring track at the edge"

        before = colours(w, slice(0, 12))
        w.button("UP"); w.button("UP", False)
        w.tick(2000, hr=120)
        assert colours(w, slice(0, 12)) != before, "UP should change the ring accent"

        w.button("START"); w.button("START", False)
        w.tick(5000, hr=120)
        w.hold("BACK")
        w.tick(6000, hr=120)
        assert "stopwatch started" in w.logs and "stopwatch reset" in w.logs, w.logs
    finally:
        w.close()
    print("lvgl_face: OK")


if __name__ == "__main__":
    main()
