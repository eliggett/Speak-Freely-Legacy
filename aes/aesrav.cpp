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

// usage: aes_rav /t:[knec] /b:[45678] /k:[45678]
//
// where:
//
// each of the symbols in square brakets can be used in any combination so
// that:
// 
//         /t:[knec]  selects the tests to be used
//         /b:[45678] selects the block lengths to be used (in 32-bit words)
//         /k:[45678] selects the key lengths to be used (in 32-bit words)
// and:
//         k: generate ECB Known Answer Test files
//         n: generate ECB Known Answer Test files (new)
//         e: generate ECB Monte Carlo Test files
//         c: generate CBC Monte Carlo Test files
//
// Note that, when compared with the NIST test vector sequences, this
// code implements one additional (all zero) test vector as the first
// vector in each set (test 0).

#ifdef AES_IN_CPP
#include "aescpp.h"
#else
#include "aes.h"
#endif
#include "aesaux.h"
#include "aestst.h"

#if defined(AES_DLL)
fn_ptrs fn;
#endif

using   std::cout;
using   std::ifstream;
using   std::ofstream;

enum test_type { ecb_vk, ecb_vt, ecb_nvk, ecb_nvt, ecb_me, ecb_md, cbc_me, cbc_md };

void ref_test(const char *in_file, const unsigned int it_cnt, enum test_type t_type, 
                                f_ctx *alg, const word blen, const word klen)
{   word        i, test_cnt, cnt, e_cnt, fe_cnt;
    byte        key[32], pt[32], iv[32], ect[32], act[64];
    char        str[128];
    line_type   ty;
    ifstream    inf;

    cout << "Test file " << in_file << ": ";

    inf.open(in_file);              // reference test vector file

    if(!inf)
    {
        cout << " file not found \n"; return;
    }

    cnt = 0; e_cnt = test_cnt = 0;

    for(;;)                         // while there are tests
    {
        ty = find_line(inf, str);   // input a line

        if(ty == bad_line)          // until end of file

            break;

        if(ty == block_len)
        {
              if((get_dec(str) >> 3) == blen) continue;
              
              return;
        }
        else if(ty == key_len)
        {
              if((get_dec(str) >> 3) == klen) continue;
              
              return;
        }
        else if(ty == test_no)
        {
            test_cnt = get_dec(str); continue;
        }
        else if(ty == iv_val)
        {
            block_in(iv, str); continue;
        }
        else if(ty == key_val)
        {
            block_in(key, str); continue;
        }
        else if(ty == pt_val)
        {
            block_in(pt, str);
            if(t_type != ecb_md && t_type != cbc_md) continue;
        }
        else if(ty == ct_val)
        {
            block_in(ect, str);
            if(t_type == ecb_md || t_type == cbc_md) continue;
        }

        if(t_type == ecb_md || t_type == cbc_md)
        {
            f_dec_key(alg, key, klen);              // set the key

            block_copy(act, ect, blen);             // encrypted text to low block

            if(t_type == cbc_md)                    // CBC Monte Carlo decryption
            {
                block_copy(act + blen, iv, blen);   // IV to high block

                for(i = 0; i < it_cnt; i += 2)      // do decryptions two at a time
                {
                    f_dec_blk(alg, act, ect);       // decrypt low block

                    block_xor(act + blen, ect, blen);   // xor into high block

                    f_dec_blk(alg, act + blen, ect);    // decrypt high block

                    block_xor(act, ect, blen);      // xor into low block
                }
            }
            else    // ECB Monte Carlo decryption
            {
                for(i = 0; i < it_cnt; ++i)

                    f_dec_blk(alg, act, act);
            }

            if(!block_cmp(pt, act, blen)) if(!e_cnt++) fe_cnt = test_cnt;

            if(t_type == ecb_md)    // test encryption if ECB mode
            {
                f_enc_key(alg, key, klen);  // set the key

                for(i = 0; i < it_cnt; ++i)

                    f_enc_blk(alg, act, act);

                if(!block_cmp(ect, act, blen)) 
                    
                    if(!e_cnt++) fe_cnt = test_cnt;
            }
        }
        else    // if(t_type == ecb_me || t_type == cbc_me || ecb_vk || ecb_vt)
        {
            f_enc_key(alg, key, klen);      // set the key

            if(t_type == cbc_me)                        // CBC Monte Carlo encryption
            {
                block_copy(act, iv, blen);
                block_copy(act + blen, pt, blen);       // copy IV and plaintext

                for(i = 0; i < it_cnt; i += 2)
                {
                    block_xor(act + blen, act, blen);   // xor low block into high block

                    f_enc_blk(alg, act + blen, act + blen); // encrypt high block

                    block_xor(act, act + blen, blen);   // xor high block into low block

                    f_enc_blk(alg, act, act);           // encrypt low block
                }
            }
            else    // ECB Monte Carlo encryption
            {
                block_copy(act, pt, blen);

                for(i = 0; i < it_cnt; ++i)

                    f_enc_blk(alg, act, act);
            }

            if(!block_cmp(ect, act, blen)) if(!e_cnt++) fe_cnt = test_cnt;

            if(t_type != cbc_me)    // if ECB mode test decrytpion
            {
                f_dec_key(alg, key, klen);  // set the key

                for(i = 0; i < it_cnt; ++i)

                    f_dec_blk(alg, act, act);

                if(!block_cmp(pt, act, blen)) if(!e_cnt++) fe_cnt = test_cnt;
            }
        }
    }

    inf.close();

    if(e_cnt > 0)   // report any errors
    {
        cout << e_cnt << " ERRORS during test (first on test " << fe_cnt << ")\n";
    }
    else            // else report all is well

        cout << "all tests correct\n";
}

void do_tests(const bool vkt, const bool vktn, const bool ecb, const bool cbc, 
              f_ctx *alg, const word blen, const word klen)
{   char    path[128], *sp;

    sp = copy_str(path, ref_path);

    if(vkt)
    {
        file_name(sp, 0, blen, klen); 
        ref_test(path, 1, ecb_vk, alg, blen, klen);
        file_name(sp, 1, blen, klen); 
        ref_test(path, 1, ecb_vt, alg, blen, klen);
    }

    if(vktn)
    {
        file_name(sp, 2, blen, klen); 
        ref_test(path, 1, ecb_nvk, alg, blen, klen);
        file_name(sp, 3, blen, klen); 
        ref_test(path, 1, ecb_nvt, alg, blen, klen);
    }

    if(ecb)
    {
        file_name(sp, 4, blen, klen); 
        ref_test(path, 10000, ecb_me, alg, blen, klen);
        file_name(sp, 5, blen, klen); 
        ref_test(path, 10000, ecb_md, alg, blen, klen);
    }

    if(cbc)
    {
        file_name(sp, 6, blen, klen); 
        ref_test(path, 10000, cbc_me, alg, blen, klen);
        file_name(sp, 7, blen, klen); 
        ref_test(path, 10000, cbc_md, alg, blen, klen);
    }
}

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
{   bool        vkt, vktn, ecb, cbc, bf[5], kf[5];
    f_ctx   alg;

#if defined(AES_DLL)
    HINSTANCE   h_dll;

    if(!(h_dll = init_dll(fn))) return -1;
#endif

#if !defined(AES_IN_CPP)
    alg.n_blk = 0;
    alg.n_rnd = 0;
#endif

    if(argc == 1)
    {
        cout << "\nusage: aes_rav /t:[knec] /b:[45678] /k:[45678]";
        cout << "\n";
        cout << "\nwhere the symbols in square brackets can be used in";
        cout << "\nany combination (without the brackets) and have the";
        cout << "\nfollowing meanings:";
        cout << "\n";
        cout << "\n        /t:[knec]   selects which tests are used";
        cout << "\n        /b:[45678]  selects the block lengths used";
        cout << "\n        /k:[45678]  selects the key lengths used";
        cout << "\nwhere:";
        cout << "\n        k: generate ECB Known Answer Test files";
        cout << "\n        n: generate ECB Known Answer Test files (new)";
        cout << "\n        e: generate ECB Monte Carlo Test files";
        cout << "\n        c: generate CBC Monte Carlo Test files";
        cout << "\n";
        cout << "\nand the characters giving block and key lengths are";
        cout << "\ndigits representing the lengths in 32-bit units.\n\n";
        exit(0);
    }

    cout << "\nRun tests for the AES algorithm"
#if defined(AES_DLL)
     " (DLL Version)\n\n";
#else
    "\n\n";
#endif

    vkt  = test_args(argc, argv, 't', 'k');
    vktn = test_args(argc, argv, 't', 'n');
    ecb  = test_args(argc, argv, 't', 'e');
    cbc  = test_args(argc, argv, 't', 'c');

    kf[0] = test_args(argc, argv, 'k', '4');
    kf[1] = test_args(argc, argv, 'k', '5');
    kf[2] = test_args(argc, argv, 'k', '6');
    kf[3] = test_args(argc, argv, 'k', '7');
    kf[4] = test_args(argc, argv, 'k', '8');

    if(!(kf[0] || kf[1] || kf[2] || kf[3] || kf[4]))
    {
        kf[0] = kf[2] = kf[4] = true;       // AES key sizes if not specified
        kf[1] = kf[3] = false;
    }

#if !defined(BLOCK_SIZE)
    bf[0] = test_args(argc, argv, 'b', '4');
    bf[1] = test_args(argc, argv, 'b', '5');
    bf[2] = test_args(argc, argv, 'b', '6');
    bf[3] = test_args(argc, argv, 'b', '7');
    bf[4] = test_args(argc, argv, 'b', '8');

    if(!(bf[0] || bf[1] || bf[2] || bf[3] || bf[4]))
    {
        bf[0] = true;                   // AES block size if not specified
        bf[1] = bf[2] = bf[3] = bf[4] = false;
    }
#else
    bf[0] = bf[1] = bf[2] = bf[3] = bf[4] = false;
    bf[(BLOCK_SIZE - 16) / 4] = true;
#endif

    for(int bi = 0; bi < 5; ++bi) if(bf[bi])
    {
#if defined(AES_DLL)
            if(fn.fn_blk_len) f_blk_len(&alg, 16 + 4 * bi);
#elif !defined(BLOCK_SIZE)
            f_blk_len(&alg, 16 + 4 * bi);
#else
            if(16 + 4 * bi != BLOCK_SIZE) continue;
#endif
        for(int ki = 0; ki < 5; ++ki) if(kf[ki])
        {
            do_tests(vkt, vktn, ecb, cbc, &alg, 16 + 4 * bi, 16 + 4 * ki);
        }
    }

#if defined(AES_DLL)
    if(h_dll) FreeLibrary(h_dll);
#endif

    cout << "\n\n";
    return 0;
}
