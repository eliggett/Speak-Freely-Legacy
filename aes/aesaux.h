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

#include <iostream>
#include <fstream>
#include <strstream>
#include <iomanip>
#include <cctype>

typedef unsigned char   byte;
typedef unsigned long   word;

enum line_type { bad_line = 0, block_len, key_len, test_no, iv_val, key_val, pt_val, ct_val };
#define NO_LTYPES   8
#define BADL_STR    "BADLINE="
#define BLEN_STR    "BLOCKSIZE="
#define KLEN_STR    "KEYSIZE=  "
#define TEST_STR    "TEST= "
#define IV_STR      "IV=   "
#define KEY_STR     "KEY=  "
#define PT_STR      "PT=   "
#define CT_STR      "CT=   "

char      *file_name(char* buf, const word type, const word blen, const word klen);
char      *copy_str(char *s, const char *fstr);
bool      get_line(std::ifstream& inf, char s[]);
void      block_out(const line_type ty, const byte b[], std::ofstream& outf, const word len);

int       find_string(const char *s1, const char s2[]);
line_type find_line(std::ifstream& inf, char str[]);

word    rand32(void);
byte    rand8(void);
int     block_in(byte l[], const char *p);
void    block_clear(byte l[], const word len);
void    block_reverse(byte l[], const word len);
void    block_copy(byte l[], const byte r[], const word len);
void    block_xor(byte l[], const byte r[], const word len);
bool    block_cmp(const byte l[], const byte r[], const word len);
void    block_rndfill(byte l[], word len);

void    put_dec(char *s, word val);
word    get_dec(const char *s);
int     cmp_nocase(const char *s1, const char *s2);
bool    test_args(int argc, char *argv[], char des_chr, char tst_chr);
