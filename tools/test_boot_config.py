"""Boot-time config resolution on every boot path, compiled from the production sources.

Production code: SMS_Config.c (config path selection and SMS_LoadConfig), the config block
of SMS_IOPInit with _cfg_resolve_fallback, main.c's re-init statement after SMS_IOPInit and
GUI_Initialize's pad-override statement. The rest of the first GUI_Initialize is mirrored by
hand. Device drivers, fio and the GUI are stubbed: fio is an in-memory file table with
per-device mount state and GUI_Initialize records the config it would apply. This checks ordering and state, not console rendering.
Set SMS_BOOT_SOURCE_REF to an older Git ref to check an independent baseline.
"""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def source(path):
    ref = os.environ.get("SMS_BOOT_SOURCE_REF")
    if ref:
        return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT).decode("utf-8", "replace")
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def function(text, name, end=None):
    match = re.search(r"(?:static )?(?:void|int) " + name + r" \(", text)
    assert match, name
    tail = "}  /* end " + (end or name) + " */"
    return text[match.start():text.index(tail, match.start()) + len(tail)]


def line(text, pattern):
    match = re.search(pattern, text, re.M)
    assert match, pattern
    return match.group(0)


def optional(text, pattern):
    match = re.search(pattern, text, re.M)
    return match.group(0) if match else ""


cfg, iop, mainc, gui, header = (source(p) for p in ("src/SMS_Config.c", "src/SMS_IOP.c", "src/main.c", "src/SMS_GUI.c", "include/SMS_Config.h"))
start = iop.index(' if (  SMS_ConfigOnFS () && strncmp ( SMS_ConfigPath (), "mc", 2 ) != 0  ) {')
iop_block = iop[start:iop.index("\n }  /* end else if */\n#endif", start) + len("\n }  /* end else if */")]
reinit = line(mainc, r"^ if \([^;]*\) GUI_Initialize \( 0 \);")
override = line(gui, r"^  if \( lVideoMode != GSVideoMode_Default \)[^\n]*")

harness = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#define BDM 1
typedef int64_t s64;
typedef struct SMS_List { int m_Unused; } SMS_List;
typedef struct SMBLoginInfo SMBLoginInfo;
""" + header[header.index("#define SMS_BF_SKIN"):header.index("} SMSConfig;") + len("} SMSConfig;")] + r"""
enum { GSVideoMode_NTSC=2, GSVideoMode_PAL=3, GSVideoMode_Default=0xFF };
enum { GSCodePage_WinLatin1=2 };
#define SMS_IOPF_SMBINFO 1
#define SMS_IOPF_MMCE 2
static unsigned g_IOPFlags;
static SMSConfig g_Config, applied;
static char g_pDefStr[]="English", g_EmptyStr[]="", g_pSMSSkn[]="mc0:/SMS/Skins", g_pExtSMI[]=".smi";
static char g_SMSSMB[128]="mc0:/SMS/SMS.smb", s_pMC0SMC[128]="mc0:/SMS/SMS.cfg", g_pBootDir[128];
static int g_MCSlot, s_CfgOnFS, s_CfgFallback, inits;

/* fio: a file is reachable only while its device is mounted; "mass:" is unit 0 */
static struct { char path[64]; const void *data; int size; } files[4];
static char mounted[4][8];
static int nfiles, nmounted, position;
static void mount(const char *device) { strcpy(mounted[nmounted++],device); }
static void normal(const char *in,char *out) {
 const char *colon=strchr(in,':'), *rest=colon+1;
 int n=(int)(colon-in);
 while(*rest=='/') ++rest;
 sprintf(out,"%.*s%s:/%s",n,in,in[n-1]>='0'&&in[n-1]<='9' ? "" : "0",rest);
}
static int reachable(const char *path) {
 int i;
 for(i=0;i<nmounted;++i) if(!strncmp(path,mounted[i],strlen(mounted[i]))) return 1;
 return 0;
}
static int fioOpen(const char *path,int flags) {
 char name[160];
 int i;
 (void)flags; normal(path,name);
 if(reachable(name)) for(i=0;i<nfiles;++i) if(!strcmp(name,files[i].path)) { position=0; return i; }
 return -1;
}
static int fioRead(int fd,void *data,int size) {
 if(size>files[fd].size-position) size=files[fd].size-position;
 memcpy(data,(const char *)files[fd].data+position,size); position+=size;
 return size;
}
static int fioClose(int fd) { (void)fd; return 0; }
static int fioDopen(const char *path) { char name[160]; normal(path,name); return reachable(name) ? 0 : -1; }
static int fioDclose(int fd) { (void)fd; return 0; }
static void add(const char *path,const void *data,int size) {
 strcpy(files[nfiles].path,path); files[nfiles].data=data; files[nfiles++].size=size;
}

/* libmc left uninitialised: MC_Sync writes nothing, so the fio probe decides */
static void MC_GetInfo(int a,int b,int *c,int *d,int *e) { (void)a;(void)b;(void)c;(void)d;(void)e; }
static void MC_Sync(int *p) { (void)p; }
static SMS_List *SMS_ListInit(void) { static SMS_List list; return &list; }
static void SMS_ListPushBack(SMS_List *list,const char *name) { (void)list;(void)name; }
static void SMS_EEScanDir(const char *path,const char *ext,SMS_List *list) { (void)path;(void)ext;(void)list; }
static void SMS_LoadPalette(void) {}
static void _load_font(int i) { (void)i; }
static void SMS_LoadSMBInfo(void) {}
static void SMS_SetDirButtons(void) {}
static void SMS_SetPalette(const unsigned *p) { (void)p; }
static void GUI_SetColors(void) {}
static void SMS_LocaleInit(void) { g_Config.m_BrowserFlags|=SMS_BF_UDFL; }   /* a valid SMS.lng is installed */
static void SMS_LocaleSet(void) {}
static void _read_ipconfig(void) {}
static void GUI_Initialize(int cold) { (void)cold; applied=g_Config; ++inits; }

/* drivers: only the one for the late device brings anything back; SMS_IOPStartHDD mounts no partition */
static const char *late;
static int start(const char *device) { if(strncmp(late,device,strlen(device))) return 0; mount(late); return 1; }
static int SMS_IOPStartUSB(int a) { (void)a; return start("mass"); }
static int SMS_IOPStartMX4SIO(int a) { (void)a; return 0; }
static int SMS_IOPStartHDD(int a) { (void)a; return 1; }
static int SMS_IOPStartUDPFS(int a) { (void)a; return start("udpfs"); }
static int SMS_IOPStartMMCE(int a) { (void)a; if(start("mmce")) g_IOPFlags|=SMS_IOPF_MMCE; return 0; }
static int checkConnectedMassDev(int unit) { char name[8]; sprintf(name,"mass%d:",unit); return reachable(name); }
""" + "\n".join([
    optional(cfg, r"^int +g_BootDisplayMode[^\n]*"), optional(iop, r"^int +g_CfgLate;"),
    function(cfg, "_derive_smb_path"), function(cfg, "SMS_ConfigSetCWD"), function(cfg, "_mc_get_info"),
    optional(cfg, r"^static int +s_CfgPtr [^\n]*"), optional(cfg, r"^static char s_CfgPtrName\[[^\n]*"),
    function(cfg, "_follow_cfg_pointer") if "_follow_cfg_pointer" in cfg else "",
    line(cfg, r"^int SMS_MCPresent \([^\n]*\}"), line(cfg, r"^int SMS_ConfigOnFS \([^\n]*\}"),
    line(cfg, r"^const char\* SMS_ConfigPath \([^\n]*\}"), line(cfg, r"^int SMS_ConfigFallback \([^\n]*\}"),
    line(cfg, r"^void SMS_ConfigUseFSPath \([^\n]*\}"), function(cfg, "SMS_ConfigClearFS"),
    function(cfg, "SMS_LoadConfig", "SMS_LoadCondig"), function(iop, "_cfg_resolve_fallback"),
    "static void config_block(void) {\n" + iop_block + "\n}",
]) + r"""
int main(int argc,char **argv) {
 static SMSConfig saved;
 const char *boot=argv[1], *config=argv[2];
 int expected_inits=atoi(argv[7]), expected_mode=(int)strtol(argv[8],NULL,0);
 int lVideoMode=(int)strtol(argv[9],NULL,0);         /* SELECT+R1 / SELECT+R2 held at boot */
 static char pointer_path[96];
 assert(argc==10||argc==11);
 late=argv[5];
 SMS_LoadConfig();                                   /* nothing mounted: the built-in defaults */
 saved=g_Config; saved.m_Version=14; saved.m_DisplayMode=(int)strtol(argv[4],NULL,0);
 if(strcmp(argv[3],"-")) strcpy(saved.m_SkinName,argv[3]);
 saved.m_DispDXDY[0][0]=8;                           /* the user moved the picture */
 memset(&g_Config,0,sizeof(g_Config));
 if(strcmp(config,"-")) add(config,&saved,896);
 if(argc==11) {                                      /* SMSPATH.TXT next to the ELF */
  normal(boot,pointer_path); strcpy(strrchr(pointer_path,'/')+1,"SMSPATH.TXT");
  add(pointer_path,argv[10],(int)strlen(argv[10]));
 }

 if(boot[0]=='m'&&boot[1]=='c') g_MCSlot=boot[2]-'0';
 SMS_ConfigSetCWD(boot);
 if(atoi(argv[6])) mount(boot[0]=='m'&&boot[1]=='c'&&boot[2]=='1' ? "mc1:" : "mc0:");   /* the card is the only device up this early */
 if(!SMS_LoadConfig()) SMS_LoadConfig();             /* GUI_Initialize ( 1 ) */
 SMS_LocaleInit(); SMS_LocaleSet();
""" + override + r"""
 GUI_Initialize(1);
 config_block();                                     /* SMS_IOPInit */
 """ + reinit + r"""
 assert(inits==expected_inits);
 assert(applied.m_DisplayMode==expected_mode && g_Config.m_DisplayMode==expected_mode);
 assert(!memcmp(&applied,&g_Config,offsetof(SMSConfig,m_ImgOffs)+sizeof(short)));   /* nothing loaded late is left unapplied */
 assert(g_Config.m_BrowserFlags&SMS_BF_UDFL);        /* the installed SMS.lng is still accepted */
 return 0;
}
"""

D, HD, NTSC, PAL = "0xFF", "0x51", "0x02", "0x03"
cases = [  # name, argv[0], SMS.cfg, saved skin, saved mode, late device, card, GUI inits, final mode[, pad override]
    ("USB, saved skin", "mass:/SMS/SMS.ELF", "mass0:/SMS/SMS.cfg", "SMS", D, "mass0:", 1, 2, D),
    ("USB, saved 1080i", "mass:/SMS/SMS.ELF", "mass0:/SMS/SMS.cfg", "-", HD, "mass0:", 1, 2, HD),
    ("USB, saved picture position only", "mass:/SMS/SMS.ELF", "mass0:/SMS/SMS.cfg", "-", D, "mass0:", 1, 2, D),
    ("USB, first boot without SMS.cfg", "mass:/SMS/SMS.ELF", "-", "-", D, "mass0:", 1, 1, D),
    ("MMCE, saved skin", "mmce0:/APPS/SMS.ELF", "mmce0:/APPS/SMS.cfg", "SMS", D, "mmce0:", 1, 2, D),
    ("HDD pfs0:, config on the card", "pfs0:/SMS/SMS.ELF", "mc0:/SMS/SMS.cfg", "SMS", D, "-", 1, 2, D),
    ("HDD hdd0:, 1080i on the card", "hdd0:+SMS:pfs:/SMS.ELF", "mc0:/SMS/SMS.cfg", "-", HD, "-", 1, 2, HD),
    ("mass: that cannot be re-mounted, config on the card", "mass:/SMS/SMS.ELF", "mc0:/SMS/SMS.cfg", "SMS", D, "-", 1, 2, D),
    ("udpfs server unreachable, config on the card", "udpfs:/SMS/SMS.ELF", "mc0:/SMS/SMS.cfg", "SMS", D, "-", 1, 2, D),
    ("memory card", "mc0:/SMS/SMS.ELF", "mc0:/SMS/SMS.cfg", "SMS", HD, "-", 1, 1, HD),
    ("host:, config on the card", "host:SMS.ELF", "mc0:/SMS/SMS.cfg", "SMS", HD, "-", 1, 1, HD),
    ("host:, no card, config on USB", "host:SMS.ELF", "mass0:/SMS.cfg", "SMS", D, "mass0:", 0, 2, D),
    ("USB, saved 1080i, SELECT+R1", "mass:/SMS/SMS.ELF", "mass0:/SMS/SMS.cfg", "-", HD, "mass0:", 1, 2, NTSC, NTSC),
    ("USB, saved auto mode, SELECT+R2", "mass:/SMS/SMS.ELF", "mass0:/SMS/SMS.cfg", "-", D, "mass0:", 1, 2, PAL, PAL),
    ("HDD, 1080i on the card, SELECT+R2", "pfs0:/SMS/SMS.ELF", "mc0:/SMS/SMS.cfg", "-", HD, "-", 1, 2, PAL, PAL),
    ("host:, 1080i on the card, SELECT+R1", "host:SMS.ELF", "mc0:/SMS/SMS.cfg", "-", HD, "-", 1, 1, NTSC, NTSC),
    ("memory card, saved 1080i, SELECT+R1", "mc0:/SMS/SMS.ELF", "mc0:/SMS/SMS.cfg", "-", HD, "-", 1, 1, NTSC, NTSC),
]
# SMSPATH.TXT next to the ELF ( the last argument is the file's text ).
pointer_cases = [
    ("APP_SMS3 on the card, SMSPATH.TXT names mc0:/SMS3/", "mc0:/APP_SMS3/SMS3.ELF", "mc0:/SMS3/SMS.cfg", "SMS", HD, "-", 1, 1, HD, D, "mc0:/SMS3/\n"),
    ("SMSPATH.TXT 'mc:' is the boot card ( mc1 )", "mc1:/APP_SMS3/SMS3.ELF", "mc1:/SMS3/SMS.cfg", "SMS", HD, "-", 1, 1, HD, D, "mc:/SMS3"),
    ("SMSPATH.TXT from a PC editor ( BOM, CRLF, spaces )", "mc0:/APP_SMS3/SMS3.ELF", "mc0:/SMS3/SMS.cfg", "SMS", HD, "-", 1, 1, HD, D, "\ufeff  mc0:/SMS3  \r\nsecond line"),
    ("USB, SMSPATH.TXT read again after the late mount", "mass:/APPS/APP_SMS3/SMS3.ELF", "mass0:/SMSCFG/SMS.cfg", "-", HD, "mass0:", 1, 2, HD, D, "mass:/SMSCFG/"),
    ("SMSPATH.TXT without a device is ignored", "mc0:/APP_SMS3/SMS3.ELF", "mc0:/APP_SMS3/SMS.cfg", "SMS", HD, "-", 1, 1, HD, D, "SMS3/"),
    ("SMSPATH.TXT naming a card root is ignored", "mc0:/APP_SMS3/SMS3.ELF", "mc0:/APP_SMS3/SMS.cfg", "SMS", HD, "-", 1, 1, HD, D, "mc0:/"),
    ("empty SMSPATH.TXT is ignored", "mc0:/APP_SMS3/SMS3.ELF", "mc0:/APP_SMS3/SMS.cfg", "SMS", HD, "-", 1, 1, HD, D, " "),
]
if "_follow_cfg_pointer" in cfg:
    cases += pointer_cases
with tempfile.TemporaryDirectory() as temp:
    src = Path(temp) / "boot.c"
    src.write_text(harness)
    exe = Path(temp) / "boot"
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    "-Wall", "-Wno-unused-function", "-Wno-unused-variable", "-Werror", str(src), "-o", str(exe)], check=True)
    for name, *args in cases:
        if subprocess.run([str(exe), *map(str, args + [D] * (9 - len(args)))], timeout=10).returncode:
            sys.exit(f"boot config: {name}")
print(f"Boot config: {len(cases)} boot paths apply the saved config exactly once and keep a pad video-mode override")
