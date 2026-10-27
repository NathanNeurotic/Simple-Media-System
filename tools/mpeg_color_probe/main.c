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
extern unsigned char probe_m2v[];
extern unsigned int size_probe_m2v;
static int sent;
static s64 pts;
static void *frame, *input;
static unsigned bytes;

static int feed ( void* unused ) {
 volatile unsigned* chcr = ( volatile unsigned* )0x1000B400;
 ( void )unused;
 if ( sent ) return 0;
 sent = 1;
 while ( *chcr & 0x100 ) {}
 *( volatile unsigned* )0x1000B410 = ( unsigned )input & 0x1FFFFFFF;
 *( volatile unsigned* )0x1000B420 = ( size_probe_m2v + 15 ) / 16;
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
 if ( !size_probe_m2v || size_probe_m2v > 65535 * 16 ) {
  printf ( "[sms-color] use a nonempty stream of at most 1048560 bytes (one DMA transfer)\n[sms-color] exit=2 frames=0\n" );
  fflush ( stdout );
  SleepThread ();
 }
 input = memalign ( 128, input_size );
 if ( !input ) { printf ( "[sms-color] input allocation failed\n" ); fflush ( stdout ); SleepThread (); }
 memset ( input, 0, input_size );
 memcpy ( input, probe_m2v, size_probe_m2v );
 FlushCache ( 0 );
 MPEG_Initialize ( feed, 0, sequence, 0, &pts );
 _MPEG_Set16 ( COLOR16 );
 out = fopen ( COLOR16 ? "host:decoded16.rgb16" : "host:decoded.rgba", "wb" );
 if ( !out ) { printf ( "[sms-color] output open failed\n" ); fflush ( stdout ); SleepThread (); }
 while ( count < 5 && MPEG_Picture ( frame, &pts ) ) {
  if ( fwrite ( ( void* )( ( unsigned )frame | 0x20000000 ), 1, bytes, out ) != bytes ) break;
  printf ( "[sms-color] frame=%d bytes=%u\n", count++, bytes );
  fflush ( stdout );
 }
 fclose ( out );
 MPEG_Destroy ();
 printf ( "[sms-color] exit=%d frames=%d\n", count == 5 ? 0 : 1, count );
 fflush ( stdout );
 /* Keep the DMA backing allocation alive until the test process is stopped. */
 SleepThread ();
 return 0;
}
