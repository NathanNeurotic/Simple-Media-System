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
