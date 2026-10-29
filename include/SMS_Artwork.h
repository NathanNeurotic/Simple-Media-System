#ifndef __SMS_ARTWORK_H
#define __SMS_ARTWORK_H

/* Bounded browser-only artwork. No decoded source survives the load callback. */
#define SMS_ARTWORK_COLUMNS 3
#define SMS_ARTWORK_SLOTS   6
#define SMS_ARTWORK_SIDE    64
#define SMS_ARTWORK_BYTES   ( SMS_ARTWORK_SIDE * SMS_ARTWORK_SIDE * 4 )
#define SMS_ARTWORK_MAX_FILE ( 256U * 1024U )
#define SMS_ARTWORK_MAX_SIDE 512U

typedef struct SMS_ArtworkSlot {
 const void*    m_pKey;
 unsigned char* m_pPixels;
 int            m_Attempted;
} SMS_ArtworkSlot;

typedef struct SMS_ArtworkCache {
 SMS_ArtworkSlot m_Slots[ SMS_ARTWORK_SLOTS ];
 unsigned int    m_Count;
} SMS_ArtworkCache;

/* The caller must drain DMA references before Clear/Page changes ownership. */
void SMS_ArtworkClear ( SMS_ArtworkCache* );
void SMS_ArtworkPage ( SMS_ArtworkCache*, const void* const*, unsigned int );
unsigned char* SMS_ArtworkGet ( SMS_ArtworkCache*, unsigned int,
 unsigned char* ( * )( const void*, void* ), void* );
int SMS_ArtworkName ( char*, unsigned int, const char*, int );
int SMS_ArtworkJPEGSize ( const unsigned char*, unsigned int );
int SMS_ArtworkThumbnail ( unsigned char*, const unsigned char*, unsigned int, unsigned int );
unsigned int SMS_ArtworkMove ( unsigned int, unsigned int, int );

#endif
