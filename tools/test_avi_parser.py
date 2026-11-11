"""Sanitized host tests of the actual AVI demuxer over a memory-backed file.

The two MIPS blocks in _ReadPacket are replaced by a C stand-in; codecs, GUI and
the packet ring are stubbed. This checks header/index parsing and packet
framing, not PS2 decoding, device behaviour or reporter acceptance.
"""
import argparse
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path)
parser.add_argument("--case", default="all")
parser.add_argument("--file", type=Path, help="print the parse summary of an existing AVI")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (args.source or root / "src/SMS_ContainerAVI.c").read_text()
source = re.sub(r'^#include "[^"]+"\s*$', '', source, flags=re.M)
asm = re.findall(r'__asm__\s*\((?:[^;"]|"(?:\\.|[^"\\])*")*?\)\s*;', source)
assert len(asm) == 2
source = source.replace(asm[0], "memset(lData,0xFF,8);")
source = source.replace(asm[1], "{uint64_t v;memcpy(&v,lData,8);v=v>>8|(uint64_t)lSize<<56;"
                                "memcpy(lData,&v,8);lSize=(int32_t)(v>>32);}")
header = (root / "include/SMS_FileContext.h").read_text()
getters = header[header.index("static INLINE int File_GetByte"):header.index("void File_Skip")]

prefix = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint64_t u64;
#define INLINE inline
#define SMS_MAX_STREAMS 8
#define SMS_NOPTS_VALUE INT64_MIN
#define SMS_MAXINT64 INT64_MAX
#define SMS_MININT64 INT64_MIN
#define SMS_TIME_BASE 1000000
#define SMS_INT64(x) x##LL
#define SMS_MKTAG(a,b,c,d) ((a)|((b)<<8)|((c)<<16)|((d)<<24))
#define SMS_CONT_FLAGS_SEEKABLE 1
#define SMS_STRM_FLAGS_AUDIO 1
#define SMS_STRM_FLAGS_VIDEO 2
#define SMS_STRM_FLAGS_SUBTL 4
#define SMS_PKT_FLAG_KEY 1
#define SMS_PKT_FLAG_SUB 2
#define SMS_AUD_SPR NULL
#define FILE_EOF(f) ((f)->m_CurPos >= (f)->m_Size)
enum SMS_CodecType { SMS_CodecTypeUnknown, SMS_CodecTypeVideo, SMS_CodecTypeAudio };
enum { SMS_CodecID_NULL, SMS_CodecID_DXSB, SMS_CodecID_Other };
typedef struct { int m_Type,m_ID,m_Tag,m_Width,m_Height,m_BitsPerSample,m_FrameRate,
                 m_FrameRateBase,m_BlockAlign,m_BitRate; } SMS_CodecContext;
typedef struct SMS_Stream {
 int64_t m_CurDTS,m_Duration; uint32_t m_RealFrameRate,m_RealFrameRateBase,m_Flags;
 SMS_CodecContext *m_pCodec; char *m_pName; void *m_pCtx,*m_pPktBuf;
 void (*Destroy)(struct SMS_Stream *);
} SMS_Stream;
typedef struct { int64_t m_PTS,m_DTS; unsigned char *m_pData; uint32_t m_Size,m_StmIdx,m_Flags; } SMS_AVPacket;
typedef struct FileContext FileContext;
struct FileContext {
 unsigned m_Size,m_CurPos; unsigned char *m_pPos,*m_pEnd;
 int (*Read)(FileContext *,void *,unsigned); int (*Seek)(FileContext *,unsigned);
 int (*Fill)(FileContext *); int (*Stream)(FileContext *,unsigned,unsigned);
};
typedef struct SMS_Container SMS_Container;
struct SMS_Container {
 SMS_Stream *m_pStm[SMS_MAX_STREAMS]; int64_t m_Duration; uint32_t m_nStm,m_Flags;
 FileContext *m_pFileCtx; char *m_pName; void *m_pCtx;
 SMS_AVPacket *(*AllocPacket)(void *,int);
 int (*ReadPacket)(SMS_Container *,int *); int (*Seek)(SMS_Container *,int,int,uint32_t);
};
static uint32_t SMS_unaligned32(const void *p) { uint32_t v; memcpy(&v,p,4); return v; }
static uint64_t SMS_unaligned64(const void *p) { uint64_t v; memcpy(&v,p,8); return v; }
""" + getters + r"""
/* Buffered device: 4 KiB refills, reads stop at EOF, seeks past EOF fail. */
static unsigned char *image;
static unsigned device,empty_reads;
static int fill_file(FileContext *c) {
 unsigned n=device<c->m_Size?c->m_Size-device:0;
 if(n>4096) n=4096;
 if(!n) ++empty_reads;
 c->m_pPos=image+device;c->m_pEnd=c->m_pPos+n;device+=n;return n;
}
static int read_file(FileContext *c,void *p,unsigned n) {
 unsigned left=n;
 while(left) {
  unsigned k=c->m_pEnd-c->m_pPos;
  if(k>left) k=left;
  if(!k) {
   if(c->Fill(c)) continue;
   if(c->m_CurPos<c->m_Size) c->m_CurPos=c->m_Size;
   break;
  }
  memcpy(p,c->m_pPos,k);p=(char *)p+k;c->m_pPos+=k;c->m_CurPos+=k;left-=k;
 }
 return n-left;
}
static int seek_file(FileContext *c,unsigned p) {
 if(p>c->m_Size) { device=c->m_Size;return -1; }
 device=c->m_CurPos=p;c->m_pPos=c->m_pEnd=image;return p;
}
static int stream_file(FileContext *c,unsigned a,unsigned b) { (void)c;(void)a;(void)b;return 0; }
static void File_Skip(FileContext *c,unsigned n) {
 unsigned char skipped[512];
 while(n) { unsigned k=n>512?512:n; if(read_file(c,skipped,k)!=(int)k) break; n-=k; }
}
static struct { char *m_pStr; } STR_LOADING_INDICES={"idx"},STR_LOADING_ODML={"odml %d"};
static char g_pAVIStr[]="AVI";
static void *g_pSynthBuffer;
static void GUI_Progress(char *s,unsigned a,int b) { (void)s;(void)a;(void)b; }
static void GUI_Status(char *s) { (void)s; }
static void GUI_UpdateStatus(void) {}
static void SMS_GUIClockSuspend(void) {}
static void SMS_GUIClockResume(void) {}
static int SMS_CodecGetID(int type,unsigned tag) { (void)type;(void)tag;return SMS_CodecID_Other; }
static int64_t SMS_Rescale(int64_t a,int64_t b,int64_t c) { return c?a*b/c:0; }
static void SMSContainer_SetPTSInfo(SMS_Stream *s,int a,int b) { (void)s;(void)a;(void)b; }
static void SMSContainer_CalcPktFields(SMS_Stream *s,SMS_AVPacket *p) { (void)s;(void)p; }
static void SMSContainer_GetWAVHeader(FileContext *c,SMS_CodecContext *codec,int n) {
 codec->m_Type=SMS_CodecTypeAudio;File_Skip(c,n);
}
static void *SMS_Realloc(void *p,unsigned *size,unsigned min) {
 if(min<*size) return p;
 *size=17*min/16+32;return realloc(p,*size);
}
"""

tests = r"""
static FileContext file;
static SMS_Container cont;
static SMS_AVPacket *pkt;
static uint64_t sig=1469598103934665603ULL;
static void mix(const void *p,unsigned n) {
 const unsigned char *b=p;
 while(n--) sig=(sig^*b++)*1099511628211ULL;
}
/* The production ring hands out a pointer for a negative size and refuses an oversized one. */
static SMS_AVPacket *allocate(void *ring,int n) {
 (void)ring;assert(n>=0);
 if(n>(1<<20)) return NULL;
 free(pkt);pkt=calloc(1,sizeof(*pkt)+n);
 pkt->m_pData=(unsigned char *)(pkt+1);pkt->m_Size=n;return pkt;
}
int main(int argc,char **argv) {
 FILE *f=fopen(argv[1],"rb");
 long n;unsigned i,pkts=0,bytes=0,empty;int open,idx,got;
 assert(argc==2 && f);fseek(f,0,SEEK_END);n=ftell(f);rewind(f);
 image=malloc(n);assert(fread(image,1,n,f)==(size_t)n);fclose(f);
 file=(FileContext){n,0,image,image,read_file,seek_file,fill_file,stream_file};
 cont.m_pFileCtx=&file;cont.AllocPacket=allocate;
 open=SMS_GetContainerAVI(&cont);empty=empty_reads;
 printf("open=%d nstm=%u seekable=%u idx=",open,cont.m_nStm,cont.m_Flags&SMS_CONT_FLAGS_SEEKABLE);
 for(i=0;i<cont.m_nStm;++i) {
  _AVIStream *s=MYSTRM(cont.m_pStm[i]);
  printf("%s%u",i?",":"",s->m_nIdx);mix(s->m_pIdx,s->m_nIdx*sizeof(_AVIdxEntry));
  cont.m_pStm[i]->m_pPktBuf=&cont;
 }
 if(open) while((got=cont.ReadPacket(&cont,&idx))>=0) {
  ++pkts;bytes+=got;mix(&idx,sizeof(idx));
  if(pkt) { mix(&pkt->m_DTS,8);mix(&pkt->m_Size,12);mix(pkt->m_pData,pkt->m_Size); }
 }
 printf(" empty=%u pkts=%u bytes=%u sig=%016llx\n",empty,pkts,bytes,(unsigned long long)sig);
 return 0;
}
"""

FRAMES = 12

def chunk(tag, data):
    return tag + struct.pack("<I", len(data)) + data + b"\0" * (len(data) & 1)

def build(streams=2, idx1=True, odml=False, indx=b""):
    """Return an interleaved AVI, the file offset of every media chunk and the chunks."""
    parts = [(b"%02d%s" % (s, b"wb" if s else b"dc"), bytes([0xA0 + 16 * s + i]) * (40 + i))
             for i in range(FRAMES) for s in range(streams)]
    ix_size = 24 + 8 * FRAMES

    def head(supers):
        strl = b""
        for s in range(streams):
            strh = (b"auds" + bytes(4) if s else b"vidsXVID") + struct.pack(
                "<IHHIIIIIIII", 0, 0, 0, 0, 1, 25, 0, FRAMES, 0, 0, 0) + bytes(8)
            strf = bytes(18) if s else struct.pack("<IiiHH4s", 40, 320, 240, 1, 24, b"XVID") + bytes(20)
            index = chunk(b"indx", struct.pack("<HBBI4s", 4, 0, 0, 1, parts[s][0]) + bytes(12) +
                          struct.pack("<QII", supers[s], ix_size, FRAMES)) if odml else b""
            strl += chunk(b"LIST", b"strl" + chunk(b"strh", strh) + chunk(b"strf", strf) + index +
                          (b"" if s else indx))
        avih = struct.pack("<IIIIIII", 40000, 0, 0, 0x10, FRAMES, 0, streams) + bytes(28)
        return b"RIFF\0\0\0\0AVI " + chunk(b"LIST", b"hdrl" + chunk(b"avih", avih) + strl)

    base = len(head([0] * streams)) + 8                 # offset of the 'movi' fourcc
    supers = [base + 4 + s * (8 + ix_size) for s in range(streams)]
    where, body = [], b""
    for tag, data in parts:
        where.append(base + 4 + odml * streams * (8 + ix_size) + len(body))
        body += chunk(tag, data)
    entries = list(zip(where, parts))
    if odml:
        body = b"".join(
            chunk(b"ix%02d" % s, struct.pack("<HBBI4sQI", 2, 0, 1, FRAMES, parts[s][0], base, 0) +
                  b"".join(struct.pack("<II", pos + 8 - base, len(data))
                           for pos, (tag, data) in entries if tag == parts[s][0]))
            for s in range(streams)) + body
    data = bytearray(head(supers) + chunk(b"LIST", b"movi" + body))
    if idx1 and not odml:
        data += chunk(b"idx1", b"".join(struct.pack("<4sIII", tag, 0x10, pos - base, len(payload))
                                        for pos, (tag, payload) in entries))
    struct.pack_into("<I", data, 4, len(data) - 8)
    return data, where, parts

def probe(exe, tmp, data, **want):
    path = Path(tmp) / "case.avi"
    path.write_bytes(data)
    run = subprocess.run([str(exe), str(path)], capture_output=True, text=True, timeout=60)
    assert run.returncode == 0, run.stderr[-2000:]
    got = dict(re.findall(r"(\w+)=(\S+)", run.stdout))
    for key, value in want.items():
        assert got[key] == str(value), (key, value, run.stdout)
    return got

def case_valid(run):
    data, where, parts = build(idx1=False)
    total = sum(len(d) + (len(d) & 1) for _, d in parts)
    run(data, open=1, nstm=2, seekable=0, idx="0,0", pkts=len(parts), bytes=total)
    # OpenDML: the super index names both streams and movi carries ix## chunks to skip.
    run(build(odml=True)[0], open=1, seekable=1, idx=f"{FRAMES},{FRAMES}", pkts=len(parts), bytes=total)

def case_packets(run):
    # The host stand-in sign-extends the chunk size by construction; on the EE only dsra32 does.
    assert "dsra32" in asm[1] and "dsrl32" not in asm[1]
    data, where, parts = build(idx1=False)
    total = sum(len(d) + (len(d) & 1) for _, d in parts)
    lost = len(parts[6][1]) + (len(parts[6][1]) & 1)
    # A chunk naming a missing stream, a negative size, or an ix## chunk running past movi
    # costs that one chunk; the demuxer resynchronises on the next one.
    for patch in (b"05dc", b"21dc", parts[6][0] + struct.pack("<I", 0xFFFFFFF0),
                  b"ix00" + struct.pack("<I", 0x7FFFFFF0)):
        bad = bytearray(data)
        bad[where[6]:where[6] + len(patch)] = patch
        run(bad, open=1, pkts=len(parts) - 1, bytes=total - lost)

def case_idx1(run):
    data, where, parts = build()
    total = sum(len(d) + (len(d) & 1) for _, d in parts)
    full = run(data, open=1, seekable=1, idx=f"{FRAMES},{FRAMES}", pkts=len(parts), bytes=total)
    # A file cut inside the sixth index entry yields the five whole entries and
    # no reads past the end of the file for the missing ones.
    cut = data.rindex(b"idx1") + 8 + 16 * 5 + 6
    got = run(data[:cut], open=1, seekable=1, idx="3,2", pkts=len(parts), bytes=total)
    assert int(got["empty"]) <= int(full["empty"]), (got, full)
    # Nothing left after the idx1 header: no index, as for a file without one.
    run(data[:cut - 16 * 5 - 6], open=1, seekable=0, idx="0,0", pkts=len(parts), bytes=total)

cases = {"valid": case_valid, "packets": case_packets, "idx1": case_idx1}
with tempfile.TemporaryDirectory() as tmp:
    cfile, exe = Path(tmp) / "avi.c", Path(tmp) / "avi"
    cfile.write_text(prefix + source + tests)
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu99", "-g", "-w", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    str(cfile), "-o", str(exe)], check=True)
    if args.file:
        print(subprocess.run([str(exe), str(args.file)], capture_output=True, text=True, check=True).stdout, end="")
        raise SystemExit
    for name, case in cases.items():
        if args.case in ("all", name):
            case(lambda data, **want: probe(exe, tmp, data, **want))
print("AVI: unindexed, idx1 and OpenDML parsing and damaged media chunks passed")
