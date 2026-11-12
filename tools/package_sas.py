"""Build the SAS app packages: APP_SMS3.psu, APP_SMS3.zip and Simple-Media-System-kit.zip.

APP_SMS3 is the folder layout of Berion's package (SMS3.ELF, title.cfg, icon.sys and his
icon_listview / icon_copy / icon_delete icons, kept in ship/APP_SMS3/). The PSU records follow
Launchstation's scripts/package_apps.py: 512-byte root / "." / ".." / file records, file
payloads padded to 1024 bytes. The ZIP holds the same files under APP_SMS3/. The kit adds the
language packs, the SMB module licences and KIT-README.txt, laid out like the Launchstation and
RiptOPL kits (APPS/APP_SMS3/ with the PSU beside APPS/).

Usage: python3 tools/package_sas.py --elf bin/SMS-packed.elf --output dist \
           --version v3.0.0-rev2 --epoch <commit time>
"""
import argparse
import datetime
from pathlib import Path
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SHIP = ROOT / "ship"
FOLDER = "APP_SMS3"
ELF_NAME = "SMS3.ELF"
ICONS = ("icon.sys", "icon_listview.icn", "icon_copy.icn", "icon_delete.icn")
KIT_NAME = "Simple-Media-System-kit.zip"
ENTRY = struct.Struct("<HHI8sII8sI28s448s")

# Berion's title.cfg, with the version, date and repository filled in for each build.
TITLE_CFG = ("title=[APP] Simple Media System\r\nboot=" + ELF_NAME + "\r\nTitle=Simple Media System\r\n"
             "Version={version}\r\n\r\n"
             "Description=Media player which supporting various of codecs like in example DivX, MPEG, MP3 etc.\r\n"
             "Developer=Eugene Plotnikov, Ripto, El_isra, Krah Jolito, Berion\r\nGenre=Homebrew\r\n"
             "Release={release}\r\n\r\nPackage=BER-004\r\n"
             "Source=https://github.com/NathanNeurotic/Simple-Media-System\r\n")


def record(name, size, mode, date):
    encoded = name.encode("ascii")
    if len(encoded) > 31 or "/" in name or "\\" in name:
        raise ValueError("Invalid memory-card name: " + name)
    return ENTRY.pack(mode, 0, size, date, 0, 0, date, 0, bytes(28), encoded.ljust(448, b"\0"))


def make_psu(members, date):
    data = bytearray(record(FOLDER, len(members) + 2, 0x8027, date))
    data += record(".", 0, 0x8027, date) + record("..", 0, 0x8027, date)
    for name, payload in members.items():
        data += record(name, len(payload), 0x8017, date)
        data += payload + bytes((-len(payload)) % 1024)
    if len(data) >= 8 * 1024 * 1024:
        raise ValueError("App save exceeds an 8 MB memory card")
    return bytes(data)


def inspect_psu(data):
    """Read a PSU back into (folder, {name: bytes}); refuses anything make_psu would not write."""
    if len(data) < 1536:
        raise ValueError("Truncated PSU directory records")
    records = [ENTRY.unpack_from(data, offset) for offset in (0, 512, 1024)]

    def name(entry):
        return entry[-1].split(b"\0", 1)[0].decode("ascii")
    if any(r[0] != 0x8027 for r in records) or name(records[1]) != "." or name(records[2]) != "..":
        raise ValueError("Invalid PSU directory records")
    position, members = 1536, {}
    for _ in range(records[0][2] - 2):
        if position + 512 > len(data):
            raise ValueError("Truncated PSU file record")
        entry = ENTRY.unpack_from(data, position)
        key = name(entry)
        start, end = position + 512, position + 512 + entry[2]
        if entry[0] != 0x8017 or key in members or end > len(data):
            raise ValueError("Invalid PSU file record")
        members[key] = data[start:end]
        position = start + (entry[2] + 1023) // 1024 * 1024
    if position != len(data):
        raise ValueError("Unexpected trailing PSU bytes")
    return name(records[0]), members


def app_members(elf, version, instant):
    if len(elf) < 52 or elf[:7] != b"\x7fELF\x01\x01\x01" or struct.unpack_from("<H", elf, 18)[0] != 8:
        raise ValueError("Not a little-endian ELF32 MIPS executable")
    if not version or any(c in version for c in "\r\n"):
        raise ValueError("Expected a one-line version")
    icons = {name: (SHIP / FOLDER / name).read_bytes() for name in ICONS}
    icon_sys = icons["icon.sys"]
    if len(icon_sys) != 964 or icon_sys[:4] != b"PS2D":
        raise ValueError("ship/APP_SMS3/icon.sys is invalid")
    for offset, name in zip((0x104, 0x144, 0x184), ICONS[1:]):
        if icon_sys[offset:offset + 64].split(b"\0", 1)[0] != name.encode("ascii") or not icons[name]:
            raise ValueError("icon.sys does not reference " + name)
    cfg = TITLE_CFG.format(version=version, release=instant.strftime("%Y-%m-%d")).encode("ascii")
    return {ELF_NAME: elf, "title.cfg": cfg, **icons}


def kit_extras():
    """Language packs, SMB module licences and the kit README, as {kit path: bytes}."""
    extras = {}
    lang = ROOT / "docs" / "translation"
    for path in sorted(lang.glob("SMS.lng*")) + [lang / "TRANSLATING.txt"]:
        extras["LANGUAGES/" + path.name] = path.read_bytes()
    smb = ROOT / "iop" / "smbman-sms"
    for path in sorted(smb.glob("LICENSE-*")) + [smb / "NOTICE-DES.txt", smb / "README.md"]:
        extras["LICENSES/smbman/" + path.name] = path.read_bytes()
    readme = (SHIP / "KIT-README.txt").read_bytes()
    if b"APPS/APP_SMS3/" not in readme:
        raise ValueError("ship/KIT-README.txt does not describe the kit")
    extras["KIT-README.txt"] = readme
    if not any(name.endswith("/SMS.lng") for name in extras) or any(not data for data in extras.values()):
        raise ValueError("A kit file is missing or empty")
    return extras


def write_zip(path, files, timestamp):
    with zipfile.ZipFile(path, "w") as archive:
        for name, payload in files.items():
            info = zipfile.ZipInfo(name, timestamp)
            # The ELF is already packed; text compresses, so only it is stored.
            info.compress_type = zipfile.ZIP_STORED if name.endswith(".ELF") else zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, payload)
    with zipfile.ZipFile(path) as archive:
        if archive.testzip() is not None or {n: archive.read(n) for n in archive.namelist()} != files:
            raise ValueError("ZIP read-back differs: " + str(path))


def package(elf_path, output, version, epoch):
    instant = datetime.datetime.fromtimestamp(epoch, datetime.timezone.utc)
    if not 2000 <= instant.year <= 2099:
        raise ValueError("Commit date is outside the PS2 timestamp range")
    date = struct.pack("<BBBBBBH", 0, instant.second, instant.minute, instant.hour,
                       instant.day, instant.month, instant.year)
    timestamp = instant.timetuple()[:6]
    members = app_members(Path(elf_path).read_bytes(), version, instant)
    psu = make_psu(members, date)
    if inspect_psu(psu) != (FOLDER, members):
        raise ValueError("PSU read-back differs")
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    (output / (FOLDER + ".psu")).write_bytes(psu)
    write_zip(output / (FOLDER + ".zip"), {FOLDER + "/" + n: d for n, d in members.items()}, timestamp)
    kit = {"APPS/" + FOLDER + "/" + n: d for n, d in members.items()}
    kit[FOLDER + ".psu"] = psu
    kit.update(kit_extras())
    write_zip(output / KIT_NAME, kit, timestamp)
    print("Verified %s.psu (%d bytes), %s.zip and %s (%d files)" % (FOLDER, len(psu), FOLDER, KIT_NAME, len(kit)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--epoch", type=int, required=True)
    args = parser.parse_args()
    package(args.elf, args.output, args.version, args.epoch)
