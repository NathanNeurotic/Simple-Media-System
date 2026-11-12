#!/usr/bin/env python3
"""Cross-check the s_DevMenu rows in SMS_GUIMenuSMS.c: each label, the handler it runs, the
setting that handler changes and every line that repaints the row must belong together.

WHY THIS EXISTS
---------------
s_DevMenu used to be addressed by hardcoded row numbers in several unrelated places, so
reordering rows meant moving ALL of them in lockstep. That went wrong once: a reorder updated
the array and the status paint block but missed the `_switch_flag` numbers, so toggling an
autostart flipped the correct flag and painted the indicator on the WRONG row. It shipped and
had to be reverted.

The rows are now placed by name ( [ DEVROW_x ] = ... ) and written through the same names, so
the order lives in one place: the DEVROW numbers. This script enforces that, and checks every
label against the handler and setting it must have, so a reorder can only change the order.

A mismatch is not always cosmetic. m_IconRight is a union in practice: MENU_ITEM_TYPE_TEXT
rows hold a STRING POINTER, plain rows hold a GUICON_* id. Writing an id into a text row --
or a pointer into an icon row -- corrupts the redraw.

The file is compiled twice (BDM=1 and BDM=0) with two DIFFERENT layouts, so this script
evaluates #ifdef BDM / #ifndef BDM / #else / #endif and checks each build on its own.

WHAT IT CHECKS, per build
-------------------------
1. layout: every static row is placed by a DEVROW name, and the names number the rows
   0..DEVROW_LAST with no gap and no repeat. A repeated designator silently replaces a row;
   a gap leaves a row with no label, which crashes the redraw.
2. label -> handler: each label runs the handler listed for it in ROWS below.
3. handler -> row: a handler writes only its own row, always by name, never by number.
4. row -> setting: every write to a row ( its handler, the status block in _device_handler,
   the network-mode repaint in _switch_netmode ) uses the setting listed for that row's label,
   and no other row's setting.
5. type agreement: GUICON_* only into plain rows, string pointers only into TEXT rows.
6. coverage: every row except Network settings is painted by the status block.
7. Start rows: each label is paired with its own handler, and that handler starts the device
   the label names.
8. size: lSize starts at DEVROW_LAST, and the static rows plus every Start row fit the array.

Run from anywhere:  python tools/check_devmenu.py [path/to/SMS_GUIMenuSMS.c]
Exit status 0 when consistent, 1 otherwise, so it can gate a build.
"""

import io
import os
import re
import sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "SMS_GUIMenuSMS.c")

# Static rows: label ( STR_x ) -> ( handler, the setting the row shows and changes ). None means
# the row opens a submenu ( Network settings ) or shows a text that no other row uses.
ROWS = {
    "AUTOSTART_USB":       ("_autousb_handler",    "SMS_DF_AUTO_USB"),
    "AUTOSTART_ILINK":     ("_autoilink_handler",  "SMS_DF_AUTO_ILINK"),
    "AUTOSTART_MX4SIO":    ("_automx4sio_handler", "SMS_DF_AUTO_MX4SIO"),
    "AUTOSTART_MMCE":      ("_automce_handler",    "SMS_DF_AUTO_MMCE"),
    "AUTOSTART_HDD":       ("_autohdd_handler",    "SMS_DF_AUTO_HDD"),       # HDD APA
    "AUTOSTART_HDD_BDM":   ("_autoata_handler",    "SMS_DF_AUTO_ATA"),
    "AUTOSTART_NETWORK":   ("_autonet_handler",    "SMS_DF_AUTO_NET"),       # non-BDM build only
    "NETWORK_SETTINGS":    ("_network_handler",    None),
    "AUTOSTART_HOST":      ("_autohost_handler",   "SMS_NETMODE_HOST"),
    "AUTOSTART_SMB":       ("_autosmb_handler",    "SMS_NETMODE_SMB"),
    "AUTOSTART_UDPFS":     ("_autoudpfs_handler",  "SMS_NETMODE_UDPFS"),
    "DISABLE_CDVD":        ("_cdvd_handler",       "SMS_DF_CDVD"),
    "CDVD_SPEED":          ("_cdvd_spd_handler",   "s_Speeds"),
    "CONTROLLER_SLOT2":    ("_cntslot_handler",    None),
    "DIRECTIONAL_BUTTONS": ("_dirbtn_handler",     "SMS_BF_DIRB"),
}
SETTINGS = {setting for _, setting in ROWS.values() if setting}

# Start rows, appended after the static rows at run time: label -> ( handler, the device
# starters it must call, and nothing else ). SMB and HOST share SMS_IOPStartNet; the SMS_DF_SMB
# bit set just before the call picks the stack, so that is checked too ( START_EXTRA ).
START_ROWS = {
    "START_USB_NOW": ("_startusb_handler",    {"SMS_IOPStartUSB"}),
    "START_ILINK":   ("_startilink_handler",  {"SMS_IOPStartILINK"}),
    "START_MX4SIO":  ("_startmx4sio_handler", {"SMS_IOPStartMX4SIO"}),
    "START_MMCE":    ("_startmce_handler",    {"SMS_IOPStartMMCE"}),
    "START_HDD_NOW": ("_starthdd_handler",    {"SMS_IOPStartHDD"}),
    "START_HDD_BDM": ("_startata_handler",    {"SMS_IOPStartATA"}),
    "START_UDPFS":   ("_startudpfs_handler",  {"SMS_IOPStartUDPFS"}),
    "START_SMB":     ("_startsmb_handler",    {"SMS_IOPStartNet"}),
    "START_HOST":    ("_starthost_handler",   {"SMS_IOPStartNet"}),
    "PAIR_BT":       ("_pairbt_handler",      set()),
    "REFRESH_CONN":  ("_refresh_handler",     set()),
}
START_EXTRA = {
    "START_SMB":  r"\|=\s*SMS_DF_SMB\b",
    "START_HOST": r"&=\s*~\s*SMS_DF_SMB\b",
    "PAIR_BT":    r"\bds34usb_set_bdaddr\b",
}


def strip_comments(text):
    """Blank out C comments, keeping their newlines, so prose never reads as code."""
    def blank(m):
        tok = m.group(0)
        return tok if tok[0] in "\"'" else re.sub(r"[^\n]", " ", tok)
    return re.sub(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|/\*.*?\*/|//[^\n]*', blank, text, flags=re.S)


def preprocess(text, bdm):
    """Resolve #ifdef BDM / #ifndef BDM / #else / #endif for one build.

    Blank out excluded lines rather than deleting them so reported line numbers stay true.
    Unrelated #ifdefs are left alone; only the BDM condition is evaluated.
    """
    out, stack = [], []          # stack of (is_bdm_cond, currently_emitting)
    for line in text.splitlines():
        s = line.strip()
        m = re.match(r"#\s*if(n?)def\s+BDM\b", s)
        if m:
            stack.append((True, bdm if not m.group(1) else not bdm))
            out.append("")
            continue
        if re.match(r"#\s*if(n?)def\b", s) or re.match(r"#\s*if\b", s):
            stack.append((False, True))
            out.append(line)
            continue
        if re.match(r"#\s*else\b", s) and stack:
            is_bdm, emit = stack[-1]
            stack[-1] = (is_bdm, (not emit) if is_bdm else emit)
            out.append("" if is_bdm else line)
            continue
        if re.match(r"#\s*endif\b", s) and stack:
            is_bdm, _ = stack.pop()
            out.append("" if is_bdm else line)
            continue
        out.append(line if all(e for _, e in stack) else "")
    return "\n".join(out)


def parse_row_consts(text):
    """Return {DEVROW_x: n} for the visible build ( `#define` and `# define` alike )."""
    return {m.group(1): int(m.group(2))
            for m in re.finditer(r"#\s*define\s+(DEVROW_\w+)\s+(\d+)", text)}


def settings_in(text):
    """The row settings ( SETTINGS ) a piece of code mentions."""
    return {s for s in SETTINGS if re.search(r"\b" + re.escape(s) + r"\b", text)}


def parse_array(text, consts):
    """Return ( rows, array size, errors ) for the visible s_DevMenu.

    rows is [ ( index, label, is_text_row, handler ) ] sorted by index. Rows are placed the way
    C places them: a designator sets the index, a positional row takes the next one.
    """
    m = re.search(r"static GUIMenuItem s_DevMenu\[\s*(\d+)\s*\][^=]*=\s*\{", text)
    if not m:
        return [], 0, ["could not find s_DevMenu"]
    size, body = int(m.group(1)), text[m.end():]
    body = body[: body.index("\n};")]
    rows, errs, used, nxt = {}, [], set(), 0
    for line in body.splitlines():
        line = line.strip()
        dm = re.match(r"\[\s*(\w+)\s*\]\s*=\s*(\{.*)", line)
        if dm:
            name, line = dm.group(1), dm.group(2)
            if name.isdigit():
                errs.append(f"a row is placed by number [ {name} ]; place it by its DEVROW name")
                idx = int(name)
            elif name in consts:
                idx = consts[name]
                used.add(name)
            else:
                errs.append(f"a row is placed by {name}, which is not defined in this build")
                continue
        elif line.startswith("{"):
            idx = nxt
        else:
            continue
        rm = re.match(r"\{\s*([A-Za-z_0-9]*)\s*,\s*&STR_(\w+)\s*,[^,]*,[^,]*,\s*([A-Za-z_0-9]+)", line)
        if not rm:
            errs.append(f"could not read row: {line}")
            continue
        if not dm:
            errs.append(f"row {idx} ( {rm.group(2)} ) is placed by position; "
                        f"place it by its DEVROW name")
        if idx in rows:
            errs.append(f"row {idx} is placed twice ( {rows[idx][0]}, then {rm.group(2)} ): "
                        f"C keeps only the last")
        rows[idx] = (rm.group(2), rm.group(1) == "MENU_ITEM_TYPE_TEXT", rm.group(3))
        nxt = idx + 1
    if not rows:
        return [], size, errs + ["s_DevMenu has no rows"]
    top = max(rows)
    for idx in range(top + 1):
        if idx not in rows:
            errs.append(f"row {idx} is empty: a row with no label crashes the redraw")
    for name, value in sorted(consts.items()):
        if name != "DEVROW_LAST" and name not in used:
            errs.append(f"{name} ( {value} ) places no row")
    if consts.get("DEVROW_LAST") != top:
        errs.append(f"DEVROW_LAST is {consts.get('DEVROW_LAST')} but the last row is {top}")
    return [(i, *rows[i]) for i in sorted(rows)], size, errs


def func_spans(text):
    """Map function name -> ( start, end ) of its body, for the visible build."""
    spans = {}
    for m in re.finditer(r"static void (_\w+)\s*\(\s*GUIMenu\s*\*[^)]*\)\s*\{", text):
        name, depth, i = m.group(1), 1, m.end()
        while i < len(text) and depth:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
            i += 1
        spans[name] = (m.end(), i)
    return spans


def is_pointer_rhs(rhs):
    """True when the value assigned to m_IconRight is a string pointer, not a GUICON id."""
    if "GUICON_" in rhs:
        return False
    return bool(re.search(r"&STR_|s_Speeds|&sl_|\)\s*l[A-Z]\w*", rhs)) or rhs.strip().startswith("lStr")


def audit(text, label):
    consts = parse_row_consts(text)
    rows, size, layout_errs = parse_array(text, consts)
    errs = [f"{label}: {e}" for e in layout_errs]
    if not rows:
        return errs, 0, rows, []
    by_index = {i: r for i, *r in rows}
    handler_rows = {}
    for i, nm, t, h in rows:
        handler_rows.setdefault(h, []).append(i)
        expected = ROWS.get(nm)
        if expected is None:
            errs.append(f"{label}: row {i} shows STR_{nm}, which has no entry in ROWS "
                        f"( add its handler and setting )")
        elif expected[0] != h:
            errs.append(f"{label}: row {i} shows STR_{nm} but runs {h}; it must run {expected[0]}")
    for h, owned in handler_rows.items():
        if len(owned) > 1:
            errs.append(f"{label}: {h} is bound to rows {owned}")

    def row_setting(idx):
        return ROWS.get(by_index[idx][0], (None, None))[1]

    def resolve(tok, where):
        if tok.isdigit():
            errs.append(f"{label}: {where} uses row number {tok}; use its DEVROW name")
            return int(tok)
        if tok not in consts:
            errs.append(f"{label}: {where} uses {tok}, which is not defined in this build")
            return -1
        return consts[tok]

    def check_write(idx, code, where, kind_rhs=None):
        """Range, type and setting checks for one write to row idx."""
        if idx not in by_index:
            errs.append(f"{label}: {where} writes row {idx}, which does not exist")
            return
        nm, is_text, _ = by_index[idx]
        want, got = row_setting(idx), settings_in(code)
        if got != ({want} if want else set()):
            errs.append(f"{label}: {where} writes row {idx} ( {nm} ) from {sorted(got) or 'no setting'}"
                        f"; that row's setting is {want or 'none'}")
        if kind_rhs is not None and is_pointer_rhs(kind_rhs) != is_text:
            kind = "pointer" if is_pointer_rhs(kind_rhs) else "GUICON"
            errs.append(f"{label}: {where} writes a {kind} into "
                        f"{'TEXT' if is_text else 'plain'} row {idx} ( {nm} ) -- crash risk")

    checked = 0
    spans = func_spans(text)

    for hname, (s, e) in spans.items():
        if hname not in handler_rows:
            continue
        owned, body = handler_rows[hname], text[s:e]
        idx0 = owned[0]
        want = row_setting(idx0)
        got = settings_in(body)
        if got - ({want} if want else set()):
            errs.append(f"{label}: {hname} ( row {idx0}, {by_index[idx0][0]} ) also touches "
                        f"{sorted(got - {want})}")
        if want and want not in got:
            errs.append(f"{label}: {hname} ( row {idx0}, {by_index[idx0][0]} ) never touches {want}")

        for m in re.finditer(r"_switch_flag\s*\(\s*apMenu\s*,\s*(\w+)\s*,([^;]*)\)\s*;", body):
            idx = resolve(m.group(1), f"{hname} _switch_flag")
            checked += 1
            if idx not in owned:
                errs.append(f"{label}: {hname} sits at row {idx0} but _switch_flag writes row {idx}")
            if idx in by_index and by_index[idx][1]:
                errs.append(f"{label}: {hname} _switch_flag puts a GUICON into TEXT row "
                            f"{idx} ( {by_index[idx][0]} ) -- crash risk")
            check_write(idx, m.group(2), f"{hname} _switch_flag")

        for m in re.finditer(r"s_DevMenu\[\s*(\w+)\s*\]\.m_IconRight\s*=\s*([^;]+);", body):
            idx, rhs = resolve(m.group(1), hname), m.group(2)
            checked += 1
            if idx not in owned:
                errs.append(f"{label}: {hname} sits at row {idx0} but writes row {idx}")
            check_write(idx, rhs, hname, rhs)

        for m in re.finditer(r"_switch_netmode\s*\(\s*apMenu\s*,\s*(\w+)\s*\)", body):
            checked += 1
            if settings_in(m.group(1)) != ({want} if want else set()):
                errs.append(f"{label}: {hname} ( row {idx0}, {by_index[idx0][0]} ) selects "
                            f"{m.group(1)}; that row's setting is {want}")

    # status block: what every row shows when the menu opens
    status_painted, start_rows, starts = set(), [], 0
    if "_device_handler" in spans:
        s, e = spans["_device_handler"]
        body = text[s:e]
        for m in re.finditer(r"s_DevMenu\[\s*(\w+)\s*\]\.m_IconRight\s*=\s*([^;]+);", body):
            idx, rhs = resolve(m.group(1), "status block"), m.group(2)
            status_painted.add(idx)
            checked += 1
            check_write(idx, rhs, "status block", rhs)

        lm = re.search(r"unsigned int\s+lSize\s*=\s*(\w+)\s*;", body)
        if not lm:
            errs.append(f"{label}: lSize's starting value not found in _device_handler")
        elif resolve(lm.group(1), "lSize") != rows[-1][0]:
            errs.append(f"{label}: lSize starts at {lm.group(1)}, not at the last static row "
                        f"{rows[-1][0]}: Start rows would overwrite or skip a row")

        starts = len(re.findall(r"\+\+\s*lSize", body))
        start_rows = re.findall(
            r"s_DevMenu\[\s*\+\+\s*lSize\s*\]\.m_pOptionName\s*=\s*&STR_(\w+)\s*;\s*"
            r"s_DevMenu\[\s*lSize\s*\]\.Handler\s*=\s*(\w+)\s*;", body)
        if starts != len(start_rows):
            errs.append(f"{label}: {starts} Start rows are added but only {len(start_rows)} "
                        f"set a label and then a handler")
        if rows[-1][0] + 1 + starts > size:
            errs.append(f"{label}: {rows[-1][0] + 1} static rows plus {starts} Start rows "
                        f"do not fit s_DevMenu[ {size} ]")
    else:
        errs.append(f"{label}: _device_handler not found")

    for nm, h in start_rows:
        checked += 1
        expected = START_ROWS.get(nm)
        if expected is None:
            errs.append(f"{label}: Start row STR_{nm} has no entry in START_ROWS")
            continue
        if expected[0] != h:
            errs.append(f"{label}: Start row STR_{nm} runs {h}; it must run {expected[0]}")
            continue
        if h not in spans:
            errs.append(f"{label}: Start row STR_{nm} runs {h}, which is not in this build")
            continue
        body = text[spans[h][0]:spans[h][1]]
        got = set(re.findall(r"\bSMS_IOPStart\w+", body))
        if got != expected[1]:
            errs.append(f"{label}: {h} ( STR_{nm} ) starts {sorted(got) or 'nothing'}; "
                        f"it must start {sorted(expected[1]) or 'nothing'}")
        extra = START_EXTRA.get(nm)
        if extra and not re.search(extra, body):
            errs.append(f"{label}: {h} ( STR_{nm} ) lacks {extra}")
        if settings_in(body):
            errs.append(f"{label}: {h} ( STR_{nm} ) touches row settings {sorted(settings_in(body))}")

    # _switch_netmode repaints the three network rows as a group through lpState->m_pItems[].
    net_rows = {i for i, nm, _, _ in rows if (ROWS.get(nm, (None, ""))[1] or "").startswith("SMS_NETMODE_")}
    group_painted = set()
    for m in re.finditer(r"m_pItems\[\s*(\w+)\s*\]\.m_IconRight\s*=\s*([^;]+);", text):
        if not m.group(1).startswith("DEVROW_"):
            continue                      # another menu's own rows
        idx = resolve(m.group(1), "_switch_netmode")
        group_painted.add(idx)
        checked += 1
        check_write(idx, m.group(2), "_switch_netmode", m.group(2))
    if group_painted != net_rows:
        errs.append(f"{label}: _switch_netmode repaints rows {sorted(group_painted)}, "
                    f"the network-mode rows are {sorted(net_rows)}")

    for i, nm, t, h in rows:
        if i not in status_painted and h != "_network_handler":
            errs.append(f"{label}: row {i} ( {nm} ) is never painted by the status block")

    return errs, checked, rows, start_rows


def main(path=SRC):
    raw = strip_comments(io.open(path, encoding="latin-1").read())
    all_errs, grand = [], 0

    for label, bdm in (("BDM=1", True), ("BDM=0", False)):
        text = preprocess(raw, bdm)
        errs, checked, rows, start_rows = audit(text, label)
        print(f"\n=== {label} : {len(rows)} static rows, {checked} writes checked ===")
        for i, nm, t, h in rows:
            setting = ROWS.get(nm, ("", ""))[1] or "-"
            print(f"  {i:2d}  {'TEXT' if t else 'icon'}  {nm:<20} {h:<20} {setting}")
        for nm, h in start_rows:
            print(f"   +  Start {nm:<20} {h}")
        all_errs += errs
        grand += checked

    print(f"\ncross-checked {grand} writes across both builds")
    if all_errs:
        print("\nINCONSISTENT:")
        for e in all_errs:
            print("  -", e)
        return 1
    print("CONSISTENT: every label runs its own handler, every write uses its row's name and "
          "setting, every row painted")
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:2]))
