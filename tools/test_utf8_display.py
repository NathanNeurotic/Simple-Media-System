"""Accented names (Spanish n-tilde, a-acute ...) must draw as one glyph each, everywhere.

USB file names reach SMS as UTF-8, but the font is a single-byte codepage, so each
accented letter is two bytes. Any display site that draws the raw bytes shows two
wrong glyphs per letter ("CanciÃ³n"), and the extra width makes the fit-to-width
loops shrink the text or cut the tail off the name.

This builds the production status line, player OSD title/playlist, browser row,
picture-viewer caption and DisplayUTF8 itself against the real font widths (SMS_GSFont.c) and the real
TranslateUTF8, and requires each UTF-8 name to draw exactly like the same name
already in Windows-1252. ASCII and legacy 1252 names must pass through untouched.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = {name: (root / 'src' / name).read_text(encoding='latin-1') for name in (
    'SMS_GS_2.c', 'SMS_GUIDesktop.c', 'SMS_PlayerControl.c', 'SMS_GUIFileMenu.c', 'SMS_JPEG.c')}
icons = '\n'.join(line for line in (root / 'include/SMS_GUIcons.h').read_text().splitlines()
                  if re.match(r'#define GUICON_\w+\s+\d', line))
gsp_size = re.search(r'#define PC_GSP_SIZE.*', (root / 'include/SMS_PlayerControl.h').read_text()).group(0)


def function(text, name):
    """Extract a production function by brace matching rather than re-implementing it."""
    match = re.search(r'^[A-Za-z_][^\n;{}()]*\b' + name + r'\s*\([^;{}]*\)\s*\{', text, re.M)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end] + '\n'


def data(name):
    """A whole production source; the EE-only .data section placement is dropped for the host."""
    return re.sub(r'__attribute__\(\s*\(\s*section\(\s*"\.data"\s*\)\s*\)\s*\)', '',
                  (root / 'src' / name).read_text(encoding='latin-1')) + '\n'


def c_string(raw):
    return '"' + ''.join(chr(b) if 32 <= b < 127 and b not in b'"\\?' else '\\%03o' % b for b in raw) + '"'


# The OSD packet's GS_XYZ call is EE assembly; any deterministic Y keeps the comparison fair.
osd = function(src['SMS_PlayerControl.c'], 'PlayerControl_GSPacket')
osd, count = re.subn(r'__asm__ __volatile__\s*\(.*?\n\s*\);', 'lY1 = lY << 4; lY2 = ( lY + lH ) << 4;', osd, flags=re.S)
assert count == 1
item = re.search(r'typedef struct _FileMenuItem \{.*?\} _FileMenuItem;', src['SMS_GUIFileMenu.c'], re.S).group(0)

NAMES = [
    'Canción Ñandú.mp3',
    'Español_ñ.avi',
    'ÁÉÍÓÚÜÑ áéíóúüñ',
    'Arifureta - Episodio 3 - Canción Español.avi',
    'Pequeña niña - Corazón (versión acústica en directo, año 2006).mp3',
    'El niño, la cigüeña y el pingüino - Capítulo 12 - Edición especial remasterizada.avi',
    'mass:/Música/Películas en español/Canción Ñandú/',
]
ASCII = ['Cancion Nandu.mp3', 'Espanol_n.avi', 'mass:/Musica/Peliculas/']

cases = []
for name in NAMES:
    cases.append('{%s,%s}' % (c_string(name.encode('utf-8')), c_string(name.encode('cp1252'))))
for name in ASCII:
    cases.append('{%s,%s}' % (c_string(name.encode()), c_string(name.encode())))

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SMS_GS.h"
#include "SMS_List.h"
''' + icons + '\n' + gsp_size + r'''
#undef UNCACHED_SEG
#define UNCACHED_SEG( p ) ( p )
#define SyncDCache( a, b ) ( ( void )0 )
#define DMA_Wait( c ) ( ( void )0 )
#define VIF_DIRECT( n ) ( n )
#define SMS_SyncMalloc malloc
#define SMS_BF_AVIF 0x00000004
typedef enum { GUIcon_Browser = 0 } GUIcon;
GSContext g_GSCtx;
char      g_GSCharWidth[ 224 ];
int       g_XShift;
static struct { unsigned int m_BrowserFlags; } g_Config;
static GSParams s_Params;
GSParams* GS_Params ( void ) { return &s_Params; }

/* What the last GSFont_RenderEx call was asked to draw. */
static char s_Drawn[ 1024 ];
static int  s_DrawnDW;
void GSFont_RenderEx ( char* apStr, int aLen, int aX, int anY, u64* apDMA, int aDW, int aDH ) {
 (void)aX;(void)anY;(void)apDMA;(void)aDH;
 assert( aLen >= 0 && aLen < ( int )sizeof ( s_Drawn ) );
 memcpy ( s_Drawn, apStr, aLen ); s_Drawn[ aLen ] = 0; s_DrawnDW = aDW;
}
static u64 s_Scratch[ 4096 ];
u64* GSContext_NewPacket ( int c, int n, GSPaintMethod m ) { (void)c;(void)n;(void)m;return s_Scratch; }
u64* GSContext_NewList ( unsigned int n ) { return calloc ( n, sizeof ( u64 ) ); }
void GSContext_DeleteList ( u64* p ) { free ( p ); }
void GSContext_Flush ( int c, GSFlushMethod m ) { (void)c;(void)m; }
void GSContext_CallList ( int c, u64* p ) { (void)c;(void)p; }
void GS_VSync ( void ) {}
void GUI_DrawIcon ( unsigned int i, int x, int y, GUIcon g, u64* p ) { (void)i;(void)x;(void)y;(void)g;(void)p; }
static void SMS_GUIClockSuspend ( void ) {}
static void SMS_GUIClockResume ( void ) {}
static void* SMS_ReallocWithAlign ( void* p, unsigned int* o, unsigned int n ) { (void)o;return realloc ( p, n ); }
typedef struct StatusLine { void ( *Render ) ( struct StatusLine*, int ); } StatusLine;
static void _sl_render ( StatusLine* p, int c ) { (void)p;(void)c; }
static StatusLine  s_SL = { _sl_render };
static StatusLine* g_pStatusLine = &s_SL;
static char         s_CWD[ 1024 ];   /* g_CWD lives inside g_SMSounds on the EE */
#define g_CWD s_CWD
static u64*         s_pDMASL;
static unsigned int s_nDMASL;
typedef struct { char* m_pStr; int m_Len; } SMString;
static SMString STR_LOADING = { "Loading %s...", 13 };
static char g_pQuote[] = "\"", g_pPeriod[] = "...";
typedef struct {
 unsigned int m_StsW; int m_StsX, m_StsY;
 u64 *m_pStsText, *m_pStsBkgnd, *m_pStsOutline, *m_pStsPgBg;
} SMS_JPEGViewer;
''' + item + '\n'

post = r'''
''' + data('SMS_GSFont.c') + r'''

static const struct { const char* m_pUTF8; const char* m_p1252; } s_Cases[] = { ''' + ',\n'.join(cases) + r''' };

static SMS_ListNode* _node ( const char* apStr ) {
 SMS_ListNode* p = calloc ( 1, sizeof ( *p ) + strlen ( apStr ) + 1 );
 strcpy ( _STR( p ), apStr ); return p;
}

typedef struct { char m_Text[ 1024 ]; int m_DW; } Drawn;

static Drawn _status ( const char* apStr ) {
 Drawn d;
 strcpy ( g_CWD, apStr ); GUI_Status ( g_CWD );
 strcpy ( d.m_Text, s_Drawn ); d.m_DW = s_DrawnDW; return d;
}

static Drawn _browser ( const char* apStr ) {
 Drawn d; SMS_ListNode* p = _node ( apStr ); _FileMenuItem* i;
 i = GUIFileMenu_RenderItem ( p, 100, g_GSCtx.m_Width - 72, 0 );
 assert( !strcmp ( _STR( p ), apStr ) );   /* display only: fioOpen keeps the real name */
 strcpy ( d.m_Text, s_Drawn ); d.m_DW = s_DrawnDW;
 free ( i -> m_pTxtPack ); free ( i ); free ( p ); return d;
}

static Drawn _viewer ( const char* apStr ) {
 Drawn d; SMS_JPEGViewer v; memset ( &v, 0, sizeof ( v ) );
 v.m_StsW = ( unsigned int )( g_GSCtx.m_Width * 0.9F + 0.5F );
 _display_text ( &v, apStr );
 strcpy ( d.m_Text, s_Drawn ); d.m_DW = s_DrawnDW;
 free ( v.m_pStsText ); return d;
}

/* The OSD packet holds one sprite per glyph; decode the glyph bytes back from the UVs and
 * keep the whole packet so the per-glyph X advance ( the shrink factor ) is compared too. */
typedef struct { unsigned int m_QWC; unsigned int m_Len; u64 m_DMA[ 2048 ]; char m_Text[ 1024 ]; } OSD;

static void _osd ( SMS_List* apList, OSD* apOut ) {
 unsigned int i, lSize = PlayerControl_GSPLen ( apList, 0 );
 u64* lpDMA = calloc ( lSize, sizeof ( u64 ) );   /* exactly what SMS_Player allocates */
 memset ( apOut, 0, sizeof ( *apOut ) );
 apOut -> m_QWC = PlayerControl_GSPacket ( 64, apList, lpDMA );
 apOut -> m_Len = ( ( apOut -> m_QWC << 1 ) - 6 ) >> 2;
 assert( ( apOut -> m_QWC << 1 ) <= lSize && apOut -> m_Len < sizeof ( apOut -> m_Text ) );
 memcpy ( apOut -> m_DMA, lpDMA, ( apOut -> m_QWC << 1 ) * sizeof ( u64 ) );
 for ( i = 0; i < apOut -> m_Len; ++i ) {
  u64 lUV = lpDMA[ 6 + i * 4 ];
  unsigned int lU = ( unsigned int )( lUV & 0xFFFF ) - 8, lV = ( unsigned int )( lUV >> 16 & 0xFFFF ) - 8;
  apOut -> m_Text[ i ] = ( char )(  ( ( lU >> 9 ) | ( ( lV >> 9 ) << 4 ) ) + ' '  );
 }
 free ( lpDMA );
}

static SMS_List* _list ( const char* const* apNames, int aCount ) {
 SMS_List* l = calloc ( 1, sizeof ( *l ) ); int i;
 for ( i = 0; i < aCount; ++i ) {
  SMS_ListNode* p = _node ( apNames[ i ] );
  if ( l -> m_pTail ) l -> m_pTail -> m_pNext = p; else l -> m_pHead = p;
  p -> m_pPrev = l -> m_pTail; l -> m_pTail = p; ++l -> m_Size;
 }
 return l;
}

static void _free_list ( SMS_List* l ) {
 SMS_ListNode* p = l -> m_pHead;
 while ( p ) { SMS_ListNode* n = p -> m_pNext; free ( p ); p = n; }
 free ( l );
}

static int s_nFail;

static void _check ( const char* apSite, const char* apName, const Drawn* apGot, const Drawn* apWant ) {
 if ( strcmp ( apGot -> m_Text, apWant -> m_Text ) || apGot -> m_DW != apWant -> m_DW ) {
  printf ( "FAIL %-8s %s\n     drawn  [%s] dw=%d\n     wanted [%s] dw=%d\n",
           apSite, apName, apGot -> m_Text, apGot -> m_DW, apWant -> m_Text, apWant -> m_DW );
  ++s_nFail;
 }
}

int main ( void ) {
 unsigned int i, j;
 /* GSFont_Init: 96 widths from the ASCII font, 128 from the active codepage font, and the
  * space borrows ':' -- the width is byte 11 + index of each font, as GSFont_UnpackChr reads it. */
 g_GSCtx.m_CodePage = GSCodePage_WinLatin1;
 for ( i = 0; i < 96;  ++i ) g_GSCharWidth[ i      ] = g_ASCII[ 11 + i ];
 for ( i = 0; i < 128; ++i ) g_GSCharWidth[ 96 + i ] = g_1252 [ 11 + i ];
 g_GSCharWidth[ 0 ] = g_GSCharWidth[ ':' - ' ' ];
 g_GSCtx.m_Width = 640; g_GSCtx.m_Height = 512; g_GSCtx.m_FontTexFmt = GSPixelFormat_PSMT4HL;
 s_Params.m_GSCRTMode = 2;

 for ( i = 0; i < sizeof ( s_Cases ) / sizeof ( s_Cases[ 0 ] ); ++i ) {
  const char* lpU = s_Cases[ i ].m_pUTF8;
  const char* lpL = s_Cases[ i ].m_p1252;
  Drawn lGot, lWant;
  OSD   lOU, lOL;
  SMS_List *lpUL, *lpLL;

  lGot = _status ( lpU ); lWant = _status ( lpL ); _check ( "status", lpL, &lGot, &lWant );
  lGot = _browser ( lpU ); lWant = _browser ( lpL ); _check ( "browser", lpL, &lGot, &lWant );
  g_Config.m_BrowserFlags = SMS_BF_AVIF;
  lGot = _browser ( lpU ); lWant = _browser ( lpL ); _check ( "browser", lpL, &lGot, &lWant );
  g_Config.m_BrowserFlags = 0;
  lGot = _viewer ( lpU ); lWant = _viewer ( lpL ); _check ( "viewer", lpL, &lGot, &lWant );

  /* Menu items convert a bounded copy in place ( the subtitle mini-browser ). */
  {
   char lBuf[ 320 ];
   strcpy ( lBuf, lpU );
   if ( strcmp ( DisplayUTF8 ( lBuf, lBuf, sizeof ( lBuf ) ), lpL ) ) { printf ( "FAIL in-place %s\n", lpL ); ++s_nFail; }
   if ( DisplayUTF8 ( lpL, lBuf, sizeof ( lBuf ) ) != lpL ) { printf ( "FAIL 1252/ASCII not passed through %s\n", lpL ); ++s_nFail; }
  }

  /* A legacy 1252 name ( or ASCII ) is drawn byte for byte. */
  lWant = _status ( lpL );
  if ( strncmp ( lWant.m_Text, lpL, strlen ( lWant.m_Text ) ) ) { printf ( "FAIL 1252 passthrough %s\n", lpL ); ++s_nFail; }

  /* Player OSD: the single-file title, then the same name inside an M3U-style list. */
  for ( j = 0; j < 2; ++j ) {
   const char* lU[ 3 ] = { lpU, "Track 02.mp3", lpU };
   const char* lL[ 3 ] = { lpL, "Track 02.mp3", lpL };
   lpUL = _list ( lU, j ? 3 : 1 ); lpLL = _list ( lL, j ? 3 : 1 );
   _osd ( lpUL, &lOU ); _osd ( lpLL, &lOL );
   if ( lOU.m_QWC != lOL.m_QWC || memcmp ( lOU.m_DMA, lOL.m_DMA, ( lOL.m_QWC << 1 ) * sizeof ( u64 ) ) ) {
    printf ( "FAIL osd      %s\n     drawn  [%.*s]\n     wanted [%.*s]\n", lpL, lOU.m_Len, lOU.m_Text, lOL.m_Len, lOL.m_Text );
    ++s_nFail;
   }
   _free_list ( lpUL ); _free_list ( lpLL );
  }
 }

 /* Locale messages are already codepage text and must not be converted, not even the one
  * shipped line ( SMS.lng_PL, CP1250 O-acute L-stroke ) whose bytes are also valid UTF-8. */
 {
  char lMsg[] = "X/K\323\243KO - wykonaj/dalej";
  GUI_Status ( lMsg );
  if ( strcmp ( s_Drawn, lMsg ) ) { printf ( "FAIL status converted a locale message\n" ); ++s_nFail; }
 }

 free ( s_pDMASL );
 if ( s_nFail ) { printf ( "%d UTF-8 display mismatches\n", s_nFail ); return 1; }
 puts ( "UTF-8 display: status line, browser row, picture caption, menu items and player OSD draw accented UTF-8 names exactly like their 1252 form; ASCII and 1252 untouched" );
 return 0;
}
'''

with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    (tmp / 'tamtypes.h').write_text('#pragma once\n#include <stdint.h>\ntypedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;'
                                    'typedef uint64_t u64;typedef int8_t s8;typedef int16_t s16;typedef int32_t s32;'
                                    'typedef int64_t s64;typedef struct { u64 lo, hi; } u128;\n')
    (tmp / 'SMS.h').write_text('#define SMS_INLINE static inline\n')
    source = (prefix + data('SMS_InverseCodePages.c')
              + function(src['SMS_GS_2.c'], 'GSFont_WidthEx')
              + function(src['SMS_GUIDesktop.c'], 'GUI_Status')
              + function(src['SMS_PlayerControl.c'], 'PlayerControl_GSPLen') + osd
              + function(src['SMS_GUIFileMenu.c'], 'GUIFileMenu_RenderItem')
              + function(src['SMS_JPEG.c'], '_display_text') + post)
    fixture = tmp / 'utf8_display.c'
    fixture.write_text(source, encoding='latin-1')
    exe = tmp / 'utf8_display'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu99', '-g', '-no-pie', '-Wall', '-Wno-unused-function',
                    '-fsanitize=address,undefined', '-fno-sanitize=shift-base', '-fno-sanitize-recover=all',
                    '-I', str(tmp), '-I', str(root / 'include'), '-I', str(root / 'src'),
                    str(fixture), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
