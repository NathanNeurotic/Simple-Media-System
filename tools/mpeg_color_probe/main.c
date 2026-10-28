/* Decoder-only control: write macroblock-ordered pixels through host:. */
#include <kernel.h>
#include <tamtypes.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "libmpeg.h"
#include "libmpeg_internal.h"

#ifndef COLOR16
#define COLOR16 0
#endif
#ifndef CAPTURE_FRAMES
#define CAPTURE_FRAMES 5
#endif
#if CAPTURE_FRAMES < 1 || CAPTURE_FRAMES > 600
#error CAPTURE_FRAMES must be between 1 and 600
#endif
extern unsigned char probe_m2v[];
extern unsigned int size_probe_m2v;
static unsigned sent, transfers;
static s64 pts;
static void *frame, *input;
static unsigned bytes;
static unsigned padded_input_size;

static int feed ( void* unused ) {
 volatile unsigned* chcr = ( volatile unsigned* )0x1000B400;
 ( void )unused;
 unsigned chunk;
 if ( sent == padded_input_size ) return 0;
 chunk = padded_input_size - sent;
 if ( chunk > 65535 * 16 ) chunk = 65535 * 16;
 while ( *chcr & 0x100 ) {}
 *( volatile unsigned* )0x1000B410 = ( ( unsigned )input + sent ) & 0x1FFFFFFF;
 *( volatile unsigned* )0x1000B420 = chunk / 16;
 sent += chunk;
 ++transfers;
 *chcr = 0x101;
 return 1;
}

static void* sequence ( void* unused, MPEGSequenceInfo* info ) {
 ( void )unused;
 bytes = info -> m_Width * info -> m_Height * ( COLOR16 ? 2 : 4 );
 frame = memalign ( 128, bytes );
 printf ( "[sms-color] sequence=%dx%d bytes=%u mode=%d\n", info -> m_Width, info -> m_Height, bytes, COLOR16 ? 16 : 32 );
 if ( !frame ) { printf ( "[sms-color] allocation failed\n" ); fflush ( stdout ); SleepThread (); }
 return frame;
}

int main ( void ) {
 int count = 0;
 unsigned input_size = ( size_probe_m2v + 15 ) & ~15;
 FILE* out;
 if ( !size_probe_m2v || size_probe_m2v > 8 * 1024 * 1024 ) {
  printf ( "[sms-color] use a nonempty stream of at most 8 MiB\n[sms-color] exit=2 frames=0\n" );
  fflush ( stdout );
  SleepThread ();
 }
 input = memalign ( 128, input_size );
 if ( !input ) { printf ( "[sms-color] input allocation failed\n" ); fflush ( stdout ); SleepThread (); }
 memset ( input, 0, input_size );
 memcpy ( input, probe_m2v, size_probe_m2v );
 padded_input_size = input_size;
 printf ( "[sms-color] input=%u target_frames=%d\n", size_probe_m2v, CAPTURE_FRAMES );
 FlushCache ( 0 );
 MPEG_Initialize ( feed, 0, sequence, 0, &pts );
 _MPEG_Set16 ( COLOR16 );
 out = fopen ( COLOR16 ? "host:decoded16.rgb16" : "host:decoded.rgba", "wb" );
 if ( !out ) { printf ( "[sms-color] output open failed\n" ); fflush ( stdout ); SleepThread (); }
 while ( count < CAPTURE_FRAMES && MPEG_Picture ( frame, &pts ) ) {
  if ( fwrite ( ( void* )( ( unsigned )frame | 0x20000000 ), 1, bytes, out ) != bytes ) break;
  printf ( "[sms-color] frame=%d bytes=%u\n", count++, bytes );
  fflush ( stdout );
 }
 fclose ( out );
 MPEG_Destroy ();
 printf ( "[sms-color] exit=%d frames=%d transfers=%u\n", count == CAPTURE_FRAMES ? 0 : 1, count, transfers );
 fflush ( stdout );
 /* Keep the DMA backing allocation alive until the test process is stopped. */
 SleepThread ();
 return 0;
}
