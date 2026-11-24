"""Check smsnet's import ABI against the modules SMS loads it beside, then pack it."""
import argparse
import importlib.util
import lzma
import pathlib
import sys

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parent
SOURCE = ROOT.parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--check', action='store_true', help='verify the committed binary without modifying it')
args = parser.parse_args()
spec = importlib.util.spec_from_file_location('irx_imports', str(SOURCE / 'tools/irx_imports.py'))
imports = importlib.util.module_from_spec(spec)
spec.loader.exec_module(imports)
target = SOURCE / 'irx/smsnet.irx.xz'


def table(data):
    return {name: (major, minor, set(ordinals))
            for _, name, major, minor, ordinals in imports._tables(data, imports.IMPORT_MAGIC)}


def check(data):
    mine = table(data)
    # The TCP/IP stack is SMSTCPIP, the one smbman already binds to: same library version,
    # and nothing called that smbman does not call on every SMB connection.
    smb = table(imports._read_module(str(SOURCE / 'irx/smbman.irx.xz')))
    assert mine['ps2ip'][:2] == smb['ps2ip'][:2] == (1, 3), (mine['ps2ip'], smb['ps2ip'])
    assert mine['ps2ip'][2] <= smb['ps2ip'][2], sorted(mine['ps2ip'][2] - smb['ps2ip'][2])
    assert set(mine) == {'ps2ip', 'sifcmd', 'thbase', 'sysclib'}, sorted(mine)


if args.check:
    check(imports._read_module(str(target)))
    print('PASS: committed smsnet binds ps2ip 1.3 with a subset of smbman\'s calls')
else:
    new = (ROOT / 'bin/smsnet.irx').read_bytes()
    check(new)
    compressed = lzma.compress(new, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32,
                               filters=[{'id': lzma.FILTER_LZMA2, 'preset': 9, 'dict_size': 1 << 20}])
    assert lzma.decompress(compressed) == new
    target.write_bytes(compressed)
    print('PASS: smsnet packed, %d bytes ( %d packed )' % (len(new), len(compressed)))
