"""Headless vwatch host: boot firmware in QEMU, drive it, save the screen.

Reference implementation of the host side of sdk/include/vw_proto.h, used for
tests and CI. The desktop app does the same thing in C++.

  python3 host/vwhost.py build/app.elf --ticks 12 --png out.png \
      --hr 120:165 --press 5:START,8:UP

Standard library only.
"""
from __future__ import annotations
import argparse, math, os, socket, struct, subprocess, sys, tempfile, time, zlib

SYNC = b"\xA5\x5A"
HELLO, TICK, BUTTON = 0x01, 0x02, 0x03
INFO, RECT, LOG, TICK_DONE = 0x81, 0x82, 0x83, 0x84
BUTTONS = {"LIGHT": 0, "UP": 1, "DOWN": 2, "START": 3, "BACK": 4}


class Watch:
    def __init__(self, elf: str, timeout: float = 10.0, qemu: str = "qemu-system-arm"):
        self.dir = tempfile.mkdtemp(prefix="vwatch-")
        path = os.path.join(self.dir, "uart.sock")
        self.srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.srv.bind(path)
        self.srv.listen(1)
        self.proc = subprocess.Popen(
            [qemu, "-machine", "mps2-an386", "-display", "none", "-monitor", "none",
             "-serial", f"unix:{path}", "-kernel", elf],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.srv.settimeout(timeout)
        self.sock, _ = self.srv.accept()
        self.sock.settimeout(timeout)
        self.buf = b""
        self.info = None
        self.fb = [[0] * 240 for _ in range(240)]
        self.logs: list[str] = []

    # ---------- wire ----------
    def send(self, typ: int, payload: bytes = b"") -> None:
        self.sock.sendall(SYNC + struct.pack("<BH", typ, len(payload)) + payload)

    def _read(self, n: int) -> bytes:
        while len(self.buf) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("firmware closed the UART (crashed or exited)")
            self.buf += chunk
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def recv(self) -> tuple[int, bytes]:
        while True:                                   # resync on garbage
            if self._read(1) != b"\xA5":
                continue
            if self._read(1) != b"\x5A":
                continue
            typ, n = struct.unpack("<BH", self._read(3))
            return typ, self._read(n)

    def handle(self, typ: int, p: bytes) -> None:
        if typ == INFO:
            ver, fmt, w, h, rnd = struct.unpack_from("<BBHHB", p)
            name = p[7:31].split(b"\0")[0].decode(errors="replace")
            self.info = {"proto": ver, "format": fmt, "w": w, "h": h, "round": rnd, "app": name}
        elif typ == RECT:
            x, y, w, h = struct.unpack_from("<HHHH", p)
            px = p[8:]
            for row in range(h):
                self.fb[y + row][x:x + w] = px[row * w:(row + 1) * w]
        elif typ == LOG:
            self.logs.append(p.decode(errors="replace"))

    def until(self, want: int) -> bytes:
        while True:
            typ, p = self.recv()
            self.handle(typ, p)
            if typ == want:
                return p

    # ---------- high level ----------
    def hello(self) -> dict:
        self.send(HELLO)
        self.until(INFO)
        return self.info

    def tick(self, t_ms: int, hr: float = 0.0, speed: float = 0.0, cadence: float = 0.0) -> dict:
        self.send(TICK, struct.pack("<Ifff", t_ms, hr, speed, cadence))
        t, nbytes = struct.unpack("<II", self.until(TICK_DONE))
        return {"t_ms": t, "frame_bytes": nbytes}

    def button(self, name: str, pressed: bool = True) -> None:
        self.send(BUTTON, struct.pack("<BB", BUTTONS[name], int(pressed)))

    def close(self) -> None:
        try:
            self.sock.close()
            self.srv.close()
        finally:
            self.proc.kill()
            self.proc.wait()

    # ---------- output ----------
    def png(self, path: str, scale: int = 2) -> None:
        """Write the screen as seen on the device: round mask, white outside."""
        w = h = 240
        rows = []
        for y in range(h * scale):
            row = bytearray([0])                       # filter: none
            sy = y // scale
            for x in range(w * scale):
                sx = x // scale
                dx, dy = sx - 119.5, sy - 119.5
                if self.info and self.info["round"] and dx * dx + dy * dy > 120 * 120:
                    row += b"\xff\xff\xff"
                else:
                    c = self.fb[sy][sx]
                    row += bytes((((c >> 4) & 3) * 85, ((c >> 2) & 3) * 85, (c & 3) * 85))
            rows.append(bytes(row))
        raw = b"".join(rows)

        def chunk(tag, data):
            return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

        with open(path, "wb") as f:
            f.write(b"\x89PNG\r\n\x1a\n")
            f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w * scale, h * scale, 8, 2, 0, 0, 0)))
            f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
            f.write(chunk(b"IEND", b""))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("elf")
    ap.add_argument("--ticks", type=int, default=10)
    ap.add_argument("--hr", default="70:150", help="lo:hi bpm, ramps over the run")
    ap.add_argument("--press", default="", help="tick:BUTTON[,tick:BUTTON...]")
    ap.add_argument("--png", default=None)
    a = ap.parse_args(argv)

    lo, hi = (float(v) for v in a.hr.split(":"))
    presses: dict[int, list[str]] = {}
    for item in filter(None, a.press.split(",")):
        k, b = item.split(":")
        presses.setdefault(int(k), []).append(b.upper())

    w = Watch(a.elf)
    try:
        t0 = time.perf_counter()
        info = w.hello()
        print(f"booted {info['app']!r}: {info['w']}x{info['h']} round={info['round']} proto v{info['proto']}")
        for i in range(a.ticks):
            for b in presses.get(i, []):
                w.button(b, True)
                w.button(b, False)
            hr = lo + (hi - lo) * i / max(1, a.ticks - 1) + 3 * math.sin(i)
            r = w.tick(i * 1000, hr=hr, speed=3.0)
            print(f"tick {i:3d}  hr={hr:5.1f}  redraw={r['frame_bytes']:6d} B")
        dt = time.perf_counter() - t0
        for line in w.logs:
            print(f"[fw] {line}")
        print(f"{a.ticks} ticks in {dt:.2f} s")
        if a.png:
            w.png(a.png)
            print(f"wrote {a.png}")
    finally:
        w.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
