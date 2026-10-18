"""Compile exact driver function bodies with mocked sockets/IOP services (AFL-3.0)."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
smb = (ROOT / 'src/smb.c').read_text()
fio = (ROOT / 'src/smb_fio.c').read_text()

def function(source, name):
    """Extract a complete named C function, preserving its original body."""
    match = re.search(r'^(?:static )?(?:int|void|u8|s64|FHANDLE \*)\s*' + name +
                      r'\([^;]*?\)[^\n]*\n\{.*?^\}', source, re.M | re.S)
    if not match:
        raise RuntimeError('Function not found: ' + name)
    return match.group()

names = ['nb_SetSessionMessage', 'nb_GetSessionMessageLength', 'nb_GetPacketType',
         'SendData', 'RecvData', 'SkipSMBReplyBytes', 'SMBReplyError', 'GetSMBServerReply',
         'smb_ReadLE16', 'smb_StringLength', 'smb_NetShareEnum', 'smb_ReadAndX', 'smb_Disconnect']
# The two small byte/string helpers return unsigned int.
def extract(name):
    """Handle the unsigned-returning RAP helpers as well as normal functions."""
    if name in ('smb_ReadLE16', 'smb_StringLength'):
        return re.search(r'^static unsigned int ' + name + r'\([^;]*?\)\s*\n\{.*?^\}',
                         smb, re.M | re.S).group()
    return function(smb, name)

bodies = '\n\n'.join(extract(name) for name in names)
bodies += '\n\n' + '\n\n'.join(function(fio, name) for name in
                                  ('smbman_getfilefreeslot', 'smb_close', 'smb_closeAll',
                                   'smb_read', 'smb_write', 'smb_lseek64'))
handle = re.search(r'typedef struct\s*\{\s*iop_file_t \*f;.*?\} FHANDLE;', fio, re.S).group()
template = (ROOT / 'tests/smb_regressions.c').read_text()
with tempfile.TemporaryDirectory() as directory:
    build = Path(directory)
    (build / 'tamtypes.h').write_text('''#include <stdint.h>
typedef uint8_t u8; typedef int8_t s8;
typedef uint16_t u16; typedef int16_t s16;
typedef uint32_t u32; typedef int32_t s32;
typedef uint64_t u64; typedef int64_t s64;
''')
    source = build / 'regressions.c'
    source.write_text(template.replace('/* HANDLE_TYPE */', handle)
                     .replace('/* DRIVER_FUNCTIONS */', bodies))
    for char in ('-fsigned-char', '-funsigned-char'):
        binary = build / (char[2:] + '.test')
        command = ['cc', '-std=c99', '-Wall', '-Werror', char, '-g',
                   '-I' + str(build), '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'include')]
        if os.environ.get('SMS_TEST_SANITIZERS') == '1':
            command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        '-fno-omit-frame-pointer']
        subprocess.run(command + [str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
