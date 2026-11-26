"""Headless vwatch host: boot firmware in QEMU, drive it, save the screen.

Reference implementation of the host side of sdk/include/vw_proto.h, used for
tests and CI. The desktop app does the same thing in C++.

  python3 host/vwhost.py build/app.elf --ticks 12 --png out.png \
      --hr 120:165 --press 5:START,8:UP
  python3 host/vwhost.py build/app.elf --fit run.fit --every 5 --png out.png

--fit replays a recorded activity: HR, speed and cadence resampled to 1 Hz,
t_ms = seconds since the first record. --every N ticks once per N seconds of
activity time and sends the mean over those N seconds, so long runs replay
fast. Standard library only, except --fit, which needs `pip install fitdecode`.
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


# ---------- recorded activities ----------
STEP_SPORTS = {"running", "walking", "hiking", "trail_running", "treadmill_running"}


def _interp_1hz(ts: list[float], vs: list[float], n: int) -> list[float]:
    """Linear interpolation of (ts, vs) onto t = 0..n-1 s, held at the ends."""
    if not ts:
        return [0.0] * n
    out, j = [], 0
    for t in range(n):
        while j + 1 < len(ts) and ts[j + 1] <= t:
            j += 1
        if t <= ts[0]:
            out.append(vs[0])
        elif j + 1 >= len(ts):
            out.append(vs[-1])
        else:
            t0, t1 = ts[j], ts[j + 1]
            out.append(vs[j] + (vs[j + 1] - vs[j]) * (t - t0) / (t1 - t0) if t1 > t0 else vs[j])
    return out


def load_fit(path: str) -> dict:
    """1 Hz HR (bpm), speed (m/s), cadence (steps/min) from a FIT activity."""
    try:
        import fitdecode
    except ImportError:
        sys.exit("--fit needs the fitdecode package: python3 -m pip install fitdecode")
    recs, sport = [], ""
    with fitdecode.FitReader(path) as fit:
        for fr in fit:
            if fr.frame_type != fitdecode.FIT_FRAME_DATA:
                continue
            if fr.name in ("sport", "session") and not sport:
                v = fr.get_value("sport", fallback=None)
                sport = str(v) if v is not None else ""
            elif fr.name == "record":
                f = {x.name: x.value for x in fr.fields}
                if f.get("timestamp") is not None:
                    recs.append(f)
    if not recs:
        sys.exit(f"{path}: no record messages")
    t0 = recs[0]["timestamp"]
    t = [(r["timestamp"] - t0).total_seconds() for r in recs]
    n = int(t[-1]) + 1

    def channel(get):
        pts = [(ti, v) for ti, r in zip(t, recs) if (v := get(r)) is not None]
        return _interp_1hz([p[0] for p in pts], [float(p[1]) for p in pts], n)

    def speed(r):
        v = r.get("enhanced_speed")
        return v if v is not None else r.get("speed")

    k = 2.0 if sport in STEP_SPORTS else 1.0       # FIT run cadence is strides/min

    def cadence(r):
        c = r.get("cadence")
        return None if c is None else k * (c + (r.get("fractional_cadence") or 0.0))

    return {"hr": channel(lambda r: r.get("heart_rate")), "speed": channel(speed),
            "cadence": channel(cadence), "sport": sport, "seconds": n}


def fit_ticks(act: dict, every: int):
    """Yield (t_ms, hr, speed, cadence) per tick: the mean over each window of
    `every` seconds, stamped with the window's last second."""
    n = act["seconds"]
    for k in range(0, n, every):
        e = min(k + every, n)
        mean = lambda xs: sum(xs[k:e]) / (e - k)
        yield (e - 1) * 1000, mean(act["hr"]), mean(act["speed"]), mean(act["cadence"])


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("elf")
    ap.add_argument("--ticks", type=int, default=None,
                    help="number of ticks (default 10; with --fit, the whole activity)")
    ap.add_argument("--hr", default="70:150", help="lo:hi bpm, ramps over the run")
    ap.add_argument("--fit", default=None, help="replay a recorded .fit activity")
    ap.add_argument("--every", type=int, default=1,
                    help="tick every N seconds of activity time (default 1)")
    ap.add_argument("--press", default="", help="tick:BUTTON[,tick:BUTTON...]")
    ap.add_argument("--png", default=None)
    a = ap.parse_args(argv)

    if a.every < 1:
        ap.error("--every must be >= 1")
    presses: dict[int, list[str]] = {}
    for item in filter(None, a.press.split(",")):
        k, b = item.split(":")
        presses.setdefault(int(k), []).append(b.upper())

    if a.fit:
        act = load_fit(a.fit)
        ticks = list(fit_ticks(act, a.every))
        if a.ticks is not None:
            ticks = ticks[:a.ticks]
        print(f"replaying {os.path.basename(a.fit)}: {act['sport'] or 'activity'}, "
              f"{act['seconds']} s, {len(ticks)} ticks every {a.every} s")
    else:
        lo, hi = (float(v) for v in a.hr.split(":"))
        n = 10 if a.ticks is None else a.ticks
        ticks = [(i * a.every * 1000, lo + (hi - lo) * i / max(1, n - 1) + 3 * math.sin(i), 3.0, 0.0)
                 for i in range(n)]
    show = max(1, len(ticks) // 20)                    # keep long replays readable

    w = Watch(a.elf)
    try:
        t0 = time.perf_counter()
        info = w.hello()
        print(f"booted {info['app']!r}: {info['w']}x{info['h']} round={info['round']} proto v{info['proto']}")
        for i, (t_ms, hr, spd, cad) in enumerate(ticks):
            for b in presses.get(i, []):
                w.button(b, True)
                w.button(b, False)
            r = w.tick(t_ms, hr=hr, speed=spd, cadence=cad)
            if i % show == 0 or i == len(ticks) - 1:
                print(f"tick {i:4d}  t={t_ms / 1000:7.0f} s  hr={hr:5.1f}  redraw={r['frame_bytes']:6d} B")
        dt = time.perf_counter() - t0
        logs = w.logs
        if len(logs) > 40:                             # long replay: first and last lines
            logs = logs[:20] + [f"... {len(logs) - 40} more ..."] + logs[-20:]
        for line in logs:
            print(f"[fw] {line}")
        print(f"{len(ticks)} ticks in {dt:.2f} s")
        if a.png:
            w.png(a.png)
            print(f"wrote {a.png}")
    finally:
        w.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
