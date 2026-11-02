"""Every per-mode display default must be reachable from the Display menu.

SMS_Config.c seeds m_DispWH[ mode ] = { width, height } and SMS_GUIMenuSMS.c
clamps the Display width / height handlers to per-mode [ min, max ] tables.
If a default lies outside its range, the first X/O press snaps it to the
bound and the shipped default can never be selected again. That happened
for 720p width: default 1216, ceiling 1190 (inherited from 2.9 rev4).

This reads the actual source tables, so it fails if either side drifts.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
config = (root / "src/SMS_Config.c").read_text()
menu = (root / "src/SMS_GUIMenuSMS.c").read_text()

MODES = ("NTSC", "PAL", "DTV480p", "DTV576p", "DTV720p", "DTV1080i", "VESA60Hz", "VESA75Hz")

defaults = {}
for mode, axis, value in re.findall(r"g_Config\.m_DispWH\[\s*(\d)\s*\]\[\s*(\d)\s*\]\s*=\s*(\d+)\s*;", config):
    defaults[(int(mode), int(axis))] = int(value)
assert len(defaults) == 16, f"expected 16 m_DispWH defaults, found {len(defaults)}"


def tables(handler):
    start = menu.index(f"static void {handler} (")
    end = menu.index(f"}}  /* end {handler} */", start)
    body = menu[start:end]
    found = {}
    for name in ("s_lMin", "s_lMax"):
        match = re.search(name + r"\[\s*8\s*\][^=]*=\s*\{([^}]*)\}", body)
        assert match, f"{name} not found in {handler}"
        found[name] = [int(v) for v in match.group(1).split(",")]
        assert len(found[name]) == 8, f"{handler} {name} must have 8 entries"
    return found["s_lMin"], found["s_lMax"]


failures = []
for axis, handler in ((0, "_dispw_handler"), (1, "_disph_handler")):
    low, high = tables(handler)
    for mode in range(8):
        value = defaults[(mode, axis)]
        if low[mode] > high[mode]:
            failures.append(f"{handler} {MODES[mode]}: min {low[mode]} > max {high[mode]}")
        if not low[mode] <= value <= high[mode]:
            failures.append(f"{handler} {MODES[mode]}: default {value} outside [{low[mode]}, {high[mode]}]")

assert not failures, "\n".join(failures)
print("display limits: all 16 defaults are inside their menu ranges")
