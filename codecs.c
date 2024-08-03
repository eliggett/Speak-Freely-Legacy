/*

		 Sound encoder and decoder interfaces

*/

#include "speakfree.h"

static struct adpcm_state adpcm;      /* ADPCM compression state */

/*  ADPCMDECOMP  --  Decompress the contents of a sound buffer using ADPCM.  */

void adpcmdecomp(struct soundbuf *sb)
{
    char *dp = (char *) sb->buffer.buffer_val;
    unsigned char *sp;
    unsigned char dob[TINY_PACKETS * 2];

    /* Restore the decoder state from the copy saved in the packet,
       independent of the byte order of the machine we're running on. */

    sp = (unsigned char *) dp + (sb->buffer.buffer_len - 3);
    adpcm.valprev = (sp[0] << 8) | sp[1];
    adpcm.index = sp[2];
    sb->buffer.buffer_len -= 3;

    adpcm_decoder_u(dp, dob, sb->buffer.buffer_len * 2, &adpcm);
    sb->buffer.buffer_len *= 2;
    bcopy(dob, dp, sb->buffer.buffer_len);
}
