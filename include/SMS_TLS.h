/* One verified HTTPS download, for Check for updates. See src/SMS_TLS.c. */
#ifndef __SMS_TLS_H
#define __SMS_TLS_H

#include <stddef.h>

/* The configuration the mbedTLS library is built with ( the Makefile ): TLS 1.2 client only. */
#ifndef MBEDTLS_CONFIG_FILE
#define MBEDTLS_CONFIG_FILE "sms_mbedtls_config.h"
#endif

enum {
    RATLS_ERR_NET = -1,      /* no network, name lookup failed, or connect refused */
    RATLS_ERR_CLOCK = -2,    /* console clock unset: nothing was sent */
    RATLS_ERR_TLS = -3,      /* handshake or record failure */
    RATLS_ERR_CERT = -4,     /* certificate invalid, expired, untrusted or for another host */
    RATLS_ERR_HTTP = -5,     /* not 200, malformed, or larger than allowed */
    RATLS_ERR_TIMEOUT = -6,  /* the deadline passed */
    RATLS_ERR_MEM = -7,      /* out of memory, or the crypto library failed to start */
    RATLS_ERR_ARG = -8,      /* bad arguments */
    RATLS_ERR_REDIRECT = -9, /* the server redirected; the target is in location */
    RATLS_ERR_SINK = -10     /* the sink refused the data */
};

/* Receives each piece of a download's body in order. 0 to go on, anything else stops the download. */
typedef int (*ratls_sink_t)(void *ctx, const char *data, int len);

typedef struct
{
    int (*send)(void *ctx, const unsigned char *buf, size_t len); /* mbedTLS BIO contract */
    int (*recv)(void *ctx, unsigned char *buf, size_t len);       /* MBEDTLS_ERR_SSL_TIMEOUT at the deadline, 0 = closed */
    void *ctx;
    long long (*now_utc)(void); /* seconds since 1970 UTC, -1 = clock unset */
} ratls_io_t;

typedef struct
{
    const unsigned char *der;
    size_t len;
} ratls_anchor_t;

/* 1 when now_utc() gives a usable time. Callers check it before any name lookup or connection. */
int raTlsClockOk(long long (*now_utc)(void));

/* The Content-Length of the body raTlsDownload last accepted, set before the first sink call. */
extern int raTlsBodyLength;

/* One streamed HTTPS GET over an already-connected io. buf ( max >= 512 ) holds the request and
   then the headers; the body goes to sink as it arrives. Only a 200 with a Content-Length of
   1..max_body and no transfer or content encoding is taken. Returns the body length,
   RATLS_ERR_REDIRECT with the Location target in location ( 301/302/303/307/308 ), or RATLS_ERR_*. */
int raTlsDownload(const ratls_io_t *io, const char *host, const char *path, const char *user_agent,
                  const ratls_anchor_t *anchors, int anchor_count, char *buf, int max, int max_body,
                  ratls_sink_t sink, void *sink_ctx, char *location, int location_size);

/* The console RTC ( already decoded from BCD, full year ) to seconds since 1970 UTC. */
long long raTlsRtcToUtc(int year, int month, int day, int hour, int minute, int second);

#endif /* __SMS_TLS_H */
