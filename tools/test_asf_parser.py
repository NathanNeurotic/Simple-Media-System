"""Sanitized host tests of the actual ASF demuxer over a memory-backed file.

STIO_Read, File_Skip and the WAV header reader are the production functions;
codecs and the packet ring are stubbed. This checks header parsing and payload
reassembly, not PS2 decoding, device behaviour or reporter acceptance.
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
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (args.source or root / "src/SMS_ContainerASF.c").read_text()
source = re.sub(r'^#include ["<][^">]+[">]\s*$', '', source, flags=re.M)
GUID = [bytes(int(x, 16) for x in re.findall(r"0x(\w\w)", row))
        for row in re.findall(r"^ \{((?:\s*0x\w\w,?){16})\s*\}", source, flags=re.M)]
assert len(GUID) == 12
ASF, DATA, PROPS, STREAM, AUDIO, VIDEO, BITRATE = 0, 1, 2, 3, 4, 5, 10

def function(path, name, signature):
    text = (root / path).read_text()
    end = "}  /* end " + name + " */"
    return text[text.index(signature):text.index(end) + len(end)]

header = (root / "include/SMS_FileContext.h").read_text()
getters = header[header.index("static INLINE int File_GetByte"):header.index("void File_Skip")]

prefix = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint64_t u64;
typedef unsigned char SMS_GUID[16];
#define INLINE inline
#define SMS_INLINE inline
#define SMS_DATA_SECTION
#define SMS_MAX_STREAMS 8
#define SMS_NOPTS_VALUE INT64_MIN
#define SMS_TIME_BASE 1000
#define SMS_STRM_FLAGS_AUDIO 1
#define SMS_PKT_FLAG_KEY 1
#define FILE_EOF(f) ((f)->m_CurPos >= (f)->m_Size)
enum SMS_CodecType { SMS_CodecTypeUnknown, SMS_CodecTypeVideo, SMS_CodecTypeAudio };
enum { SMS_CodecID_NULL, SMS_CodecID_MP3, SMS_CodecID_MPEG2, SMS_CodecID_Other };
typedef struct { int m_Type,m_ID,m_Tag,m_Width,m_Height,m_BitsPerSample,m_FrameSize,m_Channels,
                 m_SampleRate,m_BitRate,m_BlockAlign,m_UserDataLen; void *m_pUserData; } SMS_CodecContext;
typedef struct { int64_t m_Duration; uint32_t m_ID,m_Flags; SMS_CodecContext *m_pCodec;
                 void *m_pCtx,*m_pPktBuf; } SMS_Stream;
typedef struct { int64_t m_PTS; unsigned char *m_pData; uint32_t m_Size,m_StmIdx,m_Flags; } SMS_AVPacket;
typedef struct { int m_nChannels,m_SampleRate,m_BitRate; } SMS_AudioInfo;
typedef struct FileContext FileContext;
struct FileContext {
 unsigned m_Size,m_CurPos,m_Pos; unsigned char *m_pPos,*m_pEnd;
 int (*Read)(FileContext *,void *,unsigned); int (*Seek)(FileContext *,unsigned);
 int (*Fill)(FileContext *);
};
typedef struct SMS_Container SMS_Container;
struct SMS_Container {
 SMS_Stream *m_pStm[SMS_MAX_STREAMS]; int64_t m_Duration; uint32_t m_nStm;
 FileContext *m_pFileCtx; char *m_pName; void *m_pCtx;
 SMS_AVPacket *(*AllocPacket)(void *,int);
 int (*ReadPacket)(SMS_Container *,int *); void (*Destroy)(SMS_Container *,int);
};
static uint32_t SMS_unaligned32(const void *p) { uint32_t v; memcpy(&v,p,4); return v; }
static uint64_t SMS_unaligned64(const void *p) { uint64_t v; memcpy(&v,p,8); return v; }
""" + getters + r"""
/* Buffered device: 4 KiB refills that stop at EOF; seeks past EOF fail. */
static unsigned char *image,g_SMSounds[512];
static unsigned device;
static int fill_file(FileContext *c) {
 unsigned n=device<c->m_Size?c->m_Size-device:0;
 if(n>4096) n=4096;
 c->m_pPos=image+device;c->m_pEnd=c->m_pPos+n;device+=n;return n;
}
static int seek_file(FileContext *c,unsigned p) {
 if(p>c->m_Size) return -1;
 device=c->m_CurPos=p;c->m_pPos=c->m_pEnd=image;return p;
}
""" + function("src/SMS_FileContext.c", "STIO_Read", "static int STIO_Read (") + \
      function("src/SMS_FileContext.c", "File_Skip", "void File_Skip (") + r"""
static char g_pASFStr[]="ASF";
static int SMS_CodecGetID(int type,unsigned tag) { (void)type;(void)tag;return SMS_CodecID_Other; }
static void SMSContainer_SetPTSInfo(SMS_Stream *s,int a,int b) { (void)s;(void)a;(void)b; }
static void SMSContainer_CalcPktFields(SMS_Stream *s,SMS_AVPacket *p) { (void)s;(void)p; }
static int SMSContainer_SetName(SMS_Container *c,FileContext *f) { (void)c;(void)f;return 1; }
static void SMS_DestroyContainer(SMS_Container *c,int all) {
 (void)all;
 while(c->m_nStm) {
  SMS_Stream *s=c->m_pStm[--c->m_nStm];
  free(s->m_pCodec->m_pUserData);free(s->m_pCodec);free(s);
 }
 free(c->m_pCtx);c->m_pCtx=NULL;
}
""" + function("src/SMS_Container.c", "SMSContainer_GetWAVHeader", "void SMSContainer_GetWAVHeader (") + r"""
/* A console cannot supply a 16 MiB media object. */
static void *limited_realloc(void *p,size_t n) { return n>(1<<20)?NULL:realloc(p,n); }
#define realloc limited_realloc
"""

tests = r"""
#undef realloc
static FileContext file;
static SMS_Container cont;
static SMS_AVPacket *pkt;
static SMS_AVPacket *allocate(void *ring,int n) {
 (void)ring;assert(n>=0);
 free(pkt);pkt=calloc(1,sizeof(*pkt)+n);
 pkt->m_pData=(unsigned char *)(pkt+1);pkt->m_Size=n;return pkt;
}
int main(int argc,char **argv) {
 FILE *f=fopen(argv[1],"rb");
 long n;unsigned i,pkts=0,bytes=0,sum=0;int open,idx,got;
 assert(argc==2 && f);fseek(f,0,SEEK_END);n=ftell(f);rewind(f);
 image=malloc(n);assert(fread(image,1,n,f)==(size_t)n);fclose(f);
 file=(FileContext){n,0,0,image,image,STIO_Read,seek_file,fill_file};
 cont.m_pFileCtx=&file;cont.AllocPacket=allocate;
 open=SMS_GetContainerASF(&cont);
 printf("open=%d nstm=%u",open,cont.m_nStm);
 for(i=0;i<cont.m_nStm;++i) cont.m_pStm[i]->m_pPktBuf=&cont;
 if(open) {
  while((got=cont.ReadPacket(&cont,&idx))>=0) {
   ++pkts;bytes+=got;
   for(i=0;i<pkt->m_Size;++i) sum+=(idx+1)*pkt->m_pData[i];
  }
  cont.Destroy(&cont,0);
 }
 printf(" pkts=%u bytes=%u sum=%u\n",pkts,bytes,sum);
 free(pkt);free(image);
 return 0;
}
"""

PACKET = 128                                            # fixed data packet size
FRAMES = 12

def obj(kind, data):
    return GUID[kind] + struct.pack("<Q", 24 + len(data)) + data

def packet(*payloads, wide=False):
    """One data packet. A payload is (stream, object number, offset, object size, data)."""
    multi = wide or len(payloads) > 1
    body = b""
    for stream, number, offset, size, data in payloads:
        body += struct.pack("<BBIBII", 0x80 | stream, number, offset, 8, size, 0)
        if multi:
            body += struct.pack("<I" if wide else "<H", len(data))
        body += data
    tail = struct.pack("<IH", 0, 0) + (bytes([(0xC0 if wide else 0x80) | len(payloads)]) if multi else b"")
    pad = PACKET - 6 - len(tail) - len(body)
    return bytes([0x82, 0, 0, 0x08 | multi, 0x5D, pad]) + tail + body + bytes(pad)

def media():
    """Whole objects, one split across two packets, and two payloads sharing a packet."""
    objects = [(1 + i % 2, bytes([0xA0 + i]) * (40 + i)) for i in range(FRAMES)]
    packets = [packet((s, i, 0, len(d), d)) for i, (s, d) in enumerate(objects)]
    big = bytes(range(1, 151))
    packets += [packet((1, 20, 0, 150, big[:101])), packet((1, 20, 101, 150, big[101:])),
                packet((1, 21, 0, 30, b"\x31" * 30), (2, 21, 0, 20, b"\x32" * 20))]
    return packets, objects + [(1, big), (1, b"\x31" * 30), (2, b"\x32" * 20)]

def build(packets, bitrates=()):
    """Return the file and the offset of its first data packet."""
    video = struct.pack("<IIBHIiiHH4s", 320, 240, 2, 40, 40, 320, 240, 1, 24, b"WMV2") + bytes(20)
    audio = struct.pack("<HHIIHHH", 0x161, 2, 44100, 16000, 743, 16, 0)
    spread = struct.pack("<BHHHB", 1, 743, 743, 1, 0)
    def stream(kind, number, data, extra=b""):
        return obj(STREAM, GUID[kind] + bytes(24) + struct.pack("<IIHI", len(data), len(extra), number, 0) +
                   data + extra)
    objects = [obj(PROPS, bytes(16) + struct.pack("<QQQQQIIIIII", 0, 0, len(packets), 0, 0, 0, 0, 2,
                                                  PACKET, PACKET, 0)),
               stream(VIDEO, 1, video), stream(AUDIO, 2, audio, spread)]
    if bitrates:
        objects.append(obj(BITRATE, struct.pack("<H", len(bitrates)) +
                           b"".join(struct.pack("<HI", n, 64000) for n in bitrates)))
    head = b"".join(objects)
    head = GUID[ASF] + struct.pack("<QI", 30 + len(head), len(objects)) + b"\1\2" + head
    body = b"".join(packets)
    data = GUID[DATA] + struct.pack("<Q", 50 + len(body)) + bytes(16) + struct.pack("<Q", len(packets)) + b"\1\1"
    return bytearray(head + data + body), len(head) + 50

def probe(exe, tmp, data, **want):
    path = Path(tmp) / "case.asf"
    path.write_bytes(data)
    run = subprocess.run([str(exe), str(path)], capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, run.stderr[-2000:]
    got = dict(re.findall(r"(\w+)=(\S+)", run.stdout))
    for key, value in want.items():
        assert got[key] == str(value), (key, value, run.stdout)

def expect(objects):
    return dict(open=1, nstm=2, pkts=len(objects), bytes=sum(len(d) for _, d in objects),
                sum=sum(s * sum(d) for s, d in objects) & 0xFFFFFFFF)

def case_valid(run):
    packets, objects = media()
    run(build(packets)[0], **expect(objects))
    # Stream Bitrate Properties, as Microsoft encoders write it, names every stream number.
    run(build(packets, bitrates=(1, 2))[0], **expect(objects))

def case_payloads(run):
    packets, objects = media()
    data, first = build(packets)
    # A negative offset into the media object, or an object too large to allocate,
    # costs that one object.
    for at, value in ((14, -16), (19, 0xFFFFFF)):
        bad = bytearray(data)
        struct.pack_into("<i", bad, first + at, value)
        run(bad, **expect(objects[1:]))
    # A negative payload length leaves the packet accounting unusable: the stream ends there.
    packets[6] = packet((1, 6, 0, len(objects[6][1]), objects[6][1]), wide=True)
    bad, first = build(packets)
    run(bad, **expect(objects))
    struct.pack_into("<i", bad, first + 6 * PACKET + 28, -1 << 30)
    run(bad, **expect(objects[:6]))

cases = {"valid": case_valid, "payloads": case_payloads}
with tempfile.TemporaryDirectory() as tmp:
    cfile, exe = Path(tmp) / "asf.c", Path(tmp) / "asf"
    cfile.write_text(prefix + source + tests)
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu99", "-g", "-w", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    str(cfile), "-o", str(exe)], check=True)
    for name, case in cases.items():
        if args.case in ("all", name):
            case(lambda data, **want: probe(exe, tmp, data, **want))
print("ASF: header objects and payload reassembly passed")
