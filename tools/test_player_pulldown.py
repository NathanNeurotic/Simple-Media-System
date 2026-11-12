"""PAL 2:2 pulldown setup against the production packet timestamp conversion.

The pulldown retimes video by giving each time-base tick 1/25 s. This checks
which streams it is applied to and the timestamps that follow, not SPU
resampling or display timing on a console.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]


def function(path, head, name):
    source = (root / path).read_text()
    end_marker = "}  /* end " + name + " */"
    start, end = source.index(head), source.index(end_marker) + len(end_marker)
    return source[start:end]


harness = r'''
#include <assert.h>
#include <stdint.h>
#define SMS_TIME_BASE 1000
#define SMS_NOPTS_VALUE INT64_MIN
#define SMS_FLAGS_SPDIF 1
#define SMS_FLAGS_PDW22 2
#define SMS_PF_PDW22 4
enum { GSVideoMode_PAL=3,GSVideoMode_DTV_640x576P=0x53 };
typedef enum { SMS_CodecTypeVideo,SMS_CodecTypeAudio } SMS_CodecType;
typedef struct {
 uint32_t m_FrameRate,m_FrameRateBase,m_FrameSize; int32_t m_SampleRate,m_BitRate; SMS_CodecType m_Type;
} SMS_CodecContext;
typedef struct { int m_Num,m_Den; } SMS_Rational;
typedef struct { SMS_CodecContext *m_pCodec; SMS_Rational m_TimeBase; int64_t m_CurDTS; } SMS_Stream;
typedef struct { int64_t m_PTS,m_DTS; int m_Size,m_Duration; } SMS_AVPacket;
typedef struct { int m_nStm; SMS_Stream **m_pStm; int64_t m_Duration; } SMS_Container;
static struct { int m_VideoIdx,m_Flags,m_PDW22Ratio,m_PDW22Base; SMS_Container *m_pCont; } s_Player;
static struct { int m_PlayerFlags; } g_Config;
static struct GSParams { unsigned short m_GSCRTMode; } gs={GSVideoMode_PAL};
static struct GSParams *GS_Params(void) { return &gs; }
static int subtitle_ratio,subtitle_base;
static void SMS_DXSB_SetRatio(int ratio,int base) { subtitle_ratio=ratio;subtitle_base=base; }
static int64_t SMS_Rescale(int64_t v,int64_t n,int64_t d) { return (v*n+d/2)/d; }
''' + function("src/SMS_Container.c", "void SMSContainer_SetPTSInfo (", "SMSContainer_SetPTSInfo") + "\n" \
    + function("src/SMS_Container.c", "void SMSContainer_CalcPktFields (", "SMSContainer_CalcPktFields") + "\n" \
    + function("src/SMS_Player.c", "static void _setup_video_pulldown22 (", "_setup_video_pulldown22") + r'''
/* Millisecond PTS of the video packet stamped `tick` after pulldown setup. */
static int64_t video_ms(int num,int den,uint32_t rate,uint32_t base,int64_t tick) {
 SMS_CodecContext video_codec={rate,base,0,0,0,SMS_CodecTypeVideo};
 SMS_CodecContext audio_codec={0,0,1152,48000,0,SMS_CodecTypeAudio};
 SMS_Stream video={&video_codec,{num,den},0},audio={&audio_codec,{1,48000},0};
 SMS_Stream *streams[]={&video,&audio}; SMS_Container container={2,streams,100000};
 SMS_AVPacket packet={tick,tick,0,0};
 s_Player.m_VideoIdx=0;s_Player.m_Flags=0;s_Player.m_PDW22Ratio=s_Player.m_PDW22Base=1;
 s_Player.m_pCont=&container;g_Config.m_PlayerFlags=SMS_PF_PDW22;subtitle_ratio=subtitle_base=0;
 _setup_video_pulldown22();
 if(s_Player.m_Flags&SMS_FLAGS_PDW22) {
  /* 23.976 fps played at 25 fps: audio and duration shrink by 24000/25025. */
  assert(video.m_TimeBase.m_Num==1 && video.m_TimeBase.m_Den==25);
  assert(video_codec.m_FrameRate==25 && video_codec.m_FrameRateBase==1);
  assert(s_Player.m_PDW22Ratio==24000 && s_Player.m_PDW22Base==25025);
  assert(subtitle_ratio==24000 && subtitle_base==25025 && container.m_Duration==95904);
  assert(audio.m_TimeBase.m_Num==1 && audio.m_TimeBase.m_Den==50050);
 } else {
  assert(video.m_TimeBase.m_Num==num && video.m_TimeBase.m_Den==den);
  assert(video_codec.m_FrameRate==rate && video_codec.m_FrameRateBase==base);
  assert(audio.m_TimeBase.m_Den==48000 && container.m_Duration==100000 && !subtitle_ratio);
 }
 SMSContainer_CalcPktFields(&video,&packet);
 return packet.m_PTS;
}
int main(void) {
 /* AVI and constant-rate MP4: one tick per frame, so frame 24 moves from 1001 ms to 960 ms. */
 assert(video_ms(1001,24000,24000,1001,24)==960 && (s_Player.m_Flags&SMS_FLAGS_PDW22));
 /* MP4 at 90 kHz with 3754/3753-tick frames: a tick is not a frame, so the
  * stream keeps its own time base and frame 1 stays at 42 ms. */
 assert(video_ms(1,90000,90000,3754,3754)==42 && !(s_Player.m_Flags&SMS_FLAGS_PDW22));
 /* 25 fps needs no pulldown. */
 assert(video_ms(1,25,25,1,25)==1000 && !(s_Player.m_Flags&SMS_FLAGS_PDW22));
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
    c, exe = Path(temp) / "pulldown.c", Path(temp) / "pulldown"
    c.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu99", "-g", "-no-pie", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=5)
print("Player pulldown: frame-tick streams are retimed to 25 fps, other time bases are left alone")
