"""Parse real FFmpeg MP4 fixtures with the actual MOV source under sanitizers.

Only PS2 byte-swap assembly, codec registration, packet rings and device I/O are
host adapters. Parser/index/packet functions are compiled together unchanged.
This exercises atom layouts, indexing and packet timelines; it does not run a
decoder. The adapter records DTS before SMSContainer_CalcPktFields applies
SMS's existing DTS-to-PTS scheduling rule.
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
CASES = ("standard", "faststart", "co64", "extended", "extended-nested", "zero", "missing-moov", "missing-mdat", "short-stts",
         "ctts-v0", "ctts-v0-two-frame", "ctts-v1", "edit-delay", "edit-v1", "no-edits", "edit-rate", "edit-multiple", "ctts-unknown", "edit-trim", "edit-tail", "edit-before-track", "m4a-metadata", "m4a-metadata-stereo", "m4a-metadata-bad-asc")
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
struct SMS_Stream { SMS_CodecContext *m_pCodec;void *m_pCtx,*m_pPktBuf;unsigned m_ID;int64_t m_Duration;void (*Destroy)(SMS_Stream *);int time_num,time_den; };
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
static void SMSContainer_SetPTSInfo(SMS_Stream *s,int n,int d) { assert(n>0&&d>0);s->time_num=n;s->time_den=d; }
static void SMSContainer_CalcPktFields(SMS_Stream *s,SMS_AVPacket *p) {
 p->m_PTS=p->m_PTS*s->time_num*SMS_TIME_BASE/s->time_den;
 p->m_DTS=p->m_DTS*s->time_num*SMS_TIME_BASE/s->time_den;
}
static int SMSContainer_SetName(SMS_Container *c,FileContext *f) { (void)c;(void)f;return 1; }
static int64_t SMS_Rescale(int64_t n,int64_t mul,int64_t div) { assert(div);return n*mul/div; }
static void SMS_DestroyContainer(SMS_Container *,int);
static void SMS_RingBufferUnalloc(void *p,int n) { (void)p;(void)n; }
static SMS_AVPacket packet;
static unsigned char packet_data[65536];
static SMS_AVPacket *packet_alloc(void *buffer,int n) {
 (void)buffer;assert(n>0 && n<=sizeof(packet_data));memset(&packet,0,sizeof(packet));
 packet.m_Size=n;packet.m_pData=packet_data;return &packet;
}
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
 FileContext f={size,0,data,read_file,seek_file};SMS_Container c={0};c.m_pFileCtx=&f;c.AllocPacket=packet_alloc;
 if(!strcmp(argv[3],"probe-reject")) {
  SMS_AudioInfo info={0};assert(!SMS_M4AProbe(&f,&info));assert(!f.m_CurPos);
  free(data);return 0;
 }
 if(!strcmp(argv[3],"probe")) {
  int rate,channels;FILE *expected=fopen(argv[2],"r");assert(expected);
  assert(fscanf(expected,"%d %d",&rate,&channels)==2);fclose(expected);
  SMS_AudioInfo info={0};assert(SMS_M4AProbe(&f,&info)==1);
  fprintf(stderr,"M4A metadata: rate=%d channels=%d bitrate=%d position=%u\n",
          info.m_SampleRate,info.m_nChannels,info.m_BitRate,f.m_CurPos);
  assert(info.m_SampleRate==rate);assert(info.m_nChannels==channels);
  assert(info.m_BitRate>0);assert(f.m_CurPos==0);
  free(data);return 0;
 }
 int result=_GetContainerMOV(&c,s_MOVParseTbl);
 if(!strcmp(argv[3],"reject")) {
  assert(!result && !c.m_nStm && !c.m_pCtx);free(data);return 0;
 }
 assert(result==1 && c.m_nStm==2);
 unsigned counts[SMS_MAX_STREAMS]={0},read_counts[SMS_MAX_STREAMS]={0};unsigned stream,pos,bytes,total=0;
 struct { unsigned pos,bytes;long long pts,dts; } expected[SMS_MAX_STREAMS][256];
 long long pts,dts;
 in=fopen(argv[2],"r");assert(in);
 /* FFprobe is the independent oracle for every packet's on-disk range. */
 while(fscanf(in,"%u %u %u %lld %lld",&stream,&pos,&bytes,&pts,&dts)==5) {
  assert(stream<c.m_nStm);
  MOVStream *m=c.m_pStm[stream]->m_pCtx;
  assert(counts[stream]<(unsigned)m->m_nIdx);
  assert(counts[stream]<256);
  expected[stream][counts[stream]].pos=pos;expected[stream][counts[stream]].bytes=bytes;
  expected[stream][counts[stream]].pts=pts;expected[stream][counts[stream]].dts=dts;
  MOVIndex *idx=m->m_pIdx+counts[stream]++;++total;
  assert(idx->m_Pos==pos && idx->m_Size==bytes);
 }
 fclose(in);
 for(unsigned i=0;i<c.m_nStm;++i) {
  MOVStream *m=c.m_pStm[i]->m_pCtx;
  assert(counts[i]>0 && counts[i]==(unsigned)m->m_nIdx && counts[i]==m->m_nSamples);
  c.m_pStm[i]->m_pPktBuf=m;
 }
 for(unsigned i=0;i<total;++i) {
  int selected=-1;assert(_ReadPacket(&c,&selected)>0);
  assert(selected>=0 && (unsigned)selected<c.m_nStm);
  unsigned j=read_counts[selected]++;assert(j<counts[selected]);
  assert(packet.m_Size==(int)expected[selected][j].bytes);
  assert(!memcmp(packet.m_pData,data+expected[selected][j].pos,packet.m_Size));
  if(llabs(packet.m_PTS-expected[selected][j].pts)>1 || llabs(packet.m_DTS-expected[selected][j].dts)>1) {
   fprintf(stderr,"stream%d packet%u PTS=%lld expected=%lld DTS=%lld expected=%lld\n",selected,j,
           (long long)packet.m_PTS,expected[selected][j].pts,(long long)packet.m_DTS,expected[selected][j].dts);
   assert(0);
  }
 }
 int selected;assert(_ReadPacket(&c,&selected)<0);
 for(unsigned i=0;i<c.m_nStm;++i) assert(read_counts[i]==counts[i]);
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


def rewrite(data, kind, video=False):
    result = bytearray()
    for tag, body in atoms(data):
        if kind == "zero" and tag == b"udta":
            continue
        if kind == "missing-moov" and tag == b"moov":
            continue
        if kind == "missing-mdat" and tag == b"mdat":
            continue
        if tag in (b"moov", b"trak", b"mdia", b"minf", b"stbl", b"edts"):
            body = rewrite(body, kind, b"vide" in body if tag == b"trak" else video)
        if kind == "edit-before-track" and tag == b"moov":
            # The table is well-formed but has no owning track. Dispatch must
            # skip it without indexing an unpublished stream or changing packets.
            edit = bytes(4) + struct.pack(">IIihh", 1, 1000, 0, 1, 0)
            body = struct.pack(">I4s", len(edit) + 8, b"elst") + edit + body
        if tag == b"elst":
            count = struct.unpack_from(">I", body, 4)[0]
            if kind == "edit-v1":
                entries = [struct.unpack_from(">Iihh", body, 8 + 12 * i) for i in range(count)]
                body = b"\x01\x00\x00\x00" + body[4:8] + b"".join(struct.pack(">Qqhh", *entry) for entry in entries)
            elif kind == "edit-rate":
                body = body[:16] + struct.pack(">h", 2) + body[18:]
            elif kind == "edit-multiple":
                body = body[:4] + struct.pack(">I", 2) + body[8:20] * 2
            elif kind in ("edit-trim", "edit-tail"):
                body = body[:4] + struct.pack(">I", 1) + struct.pack(">Iihh", 2000, 10240 if kind == "edit-trim" else 0, 1, 0)
            elif kind == "ctts-v0-two-frame" and video:
                media_time = struct.unpack_from(">i", body, 12)[0]
                body = body[:12] + struct.pack(">i", media_time + 1024) + body[16:]
        if kind == "ctts-v0-two-frame" and tag == b"ctts":
            count = struct.unpack_from(">I", body, 4)[0]
            entries = [struct.unpack_from(">II", body, 8 + 8 * i) for i in range(count)]
            body = body[:8] + b"".join(struct.pack(">II", n, offset + 1024) for n, offset in entries)
        if kind == "ctts-unknown" and tag == b"ctts":
            body = b"\x02" + body[1:]
        if kind == "m4a-metadata-bad-asc" and tag == b"stsd" and not video:
            # Find the AudioSpecificConfig descriptor through the standard
            # ES/DecoderConfig headers, then overstate its remaining payload.
            def descriptor(pos, expected):
                assert body[pos] == expected
                pos += 1
                for _ in range(4):
                    last = pos
                    pos += 1
                    if not body[last] & 0x80:
                        return pos, last
                raise AssertionError("unterminated descriptor length")
            esds = body.index(b"esds") + 4
            payload, _ = descriptor(esds + 4, 3)
            config, _ = descriptor(payload + 3, 4)
            asc, last = descriptor(config + 13, 5)
            assert len(body) - asc < 127
            body = body[:last] + b"\x7f" + body[last + 1:]
        if kind == "short-stts" and tag == b"stts":
            count = struct.unpack_from(">I", body, 8)[0]
            assert count > 0
            body = body[:8] + struct.pack(">I", count - 1) + body[12:]
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
        if case == "m4a-metadata-stereo":
            subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(media),
                            "-c:v", "copy", "-c:a", "aac", "-ac", "2", str(fixture)], check=True, timeout=20)
        elif case == "faststart":
            subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(media), "-c", "copy",
                            "-movflags", "+faststart", str(fixture)], check=True, timeout=20)
        elif case in ("ctts-v0", "ctts-v0-two-frame", "ctts-v1", "ctts-unknown", "edit-delay", "edit-v1", "no-edits", "edit-trim", "edit-tail"):
            generated = tmp / "generated.mp4"
            command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"]
            delayed = case in ("edit-delay", "edit-v1")
            trimmed = case in ("edit-trim", "edit-tail")
            if delayed:
                command += ["-itsoffset", "2"]
            command += ["-f", "lavfi", "-i", f"testsrc2=s=64x64:r=10:d={10 if trimmed else 1}", "-f", "lavfi", "-i",
                        f"sine=sample_rate=8000:duration={10 if trimmed else 3 if delayed else 1}", "-vsync", "0",
                        "-c:v", "mpeg4", "-bf", "2" if case.startswith("ctts-") else "0", "-c:a", "aac"]
            if case == "ctts-v1":
                command += ["-movflags", "+negative_cts_offsets"]
            if case == "no-edits":
                command += ["-use_editlist", "0"]
            subprocess.run(command + [str(generated)], check=True, timeout=20)
            fixture.write_bytes(rewrite(generated.read_bytes(), case))
        else:
            fixture.write_bytes(original if case == "standard" else rewrite(original, case))
        manifest = tmp / "packets.txt"
        if case == "m4a-metadata-bad-asc":
            mode = "probe-reject"
        elif case in ("m4a-metadata", "m4a-metadata-stereo"):
            mode = "probe"
            audio = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-select_streams", "a:0",
                                                       "-show_streams", "-of", "json", str(fixture)], timeout=10))["streams"][0]
            manifest.write_text(f"{audio['sample_rate']} {audio['channels']}\n")
        elif case.startswith("missing-") or case in ("short-stts", "edit-rate", "edit-multiple", "ctts-unknown", "edit-trim", "edit-tail"):
            mode = "reject"
        else:
            mode = "accept"
            probe = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-show_packets", "-of", "json", str(fixture)], timeout=10))
            manifest.write_text("".join(f"{p['stream_index']} {p['pos']} {p['size']} {round(float(p['pts_time']) * 1000000)} {round(float(p['dts_time']) * 1000000)}\n" for p in probe["packets"]))
        subprocess.run([str(exe), str(fixture), str(manifest), mode], check=True, timeout=5)
        print(f"MOV parser: {case} passed")
