"""Actual player shutdown/renderer with the actual 64-byte-slot ring buffer.

Kernel waits and IPU handoffs are controlled stubs. This checks ownership when
threads stop, including a consumed notification and the pause handshake
semaphores, not GS or console playback.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path)
parser.add_argument("--case", default="all", choices=(
    "all", "queued", "sync", "pause", "freed", "transferred", "unposted", "wrap", "audio", "empty", "starved-pause"))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (args.source or root / "src/SMS_Player.c").read_text()


def function(name):
    end_marker = "}  /* end " + name + " */"
    end = source.index(end_marker) + len(end_marker)
    start = source.rfind("\nstatic ", 0, source.index(end_marker)) + 1
    return source[start:end]


kernel = r"""
#ifndef TEST_KERNEL_H
#define TEST_KERNEL_H
typedef struct { int init_count,count; } ee_sema_t;
typedef struct { int status; } ee_thread_status_t;
int CreateSema(ee_sema_t *);
int DeleteSema(int);
int WaitSema(int);
int SignalSema(int);
int PollSema(int);
int ReferSemaStatus(int,ee_sema_t *);
int ChangeThreadPriority(int,int);
int TerminateThread(int);
int ReferThreadStatus(int,ee_thread_status_t *);
int SleepThread(void);
int WakeupThread(int);
int DeleteThread(int);
#endif
"""
harness = r"""
#include <assert.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "SMS_RingBuffer.h"
#define __SMS_H
typedef int64_t s64;
typedef uint64_t u64;
typedef void SMS_MacroBlock;
#include "SMS_VideoBuffer.h"
#include "SMS_RingBuffer.c"
#define SMS_THREAD_PRIORITY 64
#define SMS_FT_T_TYPE 0x80000000
#define SMS_FLAGS_STOP 1
#define SMS_FLAGS_PAUSE 2
#define SMS_FLAGS_MENU 4
#define SMS_FLAGS_SUBS 8
#define SMS_FLAGS_DISPCTL 16
#define SMS_FLAGS_DYNMSK 31
#define SMS_STRM_FLAGS_AUDIO 1
#define SMS_PKT_FLAG_NWC 4
#define SMS_NOPTS_VALUE ((s64)0x8000000000000000LL)
#define THREAD_ID_VR s_ThreadIDs[0]
#define THREAD_ID_VD s_ThreadIDs[1]
#define THREAD_ID_AD s_ThreadIDs[2]
#define THREAD_ID_AR s_ThreadIDs[3]
#define UNCACHED_SEG(p) (p)
static unsigned char audio_bytes[192];
#define SMS_AUDIO_BUFFER audio_bytes
#define SMS_AUDIO_BUFFER_SIZE sizeof(audio_bytes)
typedef struct { int m_BitsPerSample; } SMS_CodecContext;
typedef struct { int m_Flags,m_Size; uint64_t m_DTS; } SMS_AVPacket;
typedef struct {
    SMS_RingBuffer *m_pPktBuf; int m_Flags; SMS_CodecContext *m_pCodec;
} SMS_Stream;
typedef struct { int m_nStm; SMS_Stream **m_pStm; } SMS_Container;
typedef struct {
    void (*StopSync)(int); void (*Sync)(void);
    void (*QueuePacket)(int,void *); void (*Display)(void *,s64);
    void (*QueueSubtitle)(void *);
} IPUContext;
typedef struct { void (*Display)(s64); } SubContext;
typedef struct { void (*Silence)(void); void (*SetVolume)(int); void (*PlayPCM)(void *); } SPUContext;
static struct {
    IPUContext *m_pIPUCtx; SubContext *m_pSubCtx; SMS_Container *m_pCont; SPUContext *m_pSPUCtx;
    int m_Flags,m_VideoIdx,m_AudioIdx,m_AudioChannels,m_AudioSampleRate,m_OSD,m_OSDQWC[10];
    void *m_OSDPackets[10]; s64 m_SVDelta,m_VideoTime,m_AudioTime,m_AVDelta;
} s_Player;
static struct { int m_PlayerVolume; } g_Config;
static int s_MainThreadID=1,s_ThreadIDs[4]={2,3,4,5};
static int s_SemaPauseAudio,s_SemaPauseVideo,s_SemaAckPause;
static SMS_RingBuffer *s_pAudioBuffer,*s_pVideoBuffer;
static unsigned char s_VideoBuffer[192] __attribute__((aligned(64)));
static struct { const char *m_pStr; } STR_PAUSE={"pause"};
static int semas[64],nsemas,rendering,sync_calls,sync_stop_at,pause_stop;
static int free_stop,transfer_stop,terminated,deletes,queue_clears;
static int audio_live,wakeups,played;
static void render_audio(void);
static SMS_FrameBuffer rgb[3],*subtitles[4],*ipu_subtitle;
static jmp_buf stopped;
int CreateSema(ee_sema_t *s) {
    assert(nsemas<63);semas[++nsemas]=s->init_count;return nsemas;
}
int DeleteSema(int s) { assert(s>0 && s<=nsemas);return 0; }
int WaitSema(int s) {
    assert(s>0 && s<=nsemas);
    if(rendering && pause_stop && s==s_SemaPauseVideo) longjmp(stopped,1);
    /* An empty producer queue is the renderer's next normal blocking point. */
    if(rendering && s==s_pVideoBuffer->m_Sema[1] && !semas[s]) longjmp(stopped,1);
    /* The audio renderer parks here until resume or shutdown signals it. */
    if(rendering && s==s_SemaPauseAudio && !semas[s]) longjmp(stopped,1);
    assert(semas[s]>0 && "unexpected blocking kernel wait");--semas[s];return 0;
}
int SignalSema(int s) { assert(s>0 && s<=nsemas);++semas[s];return 0; }
int PollSema(int s) { if(!semas[s]) return -1;--semas[s];return 0; }
int ReferSemaStatus(int s,ee_sema_t *out) { out->count=semas[s];return 0; }
int ChangeThreadPriority(int t,int p) { assert(t==1 && (p==61 || p==64));return 0; }
int TerminateThread(int t) { assert(t>=2 && t<=4);++terminated;return 0; }
int ReferThreadStatus(int t,ee_thread_status_t *out) { assert(t==5);out->status=audio_live?1:0x10;return 0; }
int SleepThread(void) {
    assert(audio_live && "audio renderer should be dormant");
    /* The renderer was blocked on its empty queue: run its wake-up path. */
    render_audio();audio_live=0;assert(wakeups==1);return 0;
}
int WakeupThread(int t) { assert(t==1);++wakeups;return 0; }
int DeleteThread(int t) { assert(t>=2 && t<=5);++deletes;return 0; }
static void checked_free(void *p) {
    for(int i=0;i<4;++i) if(subtitles[i]==p && p) {
        subtitles[i]=NULL;free(p);
        if(rendering && free_stop) longjmp(stopped,1);
        return;
    }
    assert(0 && "invalid or duplicate subtitle free");
}
static void stop_sync(int mode) { assert(mode==0 || mode==1); }
static void sync_ipu(void) {
    if(rendering && ++sync_calls==sync_stop_at) longjmp(stopped,1);
    if(!rendering && ipu_subtitle) { checked_free(ipu_subtitle);ipu_subtitle=NULL; }
}
static void queue_packet(int n,void *p) { (void)n;(void)p; }
static void display(void *p,s64 pts) {
    SMS_FrameBuffer *f=p;(void)pts;
    /* Model GIF completion of a normal frame. */
    assert(f>=rgb && f<rgb+3);f->m_FrameType=-1;
}
static void queue_subtitle(void *p) {
    assert(!ipu_subtitle);ipu_subtitle=p;
    if(transfer_stop) longjmp(stopped,1);
}
static void _draw_text(const char *p) { assert(p==STR_PAUSE.m_pStr); }
static void silence(void) {}
static void set_volume(int v) { (void)v; }
static void play_pcm(void *p) {
    (void)p;assert(!(s_Player.m_Flags&SMS_FLAGS_PAUSE) && "audio played through a pause");++played;
}
static int SPU_Index2Volume(int i) { return i; }
static void _init_spu(void) { assert(0); }
static void SMS_CodecDestroy(SMS_CodecContext *p) { (void)p;assert(0); }
static void _clear_packet_queues(void) { ++queue_clears; }
#define free checked_free
""" + function("_terminate_threads") + function("_init_queues") + function("_sms_video_renderer") + function("_sms_audio_renderer") + r"""
#undef free
static IPUContext ipu={stop_sync,sync_ipu,queue_packet,display,queue_subtitle};
static SPUContext spu={silence,set_volume,play_pcm};
static SMS_Container container;
static void init(void) {
    memset(semas,0,sizeof(semas));nsemas=0;
    memset(&s_Player,0,sizeof(s_Player));memset(rgb,0,sizeof(rgb));
    memset(subtitles,0,sizeof(subtitles));ipu_subtitle=NULL;
    rendering=sync_calls=sync_stop_at=pause_stop=free_stop=transfer_stop=0;
    terminated=deletes=queue_clears=audio_live=wakeups=played=0;
    s_Player.m_pIPUCtx=&ipu;s_Player.m_pCont=&container;s_Player.m_pSPUCtx=&spu;
    /* Reopening a player must discard stale slot contents. */
    memset(s_VideoBuffer,0xa5,sizeof(s_VideoBuffer));
    _init_queues(1);
}
static SMS_FrameBuffer *subtitle(int i) {
    assert(!subtitles[i]);subtitles[i]=calloc(1,sizeof(SMS_FrameBuffer));assert(subtitles[i]);
    subtitles[i]->m_FrameType=SMS_FT_T_TYPE;return subtitles[i];
}
static void push(SMS_FrameBuffer *f,int post) {
    SMS_FrameBuffer **slot=SMS_RingBufferAlloc(s_pVideoBuffer,4);assert(slot);
    *slot=f;if(post) SMS_RingBufferPost(s_pVideoBuffer);
}
static void render(void) {
    rendering=1;
    if(!setjmp(stopped)) _sms_video_renderer(NULL);
    rendering=0;
}
static void render_audio(void) {
    rendering=1;
    if(!setjmp(stopped)) _sms_audio_renderer(NULL);
    rendering=0;
}
static void no_stale_pause_handshake(void) {
    assert(!semas[s_SemaPauseAudio] && !semas[s_SemaPauseVideo] && !semas[s_SemaAckPause]);
}
static void finish(int remove) {
    _terminate_threads(remove);
    assert(terminated==3 && deletes==(remove?4:0));
    no_stale_pause_handshake();
    assert(s_pVideoBuffer->m_Size==s_pVideoBuffer->m_Capacity);
    assert(SMS_RingBufferCount(s_pVideoBuffer)==0);
    for(int i=0;i<4;++i) assert(!subtitles[i] && "queued subtitle was leaked");
    assert(!ipu_subtitle);
    SMS_RingBufferDestroy(s_pVideoBuffer);SMS_RingBufferDestroy(s_pAudioBuffer);
}
static void scenario(const char *which,int remove) {
    init();
    if(!strcmp(which,"audio")) {
        s_Player.m_VideoIdx=-1;
        for(int i=0;i<3;++i) SMS_RingBufferPost(s_pVideoBuffer);
        /* These are notification tokens, with no allocated frame slots. */
        finish(remove);return;
    }
    if(!strcmp(which,"empty")) { finish(remove);return; }
    if(!strcmp(which,"starved-pause")) {
        /* A seek while paused, with the audio renderer blocked on its empty queue. */
        s_Player.m_AudioIdx=-1;s_Player.m_Flags=SMS_FLAGS_PAUSE;audio_live=1;
        _init_queues(0);
        assert(terminated==3 && queue_clears==1 && !s_Player.m_Flags);
        no_stale_pause_handshake();
        /* On the next pause the restarted renderer acknowledges once and parks. */
        unsigned char *packet=SMS_RingBufferAlloc(s_pAudioBuffer,80);
        assert(packet);memset(packet,0,80);SMS_RingBufferPost(s_pAudioBuffer);
        s_Player.m_Flags=SMS_FLAGS_PAUSE;render_audio();
        assert(semas[s_SemaAckPause]==1 && !played);
        SMS_RingBufferDestroy(s_pVideoBuffer);SMS_RingBufferDestroy(s_pAudioBuffer);
        return;
    }
    if(!strcmp(which,"unposted")) {
        push(rgb,1);SMS_RingBufferWait(s_pVideoBuffer);
        push(subtitle(0),0);
        /* A producer can stop between reserving a slot and storing its pointer. */
        SMS_FrameBuffer **slot=SMS_RingBufferAlloc(s_pVideoBuffer,4);
        assert(slot && !*slot);
        finish(remove);assert(rgb[0].m_FrameType==-1);return;
    }
    if(!strcmp(which,"freed") || !strcmp(which,"transferred")) {
        push(subtitle(0),1);push(rgb,1);push(subtitle(1),1);
        if(!strcmp(which,"freed")) free_stop=1;
        else { transfer_stop=1;s_Player.m_Flags=SMS_FLAGS_SUBS; }
        render();finish(remove);assert(rgb[0].m_FrameType==-1);return;
    }
    push(rgb,1);
    if(!strcmp(which,"wrap")) {
        push(rgb+1,1);sync_stop_at=2;render();
        assert(s_pVideoBuffer->m_pOut==s_VideoBuffer+64);
        push(subtitle(0),1);push(rgb+2,0);
    } else {
        push(subtitle(0),1);push(rgb+1,1);
        if(!strcmp(which,"sync")) { sync_stop_at=1;render(); }
        if(!strcmp(which,"pause")) { pause_stop=1;s_Player.m_Flags=SMS_FLAGS_PAUSE;render(); }
    }
    finish(remove);
    assert(rgb[0].m_FrameType==-1 && rgb[1].m_FrameType==-1);
    if(!strcmp(which,"wrap")) assert(rgb[2].m_FrameType==-1);
}
int main(int argc,char **argv) {
    const char *cases[]={"queued","sync","pause","freed","transferred","unposted","wrap","audio","empty","starved-pause"};
    assert(argc==2);
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
        if(!strcmp(argv[1],"all") || !strcmp(argv[1],cases[i]))
            for(int remove=0;remove<=1;++remove) scenario(cases[i],remove);
    return 0;
}
"""
with tempfile.TemporaryDirectory() as temp:
    temp = Path(temp)
    (temp / "kernel.h").write_text(kernel)
    c, exe = temp / "drain.c", temp / "drain"
    c.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-no-pie", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", "-I" + str(temp), "-I" + str(root / "include"),
                    "-I" + str(root / "src"), str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe), args.case], check=True, timeout=5)
print("Player video drain: queued/consumed/unposted slots, pause, subtitle handoffs, wrap, audio notifications, pause handshake reset and stop/delete passed")
