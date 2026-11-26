"""End-to-end: build hello_face, boot it in QEMU, check protocol and pixels.

Run from the repo root: python3 tests/test_hello_face.py
Needs arm-none-eabi-gcc, make and qemu-system-arm on PATH.
"""
import os, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "host"))
from vwhost import Watch   # noqa: E402

EX = os.path.join(ROOT, "examples", "hello_face")
WHITE, CYAN, ORANGE, BLACK = 0x3F, 0x0F, 0x34, 0x00


def main():
    subprocess.run(["make", "-s", "-C", EX], check=True, capture_output=True)
    w = Watch(os.path.join(EX, "build", "hello_face.elf"))
    try:
        info = w.hello()
        assert info == {"proto": 2, "format": 1, "w": 240, "h": 240, "round": 1,
                        "app": "hello_face"}, info
        r = w.tick(0, hr=100)
        assert r["frame_bytes"] > 0, r
        assert w.fb[120][40] == BLACK                     # inside the dial, left of the digits
        assert w.fb[120][3] != BLACK                      # the seconds-ring track (r 112..118)
        assert ORANGE in w.fb[4], "seconds ring should use the default orange accent"
        assert any(WHITE in row for row in w.fb[70:118]), "clock digits should be white"

        w.button("UP"); w.button("UP", False)
        w.tick(1000, hr=100)
        assert CYAN in w.fb[4], "UP should switch the accent to cyan"

        w.button("START"); w.button("START", False)
        w.tick(5000, hr=100)
        assert "stopwatch started" in w.logs, w.logs
        assert w.backlight > 0, "a button press should light the screen"
        w.tick(14000, hr=100)                         # 8 s auto-off (sim time) has passed
        assert w.backlight == 0, "backlight should time out"

        w.hold("BACK")                                # hold BACK resets even while running
        w.tick(15000, hr=100)
        assert "stopwatch reset" in w.logs, w.logs
    finally:
        w.close()
    print("hello_face: OK")


if __name__ == "__main__":
    main()
