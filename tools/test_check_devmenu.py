#!/usr/bin/env python3
"""Prove tools/check_devmenu.py catches every way the device menu has been, or could be, mislabelled.

Each case breaks one link between a row's label, its handler, its setting and its repaint in a
copy of SMS_GUIMenuSMS.c, and the checker must reject that copy. The real file must pass.
"""

import contextlib
import io
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_devmenu  # noqa: E402

CASES = [
    ("two rows share a number",
     "# define DEVROW_AUTO_ILINK   1", "# define DEVROW_AUTO_ILINK   0",
     ["row 0 is placed twice", "row 1 is empty"]),
    ("two labels swapped",
     "&STR_AUTOSTART_MX4SIO,    0, 0, _automx4sio_handler", "&STR_AUTOSTART_MMCE,      0, 0, _automx4sio_handler",
     ["shows STR_AUTOSTART_MMCE but runs _automx4sio_handler"]),
    ("toggle changes another row's setting",
     "DEVROW_AUTO_USB, &g_Config.m_NetworkFlags, SMS_DF_AUTO_USB", "DEVROW_AUTO_USB, &g_Config.m_NetworkFlags, SMS_DF_AUTO_HDD",
     ["_autousb_handler _switch_flag writes row 0 ( AUTOSTART_USB ) from ['SMS_DF_AUTO_HDD']"]),
    ("toggle repaints another row",
     "_switch_flag ( apMenu, DEVROW_AUTO_MX4SIO,", "_switch_flag ( apMenu, DEVROW_AUTO_MMCE,",
     ["_automx4sio_handler sits at row 2 but _switch_flag writes row 3"]),
    ("status block paints a row from another setting",
     "s_DevMenu[ DEVROW_AUTO_ILINK  ].m_IconRight = g_Config.m_NetworkFlags & SMS_DF_AUTO_ILINK ",
     "s_DevMenu[ DEVROW_AUTO_ILINK  ].m_IconRight = g_Config.m_NetworkFlags & SMS_DF_AUTO_MMCE  ",
     ["status block writes row 1 ( AUTOSTART_ILINK ) from ['SMS_DF_AUTO_MMCE']"]),
    ("a row number instead of its name",
     "_switch_flag ( apMenu, DEVROW_CDVD,", "_switch_flag ( apMenu, 10,",
     ["_cdvd_handler _switch_flag uses row number 10"]),
    ("network-mode repaint on the wrong row",
     "m_pItems[ DEVROW_SMB   ].m_IconRight = lMode == SMS_NETMODE_SMB ",
     "m_pItems[ DEVROW_UDPFS ].m_IconRight = lMode == SMS_NETMODE_SMB ",
     ["_switch_netmode writes row 9 ( AUTOSTART_UDPFS ) from ['SMS_NETMODE_SMB']"]),
    ("network row selects another mode",
     "_switch_netmode ( apMenu, SMS_NETMODE_HOST );", "_switch_netmode ( apMenu, SMS_NETMODE_SMB );",
     ["_autohost_handler ( row 7, AUTOSTART_HOST ) selects SMS_NETMODE_SMB"]),
    ("Start row runs another handler",
     "&STR_START_MX4SIO;\n  s_DevMenu[   lSize ].Handler       = _startmx4sio_handler;",
     "&STR_START_MX4SIO;\n  s_DevMenu[   lSize ].Handler       = _startmce_handler;",
     ["Start row STR_START_MX4SIO runs _startmce_handler"]),
    ("Start handler starts another device",
     "_start_device ( apMenu, SMS_IOPStartILINK );", "_start_device ( apMenu, SMS_IOPStartUSB );",
     ["_startilink_handler ( STR_START_ILINK ) starts ['SMS_IOPStartUSB']"]),
    ("Start SMB starts HOST",
     "g_Config.m_NetworkFlags |= SMS_DF_SMB;\n _start_device", "g_Config.m_NetworkFlags &= ~SMS_DF_SMB;\n _start_device",
     ["_startsmb_handler ( STR_START_SMB ) lacks"]),
    ("Start rows begin one row early",
     "unsigned int  lSize   = DEVROW_LAST;", "unsigned int  lSize   = DEVROW_CNTSLOT;",
     ["lSize starts at DEVROW_CNTSLOT"]),
    ("a row is never painted",
     " s_DevMenu[ DEVROW_AUTO_MMCE   ].m_IconRight = g_Config.m_NetworkFlags & SMS_DF_AUTO_MMCE   ? GUICON_ON : GUICON_OFF;\n", "",
     ["row 3 ( AUTOSTART_MMCE ) is never painted"]),
    ("a text row turned into an icon row",
     "[ DEVROW_CDVD_SPEED  ] = { MENU_ITEM_TYPE_TEXT, &STR_CDVD_SPEED,          0, 0, _cdvd_spd_handler,   0, 0 }",
     "[ DEVROW_CDVD_SPEED  ] = {                   0, &STR_CDVD_SPEED,          0, 0, _cdvd_spd_handler,   0, 0 }",
     ["writes a pointer into plain row 11 ( CDVD_SPEED )"]),
    ("non-BDM rows share a number",
     "# define DEVROW_AUTO_USB     3", "# define DEVROW_AUTO_USB     2",
     ["BDM=0: row 2 is placed twice", "BDM=0: row 3 is empty"]),
]


def run(text, folder, name):
    path = os.path.join(folder, name)
    with io.open(path, "w", encoding="latin-1", newline="") as f:
        f.write(text)
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        status = check_devmenu.main(path)
    return status, out.getvalue()


def main():
    source = io.open(check_devmenu.SRC, encoding="latin-1", newline="").read()
    failures = []
    with tempfile.TemporaryDirectory() as folder:
        status, out = run(source, folder, "real.c")
        if status != 0:
            failures.append("the real SMS_GUIMenuSMS.c fails the check:\n" + out)
        for n, (what, old, new, expect) in enumerate(CASES):
            count = source.count(old)
            if count != 1:
                failures.append(f"{what}: the text to break occurs {count} times, not once")
                continue
            status, out = run(source.replace(old, new), folder, f"case{n}.c")
            missing = [e for e in expect if e not in out]
            if status != 1 or missing:
                failures.append(f"{what}: exit {status}, missing {missing}\n{out}")
            else:
                print(f"caught: {what}")
    if failures:
        print("\nFAILED:")
        for f in failures:
            print(" -", f)
        return 1
    print(f"\nall {len(CASES)} mislabelling cases are caught, and the real file passes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
