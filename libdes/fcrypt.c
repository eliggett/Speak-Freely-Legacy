/* lib/des/fcrypt.c */
/* Copyright (C) 1995 Eric Young (eay@mincom.oz.au)
 * All rights reserved.
 * 
 * This file is part of an SSL implementation written
 * by Eric Young (eay@mincom.oz.au).
 * The implementation was written so as to conform with Netscapes SSL
 * specification.  This library and applications are
 * FREE FOR COMMERCIAL AND NON-COMMERCIAL USE
 * as long as the following conditions are aheared to.
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.  If this code is used in a product,
 * Eric Young should be given attribution as the author of the parts used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Eric Young (eay@mincom.oz.au)
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>

/* Eric Young.
 * This version of crypt has been developed from my MIT compatable
 * DES library.
 * The library is available at pub/Crypto/DES at ftp.psy.uq.oz.au
 * eay@mincom.oz.au or eay@psych.psy.uq.oz.au
 */

#if !defined(_LIBC) || defined(NOCONST)
#define const
#endif

typedef unsigned char des_cblock[8];

typedef struct des_ks_struct
	{
	union	{
		des_cblock _;
		/* make sure things are correct size on machines with
		 * 8 byte longs */
		unsigned long pad[2];
		} ks;
#define _	ks._
	} des_key_schedule[16];

#define DES_KEY_SZ 	(sizeof(des_cblock))
#define DES_ENCRYPT	1
#define DES_DECRYPT	0

#define ITERATIONS 16
#define HALF_ITERATIONS 8

#define c2l(c,l)	(l =((unsigned long)(*((c)++)))    , \
			 l|=((unsigned long)(*((c)++)))<< 8, \
			 l|=((unsigned long)(*((c)++)))<<16, \
			 l|=((unsigned long)(*((c)++)))<<24)

#define l2c(l,c)	(*((c)++)=(unsigned char)(((l)    )&0xff), \
			 *((c)++)=(unsigned char)(((l)>> 8)&0xff), \
			 *((c)++)=(unsigned char)(((l)>>16)&0xff), \
			 *((c)++)=(unsigned char)(((l)>>24)&0xff))

static const unsigned long SPtrans[8][64]={
{
/* nibble 0 */
0x00820200LU, 0x00020000LU, 0x80800000LU, 0x80820200LU,
0x00800000LU, 0x80020200LU, 0x80020000LU, 0x80800000LU,
0x80020200LU, 0x00820200LU, 0x00820000LU, 0x80000200LU,
0x80800200LU, 0x00800000LU, 0x00000000LU, 0x80020000LU,
0x00020000LU, 0x80000000LU, 0x00800200LU, 0x00020200LU,
0x80820200LU, 0x00820000LU, 0x80000200LU, 0x00800200LU,
0x80000000LU, 0x00000200LU, 0x00020200LU, 0x80820000LU,
0x00000200LU, 0x80800200LU, 0x80820000LU, 0x00000000LU,
0x00000000LU, 0x80820200LU, 0x00800200LU, 0x80020000LU,
0x00820200LU, 0x00020000LU, 0x80000200LU, 0x00800200LU,
0x80820000LU, 0x00000200LU, 0x00020200LU, 0x80800000LU,
0x80020200LU, 0x80000000LU, 0x80800000LU, 0x00820000LU,
0x80820200LU, 0x00020200LU, 0x00820000LU, 0x80800200LU,
0x00800000LU, 0x80000200LU, 0x80020000LU, 0x00000000LU,
0x00020000LU, 0x00800000LU, 0x80800200LU, 0x00820200LU,
0x80000000LU, 0x80820000LU, 0x00000200LU, 0x80020200LU,
},{
/* nibble 1 */
0x10042004LU, 0x00000000LU, 0x00042000LU, 0x10040000LU,
0x10000004LU, 0x00002004LU, 0x10002000LU, 0x00042000LU,
0x00002000LU, 0x10040004LU, 0x00000004LU, 0x10002000LU,
0x00040004LU, 0x10042000LU, 0x10040000LU, 0x00000004LU,
0x00040000LU, 0x10002004LU, 0x10040004LU, 0x00002000LU,
0x00042004LU, 0x10000000LU, 0x00000000LU, 0x00040004LU,
0x10002004LU, 0x00042004LU, 0x10042000LU, 0x10000004LU,
0x10000000LU, 0x00040000LU, 0x00002004LU, 0x10042004LU,
0x00040004LU, 0x10042000LU, 0x10002000LU, 0x00042004LU,
0x10042004LU, 0x00040004LU, 0x10000004LU, 0x00000000LU,
0x10000000LU, 0x00002004LU, 0x00040000LU, 0x10040004LU,
0x00002000LU, 0x10000000LU, 0x00042004LU, 0x10002004LU,
0x10042000LU, 0x00002000LU, 0x00000000LU, 0x10000004LU,
0x00000004LU, 0x10042004LU, 0x00042000LU, 0x10040000LU,
0x10040004LU, 0x00040000LU, 0x00002004LU, 0x10002000LU,
0x10002004LU, 0x00000004LU, 0x10040000LU, 0x00042000LU,
},{
/* nibble 2 */
0x41000000LU, 0x01010040LU, 0x00000040LU, 0x41000040LU,
0x40010000LU, 0x01000000LU, 0x41000040LU, 0x00010040LU,
0x01000040LU, 0x00010000LU, 0x01010000LU, 0x40000000LU,
0x41010040LU, 0x40000040LU, 0x40000000LU, 0x41010000LU,
0x00000000LU, 0x40010000LU, 0x01010040LU, 0x00000040LU,
0x40000040LU, 0x41010040LU, 0x00010000LU, 0x41000000LU,
0x41010000LU, 0x01000040LU, 0x40010040LU, 0x01010000LU,
0x00010040LU, 0x00000000LU, 0x01000000LU, 0x40010040LU,
0x01010040LU, 0x00000040LU, 0x40000000LU, 0x00010000LU,
0x40000040LU, 0x40010000LU, 0x01010000LU, 0x41000040LU,
0x00000000LU, 0x01010040LU, 0x00010040LU, 0x41010000LU,
0x40010000LU, 0x01000000LU, 0x41010040LU, 0x40000000LU,
0x40010040LU, 0x41000000LU, 0x01000000LU, 0x41010040LU,
0x00010000LU, 0x01000040LU, 0x41000040LU, 0x00010040LU,
0x01000040LU, 0x00000000LU, 0x41010000LU, 0x40000040LU,
0x41000000LU, 0x40010040LU, 0x00000040LU, 0x01010000LU,
},{
/* nibble 3 */
0x00100402LU, 0x04000400LU, 0x00000002LU, 0x04100402LU,
0x00000000LU, 0x04100000LU, 0x04000402LU, 0x00100002LU,
0x04100400LU, 0x04000002LU, 0x04000000LU, 0x00000402LU,
0x04000002LU, 0x00100402LU, 0x00100000LU, 0x04000000LU,
0x04100002LU, 0x00100400LU, 0x00000400LU, 0x00000002LU,
0x00100400LU, 0x04000402LU, 0x04100000LU, 0x00000400LU,
0x00000402LU, 0x00000000LU, 0x00100002LU, 0x04100400LU,
0x04000400LU, 0x04100002LU, 0x04100402LU, 0x00100000LU,
0x04100002LU, 0x00000402LU, 0x00100000LU, 0x04000002LU,
0x00100400LU, 0x04000400LU, 0x00000002LU, 0x04100000LU,
0x04000402LU, 0x00000000LU, 0x00000400LU, 0x00100002LU,
0x00000000LU, 0x04100002LU, 0x04100400LU, 0x00000400LU,
0x04000000LU, 0x04100402LU, 0x00100402LU, 0x00100000LU,
0x04100402LU, 0x00000002LU, 0x04000400LU, 0x00100402LU,
0x00100002LU, 0x00100400LU, 0x04100000LU, 0x04000402LU,
0x00000402LU, 0x04000000LU, 0x04000002LU, 0x04100400LU,
},{
/* nibble 4 */
0x02000000LU, 0x00004000LU, 0x00000100LU, 0x02004108LU,
0x02004008LU, 0x02000100LU, 0x00004108LU, 0x02004000LU,
0x00004000LU, 0x00000008LU, 0x02000008LU, 0x00004100LU,
0x02000108LU, 0x02004008LU, 0x02004100LU, 0x00000000LU,
0x00004100LU, 0x02000000LU, 0x00004008LU, 0x00000108LU,
0x02000100LU, 0x00004108LU, 0x00000000LU, 0x02000008LU,
0x00000008LU, 0x02000108LU, 0x02004108LU, 0x00004008LU,
0x02004000LU, 0x00000100LU, 0x00000108LU, 0x02004100LU,
0x02004100LU, 0x02000108LU, 0x00004008LU, 0x02004000LU,
0x00004000LU, 0x00000008LU, 0x02000008LU, 0x02000100LU,
0x02000000LU, 0x00004100LU, 0x02004108LU, 0x00000000LU,
0x00004108LU, 0x02000000LU, 0x00000100LU, 0x00004008LU,
0x02000108LU, 0x00000100LU, 0x00000000LU, 0x02004108LU,
0x02004008LU, 0x02004100LU, 0x00000108LU, 0x00004000LU,
0x00004100LU, 0x02004008LU, 0x02000100LU, 0x00000108LU,
0x00000008LU, 0x00004108LU, 0x02004000LU, 0x02000008LU,
},{
/* nibble 5 */
0x20000010LU, 0x00080010LU, 0x00000000LU, 0x20080800LU,
0x00080010LU, 0x00000800LU, 0x20000810LU, 0x00080000LU,
0x00000810LU, 0x20080810LU, 0x00080800LU, 0x20000000LU,
0x20000800LU, 0x20000010LU, 0x20080000LU, 0x00080810LU,
0x00080000LU, 0x20000810LU, 0x20080010LU, 0x00000000LU,
0x00000800LU, 0x00000010LU, 0x20080800LU, 0x20080010LU,
0x20080810LU, 0x20080000LU, 0x20000000LU, 0x00000810LU,
0x00000010LU, 0x00080800LU, 0x00080810LU, 0x20000800LU,
0x00000810LU, 0x20000000LU, 0x20000800LU, 0x00080810LU,
0x20080800LU, 0x00080010LU, 0x00000000LU, 0x20000800LU,
0x20000000LU, 0x00000800LU, 0x20080010LU, 0x00080000LU,
0x00080010LU, 0x20080810LU, 0x00080800LU, 0x00000010LU,
0x20080810LU, 0x00080800LU, 0x00080000LU, 0x20000810LU,
0x20000010LU, 0x20080000LU, 0x00080810LU, 0x00000000LU,
0x00000800LU, 0x20000010LU, 0x20000810LU, 0x20080800LU,
0x20080000LU, 0x00000810LU, 0x00000010LU, 0x20080010LU,
},{
/* nibble 6 */
0x00001000LU, 0x00000080LU, 0x00400080LU, 0x00400001LU,
0x00401081LU, 0x00001001LU, 0x00001080LU, 0x00000000LU,
0x00400000LU, 0x00400081LU, 0x00000081LU, 0x00401000LU,
0x00000001LU, 0x00401080LU, 0x00401000LU, 0x00000081LU,
0x00400081LU, 0x00001000LU, 0x00001001LU, 0x00401081LU,
0x00000000LU, 0x00400080LU, 0x00400001LU, 0x00001080LU,
0x00401001LU, 0x00001081LU, 0x00401080LU, 0x00000001LU,
0x00001081LU, 0x00401001LU, 0x00000080LU, 0x00400000LU,
0x00001081LU, 0x00401000LU, 0x00401001LU, 0x00000081LU,
0x00001000LU, 0x00000080LU, 0x00400000LU, 0x00401001LU,
0x00400081LU, 0x00001081LU, 0x00001080LU, 0x00000000LU,
0x00000080LU, 0x00400001LU, 0x00000001LU, 0x00400080LU,
0x00000000LU, 0x00400081LU, 0x00400080LU, 0x00001080LU,
0x00000081LU, 0x00001000LU, 0x00401081LU, 0x00400000LU,
0x00401080LU, 0x00000001LU, 0x00001001LU, 0x00401081LU,
0x00400001LU, 0x00401080LU, 0x00401000LU, 0x00001001LU,
},{
/* nibble 7 */
0x08200020LU, 0x08208000LU, 0x00008020LU, 0x00000000LU,
0x08008000LU, 0x00200020LU, 0x08200000LU, 0x08208020LU,
0x00000020LU, 0x08000000LU, 0x00208000LU, 0x00008020LU,
0x00208020LU, 0x08008020LU, 0x08000020LU, 0x08200000LU,
0x00008000LU, 0x00208020LU, 0x00200020LU, 0x08008000LU,
0x08208020LU, 0x08000020LU, 0x00000000LU, 0x00208000LU,
0x08000000LU, 0x00200000LU, 0x08008020LU, 0x08200020LU,
0x00200000LU, 0x00008000LU, 0x08208000LU, 0x00000020LU,
0x00200000LU, 0x00008000LU, 0x08000020LU, 0x08208020LU,
0x00008020LU, 0x08000000LU, 0x00000000LU, 0x00208000LU,
0x08200020LU, 0x08008020LU, 0x08008000LU, 0x00200020LU,
0x08208000LU, 0x00000020LU, 0x00200020LU, 0x08008000LU,
0x08208020LU, 0x00200000LU, 0x08200000LU, 0x08000020LU,
0x00208000LU, 0x00008020LU, 0x08008020LU, 0x08200000LU,
0x00000020LU, 0x08208000LU, 0x00208020LU, 0x00000000LU,
0x08000000LU, 0x08200020LU, 0x00008000LU, 0x00208020LU}};
static const unsigned long skb[8][64]={
{
/* for C bits (numbered as per FIPS 46) 1 2 3 4 5 6 */
0x00000000LU,0x00000010LU,0x20000000LU,0x20000010LU,
0x00010000LU,0x00010010LU,0x20010000LU,0x20010010LU,
0x00000800LU,0x00000810LU,0x20000800LU,0x20000810LU,
0x00010800LU,0x00010810LU,0x20010800LU,0x20010810LU,
0x00000020LU,0x00000030LU,0x20000020LU,0x20000030LU,
0x00010020LU,0x00010030LU,0x20010020LU,0x20010030LU,
0x00000820LU,0x00000830LU,0x20000820LU,0x20000830LU,
0x00010820LU,0x00010830LU,0x20010820LU,0x20010830LU,
0x00080000LU,0x00080010LU,0x20080000LU,0x20080010LU,
0x00090000LU,0x00090010LU,0x20090000LU,0x20090010LU,
0x00080800LU,0x00080810LU,0x20080800LU,0x20080810LU,
0x00090800LU,0x00090810LU,0x20090800LU,0x20090810LU,
0x00080020LU,0x00080030LU,0x20080020LU,0x20080030LU,
0x00090020LU,0x00090030LU,0x20090020LU,0x20090030LU,
0x00080820LU,0x00080830LU,0x20080820LU,0x20080830LU,
0x00090820LU,0x00090830LU,0x20090820LU,0x20090830LU,
},{
/* for C bits (numbered as per FIPS 46) 7 8 10 11 12 13 */
0x00000000LU,0x02000000LU,0x00002000LU,0x02002000LU,
0x00200000LU,0x02200000LU,0x00202000LU,0x02202000LU,
0x00000004LU,0x02000004LU,0x00002004LU,0x02002004LU,
0x00200004LU,0x02200004LU,0x00202004LU,0x02202004LU,
0x00000400LU,0x02000400LU,0x00002400LU,0x02002400LU,
0x00200400LU,0x02200400LU,0x00202400LU,0x02202400LU,
0x00000404LU,0x02000404LU,0x00002404LU,0x02002404LU,
0x00200404LU,0x02200404LU,0x00202404LU,0x02202404LU,
0x10000000LU,0x12000000LU,0x10002000LU,0x12002000LU,
0x10200000LU,0x12200000LU,0x10202000LU,0x12202000LU,
0x10000004LU,0x12000004LU,0x10002004LU,0x12002004LU,
0x10200004LU,0x12200004LU,0x10202004LU,0x12202004LU,
0x10000400LU,0x12000400LU,0x10002400LU,0x12002400LU,
0x10200400LU,0x12200400LU,0x10202400LU,0x12202400LU,
0x10000404LU,0x12000404LU,0x10002404LU,0x12002404LU,
0x10200404LU,0x12200404LU,0x10202404LU,0x12202404LU,
},{
/* for C bits (numbered as per FIPS 46) 14 15 16 17 19 20 */
0x00000000LU,0x00000001LU,0x00040000LU,0x00040001LU,
0x01000000LU,0x01000001LU,0x01040000LU,0x01040001LU,
0x00000002LU,0x00000003LU,0x00040002LU,0x00040003LU,
0x01000002LU,0x01000003LU,0x01040002LU,0x01040003LU,
0x00000200LU,0x00000201LU,0x00040200LU,0x00040201LU,
0x01000200LU,0x01000201LU,0x01040200LU,0x01040201LU,
0x00000202LU,0x00000203LU,0x00040202LU,0x00040203LU,
0x01000202LU,0x01000203LU,0x01040202LU,0x01040203LU,
0x08000000LU,0x08000001LU,0x08040000LU,0x08040001LU,
0x09000000LU,0x09000001LU,0x09040000LU,0x09040001LU,
0x08000002LU,0x08000003LU,0x08040002LU,0x08040003LU,
0x09000002LU,0x09000003LU,0x09040002LU,0x09040003LU,
0x08000200LU,0x08000201LU,0x08040200LU,0x08040201LU,
0x09000200LU,0x09000201LU,0x09040200LU,0x09040201LU,
0x08000202LU,0x08000203LU,0x08040202LU,0x08040203LU,
0x09000202LU,0x09000203LU,0x09040202LU,0x09040203LU,
},{
/* for C bits (numbered as per FIPS 46) 21 23 24 26 27 28 */
0x00000000LU,0x00100000LU,0x00000100LU,0x00100100LU,
0x00000008LU,0x00100008LU,0x00000108LU,0x00100108LU,
0x00001000LU,0x00101000LU,0x00001100LU,0x00101100LU,
0x00001008LU,0x00101008LU,0x00001108LU,0x00101108LU,
0x04000000LU,0x04100000LU,0x04000100LU,0x04100100LU,
0x04000008LU,0x04100008LU,0x04000108LU,0x04100108LU,
0x04001000LU,0x04101000LU,0x04001100LU,0x04101100LU,
0x04001008LU,0x04101008LU,0x04001108LU,0x04101108LU,
0x00020000LU,0x00120000LU,0x00020100LU,0x00120100LU,
0x00020008LU,0x00120008LU,0x00020108LU,0x00120108LU,
0x00021000LU,0x00121000LU,0x00021100LU,0x00121100LU,
0x00021008LU,0x00121008LU,0x00021108LU,0x00121108LU,
0x04020000LU,0x04120000LU,0x04020100LU,0x04120100LU,
0x04020008LU,0x04120008LU,0x04020108LU,0x04120108LU,
0x04021000LU,0x04121000LU,0x04021100LU,0x04121100LU,
0x04021008LU,0x04121008LU,0x04021108LU,0x04121108LU,
},{
/* for D bits (numbered as per FIPS 46) 1 2 3 4 5 6 */
0x00000000LU,0x10000000LU,0x00010000LU,0x10010000LU,
0x00000004LU,0x10000004LU,0x00010004LU,0x10010004LU,
0x20000000LU,0x30000000LU,0x20010000LU,0x30010000LU,
0x20000004LU,0x30000004LU,0x20010004LU,0x30010004LU,
0x00100000LU,0x10100000LU,0x00110000LU,0x10110000LU,
0x00100004LU,0x10100004LU,0x00110004LU,0x10110004LU,
0x20100000LU,0x30100000LU,0x20110000LU,0x30110000LU,
0x20100004LU,0x30100004LU,0x20110004LU,0x30110004LU,
0x00001000LU,0x10001000LU,0x00011000LU,0x10011000LU,
0x00001004LU,0x10001004LU,0x00011004LU,0x10011004LU,
0x20001000LU,0x30001000LU,0x20011000LU,0x30011000LU,
0x20001004LU,0x30001004LU,0x20011004LU,0x30011004LU,
0x00101000LU,0x10101000LU,0x00111000LU,0x10111000LU,
0x00101004LU,0x10101004LU,0x00111004LU,0x10111004LU,
0x20101000LU,0x30101000LU,0x20111000LU,0x30111000LU,
0x20101004LU,0x30101004LU,0x20111004LU,0x30111004LU,
},{
/* for D bits (numbered as per FIPS 46) 8 9 11 12 13 14 */
0x00000000LU,0x08000000LU,0x00000008LU,0x08000008LU,
0x00000400LU,0x08000400LU,0x00000408LU,0x08000408LU,
0x00020000LU,0x08020000LU,0x00020008LU,0x08020008LU,
0x00020400LU,0x08020400LU,0x00020408LU,0x08020408LU,
0x00000001LU,0x08000001LU,0x00000009LU,0x08000009LU,
0x00000401LU,0x08000401LU,0x00000409LU,0x08000409LU,
0x00020001LU,0x08020001LU,0x00020009LU,0x08020009LU,
0x00020401LU,0x08020401LU,0x00020409LU,0x08020409LU,
0x02000000LU,0x0A000000LU,0x02000008LU,0x0A000008LU,
0x02000400LU,0x0A000400LU,0x02000408LU,0x0A000408LU,
0x02020000LU,0x0A020000LU,0x02020008LU,0x0A020008LU,
0x02020400LU,0x0A020400LU,0x02020408LU,0x0A020408LU,
0x02000001LU,0x0A000001LU,0x02000009LU,0x0A000009LU,
0x02000401LU,0x0A000401LU,0x02000409LU,0x0A000409LU,
0x02020001LU,0x0A020001LU,0x02020009LU,0x0A020009LU,
0x02020401LU,0x0A020401LU,0x02020409LU,0x0A020409LU,
},{
/* for D bits (numbered as per FIPS 46) 16 17 18 19 20 21 */
0x00000000LU,0x00000100LU,0x00080000LU,0x00080100LU,
0x01000000LU,0x01000100LU,0x01080000LU,0x01080100LU,
0x00000010LU,0x00000110LU,0x00080010LU,0x00080110LU,
0x01000010LU,0x01000110LU,0x01080010LU,0x01080110LU,
0x00200000LU,0x00200100LU,0x00280000LU,0x00280100LU,
0x01200000LU,0x01200100LU,0x01280000LU,0x01280100LU,
0x00200010LU,0x00200110LU,0x00280010LU,0x00280110LU,
0x01200010LU,0x01200110LU,0x01280010LU,0x01280110LU,
0x00000200LU,0x00000300LU,0x00080200LU,0x00080300LU,
0x01000200LU,0x01000300LU,0x01080200LU,0x01080300LU,
0x00000210LU,0x00000310LU,0x00080210LU,0x00080310LU,
0x01000210LU,0x01000310LU,0x01080210LU,0x01080310LU,
0x00200200LU,0x00200300LU,0x00280200LU,0x00280300LU,
0x01200200LU,0x01200300LU,0x01280200LU,0x01280300LU,
0x00200210LU,0x00200310LU,0x00280210LU,0x00280310LU,
0x01200210LU,0x01200310LU,0x01280210LU,0x01280310LU,
},{
/* for D bits (numbered as per FIPS 46) 22 23 24 25 27 28 */
0x00000000LU,0x04000000LU,0x00040000LU,0x04040000LU,
0x00000002LU,0x04000002LU,0x00040002LU,0x04040002LU,
0x00002000LU,0x04002000LU,0x00042000LU,0x04042000LU,
0x00002002LU,0x04002002LU,0x00042002LU,0x04042002LU,
0x00000020LU,0x04000020LU,0x00040020LU,0x04040020LU,
0x00000022LU,0x04000022LU,0x00040022LU,0x04040022LU,
0x00002020LU,0x04002020LU,0x00042020LU,0x04042020LU,
0x00002022LU,0x04002022LU,0x00042022LU,0x04042022LU,
0x00000800LU,0x04000800LU,0x00040800LU,0x04040800LU,
0x00000802LU,0x04000802LU,0x00040802LU,0x04040802LU,
0x00002800LU,0x04002800LU,0x00042800LU,0x04042800LU,
0x00002802LU,0x04002802LU,0x00042802LU,0x04042802LU,
0x00000820LU,0x04000820LU,0x00040820LU,0x04040820LU,
0x00000822LU,0x04000822LU,0x00040822LU,0x04040822LU,
0x00002820LU,0x04002820LU,0x00042820LU,0x04042820LU,
0x00002822LU,0x04002822LU,0x00042822LU,0x04042822LU,
} };

/* See ecb_encrypt.c for a pseudo description of these macros. */
#define PERM_OP(a,b,t,n,m) ((t)=((((a)>>(n))^(b))&(m)),\
	(b)^=(t),\
	(a)^=((t)<<(n)))

#define HPERM_OP(a,t,n,m) ((t)=((((a)<<(16-(n)))^(a))&(m)),\
	(a)=(a)^(t)^(t>>(16-(n))))\

static const int shifts2[16]={0,0,1,1,1,1,1,1,0,1,1,1,1,1,1,0};

#ifdef PROTO
static int body(unsigned long *out0, unsigned long *out1,
	des_key_schedule ks, unsigned long Eswap0, unsigned long Eswap1);
static int des_set_key(des_cblock (*key), des_key_schedule schedule);
#else
static int body();
static int des_set_key();
#endif

static int des_set_key(key, schedule)
des_cblock (*key);
des_key_schedule schedule;
	{
	register unsigned long c,d,t,s;
	register unsigned char *in;
	register unsigned long *k;
	register int i;

	k=(unsigned long *)schedule;
	in=(unsigned char *)key;

	c2l(in,c);
	c2l(in,d);

	/* I now do it in 47 simple operations :-)
	 * Thanks to John Fletcher (john_fletcher@lccmail.ocf.llnl.gov)
	 * for the inspiration. :-) */
	PERM_OP (d,c,t,4,0x0f0f0f0fL);
	HPERM_OP(c,t,-2,0xcccc0000LU);
	HPERM_OP(d,t,-2,0xcccc0000LU);
	PERM_OP (d,c,t,1,0x55555555L);
	PERM_OP (c,d,t,8,0x00ff00ffL);
	PERM_OP (d,c,t,1,0x55555555L);
	d=	(((d&0x000000ffL)<<16)| (d&0x0000ff00L)     |
		 ((d&0x00ff0000L)>>16)|((c&0xf0000000LU)>>4));
	c&=0x0fffffffL;

	for (i=0; i<ITERATIONS; i++)
		{
		if (shifts2[i])
			{ c=((c>>2)|(c<<26)); d=((d>>2)|(d<<26)); }
		else
			{ c=((c>>1)|(c<<27)); d=((d>>1)|(d<<27)); }
		c&=0x0fffffffL;
		d&=0x0fffffffL;
		/* could be a few less shifts but I am to lazy at this
		 * point in time to investigate */
		s=	skb[0][ (c     )&0x3f                 ]|
			skb[1][((c>> 6L)&0x03)|((c>> 7L)&0x3c)]|
			skb[2][((c>>13L)&0x0f)|((c>>14L)&0x30)]|
			skb[3][((c>>20L)&0x01)|((c>>21L)&0x06) |
					       ((c>>22L)&0x38)];
		t=	skb[4][ (d     )&0x3f                 ]|
			skb[5][((d>> 7L)&0x03)|((d>> 8L)&0x3c)]|
			skb[6][ (d>>15L)&0x3f                 ]|
			skb[7][((d>>21L)&0x0f)|((d>>22L)&0x30)];

		/* table contained 0213 4657 */
		*(k++)=((t<<16)|(s&0x0000ffffL))&0xffffffffLU;
		s=     ((s>>16)|(t&0xffff0000LU));
		
		s=(s<<4)|(s>>28);
		*(k++)=s&0xffffffffLU;
		}
	return(0);
	}

/******************************************************************
 * modified stuff for crypt.
 ******************************************************************/

/* The changes to this macro may help or hinder, depending on the
 * compiler and the achitecture.  gcc2 always seems to do well :-). 
 * Inspired by Dana How <how@isl.stanford.edu>
 * DO NOT use the alternative version on machines with 8 byte longs.
 */
#ifdef DES_USE_PTR
#define D_ENCRYPT(L,R,S) \
	t=(R^(R>>16)); \
	u=(t&E0); \
	t=(t&E1); \
	u=((u^(u<<16))^R^s[S  ])<<2; \
	t=(t^(t<<16))^R^s[S+1]; \
	t=(t>>2)|(t<<30); \
	L^= \
	*(unsigned long *)(des_SP+0x0100+((t    )&0xfc))+ \
	*(unsigned long *)(des_SP+0x0300+((t>> 8)&0xfc))+ \
	*(unsigned long *)(des_SP+0x0500+((t>>16)&0xfc))+ \
	*(unsigned long *)(des_SP+0x0700+((t>>24)&0xfc))+ \
	*(unsigned long *)(des_SP+       ((u    )&0xfc))+ \
  	*(unsigned long *)(des_SP+0x0200+((u>> 8)&0xfc))+ \
  	*(unsigned long *)(des_SP+0x0400+((u>>16)&0xfc))+ \
 	*(unsigned long *)(des_SP+0x0600+((u>>24)&0xfc));
#else /* original version */
#define D_ENCRYPT(L,R,S)	\
	t=(R^(R>>16)); \
	u=(t&E0); \
	t=(t&E1); \
	u=(u^(u<<16))^R^s[S  ]; \
	t=(t^(t<<16))^R^s[S+1]; \
	t=(t>>4)|(t<<28); \
	L^=	SPtrans[1][(t    )&0x3f]| \
		SPtrans[3][(t>> 8)&0x3f]| \
		SPtrans[5][(t>>16)&0x3f]| \
		SPtrans[7][(t>>24)&0x3f]| \
		SPtrans[0][(u    )&0x3f]| \
		SPtrans[2][(u>> 8)&0x3f]| \
		SPtrans[4][(u>>16)&0x3f]| \
		SPtrans[6][(u>>24)&0x3f];
#endif

static unsigned const char con_salt[128]={
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,
0x0A,0x0B,0x05,0x06,0x07,0x08,0x09,0x0A,
0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,
0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,
0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,
0x23,0x24,0x25,0x20,0x21,0x22,0x23,0x24,
0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,
0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x34,
0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,
0x3D,0x3E,0x3F,0x00,0x00,0x00,0x00,0x00,
};

static unsigned const char cov_2char[64]={
0x2E,0x2F,0x30,0x31,0x32,0x33,0x34,0x35,
0x36,0x37,0x38,0x39,0x41,0x42,0x43,0x44,
0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,
0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,
0x55,0x56,0x57,0x58,0x59,0x5A,0x61,0x62,
0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,
0x6B,0x6C,0x6D,0x6E,0x6F,0x70,0x71,0x72,
0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A
};

#ifdef PROTO
#ifdef PERL5
char *des_crypt(char *buf,char *salt);
#else
char *crypt(char *buf,char *salt);
#endif
#else
#ifdef PERL5
char *des_crypt();
#else
char *crypt();
#endif
#endif

#ifdef PERL5
char *des_crypt(buf,salt)
#else
char *crypt(buf,salt)
#endif
char *buf;
char *salt;
	{
	unsigned int i,j,x,y;
	unsigned long Eswap0=0,Eswap1=0;
	unsigned long out[2],ll;
	des_cblock key;
	des_key_schedule ks;
	static unsigned char buff[20];
	unsigned char bb[9];
	unsigned char *b=bb;
	unsigned char c,u;

	/* eay 25/08/92
	 * If you call crypt("pwd","*") as often happens when you
	 * have * as the pwd field in /etc/passwd, the function
	 * returns *\0XXXXXXXXX
	 * The \0 makes the string look like * so the pwd "*" would
	 * crypt to "*".  This was found when replacing the crypt in
	 * our shared libraries.  People found that the disbled
	 * accounts effectivly had no passwd :-(. */
	x=buff[0]=((salt[0] == '\0')?'A':salt[0]);
	Eswap0=con_salt[x];
	x=buff[1]=((salt[1] == '\0')?'A':salt[1]);
	Eswap1=con_salt[x]<<4;

	for (i=0; i<8; i++)
		{
		c= *(buf++);
		if (!c) break;
		key[i]=(c<<1);
		}
	for (; i<8; i++)
		key[i]=0;

	des_set_key((des_cblock *)(key),ks);
	body(&(out[0]),&(out[1]),ks,Eswap0,Eswap1);

	ll=out[0]; l2c(ll,b);
	ll=out[1]; l2c(ll,b);
	y=0;
	u=0x80;
	bb[8]=0;
	for (i=2; i<13; i++)
		{
		c=0;
		for (j=0; j<6; j++)
			{
			c<<=1;
			if (bb[y] & u) c|=1;
			u>>=1;
			if (!u)
				{
				y++;
				u=0x80;
				}
			}
		buff[i]=cov_2char[c];
		}
	buff[13]='\0';
	return((char *)buff);
	}

static int body(out0, out1, ks, Eswap0, Eswap1)
unsigned long *out0;
unsigned long *out1;
des_key_schedule ks;
unsigned long Eswap0;
unsigned long Eswap1;
	{
	register unsigned long l,r,t,u;
#ifdef DES_USE_PTR
	register unsigned char *des_SP=(unsigned char *)SPtrans;
#endif
	register unsigned long *s;
	register int i,j;
	register unsigned long E0,E1;

	l=0;
	r=0;

	s=(unsigned long *)ks;
	E0=Eswap0;
	E1=Eswap1;

	for (j=0; j<25; j++)
		{
		for (i=0; i<(ITERATIONS*2); i+=4)
			{
			D_ENCRYPT(l,r,  i);	/*  1 */
			D_ENCRYPT(r,l,  i+2);	/*  2 */
			}
		t=l;
		l=r;
		r=t;
		}
	t=r;
	r=(l>>1L)|(l<<31L);
	l=(t>>1L)|(t<<31L);
	/* clear the top bits on machines with 8byte longs */
	l&=0xffffffffLU;
	r&=0xffffffffLU;

	PERM_OP(r,l,t, 1,0x55555555L);
	PERM_OP(l,r,t, 8,0x00ff00ffL);
	PERM_OP(r,l,t, 2,0x33333333L);
	PERM_OP(l,r,t,16,0x0000ffffL);
	PERM_OP(r,l,t, 4,0x0f0f0f0fL);

	*out0=l;
	*out1=r;
	return(0);
	}

