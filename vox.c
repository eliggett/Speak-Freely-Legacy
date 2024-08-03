/* vox.c

   Dave Hawkes 27/IX/95

   This rle codes the GSM or other compression to reduce data
   at low signal levels.  */

#include "speakfree.h"

extern gsm gsmh;		      /* GSM handle */

static int rledecomp(len, lpData, lpBuffer)
  int len;
  char *lpData, *lpBuffer;
{
    int j, cnt, val;
    char *lpStart;
    char *lpEnd;
    if(*lpBuffer == (char) ((unsigned char) 0x80)) {
	bzero(lpData, len);
	return len;
    }
    lpStart = lpData;
    lpEnd = lpData + len;
    while(lpData < lpEnd) {
	if(*lpBuffer & 0x80) {
	    cnt = *lpBuffer++ & 0x7f;
	    val = *lpBuffer++;
	    for(j = 0; j < cnt; ++j)
		*lpData++ = val;
	} else {
	    cnt = *lpBuffer++;
	    for(j = 0; j < cnt; ++j)
		*lpData++ = *lpBuffer++;
	}
    }
    return lpData - lpStart;
}

static gsm_frame gsm_zero = {
    216, 32, 162, 225, 90, 80, 0, 73, 36, 146, 73, 36, 80, 0, 73, 36, 146, 73,
    36, 80, 0, 73, 36, 146, 73, 36, 80, 0, 73, 36, 146, 73, 36 };

static char buffer[BUFL];

void vox_gsmdecomp(soundbuf *sb)
{
    gsm_signal dst[160];
    int i, j, isz, l = 0;
    static char dcb[BUFL];
    short declen = ntohl(*((short *) sb->buffer.buffer_val));
    int cmpr;
    char *pbuffer;
    
    cmpr = (declen & 0x8000);
    declen &= ~(0x8000 | 0x4000);
    if (declen <= 0 || declen > 1600) {
	declen = 1600;
    }
    if(cmpr) {
	rledecomp(declen, buffer, ((char *) sb->buffer.buffer_val) + sizeof(short));
	pbuffer = buffer;
    } else
	pbuffer = ((char *) sb->buffer.buffer_val) + sizeof(short);
    for (i = 0; i < sb->buffer.buffer_len - sizeof(short);
		i += sizeof(gsm_frame)) {
	isz = 0;
	for(j = 0; j < sizeof(gsm_frame); ++j) {
	    isz |= pbuffer[j];
	    pbuffer[j] += gsm_zero[j];
	}
	if(isz == 0) {
	    memset(dcb, audio_s2u(0), sizeof(dcb));
	    l += 160;
	} else {
	    gsm_decode(gsmh, (gsm_byte *) pbuffer, dst);
	    for (j = 0; j < 160; j++) {
		dcb[l++] = audio_s2u(dst[j]);
	    }
	}
	pbuffer += sizeof(gsm_frame);
    }
    bcopy(dcb, sb->buffer.buffer_val, declen);
    sb->buffer.buffer_len = declen;
}
