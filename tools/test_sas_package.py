"""Build the SAS packages from a stand-in ELF and check all three agree file for file."""
import struct
import sys
import tempfile
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import package_sas as sas  # noqa: E402

# Smallest header package_sas accepts: ELF32, little-endian, version 1, e_machine 8 (MIPS).
elf = bytearray(b"\x7fELF\x01\x01\x01" + bytes(45) + b"payload" * 300)
struct.pack_into("<H", elf, 18, 8)
elf = bytes(elf)

with tempfile.TemporaryDirectory(prefix="sms-sas-") as td:
    tmp = Path(td)
    (tmp / "SMS-packed.elf").write_bytes(elf)
    sas.package(tmp / "SMS-packed.elf", tmp / "dist", "Build 1234567", 1762905600)  # 2025-11-12 00:00 UTC
    dist = tmp / "dist"

    folder, members = sas.inspect_psu((dist / "APP_SMS3.psu").read_bytes())
    assert folder == "APP_SMS3", folder
    assert sorted(members) == sorted(("SMS3.ELF", "title.cfg") + sas.ICONS), sorted(members)
    assert members["SMS3.ELF"] == elf
    for name in sas.ICONS:
        assert members[name] == (sas.SHIP / "APP_SMS3" / name).read_bytes(), name
    cfg = members["title.cfg"].decode("ascii")
    assert cfg.startswith("title=[APP] Simple Media System\r\nboot=SMS3.ELF\r\n"), cfg
    assert "\r\nVersion=Build 1234567\r\n" in cfg and "\r\nRelease=2025-11-12\r\n" in cfg, cfg
    assert "Berion" in cfg and "Package=BER-004" in cfg, cfg

    with zipfile.ZipFile(dist / "APP_SMS3.zip") as archive:
        zipped = {name: archive.read(name) for name in archive.namelist()}
    assert zipped == {"APP_SMS3/" + n: d for n, d in members.items()}, sorted(zipped)

    with zipfile.ZipFile(dist / sas.KIT_NAME) as archive:
        kit = {name: archive.read(name) for name in archive.namelist()}
        assert archive.getinfo("APPS/APP_SMS3/SMS3.ELF").compress_type == zipfile.ZIP_STORED
    for name, data in members.items():
        assert kit["APPS/APP_SMS3/" + name] == data, name
    assert kit["APP_SMS3.psu"] == (dist / "APP_SMS3.psu").read_bytes()
    assert "LANGUAGES/SMS.lng" in kit and "LANGUAGES/TRANSLATING.txt" in kit
    assert len([n for n in kit if n.startswith("LANGUAGES/SMS.lng_")]) >= 10
    assert "LICENSES/smbman/NOTICE-DES.txt" in kit and "KIT-README.txt" in kit
    assert not any(n.lower().endswith(("bios.bin", ".dat", "smspath.txt")) for n in kit), sorted(kit)

    # The PSU reader refuses what make_psu would never write.
    psu = bytearray((dist / "APP_SMS3.psu").read_bytes())
    for corrupt in (psu[:-1], psu + b"\0", psu[:512 * 3]):
        try:
            sas.inspect_psu(bytes(corrupt))
        except ValueError:
            continue
        raise AssertionError("inspect_psu accepted a damaged PSU")
    for bad in (b"MZ" + elf[2:], elf[:40]):
        try:
            sas.app_members(bad, "Build 1234567", None)
        except ValueError:
            continue
        raise AssertionError("app_members accepted a non-MIPS ELF")

print("SAS packages: PSU, ZIP and kit agree; damaged PSUs and non-MIPS ELFs refused")
