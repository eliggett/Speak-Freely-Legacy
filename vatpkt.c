/*

		 VAT packet interconversion routines

*/

#include "speakfree.h"
#include "vat.h"
#include <pwd.h>
#include <sys/param.h>

static audio_descr_t adt[] = {
/* enc	   sample ch */
  {AE_PCMU,  8000, 1},	/*  0 PCMU */
  {AE_MAX,   8000, 1},	/*  1 1016 */
  {AE_MAX,	0, 1},	/*  2 G721 32 kb/s CCITT ADPCM */
  {AE_GSM,   8000, 1},	/*  3 GSM */
  {AE_MAX,	0, 1},	/*  4 G723 24 kb/s CCITT ADPCM */
  {AE_MAX,	0, 1},	/*  5 unassigned */
  {AE_MAX,	0, 1},	/*  6 unassigned */
  {AE_MAX,	0, 1},	/*  7 unassigned */
  {AE_MAX,	0, 1},	/*  8 unassigned */
  {AE_MAX,	0, 1},	/*  9 unassigned */
  {AE_MAX,	0, 1},	/* 10 unassigned */
  {AE_MAX,	0, 1},	/* 11 unassigned */
  {AE_MAX,	0, 1},	/* 12 unassigned */
  {AE_MAX,	0, 1},	/* 13 unassigned */
  {AE_MAX,	0, 1},	/* 14 unassigned */
  {AE_MAX,	0, 1},	/* 15 unassigned */
  {AE_MAX,	0, 1},	/* 16 unassigned */
  {AE_MAX,	0, 1},	/* 17 unassigned */
  {AE_MAX,	0, 1},	/* 18 unassigned */
  {AE_MAX,	0, 1},	/* 19 unassigned */
  {AE_MAX,	0, 1},	/* 20 unassigned */
  {AE_MAX,	0, 1},	/* 21 unassigned */
  {AE_MAX,	0, 1},	/* 22 unassigned */
  {AE_MAX,	0, 1},	/* 23 unassigned */
  {AE_MAX,	0, 1},	/* 24 unassigned */
  {AE_MAX,	0, 1},	/* 25 unassigned */
  {AE_MAX,	0, 1},	/* 26 L16, 16000 samples/sec */
  {AE_MAX,	0, 1},	/* 27 L16, 44100 samples/sec, 2 channels */
  {AE_LPC,   8000, 1},	/* 28 LPC, 4 frames */
  {AE_LPC,   8000, 1},	/* 29 LPC, 1 frame */
  {AE_IDVI,  8000, 1},	/* 30 32 kb/s Intel DVI ADPCM */
  {AE_MAX,	0, 1},	/* 31 unassigned */
};

/*  ISVAT  --  Determine if this is a parseable VAT format packet.
	       If so, convert it to an equivalent sound buffer.  */

int isvat(unsigned char *pkt, int len)
{
    int findex = pkt[1] & VATHF_FMTMASK;
    char *vp;
    long v;

    /* Validate (as much as possible) whether this is a VAT packet
       that we're able to translate. */

    if (((pkt[0] & 0xC0) == 0) && ((pkt[1] & 0x60) == 0) /* &&
	(adt[findex].encoding != AE_MAX) */
    ) {

	struct soundbuf sb;
	unsigned char *payload;
	int paylen;

/*fprintf(stderr, "VAT Ver = %d, nsid = %d, ts = %d, format = %d confid = %d, tstamp = %ld\n",
    pkt[0] >> 6, pkt[0] & NSID_MASK, !!(pkt[1] & VATHF_NEWTS), findex,
    (((unsigned int) pkt[2]) << 8) | pkt[3],
    (((unsigned long) pkt[4]) << 24) |
    (((unsigned long) pkt[5]) << 16) |
    (((unsigned long) pkt[6]) <<  8) |
    (((unsigned long) pkt[7]) <<  0));
*/

	payload = pkt + (8 + 4 * (pkt[0] & NSID_MASK)); /* Payload start */
	paylen = len - ((8 + 4 * (pkt[0] & NSID_MASK))); /* Payload length */
	sb.compression = fProtocol;
	sb.buffer.buffer_len = 0;

#ifdef NEEDED
	/* Fake a VAT unique host name from the conference identifier. */

        sprintf(sb.sendinghost, ".VAT:%02X%02X",
	    pkt[2], pkt[3]);
#else
        strcpy(sb.sendinghost, ".VAT");
#endif

	switch (adt[findex].encoding) {

	    case AE_PCMU:
		sb.buffer.buffer_len = paylen;
		bcopy(payload, sb.buffer.buffer_val, paylen);
		break;

	    case AE_GSM:
		sb.buffer.buffer_len = paylen + sizeof(short);
		bcopy(payload, sb.buffer.buffer_val + 2, paylen);
    	    	v = (((long) paylen) * 160) / 33;
		vp = sb.buffer.buffer_val;
		vp[0] = v >> 8;
		vp[1] = v & 0xFF;
		sb.compression |= fCompGSM;
		break;

	    case AE_IDVI:
		bcopy(payload + 4, sb.buffer.buffer_val, paylen - 4);
		bcopy(payload, sb.buffer.buffer_val + (paylen - 4), 3);
		sb.buffer.buffer_len = paylen - 1;
		if (adt[findex].sample_rate == 8000) {
		    sb.compression |= fCompADPCM;
		} else {
#ifdef NEEDED

		    /* Bogus attempt to convert sampling rate.	We
                       really need to do this in linear mode, which isn't
		       supported on all SPARCs.  This is better than
		       nothing, though. */

		    int inc = adt[findex].sample_rate / 8000, i;
		    unsigned char *in = (unsigned char *) sb.buffer.buffer_val,
				  *out = (unsigned char *) sb.buffer.buffer_val;

		    adpcmdecomp(&sb);
		    for (i = 0; i < (paylen - 4) / inc; i++) {
			*out++ = *in;
			in += inc;
		    }
		    sb.buffer.buffer_len /= inc;
#endif
		}
		break;

	    case AE_LPC:
		sb.buffer.buffer_len = paylen + sizeof(short);
		{   int i;
		    unsigned char *in = payload,
				  *out = (unsigned char *) sb.buffer.buffer_val + 2;

		    for (i = 0; i < paylen / 14; i++) {
			bcopy(in, out, 3);
			out[3] = 0;
			bcopy(in + 3, out + 4, 10);
			in += 14;
			out += 14;
		    }
		}
    	    	v = (((long) paylen) * LPC_FRAME_SIZE) / 14;
		vp = sb.buffer.buffer_val;
		vp[0] = v >> 8;
		vp[1] = v & 0xFF;
		sb.compression |= fCompLPC;
		break;

	    case AE_L16:
		if (adt[findex].channels == 1) {
		    int i, j, k;
		
		    for (i = j = k = 0; i < (paylen / 8); i++) {
			if ((k & 3) != 2  && ((i % 580) != 579)) {
			    sb.buffer.buffer_val[j++] =
				audio_s2u((payload[i * 8] << 8) | payload[(i * 8) + 1]);
			}
			k = (k + 1) % 11;
		    }
		    sb.buffer.buffer_len = j;
		} else if (adt[findex].channels == 2) {
		    int i, j, k;
		
		    for (i = j = k = 0; i < (paylen / 16); i++) {
			if ((k & 3) != 2  && ((i % 580) != 579)) {
			    sb.buffer.buffer_val[j++] =
				audio_s2u((((payload[i * 16] << 8) | payload[(i * 16) + 1]) +
				  ((payload[(i * 16) + 2] << 8) | payload[(i * 16) + 3])) / 2);
			}
			k = (k + 1) % 11;
		    }
		    sb.buffer.buffer_len = j;
		}
		break;

	    default:
		/* Unknown compression type. */
		sb.buffer.buffer_len = 0;
		break;
	}
	bcopy(&sb, pkt, (int) (((sizeof sb - BUFL)) + sb.buffer.buffer_len));
	return TRUE;
    }
    return FALSE;
}

/*  VATOUT  --	Convert a sound buffer into a VAT packet, given the
		timestamp for the next packet sent to this connection.	*/

int vatout(soundbuf *sb, unsigned long ssrc_i, unsigned long timestamp_i, int spurt)
{
    soundbuf rp;
    char *pkt = (char *) &rp;
    LONG pl = 0;

    pkt[0] = 0;

    pkt[1] = spurt ? 0x80 : 0;

    pkt[2] = ssrc_i & 0xFF;
    pkt[3] = (ssrc_i >> 8) & 0xFF;

    pkt[4] = timestamp_i >> 24;
    pkt[5] = timestamp_i >> 16;
    pkt[6] = timestamp_i >> 8;
    pkt[7] = timestamp_i & 0xFF;

    if (sb->compression & fCompGSM) {
	pkt[1] |= VAT_AUDF_GSM;
	bcopy(sb->buffer.buffer_val + 2, pkt + 8,
		  (int) sb->buffer.buffer_len - 2);
	pl = (sb->buffer.buffer_len - 2) + 8;

    } else if (sb->compression & fCompADPCM) {
	pkt[1] |= VAT_AUDF_IDVI;
	bcopy(sb->buffer.buffer_val, pkt + 8 + 4,
		  (int) sb->buffer.buffer_len - 3);
	bcopy(sb->buffer.buffer_val + ((int) sb->buffer.buffer_len - 3),
		  pkt + 8, 3);
	pkt[8 + 3] = 0;
	pl = (sb->buffer.buffer_len + 1) + 8;

    } else if (sb->compression & fCompLPC) {
	int n = (int) ((sb->buffer.buffer_len - 2) / 14);
	int i;
	char *cp = pkt + 8, *sp = sb->buffer.buffer_val + 2;

	pkt[1] |= VAT_AUDF_LPC4;
	pl = 8;
	for (i = 0; i < n; i++) {
	    bcopy(sp, cp, 3);
	    bcopy(sp + 4, cp + 3, 10);
	    sp += 14;
	    cp += 14;
	    pl += 14;
	}

    } else {	/* Uncompressed PCMU samples */
#if (VAT_AUDF_MULAW8 != 0)
	pkt[1] |= VAT_AUDF_MULAW8;
#endif
	bcopy(sb->buffer.buffer_val, pkt + 8,
		(int) sb->buffer.buffer_len);
	pl = (int) sb->buffer.buffer_len + 8;
    }
    if (pl > 0) {
	bcopy(pkt, (char *) sb, (int) pl);
    }
    return pl;
}

/*  MAKEVATID  --  Create VAT ID packet.  */

int makeVATid(char **vp, unsigned long ssrc_i)
{
    char *sp = NULL, *cp = NULL;
    struct passwd *pw;
    char s[256];
    int l = 0;

    /* Watch as we make increasingly desperate attempts to
       obtain the user's full name. */

    sp = getenv("SPEAKFREE_ID");
    if (sp != NULL && strlen(sp) > 0) {
        char *ep = strchr(sp, ':');
	if (ep != NULL) {
	    *ep = 0;
	}
    }

    if (sp == NULL) {
	pw = getpwuid(getuid());
	if (pw != NULL) {
	    if (pw->pw_gecos != NULL) {
		strcpy(s, pw->pw_gecos);
		sp = s;
	    } else if (pw->pw_name != NULL) {
		char hn[MAXHOSTNAMELEN];

		hn[0] = 0;
		gethostname(hn, sizeof hn);
                sprintf(s, "%s@%s", pw->pw_name, hn);
		sp = s;
	    }
	}
    }

    if (sp == NULL) {
        if ((sp = getenv("SPEAKFREE_CNAME")) != NULL && strlen(sp) > 0) {
            if (sp[0] == '*') {
		sp++;
	    }
	}
    }

    if (sp == NULL) {
        sp = "Unknown User";
    }

    cp = malloc(4 + strlen(sp) + 1);
    if (cp != NULL) {
	cp[0] = 0;
	cp[1] = 1;
	cp[2] = ssrc_i & 0xFF;
	cp[3] = (ssrc_i >> 8) & 0xFF;
	strcpy(cp + 4, sp);
	l = 4 + strlen(sp) + 1;
    }
    *vp = cp;
    return l;
}

/*  MAKEVATDONE  --  Create VAT "DONE" packet.  */

int makevatdone(char *v, unsigned long ssrc_i) 
{
    v[0] = 0;
    v[1] = 2;
    v[2] = ssrc_i & 0xFF;
    v[3] = (ssrc_i >> 8) & 0xFF;
    return 4;
}
