#ifndef __types_h
#define __types_h
#include <limits.h>
#include <sys/types.h>

#ifndef TRUE
#define TRUE (1)
#define FALSE (0)
#endif

typedef unsigned char boolean;

#ifdef __STDC__
#define MAX32U 0xFFFFFFFFU
#else
#define MAX32U 0xFFFFFFFF
#endif
#define MAX32  0x8FFFFFFF

/* 32 bit machines */
#if ULONG_MAX == MAX32U
typedef short int16;
typedef int   int32;
/* Kludge if we can't get 64-bit integers. */
#ifndef NOLONGLONG
typedef long long int int64;
#else
typedef long int int64;
#endif
typedef unsigned long  u_int32;
typedef unsigned short u_int16;
#ifndef NOLONGLONG
typedef unsigned long long u_int64;
#else
typedef unsigned long u_int64;
#endif
/* 16 bit machines */
#elif ULONG_MAX == 0xFFFF
typedef int  int16;
typedef long int32;
typedef unsigned long  u_int32;
typedef unsigned int   u_int16; 
/* 64 bit machines */
#else
typedef short int16;
typedef int   int32;
typedef unsigned int   u_int32;
typedef unsigned short u_int16;
#endif
typedef char int8;
typedef unsigned char u_int8;

#endif
