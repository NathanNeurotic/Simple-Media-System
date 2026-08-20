/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# Licenced under Academic Free License version 2.0
#
*/
#include "SMS_ContainerMKV.h"
#include "SMS_FourCC.h"
#include "SMS_Locale.h"

#include <malloc.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#define MKV_UNKNOWN_SIZE 0xFFFFFFFFFFFFFFFFULL
#define MKV_MAX_TRACKS   32
#define MKV_MAX_CUES     4096

/* EBML / Matroska Element IDs */
#define EBML_ID_HEADER          0x1A45DFA3
#define EBML_ID_DOCTYPE         0x4282
#define EBML_ID_DOCTYPE_VERSION 0x4287
#define EBML_ID_DOCTYPE_RVER    0x4285

#define MKV_ID_SEGMENT          0x18538067
#define MKV_ID_SEEKHEAD         0x114D9B74
#define MKV_ID_SEEK             0x4DBB
#define MKV_ID_SEEK_ID          0x53AB
#define MKV_ID_SEEK_POS         0x53AC

#define MKV_ID_INFO             0x1549A966
#define MKV_ID_TIMECODESCALE    0x2AD7B1
#define MKV_ID_DURATION         0x4489
#define MKV_ID_TITLE            0x7BA9
#define MKV_ID_MUXINGAPP        0x4D80
#define MKV_ID_WRITINGAPP       0x5741

#define MKV_ID_TRACKS           0x1654AE6B
#define MKV_ID_TRACK_ENTRY      0xAE
#define MKV_ID_TRACK_NUMBER     0xD7
#define MKV_ID_TRACK_UID        0x73C5
#define MKV_ID_TRACK_TYPE       0x83
#define MKV_ID_FLAG_ENABLED     0xB9
#define MKV_ID_FLAG_DEFAULT     0x88
#define MKV_ID_FLAG_FORCED      0x55AA
#define MKV_ID_FLAG_LACING      0x9C
#define MKV_ID_DEFAULT_DURATION 0x23E383
#define MKV_ID_NAME             0x536E
#define MKV_ID_LANGUAGE         0x22B59C
#define MKV_ID_CODEC_ID         0x86
#define MKV_ID_CODEC_PRIVATE    0x63A2
#define MKV_ID_CODEC_NAME       0x258688

#define MKV_ID_VIDEO            0xE0
#define MKV_ID_PIXEL_WIDTH      0xB0
#define MKV_ID_PIXEL_HEIGHT     0xBA
#define MKV_ID_DISPLAY_WIDTH    0x54B0
#define MKV_ID_DISPLAY_HEIGHT   0x54BA
#define MKV_ID_FLAG_INTERLACED  0x9A

#define MKV_ID_AUDIO            0xE1
#define MKV_ID_SAMPLING_FREQ    0xB5
#define MKV_ID_OUT_SAMPLING_FRQ 0x78B5
#define MKV_ID_CHANNELS         0x9F
#define MKV_ID_BIT_DEPTH        0x6264

#define MKV_ID_CUES             0x1C53BB6B
#define MKV_ID_CUE_POINT        0xBB
#define MKV_ID_CUE_TIME         0xB3
#define MKV_ID_CUE_TRACK_POS    0xB7
#define MKV_ID_CUE_TRACK        0xF7
#define MKV_ID_CUE_CLUSTER_POS  0xF1
#define MKV_ID_CUE_REL_POS      0xF0

#define MKV_ID_CLUSTER          0x1F43B675
#define MKV_ID_CLUSTER_TIMECODE 0xE7
#define MKV_ID_POSITION         0xA7
#define MKV_ID_PREV_SIZE        0xAB
#define MKV_ID_SIMPLE_BLOCK     0xA3
#define MKV_ID_BLOCK_GROUP      0xA0
#define MKV_ID_BLOCK            0xA1
#define MKV_ID_BLOCK_DURATION   0x8B
#define MKV_ID_REFERENCE_BLOCK  0xFB

/* Matroska Track Types */
#define MKV_TRACK_TYPE_VIDEO    1
#define MKV_TRACK_TYPE_AUDIO    2
#define MKV_TRACK_TYPE_COMPLEX  3
#define MKV_TRACK_TYPE_SUBTITLE 0x11

static char s_pMKV[] __attribute__(   (  aligned( 1 ), section( ".data" )  )   ) = "MKV";

typedef struct MKVTrack {
 uint32_t      m_TrackNumber;
 uint32_t      m_TrackType;
 uint8_t       m_fEnabled;
 uint8_t       m_fDefault;
 uint8_t       m_fForced;
 uint8_t       m_fLacing;
 uint32_t      m_Width;
 uint32_t      m_Height;
 uint32_t      m_DisplayWidth;
 uint32_t      m_DisplayHeight;
 uint64_t      m_DefaultDuration; /* nanoseconds */
 uint32_t      m_SampleRate;
 uint32_t      m_Channels;
 uint32_t      m_BitDepth;
 char*         m_pCodecID;
 uint8_t*      m_pCodecPrivate;
 uint32_t      m_CodecPrivateLen;
 char*         m_pLanguage;
 char*         m_pName;
 SMS_CodecID   m_SMSCodecID;
 SMS_CodecType m_SMSCodecType;
 uint32_t      m_FourCC;
 int           m_StmIdx; /* Index in SMS_Container m_pStm (-1 if not published) */
} MKVTrack;

typedef struct MKVCue {
 uint64_t m_Time;        /* in TimestampScale units */
 uint32_t m_ClusterPos;  /* relative to Segment data start */
 uint32_t m_Track;
} MKVCue;

typedef struct MKVLacedFrame {
 uint32_t m_Offset;
 uint32_t m_Size;
} MKVLacedFrame;

typedef struct MKVContext {
 SMS_Container* m_pBase;
 uint64_t       m_TimestampScale;   /* in nanoseconds (default: 1,000,000) */
 double         m_Duration;         /* in TimestampScale units */
 uint32_t       m_SegmentDataStart; /* File offset where Segment payload starts */
 uint32_t       m_SegmentDataEnd;   /* File offset where Segment ends (or m_Size) */
 
 MKVTrack       m_Tracks[ MKV_MAX_TRACKS ];
 int            m_nTracks;
 
 MKVCue*        m_pCues;
 int            m_nCues;
 int            m_CuesAlloc;
 
 uint64_t       m_ClusterTimestamp; /* Current cluster timestamp (scaled units) */
 uint32_t       m_CurClusterPos;
 
 /* Laced frames state for ReadPacket */
 MKVLacedFrame  m_LacedFrames[ 64 ];
 int            m_nLacedFrames;
 int            m_CurLacedFrame;
 int            m_LacedStmIdx;
 int64_t        m_LacedPTS;
 uint32_t       m_LacedFlags;
} MKVContext;

/* ========================================================================= */
/* EBML Parsing Primitives                                                   */
/* ========================================================================= */

static int _ebml_read_id ( FileContext* apFileCtx, uint32_t* apID, uint32_t* apLen ) {
 int      lB0 = File_GetByte ( apFileCtx );
 uint32_t lID;
 uint32_t lLen;

 if ( lB0 <= 0 && FILE_EOF ( apFileCtx ) ) return 0;

 if ( lB0 & 0x80 ) {
  lLen = 1;
  lID  = lB0;
 } else if ( lB0 & 0x40 ) {
  lLen = 2;
  lID  = ( lB0 << 8 ) | File_GetByte ( apFileCtx );
 } else if ( lB0 & 0x20 ) {
  lLen = 3;
  lID  = ( lB0 << 16 ) | ( File_GetByte ( apFileCtx ) << 8 ) | File_GetByte ( apFileCtx );
 } else if ( lB0 & 0x10 ) {
  lLen = 4;
  lID  = ( lB0 << 24 ) | ( File_GetByte ( apFileCtx ) << 16 ) | ( File_GetByte ( apFileCtx ) << 8 ) | File_GetByte ( apFileCtx );
 } else {
  return 0; /* Invalid EBML ID (length > 4 or 0) */
 }

 if ( apID  ) *apID  = lID;
 if ( apLen ) *apLen = lLen;
 return 1;
}  /* end _ebml_read_id */

static int _ebml_read_vint ( FileContext* apFileCtx, uint64_t* apVal, uint32_t* apLen ) {
 int      lB0 = File_GetByte ( apFileCtx );
 uint32_t lLen = 0;
 uint64_t lVal = 0;
 uint32_t i;

 if ( lB0 <= 0 && FILE_EOF ( apFileCtx ) ) return 0;

 for ( i = 0; i < 8; ++i ) {
  if ( lB0 & ( 0x80 >> i ) ) {
   lLen  = i + 1;
   lVal  = ( uint64_t )( lB0 & ( 0xFF >> ( i + 1 ) ) );
   break;
  }
 }

 if ( !lLen ) return 0;

 for ( i = 1; i < lLen; ++i ) {
  lVal = ( lVal << 8 ) | ( uint8_t )File_GetByte ( apFileCtx );
 }

 if ( apLen ) *apLen = lLen;

 /* Check for unknown size (all 1 bits in data field) */
 if ( lVal == ( ( 1ULL << ( 7 * lLen ) ) - 1ULL ) ) {
  if ( apVal ) *apVal = MKV_UNKNOWN_SIZE;
 } else {
  if ( apVal ) *apVal = lVal;
 }

 return 1;
}  /* end _ebml_read_vint */

static char* _strdup ( const char* apStr ) {
 if ( !apStr ) return NULL;
 int   lLen   = strlen ( apStr );
 char* retVal = ( char* )malloc ( lLen + 1 );
 if ( retVal ) strcpy ( retVal, apStr );
 return retVal;
}  /* end _strdup */

static uint64_t _ebml_read_uint ( FileContext* apFileCtx, uint64_t aSize ) {
 uint64_t retVal = 0;
 uint32_t i;
 if ( aSize > 8 ) {
  File_Skip ( apFileCtx, ( uint32_t )aSize );
  return 0;
 }
 for ( i = 0; i < ( uint32_t )aSize; ++i ) {
  retVal = ( retVal << 8 ) | ( uint8_t )File_GetByte ( apFileCtx );
 }
 return retVal;
}  /* end _ebml_read_uint */

static int64_t _ebml_read_sint ( FileContext* apFileCtx, uint64_t aSize ) __attribute__((unused));
static int64_t _ebml_read_sint ( FileContext* apFileCtx, uint64_t aSize ) {
 int64_t  retVal = 0;
 uint32_t i;
 int      lB0;

 if ( aSize == 0 || aSize > 8 ) {
  File_Skip ( apFileCtx, ( uint32_t )aSize );
  return 0;
 }

 lB0 = File_GetByte ( apFileCtx );
 retVal = ( int8_t )lB0;

 for ( i = 1; i < ( uint32_t )aSize; ++i ) {
  retVal = ( retVal << 8 ) | ( uint8_t )File_GetByte ( apFileCtx );
 }

 return retVal;
}  /* end _ebml_read_sint */

static double _ebml_read_float ( FileContext* apFileCtx, uint64_t aSize ) {
 if ( aSize == 4 ) {
  union {
   uint32_t u;
   float    f;
  } lU;
  lU.u = ( uint32_t )_ebml_read_uint ( apFileCtx, 4 );
  return ( double )lU.f;
 } else if ( aSize == 8 ) {
  union {
   uint64_t u;
   double   d;
  } lU;
  lU.u = _ebml_read_uint ( apFileCtx, 8 );
  return lU.d;
 } else {
  File_Skip ( apFileCtx, ( uint32_t )aSize );
  return 0.0;
 }
}  /* end _ebml_read_float */

static char* _ebml_read_string ( FileContext* apFileCtx, uint64_t aSize ) {
 char*    retVal = NULL;
 uint32_t lSize  = ( uint32_t )aSize;

 if ( lSize > 4096 ) lSize = 4096;

 retVal = ( char* )malloc ( lSize + 1 );
 if ( retVal ) {
  if ( lSize > 0 ) apFileCtx -> Read ( apFileCtx, retVal, lSize );
  retVal[ lSize ] = '\0';
 }

 if ( aSize > lSize ) {
  File_Skip ( apFileCtx, ( uint32_t )( aSize - lSize ) );
 }

 return retVal;
}  /* end _ebml_read_string */

static uint8_t* _ebml_read_binary ( FileContext* apFileCtx, uint64_t aSize, uint32_t* apOutLen ) {
 uint8_t* retVal = NULL;
 uint32_t lSize  = ( uint32_t )aSize;

 if ( lSize > 65536 ) lSize = 65536;

 retVal = ( uint8_t* )malloc ( ( lSize + 15 ) & ~15 );
 if ( retVal ) {
  if ( lSize > 0 ) apFileCtx -> Read ( apFileCtx, retVal, lSize );
  if ( apOutLen ) *apOutLen = lSize;
 } else {
  if ( apOutLen ) *apOutLen = 0;
 }

 if ( aSize > lSize ) {
  File_Skip ( apFileCtx, ( uint32_t )( aSize - lSize ) );
 }

 return retVal;
}  /* end _ebml_read_binary */

/* ========================================================================= */
/* Codec Mapping and Compatibility (A4)                                      */
/* ========================================================================= */

static void _mkv_map_codec ( MKVTrack* apTrack ) {
 apTrack -> m_SMSCodecID   = SMS_CodecID_NULL;
 apTrack -> m_SMSCodecType = SMS_CodecTypeUnknown;
 apTrack -> m_FourCC       = 0;

 if ( !apTrack -> m_pCodecID ) return;

 /* Video Codecs */
 if ( apTrack -> m_TrackType == MKV_TRACK_TYPE_VIDEO ) {
  apTrack -> m_SMSCodecType = SMS_CodecTypeVideo;

  if ( !strcmp ( apTrack -> m_pCodecID, "V_MPEG4/ISO/ASP" ) ||
       !strcmp ( apTrack -> m_pCodecID, "V_MPEG4/ISO/SP"  ) ||
       !strcmp ( apTrack -> m_pCodecID, "V_MPEG4/ISO/AP"  )
  ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_MPEG4;
   apTrack -> m_FourCC     = SMS_MKTAG ( 'D', 'I', 'V', 'X' );
  } else if ( !strcmp ( apTrack -> m_pCodecID, "V_MPEG1" ) ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_MPEG1;
   apTrack -> m_FourCC     = SMS_MKTAG ( 'M', 'P', 'G', '1' );
  } else if ( !strcmp ( apTrack -> m_pCodecID, "V_MPEG2" ) ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_MPEG2;
   apTrack -> m_FourCC     = SMS_MKTAG ( 'M', 'P', 'G', '2' );
  } else if ( !strcmp ( apTrack -> m_pCodecID, "V_MS/VFW/FOURCC" ) ) {
   if ( apTrack -> m_pCodecPrivate && apTrack -> m_CodecPrivateLen >= 40 ) {
    uint32_t lComp = SMS_unaligned32 ( apTrack -> m_pCodecPrivate + 16 );
    apTrack -> m_FourCC     = lComp;
    apTrack -> m_SMSCodecID = SMS_CodecGetID ( SMS_CodecTypeVideo, lComp );
   }
  }
 }
 /* Audio Codecs */
 else if ( apTrack -> m_TrackType == MKV_TRACK_TYPE_AUDIO ) {
  apTrack -> m_SMSCodecType = SMS_CodecTypeAudio;

  if ( !strncmp ( apTrack -> m_pCodecID, "A_AAC", 5 ) ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_AAC;
   apTrack -> m_FourCC     = SMS_MKTAG ( 'M', 'P', '4', 'A' );
  } else if ( !strcmp ( apTrack -> m_pCodecID, "A_MPEG/L3" ) ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_MP3;
   apTrack -> m_FourCC     = 0x55;
  } else if ( !strcmp ( apTrack -> m_pCodecID, "A_MPEG/L2" ) || !strcmp ( apTrack -> m_pCodecID, "A_MPEG/L1" ) ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_MP2;
   apTrack -> m_FourCC     = 0x50;
  } else if ( !strcmp ( apTrack -> m_pCodecID, "A_AC3" ) ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_AC3;
   apTrack -> m_FourCC     = 0x2000;
  } else if ( !strcmp ( apTrack -> m_pCodecID, "A_DTS" ) ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_DTS;
   apTrack -> m_FourCC     = 0x2001;
  } else if ( !strcmp ( apTrack -> m_pCodecID, "A_VORBIS" ) ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_OGGV;
   apTrack -> m_FourCC     = SMS_MKTAG ( 'O', 'G', 'G', 'V' );
  } else if ( !strcmp ( apTrack -> m_pCodecID, "A_FLAC" ) ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_FLAC;
   apTrack -> m_FourCC     = SMS_MKTAG ( 'f', 'L', 'a', 'C' );
  } else if ( !strcmp ( apTrack -> m_pCodecID, "A_PCM/INT/LIT" ) ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_PCM16LE;
   apTrack -> m_FourCC     = 0x01;
  } else if ( !strcmp ( apTrack -> m_pCodecID, "A_PCM/INT/BIG" ) ) {
   apTrack -> m_SMSCodecID = SMS_CodecID_PCM16BE;
   apTrack -> m_FourCC     = 0x01;
  } else if ( !strcmp ( apTrack -> m_pCodecID, "A_MS/ACM" ) ) {
   if ( apTrack -> m_pCodecPrivate && apTrack -> m_CodecPrivateLen >= 18 ) {
    uint16_t lTag = *( uint16_t* )apTrack -> m_pCodecPrivate;
    apTrack -> m_FourCC     = lTag;
    apTrack -> m_SMSCodecID = SMS_CodecGetID ( SMS_CodecTypeAudio, lTag );
   }
  }
 }
 /* Subtitle Codecs */
 else if ( apTrack -> m_TrackType == MKV_TRACK_TYPE_SUBTITLE ) {
  if ( !strcmp ( apTrack -> m_pCodecID, "S_TEXT/UTF8" ) || !strcmp ( apTrack -> m_pCodecID, "S_TEXT/ASCII" ) ) {
   apTrack -> m_SMSCodecID   = SMS_CodecID_DXSB;
   apTrack -> m_SMSCodecType = SMS_CodecTypeUnknown;
  }
 }
}  /* end _mkv_map_codec */

/* ========================================================================= */
/* Header, Track and Cues Parsing (A3, A8)                                   */
/* ========================================================================= */

static int _mkv_parse_track_entry ( MKVContext* apCtx, FileContext* apFileCtx, uint64_t aSize ) {
 uint32_t  lEndPos = aSize == MKV_UNKNOWN_SIZE ? apFileCtx -> m_Size : apFileCtx -> m_CurPos + ( uint32_t )aSize;
 MKVTrack* lpTrack;

 if ( apCtx -> m_nTracks >= MKV_MAX_TRACKS ) {
  File_Skip ( apFileCtx, ( uint32_t )aSize );
  return 0;
 }

 lpTrack = &apCtx -> m_Tracks[ apCtx -> m_nTracks++ ];
 memset ( lpTrack, 0, sizeof ( MKVTrack ) );
 lpTrack -> m_fEnabled   = 1;
 lpTrack -> m_fDefault   = 1;
 lpTrack -> m_fLacing    = 1;
 lpTrack -> m_StmIdx     = -1;
 lpTrack -> m_SampleRate = 8000;
 lpTrack -> m_Channels   = 1;

 while ( apFileCtx -> m_CurPos < lEndPos && !FILE_EOF ( apFileCtx ) ) {
  uint32_t lElemID, lIDLen;
  uint64_t lElemSize;

  if ( !_ebml_read_id ( apFileCtx, &lElemID, &lIDLen ) ) break;
  if ( !_ebml_read_vint ( apFileCtx, &lElemSize, NULL ) ) break;

  switch ( lElemID ) {
   case MKV_ID_TRACK_NUMBER:
    lpTrack -> m_TrackNumber = ( uint32_t )_ebml_read_uint ( apFileCtx, lElemSize );
    break;

   case MKV_ID_TRACK_TYPE:
    lpTrack -> m_TrackType = ( uint32_t )_ebml_read_uint ( apFileCtx, lElemSize );
    break;

   case MKV_ID_FLAG_ENABLED:
    lpTrack -> m_fEnabled = ( uint8_t )_ebml_read_uint ( apFileCtx, lElemSize );
    break;

   case MKV_ID_FLAG_DEFAULT:
    lpTrack -> m_fDefault = ( uint8_t )_ebml_read_uint ( apFileCtx, lElemSize );
    break;

   case MKV_ID_FLAG_FORCED:
    lpTrack -> m_fForced = ( uint8_t )_ebml_read_uint ( apFileCtx, lElemSize );
    break;

   case MKV_ID_FLAG_LACING:
    lpTrack -> m_fLacing = ( uint8_t )_ebml_read_uint ( apFileCtx, lElemSize );
    break;

   case MKV_ID_DEFAULT_DURATION:
    lpTrack -> m_DefaultDuration = _ebml_read_uint ( apFileCtx, lElemSize );
    break;

   case MKV_ID_NAME:
    lpTrack -> m_pName = _ebml_read_string ( apFileCtx, lElemSize );
    break;

   case MKV_ID_LANGUAGE:
    lpTrack -> m_pLanguage = _ebml_read_string ( apFileCtx, lElemSize );
    break;

   case MKV_ID_CODEC_ID:
    lpTrack -> m_pCodecID = _ebml_read_string ( apFileCtx, lElemSize );
    break;

   case MKV_ID_CODEC_PRIVATE:
    lpTrack -> m_pCodecPrivate = _ebml_read_binary ( apFileCtx, lElemSize, &lpTrack -> m_CodecPrivateLen );
    break;

   case MKV_ID_VIDEO: {
    uint32_t lVidEnd = apFileCtx -> m_CurPos + ( uint32_t )lElemSize;
    while ( apFileCtx -> m_CurPos < lVidEnd && !FILE_EOF ( apFileCtx ) ) {
     uint32_t lSubID;
     uint64_t lSubSize;
     if ( !_ebml_read_id ( apFileCtx, &lSubID, NULL ) ) break;
     if ( !_ebml_read_vint ( apFileCtx, &lSubSize, NULL ) ) break;
     switch ( lSubID ) {
      case MKV_ID_PIXEL_WIDTH:
       lpTrack -> m_Width = ( uint32_t )_ebml_read_uint ( apFileCtx, lSubSize );
       break;
      case MKV_ID_PIXEL_HEIGHT:
       lpTrack -> m_Height = ( uint32_t )_ebml_read_uint ( apFileCtx, lSubSize );
       break;
      case MKV_ID_DISPLAY_WIDTH:
       lpTrack -> m_DisplayWidth = ( uint32_t )_ebml_read_uint ( apFileCtx, lSubSize );
       break;
      case MKV_ID_DISPLAY_HEIGHT:
       lpTrack -> m_DisplayHeight = ( uint32_t )_ebml_read_uint ( apFileCtx, lSubSize );
       break;
      default:
       File_Skip ( apFileCtx, ( uint32_t )lSubSize );
       break;
     }
    }
   } break;

   case MKV_ID_AUDIO: {
    uint32_t lAudEnd = apFileCtx -> m_CurPos + ( uint32_t )lElemSize;
    while ( apFileCtx -> m_CurPos < lAudEnd && !FILE_EOF ( apFileCtx ) ) {
     uint32_t lSubID;
     uint64_t lSubSize;
     if ( !_ebml_read_id ( apFileCtx, &lSubID, NULL ) ) break;
     if ( !_ebml_read_vint ( apFileCtx, &lSubSize, NULL ) ) break;
     switch ( lSubID ) {
      case MKV_ID_SAMPLING_FREQ:
      case MKV_ID_OUT_SAMPLING_FRQ:
       lpTrack -> m_SampleRate = ( uint32_t )_ebml_read_float ( apFileCtx, lSubSize );
       break;
      case MKV_ID_CHANNELS:
       lpTrack -> m_Channels = ( uint32_t )_ebml_read_uint ( apFileCtx, lSubSize );
       break;
      case MKV_ID_BIT_DEPTH:
       lpTrack -> m_BitDepth = ( uint32_t )_ebml_read_uint ( apFileCtx, lSubSize );
       break;
      default:
       File_Skip ( apFileCtx, ( uint32_t )lSubSize );
       break;
     }
    }
   } break;

   default:
    File_Skip ( apFileCtx, ( uint32_t )lElemSize );
    break;
  }
 }

 _mkv_map_codec ( lpTrack );
 return 1;
}  /* end _mkv_parse_track_entry */

static int _mkv_parse_tracks ( MKVContext* apCtx, FileContext* apFileCtx, uint64_t aSize ) {
 uint32_t lEndPos = aSize == MKV_UNKNOWN_SIZE ? apFileCtx -> m_Size : apFileCtx -> m_CurPos + ( uint32_t )aSize;

 while ( apFileCtx -> m_CurPos < lEndPos && !FILE_EOF ( apFileCtx ) ) {
  uint32_t lElemID;
  uint64_t lElemSize;

  if ( !_ebml_read_id ( apFileCtx, &lElemID, NULL ) ) break;
  if ( !_ebml_read_vint ( apFileCtx, &lElemSize, NULL ) ) break;

  if ( lElemID == MKV_ID_TRACK_ENTRY ) {
   _mkv_parse_track_entry ( apCtx, apFileCtx, lElemSize );
  } else {
   File_Skip ( apFileCtx, ( uint32_t )lElemSize );
  }
 }

 return 1;
}  /* end _mkv_parse_tracks */

static int _mkv_parse_info ( MKVContext* apCtx, FileContext* apFileCtx, uint64_t aSize ) {
 uint32_t lEndPos = aSize == MKV_UNKNOWN_SIZE ? apFileCtx -> m_Size : apFileCtx -> m_CurPos + ( uint32_t )aSize;

 while ( apFileCtx -> m_CurPos < lEndPos && !FILE_EOF ( apFileCtx ) ) {
  uint32_t lElemID;
  uint64_t lElemSize;

  if ( !_ebml_read_id ( apFileCtx, &lElemID, NULL ) ) break;
  if ( !_ebml_read_vint ( apFileCtx, &lElemSize, NULL ) ) break;

  switch ( lElemID ) {
   case MKV_ID_TIMECODESCALE:
    apCtx -> m_TimestampScale = _ebml_read_uint ( apFileCtx, lElemSize );
    break;

   case MKV_ID_DURATION:
    apCtx -> m_Duration = _ebml_read_float ( apFileCtx, lElemSize );
    break;

   default:
    File_Skip ( apFileCtx, ( uint32_t )lElemSize );
    break;
  }
 }

 return 1;
}  /* end _mkv_parse_info */

static int _mkv_parse_cues ( MKVContext* apCtx, FileContext* apFileCtx, uint64_t aSize ) {
 uint32_t lEndPos = aSize == MKV_UNKNOWN_SIZE ? apFileCtx -> m_Size : apFileCtx -> m_CurPos + ( uint32_t )aSize;

 while ( apFileCtx -> m_CurPos < lEndPos && !FILE_EOF ( apFileCtx ) ) {
  uint32_t lElemID;
  uint64_t lElemSize;

  if ( !_ebml_read_id ( apFileCtx, &lElemID, NULL ) ) break;
  if ( !_ebml_read_vint ( apFileCtx, &lElemSize, NULL ) ) break;

  if ( lElemID == MKV_ID_CUE_POINT ) {
   uint32_t lPtEnd = apFileCtx -> m_CurPos + ( uint32_t )lElemSize;
   uint64_t lCueTime = 0;
   uint32_t lClusterPos = 0;
   uint32_t lTrack = 0;

   while ( apFileCtx -> m_CurPos < lPtEnd && !FILE_EOF ( apFileCtx ) ) {
    uint32_t lSubID;
    uint64_t lSubSize;
    if ( !_ebml_read_id ( apFileCtx, &lSubID, NULL ) ) break;
    if ( !_ebml_read_vint ( apFileCtx, &lSubSize, NULL ) ) break;

    switch ( lSubID ) {
     case MKV_ID_CUE_TIME:
      lCueTime = _ebml_read_uint ( apFileCtx, lSubSize );
      break;

     case MKV_ID_CUE_TRACK_POS: {
      uint32_t lPosEnd = apFileCtx -> m_CurPos + ( uint32_t )lSubSize;
      while ( apFileCtx -> m_CurPos < lPosEnd && !FILE_EOF ( apFileCtx ) ) {
       uint32_t lPosID;
       uint64_t lPosSize;
       if ( !_ebml_read_id ( apFileCtx, &lPosID, NULL ) ) break;
       if ( !_ebml_read_vint ( apFileCtx, &lPosSize, NULL ) ) break;
       switch ( lPosID ) {
        case MKV_ID_CUE_TRACK:
         lTrack = ( uint32_t )_ebml_read_uint ( apFileCtx, lPosSize );
         break;
        case MKV_ID_CUE_CLUSTER_POS:
         lClusterPos = ( uint32_t )_ebml_read_uint ( apFileCtx, lPosSize );
         break;
        default:
         File_Skip ( apFileCtx, ( uint32_t )lPosSize );
         break;
       }
      }
     } break;

     default:
      File_Skip ( apFileCtx, ( uint32_t )lSubSize );
      break;
    }
   }

   if ( apCtx -> m_nCues < MKV_MAX_CUES ) {
    if ( apCtx -> m_nCues >= apCtx -> m_CuesAlloc ) {
     int     lNewAlloc = apCtx -> m_CuesAlloc ? apCtx -> m_CuesAlloc * 2 : 128;
     MKVCue* lpNewCues = ( MKVCue* )realloc ( apCtx -> m_pCues, lNewAlloc * sizeof ( MKVCue ) );
     if ( lpNewCues ) {
      apCtx -> m_pCues      = lpNewCues;
      apCtx -> m_CuesAlloc  = lNewAlloc;
     }
    }
    if ( apCtx -> m_nCues < apCtx -> m_CuesAlloc ) {
     MKVCue* lpCue = &apCtx -> m_pCues[ apCtx -> m_nCues++ ];
     lpCue -> m_Time       = lCueTime;
     lpCue -> m_ClusterPos = lClusterPos;
     lpCue -> m_Track      = lTrack;
    }
   }
  } else {
   File_Skip ( apFileCtx, ( uint32_t )lElemSize );
  }
 }

 return 1;
}  /* end _mkv_parse_cues */

static int _mkv_parse_seekhead ( MKVContext* apCtx, FileContext* apFileCtx, uint64_t aSize ) {
 uint32_t lEndPos = aSize == MKV_UNKNOWN_SIZE ? apFileCtx -> m_Size : apFileCtx -> m_CurPos + ( uint32_t )aSize;

 while ( apFileCtx -> m_CurPos < lEndPos && !FILE_EOF ( apFileCtx ) ) {
  uint32_t lElemID;
  uint64_t lElemSize;

  if ( !_ebml_read_id ( apFileCtx, &lElemID, NULL ) ) break;
  if ( !_ebml_read_vint ( apFileCtx, &lElemSize, NULL ) ) break;

  if ( lElemID == MKV_ID_SEEK ) {
   uint32_t lSeekEnd = apFileCtx -> m_CurPos + ( uint32_t )lElemSize;
   uint32_t lTargetID = 0;
   uint32_t lTargetPos = 0;

   while ( apFileCtx -> m_CurPos < lSeekEnd && !FILE_EOF ( apFileCtx ) ) {
    uint32_t lSubID;
    uint64_t lSubSize;
    if ( !_ebml_read_id ( apFileCtx, &lSubID, NULL ) ) break;
    if ( !_ebml_read_vint ( apFileCtx, &lSubSize, NULL ) ) break;

    switch ( lSubID ) {
     case MKV_ID_SEEK_ID:
      lTargetID = ( uint32_t )_ebml_read_uint ( apFileCtx, lSubSize );
      break;
     case MKV_ID_SEEK_POS:
      lTargetPos = ( uint32_t )_ebml_read_uint ( apFileCtx, lSubSize );
      break;
     default:
      File_Skip ( apFileCtx, ( uint32_t )lSubSize );
      break;
    }
   }

   /* If target is Cues, parse cues */
   if ( lTargetID == MKV_ID_CUES && lTargetPos > 0 ) {
    uint32_t lSavedPos = apFileCtx -> m_CurPos;
    uint32_t lCuesOffset = apCtx -> m_SegmentDataStart + lTargetPos;
    if ( lCuesOffset < apFileCtx -> m_Size ) {
     apFileCtx -> Seek ( apFileCtx, lCuesOffset );
     if ( _ebml_read_id ( apFileCtx, &lElemID, NULL ) && lElemID == MKV_ID_CUES ) {
      uint64_t lCuesSize;
      if ( _ebml_read_vint ( apFileCtx, &lCuesSize, NULL ) ) {
       _mkv_parse_cues ( apCtx, apFileCtx, lCuesSize );
      }
     }
     apFileCtx -> Seek ( apFileCtx, lSavedPos );
    }
   }
  } else {
   File_Skip ( apFileCtx, ( uint32_t )lElemSize );
  }
 }

 return 1;
}  /* end _mkv_parse_seekhead */

/* ========================================================================= */
/* Stream Publication and Selection (A5, A9, A10)                            */
/* ========================================================================= */

static int _mkv_publish_streams ( MKVContext* apCtx ) {
 SMS_Container* lpCont = apCtx -> m_pBase;
 int            i;
 int            lSelectedVideo = -1;
 int            lSelectedAudios[ SMS_MAX_STREAMS ];
 int            nSelectedAudios = 0;
 int            lSelectedSubs[ SMS_MAX_STREAMS ];
 int            nSelectedSubs = 0;

 /* 1. Find compatible video */
 for ( i = 0; i < apCtx -> m_nTracks; ++i ) {
  MKVTrack* lpTrk = &apCtx -> m_Tracks[ i ];
  if ( lpTrk -> m_SMSCodecType == SMS_CodecTypeVideo && lpTrk -> m_SMSCodecID != SMS_CodecID_NULL ) {
   if ( lpTrk -> m_Width <= 1024 && lpTrk -> m_Height <= 1024 ) {
    if ( lSelectedVideo < 0 || ( lpTrk -> m_fEnabled && lpTrk -> m_fDefault ) ) {
     lSelectedVideo = i;
     if ( lpTrk -> m_fEnabled && lpTrk -> m_fDefault ) break;
    }
   }
  }
 }

 /* 2. Find compatible audios */
 for ( i = 0; i < apCtx -> m_nTracks; ++i ) {
  MKVTrack* lpTrk = &apCtx -> m_Tracks[ i ];
  if ( lpTrk -> m_SMSCodecType == SMS_CodecTypeAudio && lpTrk -> m_SMSCodecID != SMS_CodecID_NULL ) {
   if ( lpTrk -> m_fEnabled && lpTrk -> m_fDefault ) {
    /* Put default audio first */
    int j;
    for ( j = nSelectedAudios; j > 0; --j ) lSelectedAudios[ j ] = lSelectedAudios[ j - 1 ];
    lSelectedAudios[ 0 ] = i;
    ++nSelectedAudios;
   } else {
    lSelectedAudios[ nSelectedAudios++ ] = i;
   }
  }
 }

 /* 3. Find subtitle tracks */
 for ( i = 0; i < apCtx -> m_nTracks; ++i ) {
  MKVTrack* lpTrk = &apCtx -> m_Tracks[ i ];
  if ( lpTrk -> m_TrackType == MKV_TRACK_TYPE_SUBTITLE && lpTrk -> m_SMSCodecID != SMS_CodecID_NULL ) {
   lSelectedSubs[ nSelectedSubs++ ] = i;
  }
 }

 /* Check if we have at least video or audio */
 if ( lSelectedVideo < 0 && nSelectedAudios == 0 ) return 0;

 /* 4. Publish streams into SMS_Container m_pStm */
 lpCont -> m_nStm = 0;

 /* Video stream */
 if ( lSelectedVideo >= 0 ) {
  MKVTrack*         lpTrk = &apCtx -> m_Tracks[ lSelectedVideo ];
  SMS_Stream*       lpStm = ( SMS_Stream* )calloc ( 1, sizeof ( SMS_Stream ) );
  SMS_CodecContext* lpCodec = ( SMS_CodecContext* )calloc ( 1, sizeof ( SMS_CodecContext ) );

  if ( !lpStm || !lpCodec ) return 0;

  lpStm -> m_pCodec = lpCodec;
  lpStm -> m_pCtx   = lpTrk;
  lpStm -> m_Flags  = SMS_STRM_FLAGS_VIDEO;
  lpStm -> m_ID     = lpCont -> m_nStm;
  lpTrk -> m_StmIdx = lpCont -> m_nStm;

  /* Timebase: 1ms if timestamp scale is 1,000,000 ns */
  lpStm -> m_TimeBase.m_Num = ( int32_t )( apCtx -> m_TimestampScale / 1000 );
  lpStm -> m_TimeBase.m_Den = 1000000;
  if ( lpStm -> m_TimeBase.m_Num <= 0 ) lpStm -> m_TimeBase.m_Num = 1;

  lpCodec -> m_Type   = SMS_CodecTypeVideo;
  lpCodec -> m_ID     = lpTrk -> m_SMSCodecID;
  lpCodec -> m_Tag    = lpTrk -> m_FourCC;
  lpCodec -> m_Width  = lpTrk -> m_Width;
  lpCodec -> m_Height = lpTrk -> m_Height;

  /* Frame rate calculation */
  if ( lpTrk -> m_DefaultDuration > 0 ) {
   lpCodec -> m_FrameRate     = 1000000000ULL;
   lpCodec -> m_FrameRateBase = ( uint32_t )lpTrk -> m_DefaultDuration;
  } else {
   lpCodec -> m_FrameRate     = 25;
   lpCodec -> m_FrameRateBase = 1;
  }

  /* Extradata / CodecPrivate */
  if ( lpTrk -> m_pCodecPrivate && lpTrk -> m_CodecPrivateLen > 0 ) {
   lpCodec -> m_pUserData = ( uint8_t* )malloc ( ( lpTrk -> m_CodecPrivateLen + 15 ) & ~15 );
   if ( lpCodec -> m_pUserData ) {
    memcpy ( lpCodec -> m_pUserData, lpTrk -> m_pCodecPrivate, lpTrk -> m_CodecPrivateLen );
    lpCodec -> m_UserDataLen = ( short )lpTrk -> m_CodecPrivateLen;
   }
  }

  lpCont -> m_pStm[ lpCont -> m_nStm++ ] = lpStm;
 }

 /* Audio streams (fill available slots up to SMS_MAX_STREAMS) */
 for ( i = 0; i < nSelectedAudios && lpCont -> m_nStm < SMS_MAX_STREAMS; ++i ) {
  MKVTrack*         lpTrk = &apCtx -> m_Tracks[ lSelectedAudios[ i ] ];
  SMS_Stream*       lpStm = ( SMS_Stream* )calloc ( 1, sizeof ( SMS_Stream ) );
  SMS_CodecContext* lpCodec = ( SMS_CodecContext* )calloc ( 1, sizeof ( SMS_CodecContext ) );

  if ( !lpStm || !lpCodec ) break;

  lpStm -> m_pCodec = lpCodec;
  lpStm -> m_pCtx   = lpTrk;
  lpStm -> m_Flags  = SMS_STRM_FLAGS_AUDIO;
  lpStm -> m_ID     = lpCont -> m_nStm;
  lpTrk -> m_StmIdx = lpCont -> m_nStm;

  lpStm -> m_TimeBase.m_Num = ( int32_t )( apCtx -> m_TimestampScale / 1000 );
  lpStm -> m_TimeBase.m_Den = 1000000;
  if ( lpStm -> m_TimeBase.m_Num <= 0 ) lpStm -> m_TimeBase.m_Num = 1;

  lpCodec -> m_Type       = SMS_CodecTypeAudio;
  lpCodec -> m_ID         = lpTrk -> m_SMSCodecID;
  lpCodec -> m_Tag        = lpTrk -> m_FourCC;
  lpCodec -> m_SampleRate = lpTrk -> m_SampleRate;
  lpCodec -> m_Channels   = ( short )lpTrk -> m_Channels;

  /* Name / Language for OSD selection */
  if ( lpTrk -> m_pLanguage && strlen ( lpTrk -> m_pLanguage ) > 0 ) {
   lpStm -> m_pName = _strdup ( lpTrk -> m_pLanguage );
  } else if ( lpTrk -> m_pName && strlen ( lpTrk -> m_pName ) > 0 ) {
   lpStm -> m_pName = _strdup ( lpTrk -> m_pName );
  }

  /* Extradata / CodecPrivate */
  if ( lpTrk -> m_pCodecPrivate && lpTrk -> m_CodecPrivateLen > 0 ) {
   lpCodec -> m_pUserData = ( uint8_t* )malloc ( ( lpTrk -> m_CodecPrivateLen + 15 ) & ~15 );
   if ( lpCodec -> m_pUserData ) {
    memcpy ( lpCodec -> m_pUserData, lpTrk -> m_pCodecPrivate, lpTrk -> m_CodecPrivateLen );
    lpCodec -> m_UserDataLen = ( short )lpTrk -> m_CodecPrivateLen;
   }
  }

  lpCont -> m_pStm[ lpCont -> m_nStm++ ] = lpStm;
 }

 /* Subtitle streams */
 for ( i = 0; i < nSelectedSubs && lpCont -> m_nStm < SMS_MAX_STREAMS; ++i ) {
  MKVTrack*         lpTrk = &apCtx -> m_Tracks[ lSelectedSubs[ i ] ];
  SMS_Stream*       lpStm = ( SMS_Stream* )calloc ( 1, sizeof ( SMS_Stream ) );
  SMS_CodecContext* lpCodec = ( SMS_CodecContext* )calloc ( 1, sizeof ( SMS_CodecContext ) );

  if ( !lpStm || !lpCodec ) break;

  lpStm -> m_pCodec = lpCodec;
  lpStm -> m_pCtx   = lpTrk;
  lpStm -> m_Flags  = SMS_STRM_FLAGS_SUBTL;
  lpStm -> m_ID     = lpCont -> m_nStm;
  lpTrk -> m_StmIdx = lpCont -> m_nStm;

  lpStm -> m_TimeBase.m_Num = ( int32_t )( apCtx -> m_TimestampScale / 1000 );
  lpStm -> m_TimeBase.m_Den = 1000000;
  if ( lpStm -> m_TimeBase.m_Num <= 0 ) lpStm -> m_TimeBase.m_Num = 1;

  lpCodec -> m_Type = SMS_CodecTypeUnknown;
  lpCodec -> m_ID   = lpTrk -> m_SMSCodecID;

  if ( lpTrk -> m_pLanguage && strlen ( lpTrk -> m_pLanguage ) > 0 ) {
   lpStm -> m_pName = _strdup ( lpTrk -> m_pLanguage );
  } else if ( lpTrk -> m_pName && strlen ( lpTrk -> m_pName ) > 0 ) {
   lpStm -> m_pName = _strdup ( lpTrk -> m_pName );
  }

  lpCont -> m_pStm[ lpCont -> m_nStm++ ] = lpStm;
 }

 /* Container duration */
 if ( apCtx -> m_Duration > 0.0 ) {
  lpCont -> m_Duration = ( int64_t )( apCtx -> m_Duration * ( double )apCtx -> m_TimestampScale / 1000.0 );
 } else {
  lpCont -> m_Duration = 0x7FFFFFFFFFFFFFFFLL;
 }

 return lpCont -> m_nStm > 0;
}  /* end _mkv_publish_streams */

/* ========================================================================= */
/* Packet Demuxing (A6, A7)                                                  */
/* ========================================================================= */

static MKVTrack* _mkv_find_track ( MKVContext* apCtx, uint32_t aTrackNum ) {
 int i;
 for ( i = 0; i < apCtx -> m_nTracks; ++i ) {
  if ( apCtx -> m_Tracks[ i ].m_TrackNumber == aTrackNum ) return &apCtx -> m_Tracks[ i ];
 }
 return NULL;
}  /* end _mkv_find_track */

static int _ReadPacket ( SMS_Container* apCont, int* apIdx ) {
 MKVContext*  lpCtx     = ( MKVContext* )apCont -> m_pCtx;
 FileContext* lpFileCtx = apCont -> m_pFileCtx;

 /* If we have pending laced frames from a previous SimpleBlock/Block */
 if ( lpCtx -> m_CurLacedFrame < lpCtx -> m_nLacedFrames ) {
  MKVLacedFrame* lpLaced = &lpCtx -> m_LacedFrames[ lpCtx -> m_CurLacedFrame++ ];
  SMS_Stream*    lpStm   = apCont -> m_pStm[ lpCtx -> m_LacedStmIdx ];

  if ( lpStm && lpStm -> m_pPktBuf && lpLaced -> m_Size > 0 ) {
   SMS_AVPacket* lpPkt = apCont -> AllocPacket ( lpStm -> m_pPktBuf, lpLaced -> m_Size );
   if ( !lpPkt || lpFileCtx -> Read ( lpFileCtx, lpPkt -> m_pData, lpLaced -> m_Size ) != ( int )lpLaced -> m_Size ) {
    return 0;
   }
   lpPkt -> m_StmIdx = lpCtx -> m_LacedStmIdx;
   lpPkt -> m_DTS    = lpCtx -> m_LacedPTS;
   lpPkt -> m_PTS    = lpCtx -> m_LacedPTS;
   lpPkt -> m_Flags  = lpCtx -> m_LacedFlags;

   SMSContainer_CalcPktFields ( lpStm, lpPkt );
   *apIdx = lpCtx -> m_LacedStmIdx;
   return lpLaced -> m_Size;
  } else {
   File_Skip ( lpFileCtx, lpLaced -> m_Size );
  }
 }

 lpCtx -> m_nLacedFrames   = 0;
 lpCtx -> m_CurLacedFrame = 0;

 while ( !FILE_EOF ( lpFileCtx ) ) {
  uint32_t lElemID;
  uint64_t lElemSize;

  if ( !_ebml_read_id ( lpFileCtx, &lElemID, NULL ) ) break;
  if ( !_ebml_read_vint ( lpFileCtx, &lElemSize, NULL ) ) break;

  if ( lElemID == MKV_ID_CLUSTER ) {
   /* Entering a new cluster, continue to read its children */
   lpCtx -> m_CurClusterPos = lpFileCtx -> m_CurPos;
   continue;
  } else if ( lElemID == MKV_ID_CLUSTER_TIMECODE ) {
   lpCtx -> m_ClusterTimestamp = _ebml_read_uint ( lpFileCtx, lElemSize );
   continue;
  } else if ( lElemID == MKV_ID_SIMPLE_BLOCK || lElemID == MKV_ID_BLOCK ) {
   uint32_t lBlockStart = lpFileCtx -> m_CurPos;
   uint32_t lBlockEnd   = lBlockStart + ( uint32_t )lElemSize;
   uint64_t lTrackNum   = 0;
   int16_t  lRelTime    = 0;
   uint8_t  lFlags      = 0;
   MKVTrack* lpTrk;
   SMS_Stream* lpStm;
   int64_t  lPTS;
   uint32_t lPktFlags = 0;
   uint8_t  lLacing;

   if ( !_ebml_read_vint ( lpFileCtx, &lTrackNum, NULL ) ) break;

   lRelTime = ( int16_t )File_GetShortBE ( lpFileCtx );
   lFlags   = ( uint8_t )File_GetByte ( lpFileCtx );
   lLacing  = ( lFlags >> 1 ) & 0x03;

   lpTrk = _mkv_find_track ( lpCtx, ( uint32_t )lTrackNum );

   if ( !lpTrk || lpTrk -> m_StmIdx < 0 || lpTrk -> m_StmIdx >= ( int )apCont -> m_nStm ) {
    /* Track not published to player, skip rest of block */
    if ( lBlockEnd > lpFileCtx -> m_CurPos ) {
     File_Skip ( lpFileCtx, lBlockEnd - lpFileCtx -> m_CurPos );
    }
    continue;
   }

   lpStm = apCont -> m_pStm[ lpTrk -> m_StmIdx ];
   if ( !lpStm -> m_pPktBuf ) {
    /* No active packet buffer for this stream, skip */
    if ( lBlockEnd > lpFileCtx -> m_CurPos ) {
     File_Skip ( lpFileCtx, lBlockEnd - lpFileCtx -> m_CurPos );
    }
    continue;
   }

   /* Calculate PTS */
   lPTS = ( int64_t )lpCtx -> m_ClusterTimestamp + ( int64_t )lRelTime;
   if ( lPTS < 0 ) lPTS = 0;

   if ( lElemID == MKV_ID_SIMPLE_BLOCK ) {
    if ( ( lFlags & 0x80 ) || ( lpStm -> m_Flags & SMS_STRM_FLAGS_AUDIO ) ) {
     lPktFlags |= SMS_PKT_FLAG_KEY;
    }
   } else {
    /* Block inside BlockGroup: default to Keyframe unless ReferenceBlock was seen */
    lPktFlags |= SMS_PKT_FLAG_KEY;
   }

   if ( lpStm -> m_Flags & SMS_STRM_FLAGS_SUBTL ) {
    lPktFlags |= SMS_PKT_FLAG_SUB;
   }

   /* Handle Lacing */
   if ( lLacing == 0 ) {
    /* No lacing: 1 frame */
    uint32_t lFrameSize = lBlockEnd - lpFileCtx -> m_CurPos;
    SMS_AVPacket* lpPkt = apCont -> AllocPacket ( lpStm -> m_pPktBuf, lFrameSize );

    if ( !lpPkt || lpFileCtx -> Read ( lpFileCtx, lpPkt -> m_pData, lFrameSize ) != ( int )lFrameSize ) {
     return 0;
    }

    lpPkt -> m_StmIdx = lpTrk -> m_StmIdx;
    lpPkt -> m_DTS    = lPTS;
    lpPkt -> m_PTS    = lPTS;
    lpPkt -> m_Flags  = lPktFlags;

    SMSContainer_CalcPktFields ( lpStm, lpPkt );
    *apIdx = lpTrk -> m_StmIdx;
    return lFrameSize;
   } else {
    /* Laced frames */
    int      nFrames = File_GetByte ( lpFileCtx ) + 1;
    uint32_t lFrameSizes[ 64 ];
    int      f;
    uint32_t lTotalLacedSize = 0;

    if ( nFrames > 64 ) nFrames = 64;

    if ( lLacing == 1 ) {
     /* Xiph lacing */
     for ( f = 0; f < nFrames - 1; ++f ) {
      uint32_t lSz = 0;
      int lB;
      do {
       lB = File_GetByte ( lpFileCtx );
       lSz += lB;
      } while ( lB == 255 );
      lFrameSizes[ f ] = lSz;
      lTotalLacedSize += lSz;
     }
     lFrameSizes[ nFrames - 1 ] = ( lBlockEnd - lpFileCtx -> m_CurPos ) - lTotalLacedSize;
    } else if ( lLacing == 2 ) {
     /* Fixed size lacing */
     uint32_t lRem = lBlockEnd - lpFileCtx -> m_CurPos;
     uint32_t lSz = lRem / nFrames;
     for ( f = 0; f < nFrames; ++f ) lFrameSizes[ f ] = lSz;
    } else if ( lLacing == 3 ) {
     /* EBML lacing */
     uint64_t lFirstSz = 0;
     int64_t  lCurSz   = 0;
     _ebml_read_vint ( lpFileCtx, &lFirstSz, NULL );
     lFrameSizes[ 0 ] = ( uint32_t )lFirstSz;
     lCurSz = ( int64_t )lFirstSz;
     lTotalLacedSize = lFrameSizes[ 0 ];

     for ( f = 1; f < nFrames - 1; ++f ) {
      uint64_t lDeltaVal = 0;
      uint32_t lDeltaLen = 0;
      int64_t  lBias, lDiff;

      _ebml_read_vint ( lpFileCtx, &lDeltaVal, &lDeltaLen );
      lBias = ( 1ULL << ( 7 * lDeltaLen - 1 ) ) - 1;
      lDiff = ( int64_t )lDeltaVal - lBias;
      lCurSz += lDiff;
      if ( lCurSz < 0 ) lCurSz = 0;
      lFrameSizes[ f ] = ( uint32_t )lCurSz;
      lTotalLacedSize += lFrameSizes[ f ];
     }
     lFrameSizes[ nFrames - 1 ] = ( lBlockEnd - lpFileCtx -> m_CurPos ) - lTotalLacedSize;
    }

    /* Set up laced frames queue */
    lpCtx -> m_nLacedFrames   = nFrames;
    lpCtx -> m_CurLacedFrame = 0;
    lpCtx -> m_LacedStmIdx   = lpTrk -> m_StmIdx;
    lpCtx -> m_LacedPTS      = lPTS;
    lpCtx -> m_LacedFlags    = lPktFlags;

    for ( f = 0; f < nFrames; ++f ) {
     lpCtx -> m_LacedFrames[ f ].m_Size = lFrameSizes[ f ];
    }

    /* Deliver first laced frame */
    {
     MKVLacedFrame* lpLaced = &lpCtx -> m_LacedFrames[ lpCtx -> m_CurLacedFrame++ ];
     SMS_AVPacket*  lpPkt   = apCont -> AllocPacket ( lpStm -> m_pPktBuf, lpLaced -> m_Size );

     if ( !lpPkt || lpFileCtx -> Read ( lpFileCtx, lpPkt -> m_pData, lpLaced -> m_Size ) != ( int )lpLaced -> m_Size ) {
      return 0;
     }

     lpPkt -> m_StmIdx = lpTrk -> m_StmIdx;
     lpPkt -> m_DTS    = lPTS;
     lpPkt -> m_PTS    = lPTS;
     lpPkt -> m_Flags  = lPktFlags;

     SMSContainer_CalcPktFields ( lpStm, lpPkt );
     *apIdx = lpTrk -> m_StmIdx;
     return lpLaced -> m_Size;
    }
   }
  } else if ( lElemID == MKV_ID_BLOCK_GROUP ) {
   /* Sub-elements will be processed on next iterations */
   continue;
  } else {
   /* Unknown or non-media element, skip */
   File_Skip ( lpFileCtx, ( uint32_t )lElemSize );
  }
 }

 return -1; /* EOF or error */
}  /* end _ReadPacket */

/* ========================================================================= */
/* Seeking (A8)                                                              */
/* ========================================================================= */

static int _Seek ( SMS_Container* apCont, int anIdx, int aDir, uint32_t aPos ) {
 MKVContext*  lpCtx     = ( MKVContext* )apCont -> m_pCtx;
 FileContext* lpFileCtx = apCont -> m_pFileCtx;
 SMS_Stream*  lpStm;
 int64_t      lTargetTime;
 int          lLow, lHigh, lBestIdx;
 uint32_t     lTargetOffset;

 if ( anIdx < 0 || anIdx >= ( int )apCont -> m_nStm ) return 0;
 lpStm = apCont -> m_pStm[ anIdx ];

 /* Convert aPos (in stream timebase) to Matroska scaled timestamp units */
 lTargetTime = SMS_Rescale (
  ( int64_t )aPos * lpStm -> m_TimeBase.m_Num,
  1000000000ULL,
  ( int64_t )lpStm -> m_TimeBase.m_Den * lpCtx -> m_TimestampScale
 );

 if ( lpCtx -> m_nCues <= 0 ) {
  /* No index: seek to start of Segment data if aPos is near 0 */
  if ( aPos == 0 ) {
   lpFileCtx -> Seek ( lpFileCtx, lpCtx -> m_SegmentDataStart );
   lpCtx -> m_nLacedFrames   = 0;
   lpCtx -> m_CurLacedFrame = 0;
   return 1;
  }
  return 0;
 }

 /* Binary search in Cues */
 lLow     = 0;
 lHigh    = lpCtx -> m_nCues - 1;
 lBestIdx = 0;

 while ( lLow <= lHigh ) {
  int lMid = ( lLow + lHigh ) >> 1;
  if ( lpCtx -> m_pCues[ lMid ].m_Time <= ( uint64_t )lTargetTime ) {
   lBestIdx = lMid;
   lLow = lMid + 1;
  } else {
   lHigh = lMid - 1;
  }
 }

 if ( aDir < 0 && lBestIdx > 0 && ( int64_t )lpCtx -> m_pCues[ lBestIdx ].m_Time > lTargetTime ) {
  --lBestIdx;
 }

 lTargetOffset = lpCtx -> m_SegmentDataStart + lpCtx -> m_pCues[ lBestIdx ].m_ClusterPos;

 if ( lTargetOffset < lpFileCtx -> m_Size ) {
  lpFileCtx -> Seek ( lpFileCtx, lTargetOffset );
  lpCtx -> m_nLacedFrames   = 0;
  lpCtx -> m_CurLacedFrame = 0;
  lpCtx -> m_ClusterTimestamp = lpCtx -> m_pCues[ lBestIdx ].m_Time;
  return 1;
 }

 return 0;
}  /* end _Seek */

/* ========================================================================= */
/* Container Destruction and Initialization (A1, A11)                        */
/* ========================================================================= */

static void _Destroy ( SMS_Container* apCont, int afAll ) {
 MKVContext* lpCtx = ( MKVContext* )apCont -> m_pCtx;
 if ( lpCtx ) {
  int i;
  for ( i = 0; i < lpCtx -> m_nTracks; ++i ) {
   MKVTrack* lpTrk = &lpCtx -> m_Tracks[ i ];
   if ( lpTrk -> m_pCodecID      ) free ( lpTrk -> m_pCodecID      );
   if ( lpTrk -> m_pCodecPrivate ) free ( lpTrk -> m_pCodecPrivate );
   if ( lpTrk -> m_pLanguage     ) free ( lpTrk -> m_pLanguage     );
   if ( lpTrk -> m_pName         ) free ( lpTrk -> m_pName         );
  }
  if ( lpCtx -> m_pCues ) free ( lpCtx -> m_pCues );
 }
 SMS_DestroyContainer ( apCont, afAll );
}  /* end _Destroy */

static int _ProbeFile ( FileContext* apFileCtx ) {
 unsigned char lSig[ 4 ];
 if ( !apFileCtx || apFileCtx -> m_Size < sizeof ( lSig ) ) return 0;
 if ( apFileCtx -> Read ( apFileCtx, lSig, sizeof ( lSig ) ) != sizeof ( lSig ) ) return 0;
 return lSig[ 0 ] == 0x1A &&
        lSig[ 1 ] == 0x45 &&
        lSig[ 2 ] == 0xDF &&
        lSig[ 3 ] == 0xA3;
}  /* end _ProbeFile */

int SMS_GetContainerMKV ( SMS_Container* apCont ) {
 int          retVal    = 0;
 FileContext* lpFileCtx = apCont -> m_pFileCtx;
 MKVContext*  lpCtx     = NULL;
 uint32_t     lElemID;
 uint64_t     lElemSize;

 if ( ( int )lpFileCtx <= 0 ) return retVal;

 if ( !_ProbeFile ( lpFileCtx ) ) {
  lpFileCtx -> Seek ( lpFileCtx, 0 );
  return 0;
 }

 lpFileCtx -> Seek ( lpFileCtx, 0 );

 lpCtx = ( MKVContext* )calloc ( 1, sizeof ( MKVContext ) );
 if ( !lpCtx ) return 0;

 lpCtx -> m_pBase          = apCont;
 lpCtx -> m_TimestampScale = 1000000ULL; /* Default: 1ms */
 apCont -> m_pCtx          = lpCtx;

 /* 1. Read EBML Header */
 if ( !_ebml_read_id ( lpFileCtx, &lElemID, NULL ) || lElemID != EBML_ID_HEADER ) goto error;
 if ( !_ebml_read_vint ( lpFileCtx, &lElemSize, NULL ) ) goto error;
 File_Skip ( lpFileCtx, ( uint32_t )lElemSize );

 /* 2. Find Segment */
 while ( !FILE_EOF ( lpFileCtx ) ) {
  if ( !_ebml_read_id ( lpFileCtx, &lElemID, NULL ) ) goto error;
  if ( !_ebml_read_vint ( lpFileCtx, &lElemSize, NULL ) ) goto error;

  if ( lElemID == MKV_ID_SEGMENT ) {
   lpCtx -> m_SegmentDataStart = lpFileCtx -> m_CurPos;
   lpCtx -> m_SegmentDataEnd   = lElemSize == MKV_UNKNOWN_SIZE ? lpFileCtx -> m_Size : lpFileCtx -> m_CurPos + ( uint32_t )lElemSize;
   break;
  } else {
   File_Skip ( lpFileCtx, ( uint32_t )lElemSize );
  }
 }

 if ( !lpCtx -> m_SegmentDataStart ) goto error;

 /* 3. Parse Top-level Elements (SeekHead, Info, Tracks) before first Cluster */
 while ( lpFileCtx -> m_CurPos < lpCtx -> m_SegmentDataEnd && !FILE_EOF ( lpFileCtx ) ) {
  uint32_t lPos = lpFileCtx -> m_CurPos;

  if ( !_ebml_read_id ( lpFileCtx, &lElemID, NULL ) ) break;
  if ( !_ebml_read_vint ( lpFileCtx, &lElemSize, NULL ) ) break;

  if ( lElemID == MKV_ID_SEEKHEAD ) {
   _mkv_parse_seekhead ( lpCtx, lpFileCtx, lElemSize );
  } else if ( lElemID == MKV_ID_INFO ) {
   _mkv_parse_info ( lpCtx, lpFileCtx, lElemSize );
  } else if ( lElemID == MKV_ID_TRACKS ) {
   _mkv_parse_tracks ( lpCtx, lpFileCtx, lElemSize );
  } else if ( lElemID == MKV_ID_CUES ) {
   _mkv_parse_cues ( lpCtx, lpFileCtx, lElemSize );
  } else if ( lElemID == MKV_ID_CLUSTER ) {
   /* Reached first cluster, seek back to cluster start */
   lpFileCtx -> Seek ( lpFileCtx, lPos );
   break;
  } else {
   File_Skip ( lpFileCtx, ( uint32_t )lElemSize );
  }
 }

 /* 4. Publish streams into SMS_Container */
 if ( !_mkv_publish_streams ( lpCtx ) ) goto error;

 apCont -> m_pName    = s_pMKV;
 apCont -> ReadPacket = _ReadPacket;
 apCont -> Destroy    = _Destroy;

 if ( lpCtx -> m_nCues > 0 ) {
  apCont -> Seek     = _Seek;
  apCont -> m_Flags |= SMS_CONT_FLAGS_SEEKABLE;
 }

 SMSContainer_SetName ( apCont, lpFileCtx );
 return 1;

error:
 _Destroy ( apCont, 0 );
 lpFileCtx -> Seek ( lpFileCtx, 0 );
 return 0;
}  /* end SMS_GetContainerMKV */


