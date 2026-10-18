"""Check the original SMS import ABI, then deterministically pack smbman (AFL-3.0)."""
import argparse
import importlib.util
import lzma
import pathlib
import sys

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parent
SOURCE = ROOT.parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--check', action='store_true', help='verify committed binary without modifying it')
args = parser.parse_args()
spec = importlib.util.spec_from_file_location('irx_imports', str(SOURCE / 'tools/irx_imports.py'))
imports = importlib.util.module_from_spec(spec)
spec.loader.exec_module(imports)
target = SOURCE / 'irx/smbman.irx.xz'
original = imports._read_module(str(target))
new = (ROOT / 'bin/smbman.irx').read_bytes()

def table(data):
    return sorted((name, major, minor, tuple(ordinals))
                  for _, name, major, minor, ordinals in imports._tables(data, imports.IMPORT_MAGIC))

assert table(original) == table(new), (table(original), table(new))
assert any(name == 'ps2ip' and major == 1 and minor == 3 for name, major, minor, _ in table(new))
compressed = lzma.compress(new, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32,
                         filters=[{'id': lzma.FILTER_LZMA2, 'preset': 9, 'dict_size': 1 << 20}])
assert lzma.decompress(compressed) == new
if args.check:
    assert target.read_bytes() == compressed, 'committed module differs from reproducible source rebuild'
else:
    target.write_bytes(compressed)
print('PASS: all import libraries/versions/ordinals unchanged; ps2ip=1.3; deterministic XZ/CRC32')
