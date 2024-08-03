/*
* Convert character string to 56-bit DES key, contained in 8 bytes
* (including parity):
* - Strip leading and trailing white space; compress multiple spaces
* - compute MD5 hash
* - extract 7 bit pieces into bytes
* - compute parity
*
* The algorithm specifier must be 16 bytes long.
*/

#include "speakfree.h"

#ifdef CRYPTO

#define MAX_KEY 256

#define MD_CTX MD5_CTX
#define MDInit MD5Init
#define MDUpdate MD5Update
#define MDFinal MD5Final

void string_DES_key(char *key, unsigned char des_key[8], char algorithm[16])
{
  char *s, *d;
  char c[MAX_KEY+1];
  int seen_white = 0;
  unsigned char digest[16];
  MD_CTX context;

  /* white space trimming, conversion and compression */
  for (s = key, d = c; *s && (d - c < MAX_KEY); s++) {
    if (isspace(*s)) {
      seen_white = 1;
    }
    else {
      if (seen_white) {
	seen_white = 0;
        if (d != c) *d++ = ' ';
      }
      *d++ = tolower(*s);
    }
  }
  *d = '\0';

  /* extract algorithm specifier, if any */
  strcpy(algorithm, "DES-CBC");
  if ((s = strchr(c, '/')) && s - key < 16) {
    *s = '\0';
    s++; /* skip slash */
    for (d = c; *d; d++) {
      *d = toupper(*d);
    }
    strcpy(algorithm, c);
  }
  else s = c;

  /* compute MD5 hash */
  MDInit(&context);
  MDUpdate(&context, (unsigned char *)s, strlen(s));
  MDFinal(digest, &context);

  /* extract 8 7-bit bytes */
  des_key[0] = (digest[0] & 0xfe) >> 1;
  des_key[1] = ((digest[0] & 0x01) << 6) | ((digest[1] & 0xfc) >> 2);
  des_key[2] = ((digest[1] & 0x03) << 5) | ((digest[2] & 0xf8) >> 3);
  des_key[3] = ((digest[2] & 0x07) << 4) | ((digest[3] & 0xf0) >> 4);
  des_key[4] = ((digest[3] & 0x0f) << 3) | ((digest[4] & 0xe0) >> 5);
  des_key[5] = ((digest[4] & 0x1f) << 2) | ((digest[5] & 0xc0) >> 6);
  des_key[6] = ((digest[5] & 0x3f) << 1) | ((digest[6] & 0x80) >> 7);
  des_key[7] = digest[6] & 0x0f;

  /* compute parity */
  des_set_odd_parity((des_cblock *)des_key);
}
#endif
