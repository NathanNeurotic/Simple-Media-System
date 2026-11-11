/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2005-2006 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#include "SMS.h"
#include "SMS_SPU.h"
#include "SMS_SIF.h"
#include "SMS_DMA.h"
#include "SMS_Config.h"
#include "SMS_Timer.h"

#include <kernel.h>
#include <iopheap.h>

#define USE_SIF2
#define SMS_AUDIO_RPC_ID  0x41534D53
#define SMS_VOLUME_RPC_ID 0x56534D53

static SPUContext         s_SPUCtx;
static SifRpcClientData_t s_ClientDataA __attribute__(   (  aligned( 64 )  )   );
static SifRpcClientData_t s_ClientDataV __attribute__(   (  aligned( 64 )  )   );
static unsigned int       s_Buffer[ 8 ] __attribute__(   (  aligned( 64 )  )   );
static int                s_SemaPCM;
static int                s_SemaVol;
#ifdef USE_SIF2
void SPU_DMAHandler ( int );
#endif  /* USE_SIF2 */
static int _Init ( void ) {

 ee_sema_t lSema;

 lSema.init_count = 0;
 lSema.max_count  = 1;
 s_SemaPCM = CreateSema ( &lSema );
 s_SemaVol = CreateSema ( &lSema );
#ifdef USE_SIF2
 AddSbusIntcHandler ( 15, SPU_DMAHandler );
#endif  /* USE_SIF2 */
 return SIF_BindRPC ( &s_ClientDataA, SMS_AUDIO_RPC_ID  ) &&
        SIF_BindRPC ( &s_ClientDataV, SMS_VOLUME_RPC_ID );

}  /* end _Init */
#ifdef USE_SIF2
__asm__(
 ".set noreorder\n\t"
 ".set nomacro\n\t"
 ".set noat\n\t"
 ".text\n\t"
 "SPU_DMAHandler:\n\t"
 "addiu     $sp, $sp, -16\n\t"
 "sw        $ra, 0($sp)\n\t"
 "lui       $a0, %hi( s_SemaPCM )\n\t"
 "jal       iSignalSema\n\t"
 "lw        $a0, %lo( s_SemaPCM )($a0)\n\t"
 "lw        $ra, 0($sp)\n\t"
 "jr        $ra\n\t"
 "nor       $v0, $zero, $zero\n\t"
 ".set at\n\t"
 ".set macro\n\t"
 ".set reorder\n\t"
);
#endif  /* USE_SIF2 */
/* THE WEDGED-SERVER HANG. A bound server is not the same as a LIVE one.
 *
 * Every wrapper here posts an RPC whose completion callback signals a sema, then waits on
 * that sema. If AUDSRV's single RPC server thread is itself stuck, the reply never comes and
 * the wait never ends -- the EE thread is gone, permanently, with whatever was last drawn
 * left on screen.
 *
 * AUDSRV can genuinely get stuck. Its UI-sound entry ( RPC func 4 ) takes a one-shot
 * semaphore and does NOT give it back; the only thing that returns that token is the SPU2
 * interrupt handler AUDSRV installs through libsd. Confirmed by disassembling the embedded
 * blob: func 4 calls thsemap ordinal 8 ( WaitSema, no timeout ) on the semaphore created
 * with init_count 1, and the sole matching iSignalSema ( ordinal 7 ) lives in the interrupt
 * handler. Drop ONE of those interrupts and the NEXT UI sound blocks forever, taking the
 * server thread and every later caller with it.
 *
 * Dropping one is possible on a PSX, whose libsd is the freesd we embed rather than Sony's:
 * its SPU2 handler returns early on a zero IRQINFO read without invoking the registered
 * handler, so the token is never released. That is fixed on the IOP side too ( see
 * tools/libsd/README-SMS.md ), but the EE must not depend on a driver being correct.
 *
 * So these waits are BOUNDED, and a timeout retires the audio server: every later call then
 * takes the server-NULL guards below and no-ops. SMS ends up silent instead of dead.
 *
 * NOT applied to SPU_PlayPCM. That runs once per audio buffer for the whole of playback, and
 * a polling wait there would burn the EE and starve the decoders on every console. It keeps
 * its blocking wait; it is also unreachable once a timeout has retired the server.
 */
#define SPU_RPC_TIMEOUT 3000   /* ms ( g_Timer units ) -- AUDSRV holds a UI-sound RPC until the previous sound ( up to ~0.9 s ) ends */

static int _spu_wait ( int aSema, SifRpcClientData_t* apClient ) {

 u64 lEnd = g_Timer + SPU_RPC_TIMEOUT;

 while (  PollSema ( aSema ) < 0  ) if ( g_Timer > lEnd ) {

  apClient -> server = NULL;   /* retire it: every wrapper below now no-ops */
  return 0;

 }  /* end if */

 return 1;

}  /* end _spu_wait */

/* THE UNBOUND-SERVER HANG, in every wrapper below.
 *
 * Each one fires an RPC with a completion callback that signals a sema, then WaitSema's on
 * it. If the server was never bound ( s_ClientData*.server == NULL ) SifCallRpc fails and
 * returns immediately, the callback never runs, and the WaitSema blocks forever on a sema
 * nothing will ever signal. Not a crash, not an error -- a permanent stop on whichever
 * thread called it, with the last drawn screen left standing.
 *
 * SPU_PlaySound already guards for exactly this ( see the note there ); the rest did not.
 * It is reachable for real now: SIF_BindRPC is BOUNDED since the PSX boot fix, so an AUDSRV
 * that registers too slowly no longer spins at boot -- it leaves a NULL server behind and
 * boot continues. Nothing then fails until playback, because the first thing playback does
 * for any file with audio is SPU_InitContext -> SPU_Destroy, which runs BEFORE the player
 * takes the screen. The console wedges with the file browser still displayed.
 *
 * Note the two servers bind independently ( _Init binds A then V ), so a machine can hold
 * a working audio server and a NULL volume server: UI sounds play, and the hang waits until
 * the first volume or silence call inside the player.
 *
 * When the server IS bound -- every console where audio works at all -- these guards change
 * nothing whatsoever. */
static void SPU_SetVolume ( int aVol ) {

 if ( !s_ClientDataV.server ) return;

 s_Buffer[ 0 ] = aVol;

 SifCallRpc (
  &s_ClientDataV, 0, SIF_RPC_M_NOWAIT, s_Buffer, 4, NULL, 0, (  void ( * )( void* )  )iSignalSema, ( void* )s_SemaVol
 );
 _spu_wait ( s_SemaVol, &s_ClientDataV );

}  /* end SPU_SetVolume */

static void SPU_Silence ( void ) {

 if ( !s_ClientDataV.server ) return;

 SifCallRpc (
  &s_ClientDataV, 1, SIF_RPC_M_NOWAIT, NULL, 0, NULL, 0, (  void ( * ) ( void* )  )iSignalSema, ( void* )s_SemaVol
 );
 _spu_wait ( s_SemaVol, &s_ClientDataV );

}  /* end Silence */

void SPU_Shutdown ( void ) {

 if ( !s_ClientDataV.server ) return;

 SifCallRpc (
  &s_ClientDataV, 2, SIF_RPC_M_NOWAIT, NULL, 0, NULL, 0, (  void ( * ) ( void* )  )iSignalSema, ( void* )s_SemaVol
 );
 _spu_wait ( s_SemaVol, &s_ClientDataV );

}  /* end SPU_Shutdown */
#ifdef USE_SIF2
static void SPU_PlayPCM ( void* apData ) {

 unsigned int lSize = *( unsigned int* )apData + 16U;

 lSize = (
  (    (   (  ( lSize + 15 ) >> 4  ) + 1  ) >> 1   ) << 1
 ) - 2;

 apData = ( unsigned int )apData << 4;
 apData = ( unsigned int )apData >> 4;

/* Same hang, one layer lower: this path does not use RPC at all, it hands the buffer to the
 * IOP over SIF2 and waits for AUDSRV to raise the SBUS interrupt that signals s_SemaPCM. No
 * AUDSRV, no interrupt, and the audio renderer thread stops here for good. The server handle
 * is the same precondition either way, so it is the test used.
 *
 * Returning early is a stop-gap, not silent playback done properly: the renderer will then
 * drain packets as fast as they decode and m_AudioTime advances off the packet PTS rather
 * than off real playback, so video paced against it runs fast. That is a bad picture instead
 * of a dead console -- worth having, still worth fixing properly by not selecting an audio
 * stream at all when there is no audio server. */
 if ( !s_ClientDataA.server ) return;

 DMA_SendA ( DMAC_SIF2, apData, 2 );
 Interrupt2Iop ( 1 );
 WaitSema ( s_SemaPCM );

 if ( lSize ) {

  DMA_SendA (   DMAC_SIF2, (  ( char* )apData  ) + 32, lSize   );
  WaitSema ( s_SemaPCM );

 }  /* end if */

}  /* end SPU_PlayPCM */
#else
static void SPU_PlayPCM ( void* apBuf ) {

 SifCallRpc (
  &s_ClientDataA, 2, SIF_RPC_M_NOWBDC | SIF_RPC_M_NOWAIT,
  apBuf, *( int* )apBuf + 16, NULL, 0,
  (  void ( * ) ( void* )  )iSignalSema, ( void* )s_SemaPCM
 );
 WaitSema ( s_SemaPCM );

}  /* end SPU_PlayPCM */
#endif  /* USE_SIF2 */
static void SPU_Destroy ( void ) {

 if ( !s_ClientDataA.server ) return;

 SifCallRpc (
  &s_ClientDataA, 1, SIF_RPC_M_NOWBDC | SIF_RPC_M_NOWAIT, NULL, 0, NULL, 0, (  void ( * ) ( void* )  )iSignalSema, ( void* )s_SemaPCM
 );
 _spu_wait ( s_SemaPCM, &s_ClientDataA );

}  /* end SPU_Destroy */

void SPU_LoadData ( void* apData, int aSize ) {

 void*            lpIOPData;
 SifDmaTransfer_t lXfrData;
 int              lID;

/* Guarded BEFORE the IOP heap allocation, not after: with no audio server this would
 * otherwise reserve IOP memory, DMA the UI sound bank into it, and then wedge on the
 * WaitSema still holding the allocation. */
 if ( !s_ClientDataA.server ) return;

 lpIOPData = SifAllocIopHeap ( aSize );

 lXfrData.src  = apData;
 lXfrData.dest = lpIOPData;
 lXfrData.size = aSize;
 lXfrData.attr = 0;

 lID = SifSetDma ( &lXfrData, 1 );

 s_Buffer[ 0 ] = ( unsigned int )lpIOPData;
 s_Buffer[ 1 ] = aSize;

 while (  SifDmaStat ( lID ) >= 0  );

 SifCallRpc (
  &s_ClientDataA, 3, SIF_RPC_M_NOWAIT, s_Buffer, 8, NULL, 0, (  void ( * ) ( void* )  )iSignalSema, ( void* )s_SemaPCM
 );
 _spu_wait ( s_SemaPCM, &s_ClientDataA );

 SifFreeIopHeap ( lpIOPData );

}  /* end SPU_LoadData */

int SPU_Index2Volume ( int anIdx ) {

 static unsigned s_lScale[ 25 ] = {
      0,   300,   800,  1120,  1600,
   2140,  2660,  3060,  3460,  4380,
   5100,  6020,  7040,  8160,  8980,
  10500, 11940, 13980, 16000, 18160,
  20900, 24060, 27120, 29880, 32767
 };

 return s_lScale[ SMS_clip ( anIdx, 0, 24 ) ];

}  /* end SPU_Index2Volume */

void SPU_PlaySound ( SMSound* apSound, int aVol ) {

/* Also require the audio RPC to be BOUND ( s_ClientDataA.server != NULL, the same test
 * SPU_Initialize uses ). A UI sound can be requested before SPU_Initialize / AUDSRV load
 * -- e.g. a GUI_Error modal raised from the early config-resolution path in SMS_IOPInit,
 * which runs before AUDSRV -- and firing the RPC then would SifCallRpc an unbound server
 * and WaitSema an uncreated sema ( s_SemaPCM == 0 ): a hang. This was latent while SDFX
 * defaulted OFF; it is live now that it defaults ON. A pre-init sound simply no-ops. */
 if ( ( g_Config.m_BrowserFlags & SMS_BF_SDFX ) && s_ClientDataA.server ) {

  s_Buffer[ 0 ] = apSound -> m_Sound;
  s_Buffer[ 1 ] = SPU_Index2Volume ( aVol );
  s_Buffer[ 2 ] = apSound -> m_Size;

  SifCallRpc (
   &s_ClientDataA, 4, SIF_RPC_M_NOWAIT, s_Buffer, 12, NULL, 0, (  void ( * ) ( void* )  )iSignalSema, ( void* )s_SemaPCM
  );
  _spu_wait ( s_SemaPCM, &s_ClientDataA );

 }  /* end if */

}  /* end SPU_PlaySound */

void SPU_Initialize ( void ) {

 if ( !s_ClientDataA.server ) _Init ();

}  /* end SPU_Initialize */

SPUContext* SPU_InitContext ( int anChannels, int aFreq, int aVolume, int aBase, int aRatio ) {

 SPU_Destroy ();

 s_Buffer[ 0 ] = aFreq;
 s_Buffer[ 1 ] = 16;
 s_Buffer[ 2 ] = anChannels;
 s_Buffer[ 3 ] = aVolume;
 s_Buffer[ 4 ] = aBase;
 s_Buffer[ 5 ] = aRatio;

/* Still returns a context when the server is missing -- every caller dereferences the result
 * without checking ( s_Player.m_pSPUCtx -> Silence (), -> SetVolume (), -> PlayPCM () ), so
 * handing back NULL would trade a hang for a null dereference. The methods it is filled with
 * are the guarded ones above, which no-op. s_Buffer[ 0 ] is zeroed rather than left holding
 * whatever the previous call put there, so m_BufTime is 0 instead of a stale figure. */
 if ( s_ClientDataA.server ) SifCallRpc ( &s_ClientDataA, 0, 0, s_Buffer, 32, s_Buffer, 4, NULL, NULL );
 else                        s_Buffer[ 0 ] = 0;

 s_SPUCtx.m_BufTime = (  ( float )s_Buffer[ 0 ] * 1000.0F  ) / (  ( aFreq << 1 ) * anChannels  );
 s_SPUCtx.PlayPCM   = SPU_PlayPCM;
 s_SPUCtx.SetVolume = SPU_SetVolume;
 s_SPUCtx.Destroy   = SPU_Destroy;
 s_SPUCtx.Silence   = SPU_Silence;

 return &s_SPUCtx;

}  /* end SPU_InitContext */
