/* mbedTLS 3.6.7 as SMS builds it: a TLS 1.2 client with full certificate verification and
 * nothing else. The same feature set as POPSLoader's and RiptOPL's updaters, plus what the
 * ee-gcc 3.2.3 toolchain needs ( 32-bit limbs, no C library calendar ). */
#ifndef SMS_MBEDTLS_CONFIG_H
#define SMS_MBEDTLS_CONFIG_H

/* 32-bit limbs with a 64-bit double: the 64-bit limb path needs 128-bit arithmetic,
 * which this compiler is not trusted with. */
#define MBEDTLS_HAVE_INT32

#define MBEDTLS_PLATFORM_C
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_PLATFORM_TIME_ALT     /* mbedtls_platform_set_time(): the console clock as UTC */
#define MBEDTLS_PLATFORM_MS_TIME_ALT  /* mbedtls_ms_time() supplied by SMS_Update.c */
#define MBEDTLS_PLATFORM_GMTIME_R_ALT /* mbedtls_platform_gmtime_r(): the SDK's libc has no gmtime */
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT  /* mbedtls_hardware_poll() supplied by SMS_Update.c */
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C

#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_ENCRYPT_THEN_MAC
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_SSL_IN_CONTENT_LEN  16384 /* a server may send full-size records */
#define MBEDTLS_SSL_OUT_CONTENT_LEN 4096  /* one GET request */

#define MBEDTLS_CIPHER_C
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_CHECK_KEY_USAGE
#define MBEDTLS_X509_CHECK_EXTENDED_KEY_USAGE
/* No socket headers on this platform: use the library's own parser for IP-address names. */
#define MBEDTLS_TEST_SW_INET_PTON

#endif /* SMS_MBEDTLS_CONFIG_H */
