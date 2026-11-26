"""Convert a recorded FIT activity to a .vwr file for replay in vwatch.

A .vwr is a tiny text format: a header of key/value lines, then one
"t_s hr speed_mps cadence_spm" row per second. It carries the watch's own
computed labels (Training Effect, VO2max) so the VM can show ground truth
beside your firmware's live output. The C++ app needs no FIT parser.

.vwr files contain personal heart-rate data; keep them out of public repos.

  python3 host/fit_to_vwr.py run.fit -o run.vwr
"""
from __future__ import annotations
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vwhost import load_fit   # noqa: E402


def garmin_labels(path: str) -> dict:
    """Pull the watch's own computed values from the session + metrics messages."""
    import fitdecode
    out: dict[str, object] = {}
    with fitdecode.FitReader(path) as fit:
        for fr in fit:
            if fr.frame_type != fitdecode.FIT_FRAME_DATA:
                continue
            if fr.name == "session":
                g = lambda k: fr.get_value(k, fallback=None)
                if g("total_training_effect") is not None:
                    out["garmin_te"] = round(float(g("total_training_effect")), 1)
                if g("total_anaerobic_training_effect") is not None:
                    out["garmin_te_anaerobic"] = round(float(g("total_anaerobic_training_effect")), 1)
                if g("start_time") is not None:
                    out["name"] = str(g("start_time"))
            elif fr.name == "time_in_zone" and "hrmax" not in out:
                v = fr.get_value("max_heart_rate", fallback=None)
                if v:
                    out["hrmax"] = int(v)
            elif fr.name == "unknown_140":                 # Garmin's per-activity EPOC (x10)
                v = {x.name: x.value for x in fr.fields}.get("unknown_9")
                if v:
                    out["garmin_epoc"] = round(v / 10.0, 1)
    return out


def write_vwr(fit_path: str, out_path: str) -> dict:
    act = load_fit(fit_path)
    meta = {"sport": act["sport"] or "unknown", "seconds": act["seconds"]}
    meta.update(garmin_labels(fit_path))
    meta.setdefault("name", os.path.basename(fit_path))
    meta.setdefault("hrmax", 190)

    hr, sp, cad = act["hr"], act["speed"], act["cadence"]
    with open(out_path, "w") as f:
        f.write("# vwatch run v1\n")
        for key in ("name", "sport", "hrmax", "garmin_te", "garmin_te_anaerobic", "garmin_epoc"):
            if key in meta:
                f.write(f"{key} {meta[key]}\n")
        f.write("# t_s hr speed_mps cadence_spm\n")
        for i in range(act["seconds"]):
            f.write(f"{i} {hr[i]:.0f} {sp[i]:.2f} {cad[i]:.0f}\n")
    return meta


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("fit")
    ap.add_argument("-o", "--out", help="output .vwr (default: alongside the FIT file)")
    a = ap.parse_args(argv)
    out = a.out or os.path.splitext(a.fit)[0] + ".vwr"
    meta = write_vwr(a.fit, out)
    mins = meta["seconds"] / 60
    extra = f", Garmin TE {meta['garmin_te']}" if "garmin_te" in meta else ""
    print(f"wrote {out}: {meta['sport']}, {mins:.0f} min{extra}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
