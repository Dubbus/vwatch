"""End-to-end: build te_screen, boot it in QEMU, feed an HR ramp, check TE.

Run from the repo root:
    python3 tests/test_te_screen.py [activity.fit]
The optional FIT file (or $VWATCH_TEST_FIT) adds a replay check through
vwhost's --fit path; it is skipped when no file is given or fitdecode is
missing. Needs arm-none-eabi-gcc, make and qemu-system-arm on PATH.
"""
import math, os, re, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "host"))
from vwhost import Watch, fit_ticks, load_fit   # noqa: E402

EX = os.path.join(ROOT, "examples", "te_screen")
ELF = os.path.join(EX, "build", "te_screen.elf")
HR_MAX = 190.0                         # the example's default build
DKGRAY = 0x15
LOG = re.compile(r"te (\d+\.\d+) epoc (\d+\.\d+) s (\d+)")


def params() -> dict:
    txt = open(os.path.join(EX, "te_params.h")).read()
    return {k: float(v) for k, v in re.findall(r"#define TE_(\w+)\s+([-\d.e+]+)f", txt)}


def reference_te(hr_1hz, p) -> float:
    """The same model in plain Python (float64), for the C build to match."""
    a_up = 1 - math.exp(-5 / p["TAU_UP"])
    a_dn = 1 - math.exp(-5 / p["TAU_DN"])
    e = peak = 0.0
    for k in range(0, len(hr_1hz) // 5 * 5, 5):
        i = min(max((sum(hr_1hz[k:k + 5]) / 5 - p["HR_FLOOR"]) / (HR_MAX - p["HR_FLOOR"]), 0.0), 1.0)
        target = math.exp(p["K"] * i) - 1
        e += (target - e) * (a_up if target > e else a_dn)
        peak = max(peak, e)
    return min(max(p["A"] * math.log1p(p["B"] * peak), 0.0), 5.0)


def te_logs(w):
    return [(float(m[1]), float(m[2]), int(m[3])) for m in map(LOG.match, w.logs) if m]


def arc_pixel(w, deg, r=111):
    a = math.radians(deg)
    return w.fb[round(120 - r * math.cos(a))][round(120 + r * math.sin(a))]


def check_ramp():
    w = Watch(ELF)
    try:
        assert w.hello()["app"] == "te_screen"
        # 20 min ramp 100 -> 185 bpm, ticked every 5 s like vwhost --every 5
        n = 1200
        hr = [100 + 85 * t / (n - 1) for t in range(n)]
        redraws = 0
        for k in range(0, n, 5):
            r = w.tick((k + 4) * 1000, hr=sum(hr[k:k + 5]) / 5, speed=3.0)
            redraws += r["frame_bytes"] > 0
            if k == 0:     # boot frame is in now (it arrives after INFO); TE still 0
                assert arc_pixel(w, 230) == DKGRAY, "gauge should start empty"
        logs = te_logs(w)
        tes = [t for t, _, _ in logs]
        assert len(logs) == n // 5 and logs[-1][2] == n, logs[-1]
        assert all(b >= a for a, b in zip(tes, tes[1:])), "TE must never fall during a session"
        assert tes[0] < 0.1 and tes[-1] > tes[0] + 2.0, "TE should rise with the ramp"
        ref = reference_te(hr, params())
        assert abs(tes[-1] - ref) < 0.01, f"app TE {tes[-1]} vs reference {ref:.3f}"
        assert redraws == n // 5, "the elapsed time changes every tick, so every tick redraws"
        assert arc_pixel(w, 230) != DKGRAY, "gauge should fill once TE > 0"

        w.button("BACK"); w.button("BACK", False)
        w.tick((n + 4) * 1000, hr=60)
        assert "session reset" in w.logs and te_logs(w)[-1][0] == 0.0, w.logs[-3:]
        print(f"te_screen ramp: TE {tes[-1]:.2f} (reference {ref:.3f}), {redraws} redraws: OK")
    finally:
        w.close()


def check_fit(path):
    try:
        import fitdecode   # noqa: F401
    except ImportError:
        print("te_screen FIT replay: SKIPPED (fitdecode not installed)")
        return
    act = load_fit(path)
    w = Watch(ELF)
    try:
        w.hello()
        for t_ms, hr, spd, cad in fit_ticks(act, 5):
            w.tick(t_ms, hr=hr, speed=spd, cadence=cad)
        logs = te_logs(w)
        tes = [t for t, _, _ in logs]
        assert logs and logs[-1][2] == act["seconds"], (logs[-1:], act["seconds"])
        assert all(b >= a for a, b in zip(tes, tes[1:]))
        print(f"te_screen FIT replay: {act['seconds']} s, final TE {tes[-1]:.2f}: OK")
    finally:
        w.close()


def main():
    subprocess.run(["make", "-s", "-C", EX, "clean"], check=True, capture_output=True)
    subprocess.run(["make", "-s", "-C", EX], check=True, capture_output=True)
    check_ramp()
    fit = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("VWATCH_TEST_FIT")
    if fit:
        check_fit(fit)
    else:
        print("te_screen FIT replay: SKIPPED (pass a .fit path or set VWATCH_TEST_FIT)")
    print("te_screen: OK")


if __name__ == "__main__":
    main()
