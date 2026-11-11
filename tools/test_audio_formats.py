"""Sanitized host tests of the actual ADTS (AAC) container over a memory-backed file.

The packet allocator, STIO_Read and File_Skip are the production functions; the
packet ring is malloc, so an oversized packet write is an ASan error. This checks
probing and packet framing, not PS2 decoding, device behaviour or reporter acceptance.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--case", default="all")
parser.add_argument("--file", type=Path, nargs="+", help="print the summary of existing files")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]


def read(path):
    return re.sub(r'^#include [<"][^>"]+[>"]\s*$', '', (root / path).read_text(), flags=re.M)


def extract(source, signature, name):
    start = source.index(signature)
    marker = "}  /* end " + name + " */"
    return source[start:source.index(marker, start) + len(marker)]


def between(source, first, last):
    return source[source.index(first):source.index(last)]


container, stio = read("src/SMS_Container.c"), read("src/SMS_FileContext.c")
bitio, filectx = read("include/SMS_Bitio.h"), read("include/SMS_FileContext.h")
production = "\n".join((
    between(filectx, "static INLINE int File_GetByte", "static INLINE unsigned int File_GetUShort"),
    extract(stio, "static int STIO_Read (", "STIO_Read"),
    extract(stio, "void File_Skip (", "File_Skip"),
    between(bitio, "#define SMS_MIN_CACHE_BITS", "#ifdef __cplusplus"),
    between(bitio, "static SMS_INLINE int SMS_unaligned32_be", "static SMS_INLINE uint32_t SMS_GetBits"),
    extract(bitio, "static SMS_INLINE void SMS_InitGetBits (", "SMS_InitGetBits"),
    extract(container, "static SMS_AVPacket* _AllocPacket (", "_AllocPacket"),
    read("src/SMS_ContainerAAC.c"),
))

prefix = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define INLINE inline
#define SMS_INLINE inline
#define SMS_MAX_STREAMS 8
#define SMS_NOPTS_VALUE INT64_MIN
#define SMS_MKTAG(a,b,c,d) ((a)|((b)<<8)|((c)<<16)|((d)<<24))
#define SMS_STRM_FLAGS_AUDIO 1
#define SMS_NEG_USR32(a,s) (((uint32_t)(a))>>(32-(s)))
#define SMS_bswap32 __builtin_bswap32
#define FILE_EOF(f) ((f)->m_CurPos >= (f)->m_Size)
enum { SMS_CodecTypeAudio=1 };
enum { SMS_CodecID_AAC=1 };
typedef struct { int m_SampleRate,m_nChannels,m_BitRate; } SMS_AudioInfo;
typedef struct { int m_Type,m_Tag,m_ID,m_SampleRate,m_BitRate; void *m_pUserData;
                 short m_UserDataLen,m_Channels,m_BitsPerSample; } SMS_CodecContext;
typedef struct SMS_RingBuffer SMS_RingBuffer;
typedef struct { int64_t m_PTS,m_DTS; uint8_t *m_pData;
                 uint32_t m_Size,m_AllocSize,m_StmIdx,m_Flags; int32_t m_Duration; } SMS_AVPacket;
typedef struct { int64_t m_CurDTS; int32_t m_SampleRate; uint32_t m_Flags;
                 SMS_CodecContext *m_pCodec; SMS_RingBuffer *m_pPktBuf; } SMS_Stream;
typedef struct FileContext FileContext;
struct FileContext {
 unsigned m_Size,m_Pos,m_CurPos; unsigned char *m_pPos,*m_pEnd;
 int (*Read)(FileContext *,void *,unsigned); int (*Seek)(FileContext *,unsigned);
 int (*Fill)(FileContext *);
};
typedef struct SMS_Container SMS_Container;
struct SMS_Container {
 SMS_Stream *m_pStm[SMS_MAX_STREAMS]; uint32_t m_nStm; FileContext *m_pFileCtx; char *m_pName;
 SMS_AVPacket *(*AllocPacket)(SMS_RingBuffer *,int); int (*ReadPacket)(SMS_Container *,int *);
};
static uint32_t SMS_unaligned32(const void *p) { uint32_t v; memcpy(&v,p,4); return v; }
static unsigned char g_SMSounds[512];
/* Buffered device: 4 KiB refills. A strict device refuses a seek past EOF (FatFs clamps it);
 * a permissive one accepts it and then reads nothing. */
static unsigned char *image;
static unsigned device,strict;
static int fill_file(FileContext *c) {
 unsigned n=device<c->m_Size?c->m_Size-device:0;
 if(n>4096) n=4096;
 c->m_pPos=image+device;c->m_pEnd=c->m_pPos+n;device+=n;return n;
}
static int seek_file(FileContext *c,unsigned p) {
 if(strict && p>c->m_Size) return -1;
 device=c->m_CurPos=c->m_Pos=p;c->m_pPos=c->m_pEnd=image;return p;
}
/* One live packet, exactly as large as the reader asked for. */
static void *packet;
static void *SMS_RingBufferAlloc(SMS_RingBuffer *ring,int n) { (void)ring;free(packet);return packet=malloc(n); }
static int SMSContainer_SetName(SMS_Container *c,FileContext *f) { (void)c;(void)f;return 1; }
"""

tests = r"""
static FileContext file;
static SMS_Container cont;
int main(int argc,char **argv) {
 FILE *f=fopen(argv[2],"rb");
 long n;unsigned pkts=0,bytes=0,start;int open,idx,got;
 assert(argc==4 && f);fseek(f,0,SEEK_END);n=ftell(f);rewind(f);
 image=malloc(n+1);assert(fread(image,1,n,f)==(size_t)n);fclose(f);
 strict=atoi(argv[3]);
 file=(FileContext){n,0,0,image,image,STIO_Read,seek_file,fill_file};
 cont.m_pFileCtx=&file;cont.AllocPacket=_AllocPacket;
 open=SMS_GetContainerAAC(&cont);
 start=file.m_CurPos;
 if(open) {
  SMS_CodecContext *codec=cont.m_pStm[0]->m_pCodec;
  printf("open=1 start=%u sr=%d ch=%d",start,codec->m_SampleRate,codec->m_Channels);
  while((got=cont.ReadPacket(&cont,&idx))>=0) { ++pkts;bytes+=got; }
  printf(" pkts=%u bytes=%u\n",pkts,bytes);
  free(codec->m_pUserData);free(codec);free(cont.m_pStm[0]);
 } else printf("open=0\n");
 free(packet);free(image);
 return 0;
}
"""


def adts(payload, length=None):
    """One 44.1 kHz stereo AAC-LC ADTS frame; length overrides the frame_length field."""
    n = len(payload) + 7 if length is None else length
    return bytes([0xFF, 0xF1, 0x50, 0x80 | n >> 11, n >> 3 & 0xFF, (n & 7) << 5, 0]) + payload


def summary(exe, kind, path, strict):
    run = subprocess.run([str(exe), kind, str(path), str(strict)], capture_output=True, text=True, timeout=60)
    assert run.returncode == 0, run.stderr[-3000:]
    return run.stdout.strip()


def probe(exe, tmp, kind, data, **want):
    path = Path(tmp) / "case.bin"
    path.write_bytes(data)
    for strict in (0, 1):
        got = dict(re.findall(r"(\w+)=(\S+)", summary(exe, kind, path, strict)))
        for key, value in want.items():
            assert got.get(key) == str(value), (kind, strict, key, value, got)


def case_aac(run):
    # The probe consumes four confirming frames and leaves the cursor just before the fifth.
    frames = [adts(bytes([0x20 + i]) * 121) for i in range(12)]
    run("aac", b"".join(frames), open=1, sr=44100, ch=2, pkts=8, bytes=8 * 128)
    # A frame_length below the header size costs that one frame; the reader resynchronises.
    for length in (0, 4, 7):
        frames[8] = adts(bytes(121), length)
        run("aac", b"".join(frames), open=1, pkts=7, bytes=7 * 128)


cases = {"aac": case_aac}
with tempfile.TemporaryDirectory() as tmp:
    cfile, exe = Path(tmp) / "audio.c", Path(tmp) / "audio"
    cfile.write_text(prefix + production + tests)
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu99", "-g", "-w", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize=shift", "-fno-sanitize-recover=all",
                    str(cfile), "-o", str(exe)], check=True)
    if args.file:
        for path in args.file:
            for kind in cases:
                print(path.name, kind, *(summary(exe, kind, path, strict) for strict in (0, 1)), sep=" | ")
        raise SystemExit
    for name, case in cases.items():
        if args.case in ("all", name):
            case(lambda kind, data, **want: probe(exe, tmp, kind, data, **want))
print("Audio formats: ADTS framing and damaged frame lengths passed")
