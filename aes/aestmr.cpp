/*
 -------------------------------------------------------------------------
 Copyright (c) 2001, Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 

 DISCLAIMER

 This software is provided 'as is' with no explicit or implied warranties
 in respect of its properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
 Issue Date: 15/01/2002
*/

// Measure the Encryption, Decryption and Key Setup Times for AES using
// the Pentium Time Stamp Counter

#include <iostream>
#include <fstream>

#ifdef AES_IN_CPP
#include "aescpp.h"
#else
#include "aes.h"
#endif
#include "aesaux.h"
#include "aestst.h"

// Use this define if testing aespp.c

//#define AESX

#if defined(AES_DLL)
fn_ptrs fn;
#endif

#define  PROCESSOR   "PIII"  // Processor

void cycles(volatile unsigned __int64 *rtn)    
{   
    __asm   // read the Pentium Time Stamp Counter
    {   cpuid
        rdtsc
        mov     ecx,rtn
        mov     [ecx],eax
        mov     [ecx+4],edx
        cpuid
    }
}

const unsigned int loops = 100; // number of timing loops

word rand32(void)
{   static word   r4,r_cnt = -1,w = 521288629,z = 362436069;

    z = 36969 * (z & 65535) + (z >> 16);
    w = 18000 * (w & 65535) + (w >> 16);

    r_cnt = 0; r4 = (z << 16) + w; return r4;
}

byte rand8(void)
{   static word   r4,r_cnt = 4;

    if(r_cnt == 4)
    {
        r4 = rand32(); r_cnt = 0;
    }

    return (char)(r4 >> (8 * r_cnt++));
}

// fill a block with random charactrers

void block_rndfill(byte l[], word len)
{   word  i;

    for(i = 0; i < len; ++i)

        l[i] = rand8();
}

// measure cycles for an encryption call

word e_cycles(const word klen, f_ctx* alg)
{   byte  pt[16], ct[16], key[32];
    word  i, c1, c2;
    unsigned volatile __int64 cy0, cy1, cy2;

    // set up a random key of 256 bits

    block_rndfill(key, 32);

    // set up a random plain text

    block_rndfill(pt, 16);

    // do a set_key in case it is necessary

    f_enc_key(alg, key, klen); c1 = c2 = 0xffffffff;

    // do an encrypt to remove any 'first time through' effects

    f_enc_blk(alg, pt, ct);

    for(i = 0; i < loops; ++i)
    {
        block_rndfill(pt, 16);

        // time one and two encryptions

        cycles(&cy0);
        f_enc_blk(alg, pt, ct);
        cycles(&cy1);
        f_enc_blk(alg, ct, ct);
        f_enc_blk(alg, ct, ct);
        f_enc_blk(alg, ct, ct);
        f_enc_blk(alg, ct, ct);
        f_enc_blk(alg, ct, ct);
        cycles(&cy2);

        cy2 -= cy1; cy1 -= cy0;     // time for one and two calls

        c1 = (word)(c1 > cy1 ? cy1 : c1); // find minimum values over the loops
        c2 = (word)(c2 > cy2 ? cy2 : c2);
    }

    return ((c2 - c1) + 1) >> 2;    // return one call timing
}

// measure cycles for a decryption call

word d_cycles(const word klen, f_ctx* alg)
{   byte  pt[16], ct[16], key[32];
    word  i, c1, c2;
    unsigned volatile __int64 cy0, cy1, cy2;

    // set up a random key of 256 bits

    block_rndfill(key, 32);

    // set up a random plain text

    block_rndfill(pt, 16);

    // do a set_key in case it is necessary

    f_dec_key(alg, key, klen); c1 = c2 = 0xffffffff;

    // do an decrypt to remove any 'first time through' effects

    f_dec_blk(alg, pt, ct);

    for(i = 0; i < loops; ++i)
    {
        block_rndfill(pt, 16);

        // time one and two encryptions

        cycles(&cy0);
        f_dec_blk(alg, pt, ct);
        cycles(&cy1);
        f_dec_blk(alg, ct, ct);
        f_dec_blk(alg, ct, ct);
        f_dec_blk(alg, ct, ct);
        f_dec_blk(alg, ct, ct);
        f_dec_blk(alg, ct, ct);
        cycles(&cy2);

        cy2 -= cy1; cy1 -= cy0;     // time for one and two calls

        c1 = (word)(c1 > cy1 ? cy1 : c1); // find minimum values over the loops

        c2 = (word)(c2 > cy2 ? cy2 : c2);
    }

    return ((c2 - c1) + 1) >> 2;    // return one call timing
}

// measure cycles for an encryption key setup

word ke_cycles(const word klen, f_ctx* alg)
{   byte  key[32];
    word  i, c1, c2;
    unsigned volatile __int64 cy0, cy1, cy2;

    // set up a random key of 256 bits

    block_rndfill(key, 32);

    // do an set_key to remove any 'first time through' effects

    f_enc_key(alg, key, klen); c1 = c2 = 0xffffffff;

    for(i = 0; i < loops; ++i)
    {
        block_rndfill(key, 32);

        // time one and two encryptions

        cycles(&cy0);
        f_enc_key(alg, key, klen);
        cycles(&cy1);
        f_enc_key(alg, key, klen);
        f_enc_key(alg, key, klen);
        f_enc_key(alg, key, klen);
        f_enc_key(alg, key, klen);
        f_enc_key(alg, key, klen);
        cycles(&cy2);

        cy2 -= cy1; cy1 -= cy0;     // time for one and two calls

        c1 = (word)(c1 > cy1 ? cy1 : c1); // find minimum values over the loops

        c2 = (word)(c2 > cy2 ? cy2 : c2);
    }

    return ((c2 - c1) + 1) >> 2;    // return one call timing
}

// measure cycles for an encryption key setup

word kd_cycles(const word klen, f_ctx* alg)
{   byte  key[32];
    word  i, c1, c2;
    unsigned volatile __int64 cy0, cy1, cy2;

    // set up a random key of 256 bits

    block_rndfill(key, 32);

    // do an set_key to remove any 'first time through' effects

    f_dec_key(alg, key, klen); c1 = c2 = 0xffffffff;

    for(i = 0; i < loops; ++i)
    {
        block_rndfill(key, 32);

        // time one and two encryptions

        cycles(&cy0);
        f_dec_key(alg, key, klen);
        cycles(&cy1);
        f_dec_key(alg, key, klen);
        f_dec_key(alg, key, klen);
        f_dec_key(alg, key, klen);
        f_dec_key(alg, key, klen);
        f_dec_key(alg, key, klen);
        cycles(&cy2);

        cy2 -= cy1; cy1 -= cy0;     // time for one and two calls

        c1 = (word)(c1 > cy1 ? cy1 : c1); // find minimum values over the loops

        c2 = (word)(c2 > cy2 ? cy2 : c2);
    }

    return ((c2 - c1) + 1) >> 2;    // return one call timing
}

static word kl[5] = { 16, 20, 24, 28, 32 };
static word ekt[5], dkt[5], et[5], dt[5];

void output(std::ofstream& outf, const word inx, const word bits)
{   word  t;
    byte  c0, c1, c2;

    outf << "\n// " << 8 * kl[inx] << " Bit:";
    outf << "   Key Setup: " << ekt[inx] << '/' << dkt[inx] << " cycles";
    t = (1000 * bits + et[inx] / 2) / et[inx]; 
    c0 = (byte)('0' + t / 100); c1 = (byte)('0' + (t / 10) % 10); c2 = (byte)('0' + t % 10);
    outf << "\n// Encrypt:   " << et[inx] << " cycles = 0."
         << c0 << c1 << c2 << " bits/cycle"; 
    t = (1000 * bits + dt[inx] / 2) / dt[inx]; 
    c0 = (byte)('0' + t / 100); c1 = (byte)('0' + (t / 10) % 10); c2 = (byte)('0' + t % 10);
    outf << "\n// Decrypt:   " << dt[inx] << " cycles = 0."
         << c0 << c1 << c2 << " bits/cycle"; 
}

#if defined(AESX)
#define INC 1
#else
#define INC 2
#endif

#if BLOCK_SIZE == 16
#define STR 0
#define CNT 1
#elif BLOCK_SIZE == 20
#define STR 1
#define CNT 2
#elif BLOCK_SIZE == 24
#define STR 2
#define CNT 3
#elif BLOCK_SIZE == 28
#define STR 3
#define CNT 4
#elif BLOCK_SIZE == 32
#define STR 4
#define CNT 5
#elif !defined(BLOCK_SIZE)
#define STR 0
#define CNT 5
#else
#error Illegal block size
#endif

#ifdef  AES_DLL

#include "windows.h"

HINSTANCE init_dll(fn_ptrs& fn)
{   HINSTANCE   h_dll;

    if(!(h_dll = LoadLibrary(dll_path)))
    {
        std::cout << "\n\nDynamic link Library AES_DLL not found\n\n"; return 0;
    }

    fn.fn_blk_len = (g_blk_len*)GetProcAddress(h_dll, "_aes_blk_len@8");
    fn.fn_enc_key = (g_enc_key*)GetProcAddress(h_dll, "_aes_enc_key@12");
    fn.fn_dec_key = (g_dec_key*)GetProcAddress(h_dll, "_aes_dec_key@12");
    fn.fn_enc_blk = (g_enc_blk*)GetProcAddress(h_dll, "_aes_enc_blk@12");
    fn.fn_dec_blk = (g_dec_blk*)GetProcAddress(h_dll, "_aes_dec_blk@12");

#if !defined(BLOCK_SIZE)
    if(!fn.fn_enc_key || !fn.fn_dec_key || !fn.fn_enc_blk  || !fn.fn_dec_blk || !fn.fn_blk_len)
#else
    if(!fn.fn_enc_key || !fn.fn_dec_key || !fn.fn_enc_blk  || !fn.fn_dec_blk)
#endif
    {
        std::cout << "\n\nRequired DLL Entry Point(s) not found\n\n"; 
        FreeLibrary(h_dll); 
        return 0;
    }

    return h_dll;
}

#endif  // AES_DLL


int main(int argc, char *argv[])
{   std::ofstream   outf;
    f_ctx   alg;

#if defined(AES_DLL)
    HINSTANCE   h_dll;

    if(!(h_dll = init_dll(fn))) return -1;
#endif

#if !defined(AES_IN_CPP)
    alg.n_blk = 0;
    alg.n_rnd = 0;
#endif

    outf.open(argc == 2 ? argv[1] : "CON", std::ios_base::out);

    outf << "\n// AES"
#if defined(AES_DLL)
        " (DLL)"
#endif
    " on " << PROCESSOR << " processor";

    for(int bi = STR; bi < CNT; bi += INC)
    {
#if defined(AES_DLL)
            if(fn.fn_blk_len) f_blk_len(&alg, 16 + 4 * bi);
#elif !defined(BLOCK_SIZE)
            f_blk_len(&alg, 16 + 4 * bi);
#else
            if(16 + 4 * bi != BLOCK_SIZE) continue;
#endif
        for(word ki = 0; ki < 5; ki += INC)
        {
            ekt[ki] = ke_cycles(kl[ki], &alg);
            dkt[ki] = kd_cycles(kl[ki], &alg);
            et[ki]  =  e_cycles(kl[ki], &alg);
            dt[ki]  =  d_cycles(kl[ki], &alg);
        }
    
        outf << "\n// Block Length: " << 128 + 32 * bi;
        for(word k = 0; k < 5; k += INC)
            output(outf, k, 128 + 32 * bi); 
    }
#if defined(AES_DLL)
    if(h_dll) FreeLibrary(h_dll);
#endif

    outf << "\n\n";
    return 0;
}
