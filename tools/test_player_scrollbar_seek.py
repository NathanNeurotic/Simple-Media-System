"""Execute the production scrollbar with controlled input and lifecycle checks.

Workers, IPU and storage are stubs: this proves control-flow and teardown
ordering, not EE scheduling, MPEG decoding or the reporter's console freeze.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path)
parser.add_argument("--case", choices=("all", "back", "forward", "forward-fallback", "subscale-back", "failed", "resume", "cancel", "cancel-moved"), default="all")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (args.source or root / "src/SMS_PlayerControl.c").read_text()
start = source.find("int PlayerControl_ScrollBar (")
end_marker = "}  /* end PlayerControl_ScrollBar */"
end = source.find(end_marker, start) if start >= 0 else -1
if start < 0 or end < 0:
    raise RuntimeError("PlayerControl_ScrollBar not found in SMS_PlayerControl.c")
function = source[start:end + len(end_marker)]
buttons = sorted(set(re.findall(r"\b(?:SMS_PAD_|RC_)[A-Z_0-9]+", function)))
defines = "\n".join(f"#define {name} {i}" for i, name in enumerate(buttons, 1))
harness = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define SMS_TIME_BASE 1000
#define SMS_MPEG12_RESET_QUEUE 1
#define SMS_MPEG12_RESET_DECODER 2
#define SMS_MPEG12_RESET_TIME 4
#define SMS_MPEG12_RESET_RECOVER 8
#define SMS_HWC_Reset 1
#define SMS_clip(v,lo,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
typedef struct SMS_CodecContext SMS_CodecContext;
struct SMS_CodecContext { void (*HWCtl)(SMS_CodecContext *,int); };
typedef struct { SMS_CodecContext *m_pCodec; struct { int m_Den,m_Num; } m_TimeBase; } SMS_Stream;
typedef struct FileContext FileContext;
struct FileContext { unsigned m_CurPos,m_StreamSize; int (*Stream)(FileContext *,unsigned,unsigned); };
typedef struct SMS_Container SMS_Container;
struct SMS_Container {
 SMS_Stream **m_pStm; FileContext *m_pFileCtx; int64_t m_Duration,m_StartTime;
 int (*Seek)(SMS_Container *,int,int,int64_t);
};
typedef struct { void (*Suspend)(void),(*Resume)(void); } IPUContext;
typedef struct { void (*Reset)(void); } SubContext;
static struct {
 SMS_Container *m_pCont; int m_VideoIdx; IPUContext *m_pIPUCtx;
 int64_t m_VideoTime,m_AudioTime; SubContext *m_pSubCtx;
} s_Player;
static struct { int m_ScrollBarNum; } g_Config;
static uint64_t g_Timer;
static int g_CMedia;
static unsigned input[8],input_count,input_pos;
static int stopped,stops,resets,streams,seeks,hwresets,subresets,suspended;
static int seek_ok,seek_dir,first_seek_dir,no_later_key,repaints,repaint_modes[8],repaint_positions[8];
static int64_t seek_time,repaint_times[8];
static void suspend_ipu(void) { assert(!suspended);suspended=1; }
static void resume_ipu(void) { assert(suspended);suspended=0; }
static void init_queues(int create) { assert(!create && !stopped);stopped=1;++stops; }
static void SMS_Codec_MPEG12_Reset(unsigned flags) {
 assert(stopped && "resetting MPEG state while playback workers are still alive");
 assert(flags==15);++resets;
}
static void hwctl(SMS_CodecContext *ctx,int ctl) { (void)ctx;assert(stopped && ctl==SMS_HWC_Reset);++hwresets; }
static void reset_subs(void) { assert(stopped);++subresets; }
static int stream(FileContext *f,unsigned pos,unsigned blocks) {
 assert(stopped && resets==1);assert(pos==f->m_CurPos);
 assert(blocks==0 || blocks==48);++streams;return 1;
}
static int seek_container(SMS_Container *c,int idx,int dir,int64_t time) {
 (void)c;assert(idx==0 && stopped && streams==1);
 if(!seeks++) first_seek_dir=dir;
 seek_dir=dir;seek_time=time;
 /* AVI and MOV fail a forward seek when no keyframe follows the target. */
 return seek_ok && !(no_later_key && dir>0);
}
static int64_t SMS_Rescale(int64_t v,int64_t n,int64_t d) { return v*n/d; }
static uint32_t GUI_ReadButtons(void) {
 g_Timer+=1000;assert(input_pos<input_count);return input[input_pos++];
}
static void _sc_repaint(int64_t time,int mode,int pos) {
 assert(repaints<8);
 repaint_times[repaints]=time;repaint_modes[repaints]=mode;repaint_positions[repaints]=pos;
 ++repaints;
}
static void _sb_init(void) {}
static void CDVD_Stop(void) {}
static void CDDA_Standby(void) {}
static void CDDA_Synchronize(void) {}
static void SMS_TimerWait(int ms) { (void)ms; }
''' + defines + "\n" + function + r'''
static void scenario(const char *name,int subtitles) {
 SMS_CodecContext codec={hwctl}; SMS_Stream video={&codec,{1000,1}};
 SMS_Stream *streams_table[]={&video}; FileContext file={12345,384,stream};
 SMS_Container container={streams_table,&file,331000,1000,seek_container};
 IPUContext ipu={suspend_ipu,resume_ipu}; SubContext subs={reset_subs};
 int subscale=!strcmp(name,"subscale-back");
 int fallback=!strcmp(name,"forward-fallback");
 int forward=fallback || !strcmp(name,"forward");
 int moved=!strcmp(name,"back") || forward || subscale || !strcmp(name,"failed") || !strcmp(name,"cancel-moved");
 int cancel=!strcmp(name,"cancel") || !strcmp(name,"cancel-moved");
 stopped=stops=resets=streams=seeks=hwresets=subresets=suspended=repaints=0;
 input_pos=input_count=0;seek_ok=strcmp(name,"failed")!=0;no_later_key=fallback;g_Timer=0;
 g_Config.m_ScrollBarNum=32;
 s_Player.m_pCont=&container;s_Player.m_VideoIdx=0;s_Player.m_pIPUCtx=&ipu;
 int64_t initial_video_time=subscale?7000:171000;
 s_Player.m_VideoTime=initial_video_time;s_Player.m_AudioTime=170000;s_Player.m_pSubCtx=subtitles?&subs:0;
 if(moved) input[input_count++]=forward?SMS_PAD_RIGHT:SMS_PAD_LEFT;
 input[input_count++]=cancel?SMS_PAD_TRIANGLE:SMS_PAD_START;
 input[input_count++]=0;
 int result=PlayerControl_ScrollBar(init_queues);
 assert(input_pos==input_count && !suspended);
 assert(repaint_times[0]==initial_video_time && repaint_modes[0]==2 && repaint_positions[0]==(subscale?1:17));
 if(moved) {
  int64_t expected_osd=forward?181000:(subscale?1100:161000);
  int expected_pos=forward?18:(subscale?0:16);
  assert(repaints==(cancel?2:3));
  assert(repaint_times[1]==expected_osd && repaint_modes[1]==2 && repaint_positions[1]==expected_pos);
  if(!cancel) assert(repaint_times[2]==expected_osd && repaint_modes[2]==0 && repaint_positions[2]==expected_pos);
 }
 if(!moved && !cancel) {
  assert(result==1 && !stops && !resets && !streams && !seeks && !hwresets);
 } else {
  assert(stops==1 && resets==1 && hwresets==1);
  if(cancel) assert(result==0 && !streams && !seeks && !subresets);
  else {
   assert(first_seek_dir==(forward?1:-1) && subresets==subtitles);
   /* Only a failed forward seek is retried, once, towards the previous keyframe. */
   assert(seeks==(fallback?2:1) && seek_dir==(fallback?-1:first_seek_dir));
   assert(seek_time==(forward?180000:(subscale?100:160000)));
   if(seek_ok) {
    assert(result==-1 && streams==2);
    assert(s_Player.m_VideoTime==0 && s_Player.m_AudioTime==0);
   } else assert(result==0 && streams==1 && "failed seek must not resume terminated workers");
  }
 }
}
int main(int argc,char **argv) {
 const char *cases[]={"back","forward","forward-fallback","subscale-back","failed","resume","cancel","cancel-moved"};
 assert(argc==2);
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
  if(!strcmp(argv[1],"all") || !strcmp(argv[1],cases[i]))
   for(int subtitles=0;subtitles<=1;++subtitles) scenario(cases[i],subtitles);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / "scrollbar.c", Path(tmp) / "scrollbar"
    src.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu99", "-g", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie",
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe), args.case], check=True, timeout=10)
print("Scrollbar seek: worker teardown before reset/storage, successful restart status, forward seek fallback, failed seek, plain resume and cancel passed")
