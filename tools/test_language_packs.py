"""Shipped language packs must load, and the English template must match the player.

SMS_LocaleInit rejects a pack whose entry count differs from the built-in table
(s_SMStringDef in src/SMS_Locale.c), so every docs/translation/SMS.lng* is split
the way the loader splits it and counted. docs/translation/SMS.lng is what
translators start from, so it must equal the built-in English strings.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/SMS_Locale.c").read_text(encoding="latin-1")
strings = {m.group(1): m.group(2).encode("latin-1").decode("unicode_escape")
           for m in re.finditer(r'(\w+)\s*\[\s*\][^=;]*=\s*"((?:[^"\\]|\\.)*)"\s*;', source)}
table = source.split("s_SMStringDef[]", 1)[1].split("};", 1)[0]
builtin = [strings[name] for name in re.findall(r"\{[^{}]*,\s*(\w+)\s*\}", table)]
assert len(builtin) > 300, len(builtin)


def entries(data):
    """Split like SMS_LocaleInit: stop at an empty line, end of file or a full table."""
    out, pos = [], 0
    while True:
        end = pos
        while end < len(data) and data[end] not in b"\r\n":
            end += 1
        out.append(data[pos:end].decode("latin-1"))
        if end == pos or end == len(data) or len(out) == len(builtin):
            return out
        pos = end + 1
        if data[pos:pos + 1] == b"\n":
            pos += 1


packs = sorted((ROOT / "docs/translation").glob("SMS.lng*"))
assert len(packs) > 1, packs
for pack in packs:
    got = entries(pack.read_bytes())
    assert len(got) == len(builtin) and all(got), (pack.name, len(got), len(builtin))

template = entries((ROOT / "docs/translation/SMS.lng").read_bytes())
drift = [(i, t, b) for i, (t, b) in enumerate(zip(template, builtin)) if t != b]
assert not drift, drift
print("%d language packs have %d entries; English template matches the built-in table"
      % (len(packs), len(builtin)))

# ---- Device settings labels, in every pack -------------------------------------------------
# check_devmenu.py proves each row runs its own handler; this proves the WORDS a pack puts on
# those rows. Each label must name its own device and no other, no two may read the same, no
# Start row may read like an Autostart one, and none may be cut off or run into its on/off icon.
import check_devmenu as dev

CODEPAGE = {"BG": "cp1251", "RU": "cp1251", "UA": "cp1251",
            "CZ": "cp1250", "SK": "cp1250", "HU": "cp1250", "PL": "cp1250", "TR": "cp1254"}
FONT = {"cp1250": "g_1250", "cp1251": "g_1251", "cp1252": "g_1252", "cp1254": "g_1252"}  # no Turkish font
# label -> the device words it must show ( rows not listed must show none of NOUNS )
OWN = {"AUTOSTART_USB": "USB", "START_USB_NOW": "USB", "AUTOSTART_ILINK": "i.LINK",
       "START_ILINK": "i.LINK", "AUTOSTART_MX4SIO": "MX4SIO", "START_MX4SIO": "MX4SIO",
       "AUTOSTART_MMCE": "MMCE", "START_MMCE": "MMCE", "AUTOSTART_HDD": "HDD APA",
       "START_HDD_NOW": "HDD APA", "AUTOSTART_HDD_BDM": "HDD BDM", "START_HDD_BDM": "HDD BDM",
       "AUTOSTART_HOST": "HOST", "START_HOST": "HOST", "AUTOSTART_SMB": "SMB", "START_SMB": "SMB",
       "AUTOSTART_UDPFS": "UDPFS", "START_UDPFS": "UDPFS", "DISABLE_CDVD": "CD/DVD",
       "CDVD_SPEED": "CD/DVD", "PAIR_BT": "Bluetooth"}
NOUNS = ["USB", "i.LINK", "MX4SIO", "MMCE", "APA", "BDM", "HOST", "SMB", "UDPFS", "CD/DVD", "Bluetooth"]

header = (ROOT / "include/SMS_Locale.h").read_text(encoding="latin-1")
index = {m.group(1): int(m.group(2))
         for m in re.finditer(r"#define\s+STR_(\w+)\s+SMS_STRING\(\s*(\d+)\s*\)", header)}
code = dev.strip_comments((ROOT / "src/SMS_GUIMenuSMS.c").read_text(encoding="latin-1"))
text_rows = {name for bdm in (True, False)
             for _, name, is_text, _ in dev.audit(dev.preprocess(code, bdm), "")[2] if is_text}
labels = list(dev.ROWS) + list(dev.START_ROWS)
missing = [k for k in labels if k not in index]
assert not missing, ("no STR_ macro for", missing)

# Pixel widths as the PS2 measures them ( GSFont_WidthEx, SMS_GS_2.c ) on a 640-wide screen.
# Menu 520 wide ( _setup_dimensions ): a row's label is cut at 504 px; a row with a text value
# gives that value the right third, leaving 331; the on/off icon sits at 482, so 474 there.
# On-screen keyboard ( SMS_GUISMBrowser.c ): help line 528 px, keys 170 px, neither is cut.
font_src = (ROOT / "src/SMS_GSFont.c").read_text(encoding="latin-1")
fonts = {m.group(1): [int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+|\d+", m.group(2))]
         for m in re.finditer(r"unsigned char (g_\w+)\[[^]]*\][^=]*=\s*\{(.*?)\};", font_src, re.S)}


def width(raw, cpfont, dw):
    table = fonts["g_ASCII"][11:107] + fonts[cpfont][11:139]
    table[0] = table[ord(":") - 32]                       # GSFont_Init: space is as wide as ':'
    return sum(int(table[c - 32] * (32 + dw) / 32.0 + 0.5) for c in raw if c >= 32)


def names(noun, s):
    return re.search(r"(?<![\w.])" + re.escape(noun) + r"(?!\w)", s)


def words(s):
    for noun in NOUNS + ["HDD", "( ps2dev )"]:
        s = s.replace(noun, " ")
    return {w.lower() for w in re.findall(r"\w+", s)}


problems = []
for pack in packs:
    lang = pack.name[8:] or "template"
    cp = CODEPAGE.get(lang, "cp1252")
    raw = [e.encode("latin-1") for e in entries(pack.read_bytes())]
    say = {k: raw[index[k]].decode(cp) for k in labels}

    seen = {}
    for k in labels:
        seen.setdefault(say[k], []).append(k)
        need = OWN.get(k, "").split()
        if not all(names(n, say[k]) for n in need):
            problems.append("%s %s %r does not name %s" % (lang, k, say[k], " ".join(need)))
        problems += ["%s %s %r names %s, another device" % (lang, k, say[k], n)
                     for n in NOUNS if n not in need and names(n, say[k])]
        limit = 504 if k in dev.START_ROWS else 331 if k in text_rows else 474
        if width(raw[index[k]], FONT[cp], -7) > limit:
            problems.append("%s %s %r is %d px, wider than %d" % (lang, k, say[k],
                                                                 width(raw[index[k]], FONT[cp], -7), limit))
    problems += ["%s %s read the same: %r" % (lang, " and ".join(ks), s) for s, ks in seen.items() if len(ks) > 1]

    auto = [k for k in dev.ROWS if k.startswith("AUTOSTART_")]
    common = set.intersection(*(words(say[k]) for k in auto))
    if not common:
        problems.append("%s: the Autostart labels share no word, so a Start label cannot be told apart" % lang)
    for k in dev.START_ROWS:
        if k.startswith("START_") and common and common <= words(say[k]):
            problems.append("%s %s %r reads like an Autostart label" % (lang, k, say[k]))

    for k, limit in (("KB_SPACE", 170), ("KB_DEL", 170), ("KB_OK", 170), ("KB_HELP", 528)):
        if width(raw[index[k]], FONT[cp], -2) > limit:
            problems.append("%s %s %r is wider than %d px" % (lang, k, raw[index[k]].decode(cp), limit))

assert not problems, "\n" + "\n".join(problems)
print("%d device labels in %d packs: each names its own device, Start never reads as Autostart, "
      "all fit on screen" % (len(labels), len(packs)))
