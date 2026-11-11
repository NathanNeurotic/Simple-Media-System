"""Sanitized host tests of the actual ADTS (AAC) and MP3 containers and the WMA decoder entry.

The packet allocator, ID3 skip, MPEG audio header decoder, STIO_Read and File_Skip
are the production functions over a memory-backed file; the packet ring is malloc,
so an oversized packet write is an ASan error. The WMA decoder runs its real init
and block header parsing; Huffman tables, the MDCT and the PCM kernels are stand-ins
(every VLC read fails, PCM output is only sized). This checks probing, packet framing
and channel bounds, not PS2 decoding, device behaviour or reporter acceptance.
"""
import argparse
import os
from pathlib import Path
import re
import struct
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
mp123, mp123h = read("src/SMS_MP123.c"), read("include/SMS_MP123.h")
production = "\n".join((
    between(filectx, "static INLINE int File_GetByte", "static INLINE unsigned int File_GetUShort"),
    extract(stio, "static int STIO_Read (", "STIO_Read"),
    extract(stio, "void File_Skip (", "File_Skip"),
    between(bitio, "#define SMS_MIN_CACHE_BITS", "#ifdef __cplusplus"),
    between(bitio, "static SMS_INLINE int SMS_unaligned32_be", "#endif  /* __SMS_Bitio_H */"),
    extract(container, "static SMS_AVPacket* _AllocPacket (", "_AllocPacket"),
    read("src/SMS_ContainerAAC.c"),
    extract(container, "int SMSContainer_DefReadPacket (", "SMSContainer_DefReadPacket"),
    extract(container, "static int SMS_INLINE _id3_match (", "_id3_match"),
    extract(container, "int SMSContainer_SkipID3 (", "SMSContainer_SkipID3"),
    between(mp123h, "typedef struct SMS_MPAInfo {", "typedef struct SMS_Codec_MPAContext"),
    between(mp123, "const uint16_t g_mpa_freq_tab", "_BandInfo s_BandInfo"),
    extract(mp123, "int MP123_CheckHeader (", "MP123_CheckHeader"),
    extract(mp123, "int MP123_DecodeHeader (", "MP123_DecodeHeader"),
    read("src/SMS_ContainerMP3.c"),
    read("src/SMS_WMA.c"),
))

prefix = r"""
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define INLINE inline
#define SMS_INLINE
#define SMS_ALIGN(d,a) d __attribute__((aligned(a)))
#define SMS_MAX_STREAMS 8
#define SMS_MPA_MONO 3
#define SMS_AUD_SPR NULL
#define ID3_HEADER_SIZE 10
#define SMS_NOPTS_VALUE INT64_MIN
#define SMS_MKTAG(a,b,c,d) ((a)|((b)<<8)|((c)<<16)|((d)<<24))
#define SMS_STRM_FLAGS_AUDIO 1
#define SMS_NEG_USR32(a,s) (((uint32_t)(a))>>(32-(s)))
#define SMS_NEG_SSR32(a,s) (((int32_t)(a))>>(32-(s)))
#define SMS_INT64(x) x##LL
#define SMS_THREAD_PRIORITY 0
#define SMS_bswap32 __builtin_bswap32
#define FILE_EOF(f) ((f)->m_CurPos >= (f)->m_Size)
enum { SMS_CodecTypeAudio=1 };
enum { SMS_CodecID_AAC=1, SMS_CodecID_MP3, SMS_CodecID_WMA_V1, SMS_CodecID_WMA_V2 };
typedef struct { int m_SampleRate,m_nChannels,m_BitRate; } SMS_AudioInfo;
typedef struct SMS_CodecContext SMS_CodecContext;
typedef struct SMS_RingBuffer SMS_RingBuffer;
struct SMS_RingBuffer { void *m_pOut; void (*UserCB)(SMS_RingBuffer *); };
typedef struct { const char *m_pName; void *m_pCtx; int32_t (*Init)(SMS_CodecContext *);
                 int32_t (*Decode)(SMS_CodecContext *,SMS_RingBuffer *,SMS_RingBuffer *);
                 void (*Destroy)(SMS_CodecContext *); } SMS_Codec;
struct SMS_CodecContext { int m_Type,m_Tag,m_ID,m_SampleRate,m_BitRate; SMS_Codec *m_pCodec; void *m_pUserData;
                          short m_UserDataLen,m_Channels,m_BitsPerSample,m_BlockAlign; };
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
 int m_DefPackSize,m_DefPackIdx;
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
static void SMS_RingBufferUnalloc(SMS_RingBuffer *ring,int n) { (void)ring;(void)n; }
static struct { unsigned char m_MP3AutoPar; } g_Config={5};
static char g_pMP3Str[]="MP3";
static void *g_pSynthBuffer;
typedef struct { int16_t (*m_pTable)[2]; } SMS_VLC;
typedef struct { int unused; } SMS_MDCTContext;
static char g_pWMA[]="WMA";
static int SMS_VLC_Init(SMS_VLC *v,int a,int b,const void *c,int d,int e,const void *f,int g,int h) { return 0; }
static void SMS_VLC_Free(SMS_VLC *v) { (void)v; }
static int SMS_GetVLC2(void *bits,int16_t (*table)[2],int n,int depth) { return -1; }
static int DSP_MDCTInit(SMS_MDCTContext *c,int n,float scale) { return 0; }
static void DSP_MDCTDestroy(SMS_MDCTContext *c) { (void)c; }
static void DSP_IMDCT(SMS_MDCTContext *c,float *out,const float *in) {}
static void DSP_VecMULA(float *d,const float *a,const float *b,const float *c,int n) {}
static void DSP_VecMULR(float *d,const float *a,const float *b,int n) {}
static void RotateThreadReadyQueue(int priority) { (void)priority; }
static int SMS_log2(unsigned v) { int n=0; while(v>>=1) ++n; return n; }
static float SMS_sqrtf(float v) { return __builtin_sqrtf(v); }
float PowF(float a,float b) { return powf(a,b); }
/* The EE kernels store 16-bit samples: one per frame for mono, two for stereo. */
void pcm_syn1(short *out,float **pcm,int n) { (void)pcm;memset(out,0,2*n); }
void pcm_syn2(short *out,float **pcm,int n) { (void)pcm;memset(out,0,4*n); }
static int SMSContainer_SetName(SMS_Container *c,FileContext *f) { (void)c;(void)f;return 1; }
"""

tests = r"""
static FileContext file;
static SMS_Container cont;
static void pcm_ready(SMS_RingBuffer *ring) { (void)ring; }
/* Init and one superframe of a 44.1 kHz 128 kbit/s WMA v2 stream whose header claims the given
 * channel count; the packet codes every channel it is asked for. */
static int wma(int channels) {
 static int extra[2]={0,1};
 static uint8_t data[4096]={0xFC};
 SMS_AVPacket pkt={0,0,data,743};
 SMS_RingBuffer in={&pkt},out={NULL,pcm_ready};
 SMS_CodecContext codec={0,0,SMS_CodecID_WMA_V2,44100,128000,NULL,extra,6,channels,16,743};
 SMS_Codec_WMA_Open(&codec);
 codec.m_pCodec->Init(&codec);
 codec.m_pCodec->Decode(&codec,&out,&in);
 codec.m_pCodec->Destroy(&codec);
 printf("ch=%d\n",codec.m_Channels);
 free(codec.m_pCodec);free(packet);
 return 0;
}
int main(int argc,char **argv) {
 FILE *f;
 long n;unsigned pkts=0,bytes=0,start;int open,idx,got;
 assert(argc==4);
 if(!strcmp(argv[1],"wma")) return wma(atoi(argv[2]));
 f=fopen(argv[2],"rb");
 assert(f);fseek(f,0,SEEK_END);n=ftell(f);rewind(f);
 image=malloc(n+1);assert(fread(image,1,n,f)==(size_t)n);fclose(f);
 strict=atoi(argv[3]);
 file=(FileContext){n,0,0,image,image,STIO_Read,seek_file,fill_file};
 cont.m_pFileCtx=&file;cont.AllocPacket=_AllocPacket;
 open=strcmp(argv[1],"mp3")?SMS_GetContainerAAC(&cont):SMS_GetContainerMP3(&cont);
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


def mpa(index, pad=0):
    """One 44.1 kHz stereo MPEG-1 Layer III frame at the given bitrate table index."""
    kbps = (0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320)[index]
    return struct.pack(">I", 0xFFFB0000 | index << 12 | pad << 9) + bytes(144000 * kbps // 44100 + pad - 4)


def case_mp3(run):
    tag = b"ID3\x03\0\0\0\0\x02\x2c" + bytes(300)
    run("mp3", tag + mpa(9) * 8, open=1, start=310, sr=44100, ch=2)
    run("mp3", b"".join(mpa(i, i & 1) for i in (5, 9, 11, 14, 7, 12, 9, 10)), open=1, start=0)
    for frames in (2, 3):
        run("mp3", mpa(9) * frames, open=1, start=0, pkts=-(-417 * frames // 1024))
    # One header-like word with less than a frame of data after it is not MPEG audio.
    run("mp3", b"\x89PNG\r\n\x1a\n" + bytes(56) + mpa(9)[:300], open=0)


def case_wma(exe):
    # Only mono and stereo exist in the decoder; any other header value is decoded as stereo.
    for channels, used in ((1, 1), (2, 2), (6, 2), (0, 2), (-1, 2)):
        assert summary(exe, "wma", channels, 0) == f"ch={used}", channels


cases = {"aac": case_aac, "mp3": case_mp3}
with tempfile.TemporaryDirectory() as tmp:
    cfile, exe = Path(tmp) / "audio.c", Path(tmp) / "audio"
    cfile.write_text(prefix + production + tests)
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu99", "-g", "-w", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize=shift", "-fno-sanitize-recover=all",
                    str(cfile), "-o", str(exe), "-lm"], check=True)
    if args.file:
        for path in args.file:
            for kind in cases:
                print(path.name, kind, *(summary(exe, kind, path, strict) for strict in (0, 1)), sep=" | ")
        raise SystemExit
    for name, case in cases.items():
        if args.case in ("all", name):
            case(lambda kind, data, **want: probe(exe, tmp, kind, data, **want))
    if args.case in ("all", "wma"):
        case_wma(exe)
print("Audio formats: ADTS framing and damaged frame lengths, MP3 probing, WMA channel counts passed")
