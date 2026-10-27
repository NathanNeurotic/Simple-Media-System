"""Parse real FFmpeg MP4 fixtures with the actual MOV source under sanitizers.

Only PS2 byte-swap assembly, codec registration, packet rings and device I/O are
host adapters. Parser/index/packet functions are compiled together unchanged.
This exercises atom layouts and sample indexing; it does not run a decoder.
"""
import argparse
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path)
CASES = ("standard", "faststart", "co64", "extended", "extended-nested", "zero", "missing-moov", "missing-mdat")
parser.add_argument("--case", default="all", choices=("all",) + CASES)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (args.source or root / "src/SMS_ContainerMOV.c").read_text()
start = source.index("static uint32_t _read_int_be (")
end = source.index("}  /* end _read_int_be */", start) + len("}  /* end _read_int_be */")
source = source[:start] + "static uint32_t _read_int_be ( FileContext* f ) { return __builtin_bswap32(File_GetUInt(f)); }" + source[end:]
source = re.sub(r'^#include "[^"\n]+"\s*$', '', source, flags=re.M)

prefix = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define SMS_MAXINT64 INT64_MAX
#define SMS_MAX_STREAMS 8
#define SMS_NOPTS_VALUE INT64_MIN
#define SMS_TIME_BASE 1000000
#define SMS_CONT_FLAGS_SEEKABLE 1
#define SMS_PKT_FLAG_KEY 1
#define SMS_MKTAG(a,b,c,d) ((unsigned)(a)|((unsigned)(b)<<8)|((unsigned)(c)<<16)|((unsigned)(d)<<24))
#define FILE_EOF(f) ((f)->m_CurPos >= (f)->m_Size)
typedef enum { SMS_CodecTypeUnknown=-1,SMS_CodecTypeVideo,SMS_CodecTypeAudio } SMS_CodecType;
typedef enum { SMS_CodecID_NULL,SMS_CodecID_MPEG4,SMS_CodecID_MPEG2,SMS_CodecID_AAC,SMS_CodecID_MP3,SMS_CodecID_MP2,SMS_CodecID_MPEG1,SMS_CodecID_OGGV,SMS_CodecID_AC3 } SMS_CodecID;
typedef struct { SMS_CodecID m_ID;unsigned m_Tag; } SMS_CodecTag;
typedef struct { unsigned m_FrameRate,m_FrameRateBase,m_FrameSize;int m_Width,m_Height,m_SampleRate,m_BitRate,m_Type,m_ID;unsigned m_Tag;void *m_pUserData;short m_UserDataLen,m_Channels,m_BitsPerSample; } SMS_CodecContext;
typedef struct FileContext FileContext;
struct FileContext { unsigned m_Size,m_CurPos;const unsigned char *data;int (*Read)(FileContext *,void *,unsigned);int (*Seek)(FileContext *,unsigned); };
typedef struct SMS_Stream SMS_Stream;
struct SMS_Stream { SMS_CodecContext *m_pCodec;void *m_pCtx,*m_pPktBuf;unsigned m_ID;int64_t m_Duration;void (*Destroy)(SMS_Stream *); };
typedef struct { int m_Size,m_StmIdx,m_Flags;unsigned char *m_pData;int64_t m_DTS,m_PTS; } SMS_AVPacket;
typedef struct SMS_Container SMS_Container;
struct SMS_Container { SMS_Stream *m_pStm[8];unsigned m_nStm,m_Flags;void *m_pCtx;FileContext *m_pFileCtx;char *m_pName; SMS_AVPacket *(*AllocPacket)(void *,int);int (*ReadPacket)(SMS_Container *,int *);int (*Seek)(SMS_Container *,int,int,unsigned); };
typedef struct { int m_SampleRate,m_nChannels,m_BitRate; } SMS_AudioInfo;
static char g_pMOV[]="MOV";
static int read_file(FileContext *f,void *p,unsigned n) { if(n>f->m_Size-f->m_CurPos)n=f->m_Size-f->m_CurPos;memcpy(p,f->data+f->m_CurPos,n);f->m_CurPos+=n;return n; }
static int seek_file(FileContext *f,unsigned p) { if(p>f->m_Size)return -1;f->m_CurPos=p;return p; }
static unsigned File_GetUInt(FileContext *f) { unsigned n=0;read_file(f,&n,4);return n; }
static int File_GetByte(FileContext *f) { unsigned char n=0;read_file(f,&n,1);return n; }
static unsigned File_GetShortBE(FileContext *f) { unsigned n=File_GetByte(f)<<8;return n|File_GetByte(f); }
static unsigned SMS_bswap32(unsigned n) { return __builtin_bswap32(n); }
static void File_Skip(FileContext *f,unsigned n) { unsigned p=n>f->m_Size-f->m_CurPos?f->m_Size:f->m_CurPos+n;seek_file(f,p); }
static SMS_CodecID SMS_CodecGetID(int type,unsigned tag) { (void)tag;return type==SMS_CodecTypeVideo?SMS_CodecID_MPEG4:SMS_CodecID_AAC; }
static void *SMS_Realloc(void *p,unsigned *size,unsigned need) { if(need<*size)return p;*size=17*need/16+32;return realloc(p,*size); }
static void SMSContainer_SetPTSInfo(SMS_Stream *s,int n,int d) { (void)s;assert(n>0&&d>0); }
static void SMSContainer_CalcPktFields(SMS_Stream *s,SMS_AVPacket *p) { (void)s;(void)p; }
static int SMSContainer_SetName(SMS_Container *c,FileContext *f) { (void)c;(void)f;return 1; }
static int64_t SMS_Rescale(int64_t n,int64_t mul,int64_t div) { assert(div);return n*mul/div; }
static void SMS_DestroyContainer(SMS_Container *,int);
static void SMS_RingBufferUnalloc(void *p,int n) { (void)p;(void)n; }
"""
post = r"""
static void SMS_DestroyContainer(SMS_Container *c,int all) {
 (void)all;
 for(unsigned i=0;i<c->m_nStm;++i) {
  SMS_Stream *s=c->m_pStm[i];if(!s)continue;
  s->Destroy(s);free(s->m_pCodec->m_pUserData);free(s->m_pCodec);free(s);
 }
 free(c->m_pCtx);c->m_pCtx=NULL;
}
int main(int argc,char **argv) {
 assert(argc==4);
 FILE *in=fopen(argv[1],"rb");assert(in);
 fseek(in,0,SEEK_END);long size=ftell(in);rewind(in);
 unsigned char *data=malloc(size);assert(data);
 assert(fread(data,1,size,in)==(size_t)size);fclose(in);
 FileContext f={size,0,data,read_file,seek_file};SMS_Container c={0};c.m_pFileCtx=&f;
 int result=_GetContainerMOV(&c,s_MOVParseTbl);
 if(!strcmp(argv[3],"reject")) {
  assert(!result && !c.m_nStm && !c.m_pCtx);free(data);return 0;
 }
 assert(result==1 && c.m_nStm==2);
 unsigned counts[SMS_MAX_STREAMS]={0};unsigned stream,pos,bytes;
 in=fopen(argv[2],"r");assert(in);
 /* FFprobe is the independent oracle for every packet's on-disk range. */
 while(fscanf(in,"%u %u %u",&stream,&pos,&bytes)==3) {
  assert(stream<c.m_nStm);
  MOVStream *m=c.m_pStm[stream]->m_pCtx;
  assert(counts[stream]<(unsigned)m->m_nIdx);
  MOVIndex *idx=m->m_pIdx+counts[stream]++;
  assert(idx->m_Pos==pos && idx->m_Size==bytes);
 }
 fclose(in);
 for(unsigned i=0;i<c.m_nStm;++i) {
  MOVStream *m=c.m_pStm[i]->m_pCtx;
  assert(counts[i]>0 && counts[i]==(unsigned)m->m_nIdx && counts[i]==m->m_nSamples);
 }
 SMS_DestroyContainer(&c,0);free(data);return 0;
}
"""


def atoms(data):
    pos = 0
    while pos < len(data):
        size, tag = struct.unpack_from(">I4s", data, pos)
        assert size >= 8 and pos + size <= len(data)
        yield tag, data[pos + 8:pos + size]
        pos += size
    assert pos == len(data)


def rewrite(data, kind):
    result = bytearray()
    for tag, body in atoms(data):
        if kind == "zero" and tag == b"udta":
            continue
        if kind == "missing-moov" and tag == b"moov":
            continue
        if kind == "missing-mdat" and tag == b"mdat":
            continue
        if tag in (b"moov", b"trak", b"mdia", b"minf", b"stbl"):
            body = rewrite(body, kind)
        if kind == "co64" and tag == b"stco":
            count = struct.unpack_from(">I", body, 4)[0]
            body = body[:8] + b"".join(struct.pack(">Q", struct.unpack_from(">I", body, 8 + 4 * i)[0]) for i in range(count))
            tag = b"co64"
        extended = tag == b"moov" and kind == "extended"
        extended |= kind == "extended-nested" and tag in (b"moov", b"trak", b"mdia", b"minf", b"stbl")
        if extended:
            result += struct.pack(">I4sQ", 1, tag, len(body) + 16) + body
        elif kind == "zero" and tag == b"moov":
            result += struct.pack(">I4s", 0, tag) + body
        else:
            result += struct.pack(">I4s", len(body) + 8, tag) + body
    return bytes(result)


with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    src, exe, media = tmp / "mov.c", tmp / "mov", tmp / "source.mp4"
    src.write_text(prefix + source + post)
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-no-pie", "-Wno-pointer-to-int-cast",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all", str(src), "-o", str(exe)], check=True)
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "color=c=red:s=32x32:r=2:d=1",
                    "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=8000:duration=1", "-shortest",
                    "-c:v", "mpeg4", "-bf", "0", "-c:a", "aac", str(media)], check=True, timeout=20)
    original = media.read_bytes()
    cases = CASES if args.case == "all" else (args.case,)
    for case in cases:
        fixture = tmp / f"{case}.mp4"
        if case == "faststart":
            subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(media), "-c", "copy",
                            "-movflags", "+faststart", str(fixture)], check=True, timeout=20)
        else:
            fixture.write_bytes(original if case == "standard" else rewrite(original, case))
        manifest = tmp / "packets.txt"
        if case.startswith("missing-"):
            mode = "reject"
        else:
            mode = "accept"
            probe = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-show_packets", "-of", "json", str(fixture)], timeout=10))
            manifest.write_text("".join(f"{p['stream_index']} {p['pos']} {p['size']}\n" for p in probe["packets"]))
        subprocess.run([str(exe), str(fixture), str(manifest), mode], check=True, timeout=5)
        print(f"MOV parser: {case} passed")
