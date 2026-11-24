/* <stdint.h> for the ee-gcc 3.2.3 toolchain, which ships none. Used only by the mbedTLS build. */
#ifndef SMS_STDINT_H
#define SMS_STDINT_H
#ifndef __SMS_H /* SMS.h, when a file includes it first, has these eight already */
typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef long               int64_t;
typedef unsigned long      uint64_t;
#endif
typedef int                intptr_t;
typedef unsigned int       uintptr_t;
typedef long               intmax_t;
typedef unsigned long      uintmax_t;
typedef int32_t            int_fast32_t;
typedef uint32_t           uint_fast32_t;
typedef uint16_t           uint_least16_t;
typedef uint8_t            uint_fast8_t;
typedef uint16_t           uint_fast16_t;
#define INT8_MAX   127
#define UINT8_MAX  255
#define INT16_MAX  32767
#define UINT16_MAX 65535
#define INT32_MAX  2147483647
#define INT32_MIN  ( -INT32_MAX - 1 )
#define UINT32_MAX 4294967295U
#define INT64_MAX  9223372036854775807L
#define INT64_MIN  ( -INT64_MAX - 1 )
#define UINT64_MAX 18446744073709551615UL
#define SIZE_MAX   4294967295U
#define UINTPTR_MAX 4294967295U
#define INT32_C( x )  x
#define UINT32_C( x ) x##U
#define INT64_C( x )  x##L
#define UINT64_C( x ) x##UL
#endif
