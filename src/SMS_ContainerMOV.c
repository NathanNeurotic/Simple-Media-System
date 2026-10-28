/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# Copyright (c) 2001 Fabrice Bellard.
#               2008 - Adopted for SMS by Eugene Plotnikov
# Licensed (like the original ffmpeg source code) under the terms of the
# GNU Lesser General Public License as published by the Free Software Foundation;
# either version 2 of the License, or (at your option) any later version.
# 
*/
#include "SMS.h"
#include "SMS_ContainerMOV.h"
#include "SMS_FourCC.h"
#include "SMS_Locale.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct MOVIndex {
 uint32_t m_Flags;
 uint32_t m_DTS;
 uint32_t m_Pos;
 uint32_t m_Size;
} MOVIndex;

typedef struct {
 int m_Count;
 int m_Duration;
} MOVStts;

typedef struct {
 int m_First;
 int m_Count;
 int m_ID;
} MOVStsc;

typedef struct {
 unsigned int m_Type;
 int64_t      m_Offset;
 int64_t      m_Size;
} MOVAtom;

typedef struct {
 int64_t m_Offset;
 int64_t m_Size;
} MOVMDat;

typedef struct MOVStream {
 int          m_nIdx;
 MOVIndex*    m_pIdx;
 uint32_t     m_IdxAllocSize;
 int          m_NextChunk;
 unsigned int m_nChunks;
 int64_t*     m_pChunkOffsets;
 unsigned int m_nStts;
 MOVStts*     m_pStts;
 unsigned int m_nCtts;
 MOVStts*     m_pCtts;
 unsigned int m_nSample2Chunk;
 MOVStsc*     m_pSample2Chunk;
 unsigned int m_Sample2CTimeIdx;
 int          m_Sample2CTimeSample;
 unsigned int m_SampleSize;
 unsigned int m_nSamples;
 int*         m_pSampleSize;
 unsigned int m_nKeyFrames;
 int*         m_pKeyFrames;
 int          m_TimeScale;
 int          m_TimeRate;
 unsigned int m_CurSample;
 unsigned int m_BytesPerFrame;
 unsigned int m_SamplesPerFrame;
 int64_t      m_TimeOffset;       /* Edited timeline offset, in indexed time-base units. */
 int64_t      m_DTSShift;         /* Decode preroll for signed composition offsets. */
 int64_t      m_EditEmpty;        /* Initial empty edit, in movie timescale units. */
 int64_t      m_EditStart;        /* First media time, in track timescale units. */
 int64_t      m_EditDuration;     /* Media edit duration, in movie timescale units. */
 int          m_HasEdit;
} MOVStream;

struct MOVContext;

typedef int ( *mov_parse_func ) ( struct MOVContext*, FileContext*, MOVAtom* );

typedef struct MOVParseEntry {
 uint32_t       m_Type;
 mov_parse_func m_Func;
} MOVParseEntry;

typedef struct MOVContext {
 SMS_Container*       m_pBase;
 int                  m_TimeScale;
 int64_t              m_Duration;
 int64_t              m_MDATOffset;
 int                  m_nMDAT;
 MOVMDat*             m_pMDAT;
 const MOVParseEntry* m_pParseTbl;
 char                 m_fMOOV;
 char                 m_fMDAT;
 char                 m_fISOM;
 char                 m_fError;
} MOVContext;

static uint32_t _read_int_be ( FileContext* apFileCtx ) {
 uint32_t retVal = File_GetUInt ( apFileCtx );
 __asm__ __volatile__(
  "dsll32   %0, %0, 0\n\t"
  "dsrl32   %1, %0, 0\n\t"
  : "=r"( retVal ) : "r"( retVal )
 );
 return SMS_bswap32 ( retVal );
}  /* end _read_int_be */

static uint64_t _read_long_be ( FileContext* apFileCtx ) {
 uint64_t lHigh  = SMS_bswap32 (  File_GetUInt ( apFileCtx )  );
 uint64_t lLow   = SMS_bswap32 (  File_GetUInt ( apFileCtx )  );
 return ( lHigh << 32 ) | lLow;
}  /* end _read_long_be */

static void _free ( void* apPtr ) {
 if ( apPtr ) free ( apPtr );
}  /* end _free */

/* A sample-table entry count comes straight out of the file. The UINT_MAX / sizeof tests in the
 * readers below only stop the multiply from wrapping -- a crafted or merely corrupt count of a
 * few hundred million still asks malloc for hundreds of megabytes on a 32 MB heap, and most of
 * those readers then write through the returned pointer without ever looking at it. Both halves
 * are closed here: the count is cross-checked against the space the atom actually declares
 * ( every one of these tables stores fixed-width entries, so the declared size is a hard ceiling
 * on how many can be present ), and each allocation is checked. m_Size is already the declared
 * size minus the 8-byte atom header, and the version/flags word plus the entry count consume 8
 * more -- but stsz reads a sample size on top of that, so the caller passes what it actually
 * consumed. aWidth is the ON-DISK width, which is not always the in-memory struct width --
 * stco stores 4-byte offsets into an 8-byte int64_t, co64 stores 8.
 * Degrading means zero the count and return WITHOUT skipping: _mov_read_dflt re-syncs to the end
 * of the atom from wherever its handler left the cursor, so the remaining traks still parse. The
 * previous hand-rolled degrade paths skipped apAtom->m_Size themselves, which overshot the atom
 * end by the 8 bytes already consumed and desynced the very dispatcher that was about to fix
 * things up. */
static int _tbl_fits ( const MOVAtom* apAtom, unsigned int anEntries, unsigned int aWidth, unsigned int aConsumed ) {
 int64_t lAvail = apAtom -> m_Size - aConsumed;
 return lAvail >= 0 && ( int64_t )anEntries * aWidth <= lAvail;
}  /* end _tbl_fits */

static int _mov_read_dflt ( MOVContext*, FileContext*, MOVAtom* );

static void _mov_free_tables ( MOVStream* apStm, int afChunks ) {
 if ( afChunks ) { free ( apStm -> m_pChunkOffsets ); apStm -> m_pChunkOffsets = NULL; }
 free ( apStm -> m_pSample2Chunk ); apStm -> m_pSample2Chunk = NULL;
 free ( apStm -> m_pSampleSize   ); apStm -> m_pSampleSize   = NULL;
 free ( apStm -> m_pKeyFrames    ); apStm -> m_pKeyFrames    = NULL;
 free ( apStm -> m_pStts         ); apStm -> m_pStts         = NULL;
}  /* end _mov_free_tables */

static void _DestroyStream ( SMS_Stream* apStm ) {
 MOVStream* lpStm = ( MOVStream* )apStm -> m_pCtx;
 _mov_free_tables ( lpStm, 1 );
 if ( lpStm -> m_pIdx  ) free ( lpStm -> m_pIdx  );
 if ( lpStm -> m_pCtts ) free ( lpStm -> m_pCtts );
}  /* end _DestroyStream */

static int _mov_read_ftyp ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 uint32_t lType = File_GetUInt ( apFileCtx );
 if (  lType != SMS_MKTAG( 'q', 't', ' ', ' ' )  ) apCtx -> m_fISOM = 1;
 File_GetUInt ( apFileCtx );
/* m_Size is already the DECLARED size minus the 8-byte atom header, and the two File_GetUInt
 * calls above have just consumed 8 more bytes of it -- so a short or truncated ftyp ( declared
 * 8..15 ) makes this subtraction underflow to a value near 4 GB. The probe only checks that the
 * 'ftyp' TAG is present, never its size, so any file with those four bytes at offset 4 reaches
 * here. File_Skip now stops at EOF so this can no longer hang, but skipping the right amount
 * keeps the atom dispatcher byte-synced instead of desynced-then-truncated. */
 if (  apAtom -> m_Size >= 8  ) File_Skip (  apFileCtx, ( uint32_t )( apAtom -> m_Size - 8 )  );
 return 0;
}  /* end _mov_read_ftyp */

static int _mov_read_moov ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom) {
 int lResult = _mov_read_dflt ( apCtx, apFileCtx, apAtom );
 if ( lResult < 0 ) return lResult;
 apCtx -> m_fMOOV = 1;
 if ( apCtx -> m_fMDAT ) return 1;
 return 0;
}  /* end _mov_read_moov */

static int _mov_read_mvhd ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 int lVer = File_GetByte ( apFileCtx );
 File_Skip (  apFileCtx, lVer == 1 ? 19 : 11 );
 apCtx -> m_TimeScale = _read_int_be ( apFileCtx );
 apCtx -> m_Duration  = lVer == 1 ? _read_long_be ( apFileCtx ) : _read_int_be ( apFileCtx );
 File_Skip ( apFileCtx, 80 );
 return 0;
}  /* end _mov_read_mvhd */

static int _mov_read_trak ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm;
 MOVStream*  lpMyStm;
 if (  apCtx -> m_pBase -> m_nStm >= SMS_MAX_STREAMS - 1  ) {   /* Reserve the trailing NULL used by stream walkers. Skip the whole excess trak so its children cannot modify the last retained stream. */
  File_Skip ( apFileCtx, ( uint32_t )apAtom -> m_Size );
  return 0;
 }  /* end if */
 lpStm             = ( SMS_Stream*       )calloc (  1, sizeof ( SMS_Stream ) + sizeof ( MOVStream )  );
 if ( !lpStm ) return -1;
 lpStm -> m_pCodec = ( SMS_CodecContext* )calloc (  1, sizeof ( SMS_CodecContext )  );
 if ( !lpStm -> m_pCodec ) { free ( lpStm ); return -1; }
 lpMyStm           = ( MOVStream* )(  ( char* )lpStm + sizeof( SMS_Stream )  );
 lpStm -> m_pCtx             = lpMyStm;
 lpStm -> m_pCodec -> m_Type = SMS_CodecTypeUnknown;
 lpStm -> Destroy            = _DestroyStream;
 apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm++ ] = lpStm;
 return _mov_read_dflt ( apCtx, apFileCtx, apAtom );
}  /* end _mov_read_trak */

static int _mov_read_tkhd ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 int         lVer  = File_GetByte ( apFileCtx );
 File_Skip (  apFileCtx, lVer == 1 ? 19 : 11 );
 lpStm -> m_ID = _read_int_be ( apFileCtx );
 File_Skip (  apFileCtx, lVer == 1 ? 72 : 68 );
 return 0;
}  /* end _mov_read_tkhd */

static int _mov_read_mdhd ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm   = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 MOVStream*  lpMyStm = ( MOVStream* )lpStm -> m_pCtx;
 int         lVer    = File_GetByte ( apFileCtx );
 int         lLang;
 if ( lVer > 1 ) return 1;
 File_Skip ( apFileCtx, lVer == 1 ? 19 : 11 );
 lpMyStm -> m_TimeScale = _read_int_be ( apFileCtx );
 lpStm -> m_Duration    = lVer == 1 ? _read_long_be ( apFileCtx ) : _read_int_be ( apFileCtx );
 lLang                  = File_GetShortBE ( apFileCtx );
 File_Skip ( apFileCtx, 2 );
 return 0;
}  /* end _mov_read_mdhd */

static int _mov_read_elst ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 MOVStream* lpMyStm;
 unsigned int lVersion, lCount, i, lStart = apFileCtx -> m_CurPos;
 if ( !lpStm ) return 0;
 lpMyStm = ( MOVStream* )lpStm -> m_pCtx;
 if ( lpMyStm -> m_HasEdit || apAtom -> m_Size < 8 ) goto error;
 lVersion = File_GetByte ( apFileCtx );
 File_Skip ( apFileCtx, 3 );
 lCount = _read_int_be ( apFileCtx );
 if ( lVersion > 1 || lCount < 1 || lCount > 2 ||
      !_tbl_fits ( apAtom, lCount, lVersion ? 20 : 12, 8 ) ) goto error;
 for ( i = 0; i < lCount; ++i ) {
  uint64_t lDuration = lVersion ? _read_long_be ( apFileCtx ) : _read_int_be ( apFileCtx );
  int64_t lMediaTime = lVersion ? ( int64_t )_read_long_be ( apFileCtx ) : ( int32_t )_read_int_be ( apFileCtx );
  int lRate = File_GetShortBE ( apFileCtx );
  int lFraction = File_GetShortBE ( apFileCtx );
  /* SMS has one continuous track timeline. Do not silently misplay splices,
   * repeated segments, dwell edits or speed changes as an ordinary track. */
  if ( lRate != 1 || lFraction || lDuration > UINT_MAX ) goto error;
  if ( lMediaTime == -1 && i == 0 && lCount == 2 )
   lpMyStm -> m_EditEmpty = lDuration;
  else {
   if ( i + 1 != lCount || lMediaTime < 0 || lMediaTime > UINT_MAX ) goto error;
   lpMyStm -> m_EditStart = lMediaTime;
   lpMyStm -> m_EditDuration = lDuration;
  }  /* end else */
 }  /* end for */
 if ( apFileCtx -> m_CurPos != lStart + 8 + lCount * ( lVersion ? 20 : 12 ) ) goto error;
 if ( !lpMyStm -> m_EditDuration ) goto error;
 lpMyStm -> m_HasEdit = 1;
 return 0;
error:
 apCtx -> m_fError = 1;
 return -1;
}  /* end _mov_read_elst */

static int _mov_read_hdlr ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 uint32_t    lType;
 uint32_t    lSubType;
 File_Skip ( apFileCtx, 4 );
 lType    = File_GetUInt ( apFileCtx );
 lSubType = File_GetUInt ( apFileCtx );
 if ( !lType ) apCtx -> m_fISOM = 1;
 if (  lSubType == SMS_MKTAG( 'v', 'i', 'd', 'e' )  )
  lpStm ->m_pCodec -> m_Type = SMS_CodecTypeVideo;
 else if (  lSubType == SMS_MKTAG( 's', 'o', 'u', 'n' )  )
  lpStm ->m_pCodec -> m_Type = SMS_CodecTypeAudio;
 else if (  lSubType == SMS_MKTAG( 'm', '1', 'a', ' ' )  ) {
  lpStm ->m_pCodec -> m_Type = SMS_CodecTypeAudio;
  lpStm ->m_pCodec -> m_ID   = SMS_CodecID_MP2;
 }  /* end if */
 File_Skip ( apFileCtx, 12 );
 if ( apAtom -> m_Size <= 24 ) return 0;
 File_Skip (   apFileCtx, ( uint32_t )(  apAtom -> m_Size - ( apFileCtx -> m_CurPos - apAtom -> m_Offset )  )   );
 return 0;
}  /* end _mov_read_hdlr */

static int _mov_read_hdlr_m4a ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 if ( lpStm ) {
  uint32_t    lType;
  uint32_t    lSubType;
  File_Skip ( apFileCtx, 4 );
  lType    = File_GetUInt ( apFileCtx );
  lSubType = File_GetUInt ( apFileCtx );
  if ( !lType ) apCtx -> m_fISOM = 1;
  if (  lSubType == SMS_MKTAG( 's', 'o', 'u', 'n' )  )
   lpStm -> m_pCodec -> m_Type = SMS_CodecTypeAudio;
  else if ( lSubType == SMS_MKTAG( 'v', 'i', 'd', 'e' ) ||
            lSubType == SMS_MKTAG( 'm', '1', 'a', ' ' )
  ) {
   free ( lpStm -> m_pCodec );
   free ( lpStm );
   apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ] = NULL;
  }  /* end else */
  File_Skip ( apFileCtx, 12 );
  if ( apAtom -> m_Size <= 24 ) return 0;
  File_Skip (   apFileCtx, ( uint32_t )(  apAtom -> m_Size - ( apFileCtx -> m_CurPos - apAtom -> m_Offset )  )   );
 } else File_Skip (  apFileCtx, ( uint32_t )apAtom -> m_Size  );
 return 0;
}  /* end _mov_read_hdlr_m4a */

static int _mov_read_stsd ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 if ( lpStm ) {
  SMS_CodecContext* lpCodecCtx = lpStm -> m_pCodec;
  MOVStream*        lpMyStm    = ( MOVStream* )lpStm -> m_pCtx;
  int               lnEntries;
  uint32_t          lFmt;
  File_Skip ( apFileCtx, 4 );
  lnEntries = _read_int_be ( apFileCtx );
  while ( lnEntries-- ) {
   MOVAtom     lAtom     = { 0, 0, 0 };
   uint32_t    lStartPos = apFileCtx -> m_CurPos;
   int         lSize     = _read_int_be ( apFileCtx );
   lFmt = File_GetUInt ( apFileCtx );
   File_Skip ( apFileCtx, 8 );
   if ( lpCodecCtx -> m_Tag ) {
    File_Skip (  apFileCtx, lSize - ( apFileCtx -> m_CurPos - lStartPos )  );
    continue;
   }  /* end if */
   lpCodecCtx -> m_Tag = lFmt;
   lpCodecCtx -> m_ID  = SMS_CodecGetID ( lpStm -> m_pCodec -> m_Type, lFmt );
   if ( lpCodecCtx -> m_Type == SMS_CodecTypeVideo ) {
    File_Skip ( apFileCtx, 16 );
    lpCodecCtx -> m_Width  = File_GetShortBE ( apFileCtx );
    lpCodecCtx -> m_Height = File_GetShortBE ( apFileCtx );
    File_Skip ( apFileCtx, 46 );
    lpCodecCtx -> m_BitsPerSample = File_GetShortBE ( apFileCtx );
   } else if ( lpCodecCtx -> m_Type == SMS_CodecTypeAudio ) {
    uint16_t lVer = File_GetShortBE ( apFileCtx );
    File_Skip ( apFileCtx, 6 );
    lpCodecCtx -> m_Channels      = File_GetShortBE ( apFileCtx );
    lpCodecCtx -> m_BitsPerSample = File_GetShortBE ( apFileCtx );
    File_Skip ( apFileCtx, 4 );
    lpCodecCtx -> m_SampleRate    = _read_int_be ( apFileCtx ) >> 16;
    if ( !apCtx -> m_fISOM ) {
     if ( lVer == 1 ) {
      lpMyStm -> m_SamplesPerFrame = _read_int_be ( apFileCtx );
      File_Skip ( apFileCtx, 4 );
      lpMyStm -> m_BytesPerFrame   = _read_int_be ( apFileCtx );
      File_Skip ( apFileCtx, 4 );
     } else if ( lVer == 2 ) {
      double lVal;
      File_Skip ( apFileCtx, 4 );
      *( uint64_t* )&lVal = _read_long_be ( apFileCtx );
      lpCodecCtx -> m_SampleRate = ( int32_t )lVal;
      lpCodecCtx -> m_Channels   = _read_int_be ( apFileCtx );
      File_Skip ( apFileCtx, 20 );
     }  /* end if */
    }  /* end if */
   } else File_Skip ( apFileCtx, apFileCtx -> m_CurPos - lStartPos );
   lAtom.m_Size = lSize - ( apFileCtx -> m_CurPos - lStartPos );
   if ( lAtom.m_Size > 8 ) {
    int lResult = _mov_read_dflt ( apCtx, apFileCtx, &lAtom );
    if ( lResult ) return lResult;
   }  /* end if */
   else if ( lAtom.m_Size > 0 ) File_Skip (  apFileCtx, ( uint32_t )lAtom.m_Size  );
  }  /* end while */
 } else File_Skip (  apFileCtx, ( uint32_t )apAtom -> m_Size  );
 return 0;
}  /* end _mov_read_stsd */

static int _mp4_read_descr_len ( FileContext* apFileCtx) {
 int lLen   = 0;
 int lCount = 4;
 while ( lCount-- ) {
  int lChr = File_GetByte ( apFileCtx );
  lLen = ( lLen << 7 ) | ( lChr & 0x7F );
  if (  !( lChr & 0x80 )  ) break;
 }  /* end while */
 return lLen;
}  /* end _mp4_read_descr_len */

static int _mp4_read_descr ( FileContext* apFileCtx, int* apTag ) {
 apTag[ 0 ] = File_GetByte ( apFileCtx );
 return _mp4_read_descr_len ( apFileCtx );
}  /* end _mp4_read_descr */

#define MP4ESDescrTag          0x03
#define MP4DecConfigDescrTag   0x04
#define MP4DecSpecificDescrTag 0x05

const SMS_CodecTag s_MP4ObjType[] = {
 { SMS_CodecID_MPEG4,  32 },
 { SMS_CodecID_AAC,    64 },
 { SMS_CodecID_MPEG2,  96 },
 { SMS_CodecID_MPEG2,  97 },
 { SMS_CodecID_AAC,   102 },
 { SMS_CodecID_AAC,   103 },
 { SMS_CodecID_MP3,   105 },
 { SMS_CodecID_MPEG1, 106 },
 { SMS_CodecID_MP3,   107 },
 { SMS_CodecID_OGGV,  221 },
 { SMS_CodecID_AC3,   226 }
};

static SMS_CodecID _mp4_get_id ( int aTag ) {
 int i;
 SMS_CodecID retVal = SMS_CodecID_NULL;
 for ( i = 0; i < sizeof ( s_MP4ObjType ) / sizeof ( s_MP4ObjType[ 0 ] ); ++i )
  if (  s_MP4ObjType[ i ].m_Tag == ( unsigned int )aTag  ) {
   retVal = s_MP4ObjType[ i ].m_ID;
   break;
  }  /* end if */
 return retVal;
}  /* end _mp4_get_id */

static int _mov_read_esds ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 int         lTag, lLen;
/* Every other stream-child reader here tests the slot; this one did not. The slot really can be
 * NULL by the time an esds arrives: _mov_read_hdlr_m4a frees the stream and NULLs it in place
 * ( without decrementing m_nStm ) whenever it meets a video trak on the audio-only path, and
 * esds sits in the dispatch table at trak level as well as nested inside stsd -- so a file that
 * puts one there walks straight into lpStm -> m_pCodec below. */
 if ( !lpStm ) {
  File_Skip (  apFileCtx, ( uint32_t )apAtom -> m_Size  );
  return 0;
 }  /* end if */
 File_Skip ( apFileCtx, 4 );
 lLen = _mp4_read_descr ( apFileCtx, &lTag );
 File_Skip ( apFileCtx, lTag == MP4ESDescrTag ? 3 : 2 );
 lLen = _mp4_read_descr ( apFileCtx, &lTag );
 if ( lTag == MP4DecConfigDescrTag ) {
  int               lObjTypeID = File_GetByte ( apFileCtx );
  SMS_CodecContext* lpCodecCtx = lpStm -> m_pCodec;
  File_Skip ( apFileCtx, 8 );
  lpCodecCtx -> m_BitRate = _read_int_be ( apFileCtx );
  lpCodecCtx -> m_ID      = _mp4_get_id ( lObjTypeID );
  lLen = _mp4_read_descr ( apFileCtx, &lTag );
/* lLen is up to 4 septets straight from the file ( ~256 MB ) and is not bounded by the atom, so
 * both the length and the allocation need testing before the read -- otherwise a bogus
 * DecoderSpecificInfo either reads far past the atom or, once the oversized malloc fails on a
 * 32 MB heap, reads directly into a NULL pointer. Leaving m_pUserData NULL is the documented
 * no-extradata state that both consumers already handle ( SMS_AAC.c _aac_init and the MPEG-4
 * seeding in SMS_MPEG4.c both gate on m_pUserData && m_UserDataLen > 0 ). */
  if ( lTag == MP4DecSpecificDescrTag && lLen > 0 && _tbl_fits ( apAtom, lLen, 1, 0 )  ) {
   lpCodecCtx -> m_pUserData = ( uint8_t* )malloc (  ( lLen + 15 ) & ~15  );
   if ( lpCodecCtx -> m_pUserData ) {
    apFileCtx -> Read ( apFileCtx, lpCodecCtx -> m_pUserData, lLen );
    lpCodecCtx -> m_UserDataLen = lLen;
   }  /* end if */
  }  /* end if */
 }  /* end if */
 return 0;
}  /* end _mov_read_esds */

static int32_t _gcd ( int32_t anA, int32_t aB ) {
 int64_t lA = anA, lB = aB;
 if ( lA < 0 ) lA = -lA;
 if ( lB < 0 ) lB = -lB;
 while ( lB ) {
  int64_t lRemainder = lA % lB;
  lA = lB;
  lB = lRemainder;
 }  /* end while */
 return lA > INT_MAX ? 1 : ( int32_t )lA;
}  /* end _gcd */

static int _mov_read_stts ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 if ( lpStm ) {
  MOVStream*   lpMyStm = ( MOVStream* )lpStm -> m_pCtx;
  unsigned int i, lnEntries;
  int64_t      lDuration = 0;
  File_Skip ( apFileCtx, 4 );
  lnEntries = _read_int_be ( apFileCtx );
  if ( lnEntries >= UINT_MAX / sizeof ( MOVStts )  ) return -1;
  lpMyStm -> m_TimeRate = 0;
  lpMyStm -> m_nStts    = 0;
  if (  !_tbl_fits ( apAtom, lnEntries, 8, 8 )  ) return 0;
  lpMyStm -> m_pStts    = ( MOVStts* )malloc ( lnEntries * sizeof ( MOVStts )  );
  if ( !lpMyStm -> m_pStts ) return 0;   /* 32MB heap: a large ( or bogus ) entry count can fail */
  lpMyStm -> m_nStts    = lnEntries;
  for ( i = 0; i < lnEntries; ++i ) {
   int lSampleCount    = _read_int_be ( apFileCtx );
   int lSampleDuration = _read_int_be ( apFileCtx );
   lpMyStm -> m_pStts[ i ].m_Count    = lSampleCount;
   lpMyStm -> m_pStts[ i ].m_Duration = lSampleDuration;
   lpMyStm -> m_TimeRate = _gcd ( lpMyStm -> m_TimeRate, lSampleDuration );
   lDuration  += ( int64_t )lSampleDuration * lSampleCount;
  }  /* end for */
  if ( lDuration ) lpStm -> m_Duration = lDuration;

/* FRAME RATE -- required, not cosmetic. m_FrameRate / m_FrameRateBase were set in exactly ONE
 * place in the whole tree ( SMS_ContainerAVI.c, the AVI header reader ), and this demuxer never
 * touched them. Its codec context is calloc'd, so both stayed 0 and MPEG4_Init computed
 * ( float )0 / ( float )0 -- which is then rounded and written into COP0 $11, the Compare
 * register, to drive the CPU timer handler. Harmless while .mp4 went to the audio-only reader
 * and no video decoder ever started; the moment video is kept ( below ) it becomes a garbage
 * value in a timer control register, so this has to land FIRST.
 * ISO-BMFF states frame duration rather than a rate: fps = media timescale / per-sample delta,
 * which is exactly the numerator/denominator pair AVI supplies as rate/scale ( 30000/1001 and
 * friends ), so the existing contract is reused unchanged rather than a new convention added.
 * Entry 0 is the right delta: constant-frame-rate files have exactly one stts entry, and for
 * variable ones the first is the dominant delta -- and this pair only sets the decoder's
 * nominal rate, while actual presentation timing still comes from the per-sample DTS the index
 * builds. m_TimeScale is read from mdhd, which precedes stbl/stts inside the same trak, so it
 * is always populated by the time we get here. Audio streams are untouched. */
  if ( lpStm -> m_pCodec -> m_Type == SMS_CodecTypeVideo &&
       lnEntries                                        &&
       lpMyStm -> m_pStts[ 0 ].m_Duration > 0            &&
       lpMyStm -> m_TimeScale            > 0
  ) {
   lpStm -> m_pCodec -> m_FrameRate     = lpMyStm -> m_TimeScale;
   lpStm -> m_pCodec -> m_FrameRateBase = lpMyStm -> m_pStts[ 0 ].m_Duration;
  }  /* end if */

 } else File_Skip (  apFileCtx, ( uint32_t )apAtom -> m_Size  );
 return 0;
}  /* end _mov_read_stts */

static int _mov_read_stsc ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm   = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 if ( lpStm ) {
  MOVStream*   lpMyStm = ( MOVStream* )lpStm -> m_pCtx;
  unsigned int i, lnEntries;
  File_Skip ( apFileCtx, 4 );
  lnEntries = _read_int_be ( apFileCtx );
  if (  lnEntries >= UINT_MAX / sizeof ( MOVStsc )  ) return -1;
  lpMyStm -> m_nSample2Chunk = 0;
  if (  !_tbl_fits ( apAtom, lnEntries, 12, 8 )  ) return 0;
  lpMyStm -> m_pSample2Chunk = ( MOVStsc* )malloc (  lnEntries * sizeof ( MOVStsc )  );
  if ( !lpMyStm -> m_pSample2Chunk ) return 0;
  lpMyStm -> m_nSample2Chunk = lnEntries;
  for ( i = 0; i < lnEntries; ++i ) {
   lpMyStm -> m_pSample2Chunk[ i ].m_First = _read_int_be ( apFileCtx );
   lpMyStm -> m_pSample2Chunk[ i ].m_Count = _read_int_be ( apFileCtx );
   lpMyStm -> m_pSample2Chunk[ i ].m_ID    = _read_int_be ( apFileCtx );
  }  /* end for */
 } else File_Skip (  apFileCtx, ( uint32_t )apAtom -> m_Size  );
 return 0;
}  /* end _mov_read_stsc */

static int _mov_read_stsz ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 if ( lpStm ) {
  MOVStream* lpMyStm = ( MOVStream* )lpStm -> m_pCtx;
  unsigned int i, lnEntries, lSampleSize;
  File_Skip ( apFileCtx, 4 );
  lSampleSize = _read_int_be ( apFileCtx );
  if ( !lpMyStm -> m_SampleSize ) lpMyStm -> m_SampleSize = lSampleSize;
  lnEntries = _read_int_be ( apFileCtx );
  if (  lnEntries >= UINT_MAX / sizeof ( int )  ) return -1;
  lpMyStm -> m_nSamples = lnEntries;
  if ( lSampleSize ) return 0;
/* Fixed-size samples carry no per-sample table, so the count is only bounded by the atom on
 * the variable-size path -- 12 consumed here ( version/flags, sample size, entry count ). */
  if (  !_tbl_fits ( apAtom, lnEntries, 4, 12 )  ) {
   lpMyStm -> m_nSamples = 0;
   return 0;
  }  /* end if */
  lpMyStm -> m_pSampleSize = ( int* )malloc (  lnEntries * sizeof ( int )  );
  if ( !lpMyStm -> m_pSampleSize ) {
   lpMyStm -> m_nSamples = 0;
   return 0;
  }  /* end if */
  for ( i = 0; i < lnEntries; ++i ) lpMyStm -> m_pSampleSize[ i ] = _read_int_be ( apFileCtx );
 } else File_Skip (  apFileCtx, ( uint32_t )apAtom -> m_Size  );
 return 0;
}  /* end mov_read_stsz */

static int _mov_read_stco ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm   = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 if ( lpStm ) {
  MOVStream* lpMyStm = ( MOVStream* )lpStm -> m_pCtx;
  unsigned int i, lnEntries;
  File_Skip ( apFileCtx, 4 );
  lnEntries = _read_int_be ( apFileCtx );
  if (  lnEntries >= UINT_MAX / sizeof ( int64_t )  ) return -1;
  lpMyStm -> m_nChunks       = 0;
/* stco stores 32-bit offsets, co64 stores 64-bit ones -- same in-memory table, different
 * on-disk width, so the ceiling differs by a factor of two. */
  if (  !_tbl_fits (
         apAtom, lnEntries,
         apAtom -> m_Type == SMS_MKTAG( 'c', 'o', '6', '4' ) ? 8 : 4, 8
        )
  ) return 0;
  lpMyStm -> m_pChunkOffsets = ( int64_t* )malloc (  lnEntries * sizeof ( int64_t )  );
  if ( !lpMyStm -> m_pChunkOffsets ) return 0;
  if (  apAtom -> m_Type == SMS_MKTAG( 's', 't', 'c', 'o' )  )
   for ( i = 0; i < lnEntries; ++i ) lpMyStm -> m_pChunkOffsets[ i ] = _read_int_be ( apFileCtx );
  else if (  apAtom -> m_Type == SMS_MKTAG( 'c', 'o', '6', '4' )  )
   for ( i = 0; i < lnEntries; ++i ) lpMyStm -> m_pChunkOffsets[ i ] = _read_long_be( apFileCtx );
/* Count is published only once the table is actually FILLED. The dispatcher turns a -1 into a
 * stop, not an abort -- _mov_read_header still indexes whatever traks it has -- so publishing
 * the count before this branch would have handed _mov_build_index a malloc'd-but-unwritten
 * offset table to walk. */
  else return -1;
  lpMyStm -> m_nChunks       = lnEntries;
 } else File_Skip (  apFileCtx, ( uint32_t )apAtom -> m_Size  );
 return 0;
}  /* end _mov_read_stco */

static int _mov_read_wave ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 if (  ( uint64_t )apAtom -> m_Size > ( 1 << 30 )  )
  return -1;
 else if ( apAtom -> m_Size > 8 )
  return _mov_read_dflt ( apCtx, apFileCtx, apAtom );
 else File_Skip (  apFileCtx, ( uint32_t )apAtom -> m_Size  );
 return 0;
}  /* end _mov_read_wave */

static int _mov_read_ctts ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 if ( lpStm ) {
  MOVStream* lpMyStm = ( MOVStream* )lpStm -> m_pCtx;
  unsigned int i, lnEntries, lVersion;
  lVersion = File_GetByte ( apFileCtx );
  File_Skip ( apFileCtx, 3 );
  if ( lVersion > 1 ) { apCtx -> m_fError = 1; return -1; }
  lnEntries = _read_int_be ( apFileCtx );
  if (  lnEntries >= UINT_MAX / sizeof ( MOVStts )  ) return -1;
  lpMyStm -> m_nCtts = 0;
  if (  !_tbl_fits ( apAtom, lnEntries, 8, 8 )  ) return 0;
  lpMyStm -> m_pCtts = ( MOVStts* )malloc (  lnEntries * sizeof ( MOVStts )  );
  if ( !lpMyStm -> m_pCtts ) return 0;
  lpMyStm -> m_nCtts = lnEntries;
  for ( i = 0; i < lnEntries; ++i ) {
   int lCount    = _read_int_be ( apFileCtx );
   int lDuration = _read_int_be ( apFileCtx );
   if ( !lVersion && lDuration < 0 ) { apCtx -> m_fError = 1; return -1; }
   if ( lVersion && -( int64_t )lDuration > lpMyStm -> m_DTSShift ) lpMyStm -> m_DTSShift = -( int64_t )lDuration;
   lpMyStm -> m_pCtts[ i ].m_Count    = lCount;
   lpMyStm -> m_pCtts[ i ].m_Duration = lDuration;
   lpMyStm -> m_TimeRate = _gcd ( lpMyStm -> m_TimeRate, lDuration );
  }  /* end for */
 } else File_Skip (  apFileCtx, ( uint32_t )apAtom -> m_Size  );
 return 0;
}  /* end _mov_read_ctts */

static int _mov_read_stss ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_Stream* lpStm = apCtx -> m_pBase -> m_pStm[ apCtx -> m_pBase -> m_nStm - 1 ];
 if ( lpStm ) {
  MOVStream*   lpMyStm = ( MOVStream* )lpStm -> m_pCtx;
  unsigned int i, lnEntries;
  File_Skip ( apFileCtx, 4 );
  lnEntries = _read_int_be ( apFileCtx );
  if (  lnEntries >= UINT_MAX / sizeof ( int )  ) return -1;
/* Same paranoia as _mov_read_stts: a bogus entry count can make a 32MB heap fail the
 * malloc. Falling back to m_nKeyFrames == 0 is safe -- ISO-BMFF says that without stss
 * EVERY sample is a sync sample, which is exactly how _mov_build_index treats it. */
  lpMyStm -> m_nKeyFrames = 0;
  if (  !_tbl_fits ( apAtom, lnEntries, 4, 8 )  ) return 0;
  lpMyStm -> m_pKeyFrames = ( int* )malloc (  lnEntries * sizeof ( int )  );
  if ( !lpMyStm -> m_pKeyFrames ) return 0;
  lpMyStm -> m_nKeyFrames = lnEntries;
  for ( i = 0; i < lnEntries; ++i ) lpMyStm -> m_pKeyFrames[ i ] = _read_int_be ( apFileCtx );
 } else File_Skip (  apFileCtx, ( uint32_t )apAtom -> m_Size  );
 return 0;
}  /* end _mov_read_stss */

static int _mov_read_mdat ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 MOVMDat* lpMDAT;
 if ( apAtom -> m_Size == 0 ) return 0;
 if ( apCtx -> m_nMDAT < 0 || ( unsigned int )apCtx -> m_nMDAT >= UINT_MAX / sizeof ( MOVMDat ) - 1 ) return -1;
 lpMDAT = ( MOVMDat* )realloc ( apCtx -> m_pMDAT, ( apCtx -> m_nMDAT + 1 ) * sizeof ( MOVMDat ) );
 if ( !lpMDAT ) return -1;
 apCtx -> m_pMDAT = lpMDAT;
 apCtx -> m_pMDAT[ apCtx ->m_nMDAT ].m_Offset = apAtom -> m_Offset;
 apCtx -> m_pMDAT[ apCtx ->m_nMDAT ].m_Size   = apAtom -> m_Size;
 ++apCtx -> m_nMDAT;
 apCtx -> m_fMDAT      = 1;
 apCtx -> m_MDATOffset = apAtom -> m_Offset;
 if ( apCtx -> m_fMOOV ) return 1;
 File_Skip (  apFileCtx, ( uint32_t )( apAtom -> m_Size )  );
 return 0;
}  /* end _mov_read_mdat */

static int _mov_read_hdlr_m4ap ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_AudioInfo* lpInfo = ( SMS_AudioInfo* )apCtx -> m_pMDAT;
 if ( !lpInfo -> m_SampleRate ) {
  uint32_t lType;
  uint32_t lSubType;
  File_Skip ( apFileCtx, 4 );
  lType    = File_GetUInt ( apFileCtx );
  lSubType = File_GetUInt ( apFileCtx );
  if ( !lType ) apCtx -> m_fISOM = 1;
  if (  lSubType == SMS_MKTAG( 's', 'o', 'u', 'n' )  ) lpInfo -> m_nChannels = 0x80000000;
  File_Skip ( apFileCtx, 12 );
  if ( apAtom -> m_Size <= 24 ) return 0;
  File_Skip (   apFileCtx, ( uint32_t )(  apAtom -> m_Size - ( apFileCtx -> m_CurPos - apAtom -> m_Offset )  )   );
 } else File_Skip (  apFileCtx, ( uint32_t )( apAtom -> m_Size )  );
 return 0;
}  /* end _mov_read_hdlr_m4ap */

static int _mov_read_stsd_m4ap ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_AudioInfo* lpInfo = ( SMS_AudioInfo* )apCtx -> m_pMDAT;
 if ( lpInfo -> m_nChannels < 0 ) {
  int      lnEntries;
  uint32_t lFmt;
  File_Skip ( apFileCtx, 4 );
  lnEntries = _read_int_be ( apFileCtx );
  while ( lnEntries-- ) {
   MOVAtom     lAtom     = { 0, 0, 0 };
   uint32_t    lStartPos = apFileCtx -> m_CurPos;
   int         lSize     = _read_int_be ( apFileCtx );
   uint16_t    lVer;
   lFmt = File_GetUInt ( apFileCtx );
   File_Skip ( apFileCtx, 8 );
   if ( lpInfo -> m_nChannels > 0 ) {
    File_Skip (  apFileCtx, lSize - ( apFileCtx -> m_CurPos - lStartPos )  );
    continue;
   }  /* end if */
   lVer = File_GetShortBE ( apFileCtx );
   File_Skip ( apFileCtx, 6 );
   lpInfo -> m_nChannels  = File_GetShortBE ( apFileCtx );
   File_Skip ( apFileCtx, 6 );
   lpInfo -> m_SampleRate = _read_int_be ( apFileCtx ) >> 16;
   if ( !apCtx -> m_fISOM ) {
    if ( lVer == 1 )
     File_Skip ( apFileCtx, 16 );
    else if ( lVer == 2 ) {
     double lVal;
     File_Skip ( apFileCtx, 4 );
     *( uint64_t* )&lVal = _read_long_be ( apFileCtx );
     lpInfo -> m_SampleRate = ( int32_t )lVal;
     lpInfo -> m_nChannels  = _read_int_be ( apFileCtx );
     File_Skip ( apFileCtx, 20 );
    }  /* end if */
   }  /* end if */
   lAtom.m_Size = lSize - ( apFileCtx -> m_CurPos - lStartPos );
   if ( lAtom.m_Size > 8 ) {
    int lResult = _mov_read_dflt ( apCtx, apFileCtx, &lAtom );
    if ( lResult ) return lResult;
   }  /* end if */
   else if ( lAtom.m_Size > 0 ) File_Skip (  apFileCtx, ( uint32_t )lAtom.m_Size  );
  }  /* end while */
 } else File_Skip (  apFileCtx, ( uint32_t )( apAtom -> m_Size )  );
 return 0;
}  /* end _mov_read_stsd_m4ap */

static int _mov_read_esds_m4ap ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {
 SMS_AudioInfo* lpInfo = ( SMS_AudioInfo* )apCtx -> m_pMDAT;
 uint32_t lStartPos = apFileCtx -> m_CurPos;
 if ( lpInfo -> m_nChannels > 0 ) {
  int         lTag, lLen;
  File_Skip ( apFileCtx, 4 );
  lLen = _mp4_read_descr ( apFileCtx, &lTag );
  File_Skip ( apFileCtx, lTag == MP4ESDescrTag ? 3 : 2 );
  lLen = _mp4_read_descr ( apFileCtx, &lTag );
  if ( lTag == MP4DecConfigDescrTag ) {
   int lObjectType = File_GetByte ( apFileCtx );
   File_Skip ( apFileCtx, 8 );
   lpInfo -> m_BitRate = _read_int_be ( apFileCtx );
   lLen = _mp4_read_descr ( apFileCtx, &lTag );
   if ( lTag == MP4DecSpecificDescrTag ) {
    uint64_t lUsed = apFileCtx -> m_CurPos - lStartPos;
    if ( apFileCtx -> m_CurPos < lStartPos || lUsed > apAtom -> m_Size ||
         lLen < 0 || ( uint64_t )lLen > apAtom -> m_Size - lUsed ) return -1;
    /* mp4a's sample entry can say stereo even for mono AAC. For ordinary
     * AAC-LC use the encoded AudioSpecificConfig; retain the entry values
     * for PCE, explicit rates and other object types handled by the decoder. */
    if ( _mp4_get_id ( lObjectType ) == SMS_CodecID_AAC && lLen >= 2 ) {
     static const unsigned int s_Rates[] = {
      96000, 88200, 64000, 48000, 44100, 32000, 24000,
      22050, 16000, 12000, 11025, 8000, 7350
     };
     unsigned int lASC = File_GetShortBE ( apFileCtx );
     unsigned int lRate = ( lASC >> 7 ) & 15;
     unsigned int lChannels = ( lASC >> 3 ) & 15;
     if ( ( lASC >> 11 ) == 2 && lRate < 13 && lChannels > 0 && lChannels <= 7 ) {
      lpInfo -> m_SampleRate = s_Rates[ lRate ];
      lpInfo -> m_nChannels = lChannels == 7 ? 8 : lChannels;
     }  /* end if */
     lLen -= 2;
    }  /* end if */
    File_Skip ( apFileCtx, lLen );
   }  /* end if */
  }  /* end if */
 }  /* end if */
 return 0;
}  /* end _mov_read_esds_m4ap */

static MOVParseEntry s_MOVParseTbl[] = {
 { SMS_MKTAG( 'h', 'd', 'l', 'r' ), _mov_read_hdlr },
 { SMS_MKTAG( 'f', 't', 'y', 'p' ), _mov_read_ftyp },
 { SMS_MKTAG( 'm', 'o', 'o', 'v' ), _mov_read_moov },
 { SMS_MKTAG( 'm', 'v', 'h', 'd' ), _mov_read_mvhd },
 { SMS_MKTAG( 't', 'r', 'a', 'k' ), _mov_read_trak },
 { SMS_MKTAG( 't', 'k', 'h', 'd' ), _mov_read_tkhd },
 { SMS_MKTAG( 'e', 'd', 't', 's' ), _mov_read_dflt },
 { SMS_MKTAG( 'e', 'l', 's', 't' ), _mov_read_elst },
 { SMS_MKTAG( 'm', 'd', 'i', 'a' ), _mov_read_dflt },
 { SMS_MKTAG( 'm', 'd', 'h', 'd' ), _mov_read_mdhd },
 { SMS_MKTAG( 'm', 'i', 'n', 'f' ), _mov_read_dflt },
 { SMS_MKTAG( 's', 't', 'b', 'l' ), _mov_read_dflt },
 { SMS_MKTAG( 's', 't', 's', 'd' ), _mov_read_stsd },
 { SMS_MKTAG( 'e', 's', 'd', 's' ), _mov_read_esds },
 { SMS_MKTAG( 's', 't', 't', 's' ), _mov_read_stts },
 { SMS_MKTAG( 's', 't', 's', 's' ), _mov_read_stss },
 { SMS_MKTAG( 'w', 'a', 'v', 'e' ), _mov_read_wave },
 { SMS_MKTAG( 's', 't', 's', 'c' ), _mov_read_stsc },
 { SMS_MKTAG( 'c', 't', 't', 's' ), _mov_read_ctts },
 { SMS_MKTAG( 's', 't', 's', 'z' ), _mov_read_stsz },
 { SMS_MKTAG( 's', 't', 'c', 'o' ), _mov_read_stco },
 { SMS_MKTAG( 'c', 'o', '6', '4' ), _mov_read_stco },
 { SMS_MKTAG( 'm', 'd', 'a', 't' ), _mov_read_mdat },
 {                               0,           NULL }
};

static MOVParseEntry s_M4AProbeTbl[] = {
 { SMS_MKTAG( 'm', 'o', 'o', 'v' ), _mov_read_dflt      },
 { SMS_MKTAG( 't', 'r', 'a', 'k' ), _mov_read_dflt      },
 { SMS_MKTAG( 'm', 'd', 'i', 'a' ), _mov_read_dflt      },
 { SMS_MKTAG( 'm', 'i', 'n', 'f' ), _mov_read_dflt      },
 { SMS_MKTAG( 's', 't', 'b', 'l' ), _mov_read_dflt      },
 { SMS_MKTAG( 'w', 'a', 'v', 'e' ), _mov_read_dflt      },
 { SMS_MKTAG( 'h', 'd', 'l', 'r' ), _mov_read_hdlr_m4ap },
 { SMS_MKTAG( 's', 't', 's', 'd' ), _mov_read_stsd_m4ap },
 { SMS_MKTAG( 'e', 's', 'd', 's' ), _mov_read_esds_m4ap },
 {                               0,           NULL      }
};

static int _mov_read_dflt ( MOVContext* apCtx, FileContext* apFileCtx, MOVAtom* apAtom ) {

 int64_t              lTotalSize = 0;
 MOVAtom              lAtom = { 0, 0, 0 };
 int                  retVal = 0;
 const MOVParseEntry* lpIt;

 lAtom.m_Offset = apAtom -> m_Offset;

 if ( apAtom -> m_Size < 0 ) apAtom -> m_Size = SMS_MAXINT64;

 while (   (  ( lTotalSize + 8 ) <= apAtom -> m_Size ) && !FILE_EOF( apFileCtx ) && !retVal && !apCtx -> m_fError   ) {
  int lHeaderSize = 8;
  lAtom.m_Size = apAtom -> m_Size;
  lAtom.m_Type = 0;
  if ( apAtom -> m_Size >= 8 ) {
   lAtom.m_Size = _read_int_be ( apFileCtx );
   lAtom.m_Type = File_GetUInt ( apFileCtx );
  }  /* end if */
  lTotalSize     += 8;
  lAtom.m_Offset += 8;
  if ( lAtom.m_Size == 1 ) {
   lAtom.m_Size    = _read_long_be ( apFileCtx );
   lHeaderSize     = 16;
   lAtom.m_Offset += 8;
   lTotalSize     += 8;
  }  /* end if */
  if ( lAtom.m_Size == 0 ) {
/* Size zero extends to the parent end; extended sizes include their full
 * 16-byte header. Both must leave the exact payload length for child parsing. */
   lAtom.m_Size = apAtom -> m_Size - lTotalSize;
  } else lAtom.m_Size -= lHeaderSize;
  if ( lAtom.m_Size < 0 || lAtom.m_Size > apAtom -> m_Size - lTotalSize ) break;
  for ( lpIt = apCtx -> m_pParseTbl; lpIt -> m_Type && lpIt -> m_Type != lAtom.m_Type; ++lpIt );
  if ( !lpIt -> m_Type )
   File_Skip (  apFileCtx, ( uint32_t )lAtom.m_Size  );
/* A stream-child atom ( tkhd/mdhd/hdlr/stsd/... ) dispatched before ANY trak
 * ( m_nStm == 0 -- only in a crafted/corrupt file ) would index m_pStm[ -1 ] and
 * deref garbage. Those handlers are valid only inside a trak, so skip them here. */
  else if (  apCtx -> m_pBase -> m_nStm == 0 && (
              lpIt -> m_Func == _mov_read_tkhd || lpIt -> m_Func == _mov_read_mdhd     ||
              lpIt -> m_Func == _mov_read_elst ||
              lpIt -> m_Func == _mov_read_hdlr || lpIt -> m_Func == _mov_read_hdlr_m4a ||
              lpIt -> m_Func == _mov_read_stsd || lpIt -> m_Func == _mov_read_esds     ||
              lpIt -> m_Func == _mov_read_stts || lpIt -> m_Func == _mov_read_stsc     ||
              lpIt -> m_Func == _mov_read_stss || lpIt -> m_Func == _mov_read_stsz     ||
              lpIt -> m_Func == _mov_read_stco || lpIt -> m_Func == _mov_read_ctts )  )
   File_Skip (  apFileCtx, ( uint32_t )lAtom.m_Size  );
  else {
   uint32_t lStartPos = apFileCtx -> m_CurPos;
   int64_t  lLeft;
   retVal = lpIt -> m_Func ( apCtx, apFileCtx, &lAtom );
   lLeft  = lAtom.m_Size - apFileCtx -> m_CurPos + lStartPos;
   if ( lLeft > 0 ) File_Skip (  apFileCtx, ( uint32_t )lLeft  );
  }  /* end else */
  lAtom.m_Offset += lAtom.m_Size;
  lTotalSize     += lAtom.m_Size;
 }  /* end while */

 if ( !retVal && lTotalSize < lAtom.m_Size && lAtom.m_Size < 0x7FFFF ) File_Skip (  apFileCtx, ( uint32_t )( lAtom.m_Size - lTotalSize )  );

 return retVal;

}  /* end _mov_read_dflt */

static int _add_idx_entry ( MOVStream* apMyStm, int afKF, uint32_t aDTS, uint32_t anOffset, uint32_t aSize ) {
 MOVIndex* lpIdx, *lpIndices;
 unsigned int lAllocSize = apMyStm -> m_IdxAllocSize;
/* SMS_Realloc grows by 17/16 and publishes its size even if realloc fails. */
 if ( apMyStm -> m_nIdx < 0 || ( unsigned int )apMyStm -> m_nIdx >= UINT_MAX / 17 / sizeof ( MOVIndex ) - 1 ) return 0;
 lpIndices = ( MOVIndex* )SMS_Realloc (
  apMyStm -> m_pIdx, &lAllocSize,
  ( apMyStm -> m_nIdx + 1 ) * sizeof ( MOVIndex )
 );
 if ( !lpIndices ) return 0;
 apMyStm -> m_IdxAllocSize = lAllocSize;
 apMyStm -> m_pIdx = lpIndices;
 lpIdx = &lpIndices[ apMyStm -> m_nIdx++ ];
 lpIdx -> m_Flags  = afKF;
 lpIdx -> m_DTS    = aDTS;
 lpIdx -> m_Pos    = anOffset;
 lpIdx -> m_Size   = aSize;
 return 1;
}  /* end _add_idx_entry */

static int _mov_build_index ( SMS_Container* apCont, MOVStream* apMyStm, SMS_Stream* apStm ) {

 int64_t      lCurOffset;
 int64_t      lCurDTS  = 0;
 unsigned int lSTTSIdx = 0;
 unsigned int lSTSCIdx = 0;
 unsigned int lSTSSIdx = 0;
 MOVContext*  lpCtx    = ( MOVContext* )apCont -> m_pCtx;
 unsigned int i, j, k;

/* Both walks below need entry 0 of m_pSample2Chunk and m_pStts. The gate in
 * _mov_read_header refuses to call this at all unless m_nStts, m_nChunks
 * and m_nSample2Chunk are non-zero ( and repairs a zero m_TimeRate, the divisor here, to 1 ).
 * That gate tests COUNTS while this code derefs POINTERS, so it is only sound as long as the
 * two move together -- which is exactly the invariant the readers above now maintain: a count is
 * published only once its table has been allocated and filled. Do not set a count before its
 * malloc is checked, or this walks a NULL table. The timing walks must also
 * account for every indexed sample, rather than reusing an exhausted entry. */
 if ( apMyStm -> m_pSampleSize || apStm -> m_pCodec -> m_Type == SMS_CodecTypeVideo ) {
  unsigned int lCurSample = 0;
  unsigned int lSTTSample = 0;
  unsigned int lKeyFrame, lSampleSize;
  for ( i = 0; i < apMyStm -> m_nChunks; ++i ) {
   lCurOffset = apMyStm -> m_pChunkOffsets[ i ];
   if ( lSTSCIdx + 1  < apMyStm -> m_nSample2Chunk &&
        i        + 1 == ( unsigned int )apMyStm -> m_pSample2Chunk[ lSTSCIdx + 1 ].m_First
   ) ++lSTSCIdx;
   for (  j = 0; j < ( unsigned int )apMyStm -> m_pSample2Chunk[ lSTSCIdx ].m_Count; ++j ) {
    if ( lCurSample >= apMyStm -> m_nSamples ) return 1;
    while ( lSTTSIdx < apMyStm -> m_nStts &&
            ( apMyStm -> m_pStts[ lSTTSIdx ].m_Count <= 0 ||
              lSTTSample >= ( unsigned int )apMyStm -> m_pStts[ lSTTSIdx ].m_Count ) ) {
     ++lSTTSIdx;
     lSTTSample = 0;
    }  /* end while */
    if ( lSTTSIdx == apMyStm -> m_nStts ) return 0;
/* A stss entry the cursor has already passed can never match again ( entry beyond the
 * indexed samples in a truncated file, or non-monotonic garbage in a crafted one ) and
 * would pin lSTSSIdx, mis-flagging every later sample as non-key. Well-formed tables are
 * strictly increasing, so on real files this loop does nothing. */
    while (  lSTSSIdx + 1 < apMyStm -> m_nKeyFrames &&
             lCurSample + 1 > ( unsigned int )apMyStm -> m_pKeyFrames[ lSTSSIdx ]
    ) ++lSTSSIdx;
    lKeyFrame = !apMyStm -> m_nKeyFrames || lCurSample + 1 == ( unsigned int )apMyStm -> m_pKeyFrames[ lSTSSIdx ];
    if ( lKeyFrame ) {
     if ( lSTSSIdx + 1 < apMyStm -> m_nKeyFrames ) ++lSTSSIdx;
    }  /* end if */
    lSampleSize = apMyStm -> m_SampleSize > 0 ? apMyStm -> m_SampleSize : apMyStm -> m_pSampleSize[ lCurSample ];
    if ( !_add_idx_entry ( apMyStm, lKeyFrame, ( uint32_t )lCurDTS, ( uint32_t )lCurOffset, lSampleSize ) ) return 0;
    lCurOffset += lSampleSize;
    lCurDTS    += apMyStm -> m_pStts[ lSTTSIdx ].m_Duration / apMyStm -> m_TimeRate;
    lSTTSample += 1;
    lCurSample += 1;;
/* This advance walked lSTSSIdx -- the KEYFRAME cursor -- instead of lSTTSIdx. Harmless while
 * stss was never parsed ( lSTSSIdx was always 0 and unused ), but with a real stss table every
 * stts segment boundary would knock the keyframe walk out of sync, and lSTTSIdx never moved,
 * so DTS deltas after the first stts segment were wrong as well. */
    if ( lSTTSIdx + 1  < apMyStm -> m_nStts &&
         lSTTSample   == ( unsigned int )apMyStm -> m_pStts[ lSTTSIdx ].m_Count
    ) {
     lSTTSample = 0;
     lSTTSIdx  += 1;
    }  /* end if */
   }  /* end for */
  }  /* end for */
 } else {
  unsigned int lnChunkSamples, lChunkSize;
  int64_t lChunkDuration;
  for ( i = 0; i < apMyStm -> m_nChunks; ++i ) {
   lCurOffset = apMyStm -> m_pChunkOffsets[ i ];
   if ( lSTSCIdx + 1  < apMyStm -> m_nSample2Chunk &&
        i        + 1 == ( unsigned int )apMyStm -> m_pSample2Chunk[ lSTSCIdx + 1 ].m_First
   ) ++lSTSCIdx;
   lnChunkSamples = apMyStm -> m_pSample2Chunk[ lSTSCIdx ].m_Count;
   if (  apMyStm -> m_SampleSize > 1 )
    lChunkSize = lnChunkSamples * apMyStm -> m_SampleSize;
   else if (  apMyStm -> m_SamplesPerFrame > 0 && ( lnChunkSamples * apMyStm -> m_BytesPerFrame % apMyStm -> m_SamplesPerFrame == 0 )  )
    lChunkSize = lnChunkSamples * apMyStm -> m_BytesPerFrame / apMyStm -> m_SamplesPerFrame;
   else {
    lChunkSize = INT_MAX;
    for (  j = 0; j < ( unsigned int )apCont -> m_nStm; ++j  ) {
     MOVStream* lpMyStm = apCont -> m_pStm[ j ] -> m_pCtx;
     for ( k = lpMyStm -> m_NextChunk; k < lpMyStm -> m_nChunks; ++k) {
     if ( lpMyStm -> m_pChunkOffsets[ k ] > lCurOffset &&
          lpMyStm -> m_pChunkOffsets[ k ] - lCurOffset < lChunkSize
     ) {
      lChunkSize = ( unsigned int )( lpMyStm -> m_pChunkOffsets[ k ] - lCurOffset );
      lpMyStm -> m_NextChunk = k;
      break;
     }  /* end if */
    }  /* end for */
   }  /* end for */
   if ( lChunkSize == INT_MAX )
    for (  j = 0; j < ( unsigned int )lpCtx -> m_nMDAT; ++j ) {
     if ( lpCtx -> m_pMDAT[ j ].m_Offset <= lCurOffset &&
          lpCtx -> m_pMDAT[ j ].m_Offset + lpCtx -> m_pMDAT[ j ].m_Size > lCurOffset
     ) lChunkSize = ( unsigned int )( lpCtx -> m_pMDAT[ j ].m_Offset + lpCtx -> m_pMDAT[ j ].m_Size - lCurOffset );
    }  /* end for */
    for (  j = 0; j < ( unsigned int )apCont -> m_nStm; ++j  ) {
     MOVStream* lpMyStm = ( MOVStream* )apCont -> m_pStm[ j ] -> m_pCtx;
     lpMyStm -> m_NextChunk = 0;
    }  /* end for */
   }  /* end else */
   if ( !_add_idx_entry ( apMyStm, 1, ( uint32_t )lCurDTS, ( uint32_t )lCurOffset, lChunkSize ) ) return 0;
   lChunkDuration = 0;
   while ( lnChunkSamples > 0 && lSTTSIdx < apMyStm -> m_nStts ) {
    MOVStts* lpTime = &apMyStm -> m_pStts[ lSTTSIdx ];
    unsigned int lTake;
    if ( lpTime -> m_Count <= 0 ) { ++lSTTSIdx; continue; }
    lTake = lnChunkSamples < ( unsigned int )lpTime -> m_Count ? lnChunkSamples : ( unsigned int )lpTime -> m_Count;
    lChunkDuration += ( int64_t )lpTime -> m_Duration * lTake;
    lnChunkSamples -= lTake;
    lpTime -> m_Count -= lTake;
    if ( !lpTime -> m_Count ) ++lSTTSIdx;
   }  /* end while */
   if ( lnChunkSamples ) return 0;
   lCurDTS += lChunkDuration / apMyStm -> m_TimeRate;
  }  /* end for */
 }  /* end else */

 return 1;
}  /* end _mov_build_index */

static int _mov_edit_fits ( SMS_Stream* apStm, MOVStream* apMyStm, MOVContext* apCtx ) {
 int64_t lStart = 0, lEnd = apStm -> m_Duration;
 int64_t lLength = SMS_Rescale ( apMyStm -> m_EditDuration, apMyStm -> m_TimeScale, apCtx -> m_TimeScale );
 int lFirstDelta = apMyStm -> m_pStts[ 0 ].m_Duration;
 int lLastDelta = apMyStm -> m_pStts[ apMyStm -> m_nStts - 1 ].m_Duration;
 unsigned int i, lCTimeIdx = 0, lCTimeSample = 0;
 if ( apMyStm -> m_pCtts && apMyStm -> m_nCtts ) {
  lStart = SMS_MAXINT64;
  lEnd = -SMS_MAXINT64;
  for ( i = 0; i < ( unsigned int )apMyStm -> m_nIdx; ++i ) {
   int lOffset = 0;
   int64_t lPTS, lDelta;
   while ( lCTimeIdx < apMyStm -> m_nCtts && apMyStm -> m_pCtts[ lCTimeIdx ].m_Count <= 0 ) ++lCTimeIdx;
   if ( lCTimeIdx < apMyStm -> m_nCtts ) {
    MOVStts* lpTime = &apMyStm -> m_pCtts[ lCTimeIdx ];
    lOffset = lpTime -> m_Duration;
    if ( ++lCTimeSample == ( unsigned int )lpTime -> m_Count ) { ++lCTimeIdx; lCTimeSample = 0; }
   }  /* end if */
   lPTS = ( int64_t )apMyStm -> m_pIdx[ i ].m_DTS * apMyStm -> m_TimeRate + lOffset;
   lDelta = i + 1 < ( unsigned int )apMyStm -> m_nIdx ?
            ( int64_t )( apMyStm -> m_pIdx[ i + 1 ].m_DTS - apMyStm -> m_pIdx[ i ].m_DTS ) * apMyStm -> m_TimeRate : lLastDelta;
   if ( lPTS < lStart ) lStart = lPTS;
   if ( lPTS + lDelta > lEnd ) lEnd = lPTS + lDelta;
  }  /* end for */
 }  /* end if */
 /* Retain at most one codec sample of priming/tail rounding. Composition
  * reorder origin is part of the presentation coverage, not a content trim. */
 return lFirstDelta > 0 && lLastDelta > 0 && apMyStm -> m_EditStart <= lStart + lFirstDelta &&
        lEnd >= apMyStm -> m_EditStart && lLength >= lEnd - apMyStm -> m_EditStart - lLastDelta &&
        lLength <= lEnd - apMyStm -> m_EditStart + lLastDelta;
}  /* end _mov_edit_fits */

static int _mov_read_header ( SMS_Container* apCont ) {

 MOVContext*  lpMOV     = ( MOVContext* )apCont -> m_pCtx;
 FileContext* lpFileCtx = apCont -> m_pFileCtx;
 MOVAtom      lAtom     = { 0, 0, lpFileCtx -> m_Size };
 int          i, retVal;

 retVal = _mov_read_dflt ( lpMOV, lpFileCtx, &lAtom );

 if (  retVal < 0 || lpMOV -> m_fError || !lpMOV -> m_fMOOV || !lpMOV -> m_fMDAT  ) return 0;

 if ( lpFileCtx -> m_CurPos != lpMOV -> m_MDATOffset ) lpFileCtx -> Seek (  lpFileCtx, ( uint32_t )( lpMOV -> m_MDATOffset )  );

 for (  i = 0; i < ( int )apCont -> m_nStm; ++i  ) {

  SMS_Stream* lpStm   = apCont -> m_pStm[ i ];
  MOVStream*  lpMyStm;

  if ( !lpStm ) {
   memmove (
    &apCont -> m_pStm[ i ], &apCont -> m_pStm[ i + 1 ],
    ( &apCont -> m_pStm[ SMS_MAX_STREAMS ] - &apCont -> m_pStm[ i + 1 ] ) * 4
   );
   --apCont -> m_nStm;
   --i;
   continue;
  }  /* end if */

  lpMyStm = ( MOVStream* )lpStm -> m_pCtx;

  if ( !lpMyStm -> m_nStts        || !lpMyStm -> m_nChunks ||
       !lpMyStm -> m_nSample2Chunk|| (
         !lpMyStm -> m_SampleSize && !lpMyStm -> m_nSamples
        )
  ) {
   lpMyStm -> m_nSamples = 0;
   continue;
  }  /* end if */

  if ( !lpMyStm -> m_TimeRate  ) lpMyStm -> m_TimeRate  = 1;
  if ( !lpMyStm -> m_TimeScale ) lpMyStm -> m_TimeScale = lpMOV -> m_TimeScale;

  if ( lpMyStm -> m_HasEdit ) {
   int64_t lOffset;
   if ( lpMOV -> m_TimeScale <= 0 || lpMyStm -> m_TimeScale <= 0 ||
        lpMyStm -> m_EditEmpty + lpMyStm -> m_EditDuration > UINT_MAX ) return 0;
   lOffset = SMS_Rescale ( lpMyStm -> m_EditEmpty, lpMyStm -> m_TimeScale, lpMOV -> m_TimeScale ) - lpMyStm -> m_EditStart;
   lpMyStm -> m_TimeRate = _gcd ( lpMyStm -> m_TimeRate, ( int32_t )( lOffset % lpMyStm -> m_TimeRate ) );
   lpMyStm -> m_TimeOffset = lOffset / lpMyStm -> m_TimeRate;
   if ( lpMyStm -> m_TimeOffset < INT_MIN || lpMyStm -> m_TimeOffset > UINT_MAX ) return 0;
  }  /* end if */

  lpMyStm -> m_DTSShift /= lpMyStm -> m_TimeRate;

  SMSContainer_SetPTSInfo ( lpStm, lpMyStm -> m_TimeRate, lpMyStm -> m_TimeScale );

  if ( lpStm -> m_pCodec -> m_Type == SMS_CodecTypeAudio &&
       lpMyStm -> m_nStts == 1
  ) lpStm -> m_pCodec -> m_FrameSize = lpMyStm -> m_pStts[ 0 ].m_Duration;

  if ( !_mov_build_index ( apCont, lpMyStm, lpStm ) ) return 0;
  if ( lpMyStm -> m_HasEdit ) {
   if ( !_mov_edit_fits ( lpStm, lpMyStm, lpMOV ) ) return 0;
   lpStm -> m_Duration = SMS_Rescale ( lpMyStm -> m_EditEmpty + lpMyStm -> m_EditDuration,
                                     lpMyStm -> m_TimeScale, lpMOV -> m_TimeScale );
  }  /* end if */
  if ( lpStm -> m_Duration != SMS_NOPTS_VALUE ) lpStm -> m_Duration /= lpMyStm -> m_TimeRate;
  if ( lpMyStm -> m_HasEdit ) {
   unsigned int j;
   for ( j = 0; j < ( unsigned int )lpMyStm -> m_nIdx; ++j )
    if ( ( int64_t )lpMyStm -> m_pIdx[ j ].m_DTS + lpMyStm -> m_TimeOffset > UINT_MAX ) return 0;
  }  /* end if */

/* _ReadPacket bounds the sample cursor with m_nSamples but indexes m_pIdx with it, and the
 * seek repositions that same cursor -- so a file whose tables declare more samples than the
 * chunks really contain ( truncated download ) would walk off the end of the index. Clamp
 * to what was actually indexed. For the per-chunk audio path below this is also the more
 * correct bound: there m_CurSample counts index entries ( chunks ), not samples. */
  if (  lpMyStm -> m_nSamples > ( unsigned int )lpMyStm -> m_nIdx  )
   lpMyStm -> m_nSamples = lpMyStm -> m_nIdx;

  _mov_free_tables ( lpMyStm, 0 );
 }  /* end for */

/* Per-chunk audio sizing can inspect another stream's chunk offsets. Retain
 * those offsets until all indices are built; release the other tables early. */
 for ( i = 0; i < ( int )apCont -> m_nStm; ++i )
  _mov_free_tables ( ( MOVStream* )apCont -> m_pStm[ i ] -> m_pCtx, 1 );
 _free ( lpMOV -> m_pMDAT );
 lpMOV -> m_pMDAT = NULL;

 return 1;

}  /* end _mov_read_header */

static uint32_t _mov_probe ( FileContext* apFileCtx ) {

 unsigned int lSize;
 unsigned int lTag   = 0;
 unsigned int lType  = 0;

 lSize = File_GetUInt ( apFileCtx );
 lType = File_GetUInt ( apFileCtx );
 lTag  = File_GetUInt ( apFileCtx );

 lSize = lType == SMS_MKTAG( 'f', 't', 'y', 'p' ) ? lTag : 0;

 apFileCtx -> Seek ( apFileCtx, 0 );

 return lSize;

}  /* end _mov_probe */

static int _mov_next_ctime ( MOVStream* apStm ) {
 int lOffset = 0;
 if ( !apStm -> m_pCtts ) return 0;
 while ( apStm -> m_Sample2CTimeIdx < apStm -> m_nCtts &&
         apStm -> m_pCtts[ apStm -> m_Sample2CTimeIdx ].m_Count <= 0 )
  ++apStm -> m_Sample2CTimeIdx;
 if ( apStm -> m_Sample2CTimeIdx < apStm -> m_nCtts ) {
  MOVStts* lpTime = &apStm -> m_pCtts[ apStm -> m_Sample2CTimeIdx ];
  lOffset = lpTime -> m_Duration;
  if ( ++apStm -> m_Sample2CTimeSample >= lpTime -> m_Count ) {
   ++apStm -> m_Sample2CTimeIdx;
   apStm -> m_Sample2CTimeSample = 0;
  }
 }
 return lOffset;
}  /* end _mov_next_ctime */

static int _ReadPacket ( SMS_Container* apCont, int* apIdx ) {

 FileContext*  lpFileCtx = apCont -> m_pFileCtx;
 SMS_Stream*   lpStm     = NULL;
 MOVIndex*     lpSample  = NULL;
 MOVStream*    lpMyStm   = NULL;
 unsigned int  i, lIdx   = 0;
 SMS_AVPacket* lpPkt;
 int           lCTime;

 do {

  int64_t lBestDTS = SMS_MAXINT64;
  lpSample = NULL;

  for ( i = 0; i < apCont -> m_nStm; ++i ) {
   SMS_Stream* lpCurStm   = apCont -> m_pStm[ i ];
   MOVStream*  lpMyCurStm = ( MOVStream* )lpCurStm -> m_pCtx;
   if ( lpMyCurStm -> m_pIdx && lpMyCurStm -> m_TimeScale > 0 && lpMyCurStm -> m_TimeRate > 0 &&
        lpMyCurStm -> m_CurSample < lpMyCurStm -> m_nSamples &&
        lpMyCurStm -> m_CurSample < ( unsigned int )lpMyCurStm -> m_nIdx ) {
    MOVIndex* lpCurSample = &lpMyCurStm -> m_pIdx[ lpMyCurStm -> m_CurSample ];
    int64_t   lDTS = SMS_Rescale ( ( ( int64_t )lpCurSample -> m_DTS + lpMyCurStm -> m_TimeOffset - lpMyCurStm -> m_DTSShift ) * lpMyCurStm -> m_TimeRate, SMS_TIME_BASE, lpMyCurStm -> m_TimeScale );
    if ( lDTS < lBestDTS ) {
     lpSample = lpCurSample;
     lBestDTS = lDTS;
     lpStm    = lpCurStm;
     lpMyStm  = lpMyCurStm;
     lIdx     = i;
    }  /* end if */
   }   /* end if */
  }  /* end for */

  if ( !lpSample ) return -1;

  ++lpMyStm -> m_CurSample;
  lCTime = _mov_next_ctime ( lpMyStm );

 } while ( !lpStm -> m_pPktBuf || !lpSample -> m_Size );

 if ( lpSample -> m_Pos > lpFileCtx -> m_Size ||
      lpSample -> m_Size > lpFileCtx -> m_Size - lpSample -> m_Pos ) return -1;

 lpPkt = apCont -> AllocPacket ( lpStm -> m_pPktBuf, lpSample -> m_Size );

 if ( !lpPkt ) return -1;

 if ( lpFileCtx -> m_CurPos != lpSample -> m_Pos ) {
  if ( lpFileCtx -> m_CurPos < lpSample -> m_Pos )
   File_Skip ( lpFileCtx, lpSample -> m_Pos - lpFileCtx -> m_CurPos );
  else lpFileCtx -> Seek ( lpFileCtx, lpSample -> m_Pos );
 }  /* end if */

 if ( lpFileCtx -> m_CurPos != lpSample -> m_Pos ||
      lpFileCtx -> Read ( lpFileCtx, lpPkt -> m_pData, lpPkt -> m_Size ) != lpPkt -> m_Size ) {
  SMS_RingBufferUnalloc ( lpStm -> m_pPktBuf, lpSample -> m_Size + 64 );
  return -1;
 }  /* end if */

 lpPkt -> m_StmIdx = lIdx;
 lpPkt -> m_DTS = ( int64_t )lpSample -> m_DTS + lpMyStm -> m_TimeOffset - lpMyStm -> m_DTSShift;
 lpPkt -> m_PTS = ( int64_t )lpSample -> m_DTS + lpMyStm -> m_TimeOffset + lCTime / lpMyStm -> m_TimeRate;

 lpPkt -> m_Flags |= lpSample -> m_Flags ? SMS_PKT_FLAG_KEY : 0;

 apIdx[ 0 ] = lpPkt -> m_StmIdx;

 SMSContainer_CalcPktFields ( lpStm, lpPkt );

 return lpPkt -> m_Size;

}  /* end _ReadPacket */

static unsigned int _mov_locate_sample ( MOVIndex* apIdx, int anIdx, uint32_t aDTS ) {
 unsigned int lA, lB, lM;
 if ( anIdx <= 0 ) return 0;
 lA = 0;
 lB = anIdx;
 while ( lA < lB ) {
  lM = lA + ( ( lB - lA ) >> 1 );
  if ( apIdx[ lM ].m_DTS <= aDTS ) lA = lM + 1;
  else lB = lM;
 }  /* end while */
 return lA ? lA - 1 : 0;
}  /* end _mov_locate_sample */

static void _mov_sync_ctime ( MOVStream* apMyStm ) {

 unsigned int i;
 uint64_t lCovered;

/* _ReadPacket walks the ctts table linearly alongside the sample cursor, so a seek that
 * moves m_CurSample must replay this walk up to the same sample or every PTS from here on
 * is garbage. No ctts ( no B-frames ) means PTS == DTS and there is nothing to replay.
 * A retained allocation with no published entries is also nothing to replay. */
 if ( !apMyStm -> m_pCtts || !apMyStm -> m_nCtts ) return;

 lCovered = 0;

 for ( i = 0; i < apMyStm -> m_nCtts; ++i ) {

  if ( apMyStm -> m_pCtts[ i ].m_Count <= 0 ) continue;

  if (  lCovered + ( unsigned int )apMyStm -> m_pCtts[ i ].m_Count > apMyStm -> m_CurSample  ) {

   apMyStm -> m_Sample2CTimeIdx    = i;
   apMyStm -> m_Sample2CTimeSample = apMyStm -> m_CurSample - lCovered;
   return;

  }  /* end if */

  lCovered += apMyStm -> m_pCtts[ i ].m_Count;

 }  /* end for */

/* Past a truncated table, no offset is known. The packet reader uses DTS. */
 apMyStm -> m_Sample2CTimeIdx    = apMyStm -> m_nCtts;
 apMyStm -> m_Sample2CTimeSample = 0;

}  /* end _mov_sync_ctime */

static int _Seek ( SMS_Container* apCont, int anIdx, int aDir, uint32_t aPos ) {

 SMS_Stream*  lpStm;
 MOVStream*   lpMyStm;
 FileContext* lpFileCtx = apCont -> m_pFileCtx;
 int64_t      lTime, lTarget;
 uint32_t     lSample, lEnd, lPos, i, lOldPos;
 uint32_t     lSamples[ SMS_MAX_STREAMS ];

 if (  anIdx < 0 || anIdx >= ( int )apCont -> m_nStm || apCont -> m_nStm > SMS_MAX_STREAMS  ) return 0;

 lpStm   = apCont -> m_pStm[ anIdx ];
 lpMyStm = ( MOVStream* )lpStm -> m_pCtx;

 if (  lpMyStm -> m_nIdx <= 0 || !lpMyStm -> m_pIdx ||
       lpMyStm -> m_TimeRate <= 0 || lpMyStm -> m_TimeScale <= 0  ) return 0;

/* CONTRACT -- mirrors _Seek in SMS_ContainerAVI.c, which every caller was written against.
 * aPos is the target timestamp in the INDEXED ( video ) stream's time-base units: callers
 * ( PlayerControl_Scroll / PlayerControl_ScrollBar for FF-REW and the scrollbar, _sms_play
 * for resume-from-history ) rescale wall time with lpStm -> m_TimeBase, and the index DTS
 * is stored in those units before the edit-list offset, which is removed below. aDir is
 * +1 forward / -1 backward and picks which side of aPos the keyframe is taken from; return
 * is 1 on success, 0 when the target is out of range. The packet ring buffers are NOT
 * flushed here: every caller drains and resets them around the seek ( _init_queues /
 * InitQueues ), exactly as with AVI. aDir == 0 never comes from the video seek paths --
 * treat it as backward instead of walking "in direction 0" forever. */
 aDir = aDir > 0 ? 1 : -1;

/* Same range check AVI does ( target past the last frame fails ). */
 lTarget = ( int64_t )aPos - lpMyStm -> m_TimeOffset;
 if ( lTarget < 0 ) lTarget = 0;
 if (  lTarget > lpMyStm -> m_pIdx[ lpMyStm -> m_nIdx - 1 ].m_DTS  ) return 0;

 lSample = _mov_locate_sample ( lpMyStm -> m_pIdx, lpMyStm -> m_nIdx, ( uint32_t )lTarget );
 if ( aDir > 0 && lpMyStm -> m_pIdx[ lSample ].m_DTS < lTarget ) ++lSample;
 lEnd    = aDir > 0 ? ( uint32_t )lpMyStm -> m_nIdx : 0;

 while (  lSample != lEnd && !lpMyStm -> m_pIdx[ lSample ].m_Flags  ) lSample += aDir;

 if (  lSample == ( uint32_t )lpMyStm -> m_nIdx || !lpMyStm -> m_pIdx[ lSample ].m_Flags  ) return 0;

/* The keyframe's timestamp in the neutral SMS_TIME_BASE units _ReadPacket uses to pick the
 * lowest-DTS stream. The other streams' cursors are placed on that same instant so the
 * interleave resumes consistently -- audio typically lands a few frames BEFORE the video
 * keyframe, which is exactly what the decoders expect after a seek. */
 lTime = SMS_Rescale (
  ( ( int64_t )lpMyStm -> m_pIdx[ lSample ].m_DTS + lpMyStm -> m_TimeOffset ) * lpMyStm -> m_TimeRate,
  SMS_TIME_BASE, lpMyStm -> m_TimeScale
 );

 for ( i = 0; i < apCont -> m_nStm; ++i )
  lSamples[ i ] = ( ( MOVStream* )apCont -> m_pStm[ i ] -> m_pCtx ) -> m_CurSample;
 lSamples[ anIdx ] = lSample;

 lPos = lpMyStm -> m_pIdx[ lSample ].m_Pos;

 for ( i = 0; i < apCont -> m_nStm; ++i ) {

  MOVStream* lpMyCurStm;

  if ( i == ( uint32_t )anIdx ) continue;

  lpMyCurStm = ( MOVStream* )apCont -> m_pStm[ i ] -> m_pCtx;

  if (  !lpMyCurStm -> m_nIdx  ) continue;
  if ( !lpMyCurStm -> m_pIdx || lpMyCurStm -> m_TimeRate <= 0 || lpMyCurStm -> m_TimeScale <= 0 ) return 0;

  lTarget = SMS_Rescale ( lTime, lpMyCurStm -> m_TimeScale, SMS_TIME_BASE * ( int64_t )lpMyCurStm -> m_TimeRate ) - lpMyCurStm -> m_TimeOffset;
  if ( lTarget < 0 ) lTarget = 0;
  if ( lTarget > UINT_MAX ) lTarget = UINT_MAX;
  lSample = _mov_locate_sample (
   lpMyCurStm -> m_pIdx, lpMyCurStm -> m_nIdx,
   ( uint32_t )lTarget
  );

  lSamples[ i ] = lSample;

  if (  lpMyCurStm -> m_pIdx[ lSample ].m_Pos < lPos  ) lPos = lpMyCurStm -> m_pIdx[ lSample ].m_Pos;

 }  /* end for */

/* The scrollbar restarts file streaming at m_CurPos right after this returns, so leave the
 * file on the earliest sample ANY stream will read, not on the video keyframe. _ReadPacket
 * re-seeks per sample anyway, but starting the streamer here keeps the buffer ahead full. */
 if ( lPos >= lpFileCtx -> m_Size ) return 0;
 lOldPos = lpFileCtx -> m_CurPos;
 if ( lOldPos != lPos ) {
  lpFileCtx -> Seek ( lpFileCtx, lPos );
  if ( lpFileCtx -> m_CurPos != lPos ) {
   if ( lpFileCtx -> m_CurPos != lOldPos ) lpFileCtx -> Seek ( lpFileCtx, lOldPos );
   return 0;
  }  /* end if */
 }  /* end if */
 /* Publish all sample/composition cursors only once positioning succeeds. */
 for ( i = 0; i < apCont -> m_nStm; ++i ) {
  MOVStream* lpCur = ( MOVStream* )apCont -> m_pStm[ i ] -> m_pCtx;
  lpCur -> m_CurSample = lSamples[ i ];
  _mov_sync_ctime ( lpCur );
 }  /* end for */
 return 1;

}  /* end _Seek */

static int _GetContainerMOV ( SMS_Container* apCont, const MOVParseEntry* apParseTbl ) {

 int          retVal    = 0;
 FileContext* lpFileCtx = apCont -> m_pFileCtx;
 uint32_t     lType     = _mov_probe (  lpFileCtx );

 if ( lType ) {

  MOVContext* lpMyCtx = ( MOVContext* )calloc (  1, sizeof ( MOVContext )  );

  if ( !lpMyCtx ) return 0;
  apCont  -> m_pCtx      = lpMyCtx;
  lpMyCtx -> m_pBase     = apCont;
  lpMyCtx -> m_pParseTbl = apParseTbl;

  if (  _mov_read_header ( apCont )  ) {

   int i;

   apCont -> m_pName    = g_pMOV;
   apCont -> ReadPacket = _ReadPacket;

/* FF/REW and the scrollbar reach Seek only through SMS_CONT_FLAGS_SEEKABLE, so gate both on
 * a video stream with a built index. This function is shared with the M4A reader, but there
 * _mov_read_hdlr_m4a has already freed every video track ( and an audio-only .mp4 simply has
 * none ) -- those must NOT become seekable, because the audio-only player path ( _FFwd_A /
 * _Rew_A in SMS_Player.c ) passes a LIST NODE POINTER as the seek position, which a
 * timestamp seek would chase into the weeds. With no video there is no keyframe to land on
 * anyway, so such files keep the old inert behavior. */
   for (  i = 0; i < ( int )apCont -> m_nStm; ++i  ) {

    SMS_Stream* lpStm = apCont -> m_pStm[ i ];

    if (  lpStm -> m_pCodec -> m_Type == SMS_CodecTypeVideo &&
          (  ( MOVStream* )lpStm -> m_pCtx  ) -> m_nIdx > 0
    ) {
     apCont -> Seek     = _Seek;
     apCont -> m_Flags |= SMS_CONT_FLAGS_SEEKABLE;
     break;
    }  /* end if */

   }  /* end for */

   retVal = 1;

  } else {
   _free ( lpMyCtx -> m_pMDAT );
   SMS_DestroyContainer ( apCont, 0 );
   apCont -> m_nStm = 0;
  }  /* end else */

 }  /* end if */

 return retVal;

}  /* end _GetContainerMOV */

uint64_t SMS_M4AProbe ( FileContext* apFileCtx, SMS_AudioInfo* apInfo ) {

 uint64_t retVal = 0;
 uint32_t lType  = _mov_probe ( apFileCtx );

 if ( lType ) {

  SMS_Container lCont; memset ( &lCont, 0, sizeof ( lCont ) );
  MOVContext    lMyCont; memset (  &lMyCont, 0, sizeof ( lMyCont )  );
  MOVAtom       lAtom = { 0, 0, apFileCtx -> m_Size };

  lCont.m_pCtx        = &lMyCont;
  lCont.m_pFileCtx    = apFileCtx;
  lMyCont.m_pBase     = &lCont;
  lMyCont.m_pMDAT     = ( MOVMDat* )apInfo;
  lMyCont.m_pParseTbl = s_M4AProbeTbl;

  apInfo -> m_SampleRate = 0;
  apInfo -> m_nChannels  = 0;
  apInfo -> m_BitRate    = 0;

  /* The metadata table does not create/index streams or set MOOV/MDAT flags.
   * Use its parser result directly; _mov_read_header maps errors to zero. */
  retVal = _mov_read_dflt ( &lMyCont, apFileCtx, &lAtom ) >= 0 && !lMyCont.m_fError &&
           apInfo -> m_SampleRate && apInfo -> m_nChannels && apInfo -> m_BitRate;

 }  /* end if */

 apFileCtx -> Seek ( apFileCtx, 0 );

 return retVal;

}  /* end SMS_M4AProbe */

int SMS_GetContainerMOV ( SMS_Container* apCont ) {

 int retVal = 0;

 if (  ( int )apCont -> m_pFileCtx <= 0  ) return retVal;

 s_MOVParseTbl[ 0 ].m_Func = _mov_read_hdlr;

 if (   (  retVal = _GetContainerMOV ( apCont, s_MOVParseTbl )  )   )
  retVal = SMSContainer_SetName ( apCont, apCont -> m_pFileCtx );

 return retVal;

}  /* end SMS_GetContainerMOV */

int SMS_GetContainerM4A ( SMS_Container* apCont ) {

 int retVal = 0;

 if (  ( int )apCont -> m_pFileCtx <= 0  ) return retVal;

 s_MOVParseTbl[ 0 ].m_Func = _mov_read_hdlr_m4a;

 if (   (  retVal = _GetContainerMOV ( apCont, s_MOVParseTbl )  )   )
  retVal = SMSContainer_SetName ( apCont, apCont -> m_pFileCtx );

 return retVal;

}  /* end SMS_GetContainerM4A */
