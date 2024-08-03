/*#define USE_CURSES*/
#define BYTE_SWAP_DEBUG
/* #define PGP_DEBUG */
/*

			Speak Freely for Unix
		  Network sound transmission program

	Designed and implemented in July of 1991 by John Walker

*/

#include "speakfree.h"
#include "version.h"

#ifdef HAVE_DEV_RANDOM

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#endif
/*  Destination host descriptor.  */

struct destination {
    struct destination *dnext;	      /* Next destination in list */
    char *server;		      /* Host name identifier string */
    struct sockaddr_in name;	      /* Internet address */
    struct sockaddr_in ctrl;	      /* RTCP port address */
    unsigned char time_to_live;       /* Multicast scope specification */
#ifdef NAT_LAUNCH
    int dsock;	    	    	      /* Data socket file descriptor */
    int csock;	    	    	      /* Control socket file descriptor */
#endif
    char deskey[9];		      /* Destination DES key, if any */
    char rtpdeskey[9];		      /* Destination RTP DES key, if any */
    char vatdeskey[9];		      /* Destination VAT DES key, if any */
    char ideakey[17];		      /* Destination IDEA key, if any */
    char pgpkey[17];		      /* Destination PGP key, if any */
    char blowfish_spec; 	      /* Nonzero if Blowfish key specified */
#ifdef CRYPTO
    BF_KEY blowfishkey; 	      /* Destination Blowfish key, if any */
#endif
    char aes_spec;          	      /* Nonzero if AES key specified */
#ifdef CRYPTO
    aes_ctx aes_ctx;	    	      /* AES context */
#endif
    char *otp;			      /* Key file */
};

static char *progname;		      /* Program name */
static int sock;		      /* Communication socket */
#ifdef NAT_LAUNCH
static int xnat_socketsharing = FALSE;  /* Is NAT socket sharing in use ? */
static int xnat_consock, xnat_datasock; /* NAT socket FDs passed by sfspeaker */
#endif
static struct destination *dests = NULL, *dtail;
static int compressing = FALSE;       /* Compress sound: simple 2X */
static int gsmcompress = TRUE;	      /* GSM compress buffers */
#ifdef BYTE_SWAP_DEBUG
static int gsm_byte_order_debug = FALSE; /* Send GSM length field with swapped bytes */
#endif
static int lpccompress = FALSE;       /* LPC compress buffers */
static int lpc10compress = FALSE;     /* LPC-10 compress buffers */
static int adpcmcompress = FALSE;     /* ADPCM compress buffers */
static int celpcompress = FALSE;      /* CELP compress buffers */
static int toasted = FALSE;	      /* Sending already-compressed file ? */
static int robust = 1;		      /* Robust duplicate packets mode */
static int rseq = 0;		      /* Robust mode packet sequence number */

#define DEFAULT_SQUELCH 4096	      /* Default squelch level */
static int squelch = 0; 	      /* Squelch level if > 0 */
static int sqdelay = 12000;	      /* Samples to delay before squelch */
static int sqwait = 0;		      /* Squelch delay countdown */
#ifdef PUSH_TO_TALK
static int push = TRUE; 	      /* Push to talk mode */
static int talking = FALSE;	      /* Push to talk button state */
static int rawmode = FALSE;	      /* Is terminal in raw mode ? */
#endif
static int ring = FALSE;	      /* Force speaker & level on next pkt ? */
static int rtp = FALSE; 	      /* Use Internet Real-Time Protocol */
static int vat = FALSE; 	      /* Use VAT protocol */
static int agc = FALSE; 	      /* Automatic gain control active ? */
static int rgain = 33;		      /* Current recording gain level */
static int spurt = TRUE;	      /* True for first packet of talk spurt */
static int debugging = FALSE;	      /* Debugging enabled here and there ? */
static FILE *audioDumpFile = NULL;    /* Audio dump file, if any */
static int havesound = FALSE;         /* True if we've acquired sound input */
static char hostname[20];	      /* Host name to send with packets */
static int loopback = FALSE;	      /* Remote loopback mode */
static gsm gsmh;		      /* GSM handle */
static struct adpcm_state adpcm = {0, 0}; /* ADPCM compression state */

static char *vatid = NULL;	      /* VAT ID packet */
static int vatidl;		      /* VAT ID packet length */

static unsigned long ssrc;	      /* RTP synchronisation source identifier */
static unsigned long timestamp;       /* RTP packet timestamp */
static unsigned short seq;	      /* RTP packet sequence number */
static unsigned long rtpdesrand;      /* RTP DES random RTCP prefix */

static char *sdes = NULL;	      /* RTP SDES packet */
static int sdesl;		      /* RTP SDES packet length */

static char curkey[9] = "";           /* Current DES key if curkey[0] != 0 */
static char currtpkey[9] = "";        /* Current RTP DES key if currtpkey[0] != 0 */
static char curvatkey[9] = "";        /* Current VAT DES key if currtpkey[0] != 0 */
static char curideakey[17] = "";      /* Current IDEA key if curideakey[0] != 0 */
static char curpgpkey[17] = "";       /* Current PGP key if curpgpkey[0] != 0 */
static char curblowfish_spec = FALSE; /* Nonzero if Blowfish key specified */
#ifdef CRYPTO
static BF_KEY curblowfishkey;	      /* Blowfish key */
#endif
static char curaes_spec = FALSE;      /* Nonzero if AES key specified */
#ifdef CRYPTO
static aes_ctx curaes_ctx;  	      /* AES context */
#endif
static char *curotp = NULL;	      /* Key file buffer */
static int sound_packet;	      /* Ideal samples/packet */
static struct soundbuf *pgpsb = NULL; /* PGP key sound buffer, if any */
static LONG pgpsbl;		      /* Length of PGP key sound buffer data */
#ifdef HALF_DUPLEX
static struct in_addr localhost;      /* Our internet address */
static int halfDuplexMuted = FALSE;   /* Muted by half-duplex transmission */
#endif
static int hasFace = FALSE;	      /* Is a face image available ? */

#ifdef sgi
static long usticks;		      /* Microseconds per clock tick */
#endif

#define TimerStep   (7 * 1000000L)    /* Alarm interval in microseconds */

#ifdef HEWLETT_PACKARD
extern int HPTermHookChar;
#endif

/* Audio input and control device file names. */

#ifdef AUDIO_DEVICE_FILE
extern char *devAudioInput, *devAudioControl;
#endif

#define ucase(x)    (islower(x) ? toupper(x) : (x))

#ifndef USE_CURSES

#ifndef UNIX420
#define UNIX5
#endif

#ifdef UNIX420
#include <sgtty.h>
#endif /* UNIX420 */

#ifdef UNIX5
#include <termio.h>
static struct termio old_term_params;
#endif /* UNIX5 */

/* Set raw mode on terminal file.  Basically, get the terminal into a
   mode in which all characters can be read as they are entered.  CBREAK
   mode is not sufficient.  */

static void tty_rawmode(void)
{
#ifdef UNIX420
   struct sgttyb arg;

   ioctl(fileno(stdin), TIOCGETP, &arg);	  /* get basic parameters */
   arg.sg_flags |= RAW; 	      /* set raw mode */
   arg.sg_flags &= ~ECHO;	      /* clear echo mode */
   ioctl(fileno(stdin), TIOCSETP, &arg);	  /* set it */
#endif /* UNIX420 */

#ifdef UNIX5
   struct termio term_params;

   ioctl(fileno(stdin), TCGETA, &old_term_params);
   term_params = old_term_params;
   term_params.c_iflag &= ~(ICRNL|IXON|IXOFF);	/* no cr translation */
   term_params.c_iflag &= ~(ISTRIP);   /* no stripping of high order bit */
   term_params.c_oflag &= ~(OPOST);    /* no output processing */	
   term_params.c_lflag &= ~(ISIG|ICANON|ECHO); /* raw mode */
   term_params.c_cc[4] = 1;  /* satisfy read after 1 char */
   ioctl(fileno(stdin), TCSETAF, &term_params);
#endif /* UNIX5 */
/*printf("\n(raw)\n");*/
}

/* Restore tty mode */

static void tty_normode(void)
{
#ifdef UNIX420
struct sgttyb arg;

   ioctl(fileno(stdin), TIOCGETP, &arg);	  /* get basic parameters */
   arg.sg_flags &= ~RAW;	      /* clear raw mode */
   arg.sg_flags |= ECHO;	      /* set echo mode */
   ioctl(fileno(stdin), TIOCSETP, &arg);	  /* set it */
#endif /* UNIX420 */

#ifdef UNIX5
   ioctl(fileno(stdin), TCSETAF, &old_term_params);
#endif /* UNIX5 */
/*printf("\n(cooked)\n");*/
}
#endif

/*  ULARM  --  Wrapper for setitimer() that looks like alarm()
	       but accepts a time in microseconds.  */

static void ularm(long t)
{
    struct itimerval it;

    it.it_value.tv_sec = t / 1000000L;
    it.it_value.tv_usec = t % 1000000L;
    it.it_interval.tv_sec = it.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &it, NULL);
}

/*  GSMCOMP  --  Compress the contents of a sound buffer using GSM.  */

static void gsmcomp(struct soundbuf *sb)
{
    gsm_signal src[160];
    gsm_frame dst;
    int i, j, l = 0;
    char *dp = ((char *) sb->buffer.buffer_val) + sizeof(short);

    sb->compression |= fCompGSM;
    for (i = 0; i < sb->buffer.buffer_len; i += 160) {
	for (j = 0; j < 160; j++) {
	    if ((i + j) < sb->buffer.buffer_len) {
		src[j] = audio_u2s(sb->buffer.buffer_val[i + j]);
	    } else {
		src[j] = 0;
	    }
	}
	gsm_encode(gsmh, src, dst);
	bcopy(dst, dp, sizeof dst);
	dp += sizeof dst;
	l += sizeof dst;
    }

    /* Hide original uncompressed buffer length in first 2 bytes of buffer. */
    
    *((short *) sb->buffer.buffer_val) = htons((short) sb->buffer.buffer_len);
#ifdef BYTE_SWAP_DEBUG
    if (gsm_byte_order_debug) {
	unsigned char *bsp = (unsigned char *) sb->buffer.buffer_val;

	bsp[0] = sb->buffer.buffer_len & 0xFF;
	bsp[1] = (sb->buffer.buffer_len >> 8) & 0xFF;
    }
#endif
    sb->buffer.buffer_len = l + sizeof(short);
}

/*  ADPCMCOMP  --  Compress the contents of a sound buffer using ADPCM.  */

static void adpcmcomp(struct soundbuf *sb)
{
    unsigned char *dp = (unsigned char *) sb->buffer.buffer_val;
    struct adpcm_state istate;

    istate = adpcm;
    sb->compression |= fCompADPCM;
    adpcm_coder_u(dp, (char *) dp, sb->buffer.buffer_len, &adpcm);
    sb->buffer.buffer_len /= 2;

    /* Hide the ADPCM encoder state at the end of this buffer.
       The shifting and anding makes the code byte-order
       insensitive. */

    dp += sb->buffer.buffer_len;
    *dp++ = ((unsigned int) istate.valprev) >> 8;
    *dp++ = istate.valprev & 0xFF;
    *dp = istate.index;
    sb->buffer.buffer_len += 3;
}

/*  LPCCOMP  --  Compress the contents of a sound buffer using LPC.  */

static void lpccomp(struct soundbuf *sb)
{
    int i, l = 0;
    char *dp = ((char *) sb->buffer.buffer_val) + sizeof(short);
    unsigned char *src = ((unsigned char *) sb->buffer.buffer_val);
    lpcparams_t lp;

    sb->compression |= fCompLPC;
    for (i = 0; i < sb->buffer.buffer_len; i += LPC_FRAME_SIZE) {
	lpc_analyze(src + i, &lp);
	bcopy(&lp, dp, LPCRECSIZE);
	dp += LPCRECSIZE;
	l += LPCRECSIZE;
    }

    /* Hide original uncompressed buffer length in first 2 bytes of buffer. */

    *((short *) sb->buffer.buffer_val) = htons((short) sb->buffer.buffer_len);
    sb->buffer.buffer_len = l + sizeof(short);
}

/*  LPC10COMP  --  Compress the contents of a sound buffer using LPC-10.  */

static void lpc10comp(struct soundbuf *sb)
{
    unsigned char *dp = (unsigned char *) sb->buffer.buffer_val;
    unsigned char *src = ((unsigned char *) sb->buffer.buffer_val);

    sb->compression |= fCompLPC10;
    sb->buffer.buffer_len = lpc10encode(src, dp, sb->buffer.buffer_len);
}

/*  LPC10STUFF	--  Stuff last 16 bytes of LPC10 packet in sendinghost. */

static int lpc10stuff(struct soundbuf *sb, int pktlen)
{
    if ((sb->compression & fCompLPC10) && (sb->buffer.buffer_len > 16)) {
	bcopy(((char *) sb) + (pktlen - (sizeof sb->sendinghost)),
	    sb->sendinghost, sizeof sb->sendinghost);
	pktlen -= sizeof sb->sendinghost;
    }
    return pktlen;
}

/*  CELPCOMP  --  Compress the contents of a sound buffer using CELP.  */

static void celpcomp(struct soundbuf *sb)
{
    short src[240];
    char celp_frame[18];
    int i, j, l = 0;
    char *dp = ((char *) sb->buffer.buffer_val) + sizeof(short);

    sb->compression |= fCompCELP;
    for (i = 0; i < sb->buffer.buffer_len; i += 240) {
	for (j = 0; j < 240; j++) {
	    if ((i + j) < sb->buffer.buffer_len) {
		src[j] = audio_u2s(sb->buffer.buffer_val[i + j]);
	    } else {
		src[j] = 0;
	    }
	}
	celp_encode(src, celp_frame);
	bcopy(celp_frame, dp, sizeof celp_frame);
	dp += sizeof celp_frame;
	l += sizeof celp_frame;
    }

    /* Hide original uncompressed buffer length in first 2 bytes of buffer. */
    
    *((short *) sb->buffer.buffer_val) = htons((short) sb->buffer.buffer_len);
    sb->buffer.buffer_len = l + sizeof(short);
}

/*  CELPSTUFF	--  Stuff last 16 bytes of CELP packet in sendinghost. */

static int celpstuff(struct soundbuf *sb, int pktlen)
{
    if ((sb->compression & fCompCELP) && (sb->buffer.buffer_len > 16)) {
	bcopy(((char *) sb) + (pktlen - (sizeof sb->sendinghost)),
	    sb->sendinghost, sizeof sb->sendinghost);
	pktlen -= sizeof sb->sendinghost;
    }
    return pktlen;
}

/*  AES_cbc_encrypt  --  Encrypt buffer with AES.  Cipher block
    	    	    	 chaining is done within each buffer.  */
			 
static void AES_cbc_encrypt(unsigned char *in,
			    unsigned char *out,
			    int len, aes_ctx *ctx)
{
    int i, j;
    unsigned char feedback[AES_BLOCK_SIZE];
    
    /*	Initially zero the feedback buffer.  */
    bzero(feedback, AES_BLOCK_SIZE);
    assert((len % AES_BLOCK_SIZE) == 0);  	/* Attempt to encrypt non-multiple of block length */
    
    /*	Loop over encryption blocks in the buffer.  */
    for (i = 0; i < len; i += AES_BLOCK_SIZE) {
    	/*  XOR the next block to be encrypted with the
	    last encrypted block.  */
    	for (j = 0; j < AES_BLOCK_SIZE; j++) {
	    in[j] ^= feedback[j];
	}
	/*  Encrypt the block.  */
	aes_enc_blk(in, out, ctx);
	/*  Save encrypted block in feedback to XOR with next.  */
	bcopy(out, feedback, AES_BLOCK_SIZE);
	/*  Advance input and output pointers to next block.  */
	in += AES_BLOCK_SIZE;
	out += AES_BLOCK_SIZE;
    }
}

/*  SENDRTPCTRL  --  Send RTP or VAT control message, encrypting if
		     necessary.  */

static int sendrtpctrl(struct destination *d, struct sockaddr *destaddr,
		       char *msg, int msgl, int rtppkt)
{
    char aux[1024];

#ifdef CRYPTO
    if ((rtp || vat) && d->rtpdeskey[0]) {
	int vlen;
	des_key_schedule sched;
	des_cblock ivec;

	bzero(ivec, 8);

	if (rtppkt) {

	    /* Encrypted RTCP messages are prefixed with 4 random
	       bytes to prevent known plaintext attacks. */

	    bcopy(&rtpdesrand, aux, 4);
	    bcopy(msg, aux + 4, msgl);
	    msgl += 4;
	} else {
	    bcopy(msg, aux, msgl);
	}

        /* If we're DES encrypting we must round the size of
	   the data to be sent to be a multiple of 8 so that
	   the entire DES frame is sent.  This applies only to
	   VAT, as the code that creates RTCP packets guarantees
           they're already padded to a multiple of 8 bytes. */

	vlen = msgl;
	msgl = (msgl + 7) & (~7);
	if (msgl > vlen) {
	    bzero(aux + vlen, msgl - vlen);
	}
	if (debugging) {
            fprintf(stderr, "Encrypting %d VAT/RTP bytes with DES key.\r\n",
		    msgl);
	}
	des_set_key((des_cblock *) ((rtppkt ? d->rtpdeskey : d->vatdeskey) + 1), sched);
	des_ncbc_encrypt((des_cblock *) aux,
	    (des_cblock *) aux, msgl, sched,
	    (des_cblock *) ivec, DES_ENCRYPT);
	msg = aux;
    }
#endif
    return sendto(
#ifdef NAT_LAUNCH
    	    	  d->csock,
#else
    	    	  sock,
#endif
    	    	   msg, msgl, 0, destaddr, sizeof d->ctrl);
}

/*  ADDEST  --	Add destination host to host list.  */

static int addest(char *host)
{
    struct destination *d;
    struct hostent *h;
    long naddr;
    unsigned int ttl = 1;
    char *mcs;
#ifdef NAT_LAUNCH
    int dataport = Internet_Port;   	    /* Default data port number */
    int ctrlport = Internet_Port + 1;	    /* Default control port number */
#else
    int curport = Internet_Port;
#endif

    /* If a multicast scope descriptor appears in the name, scan
       it and lop it off the end.  We'll apply it later if we discover
       this is actually a multicast address. */

    if ((mcs = strrchr(host, '/')) != NULL &&
        ((strchr(host, ':') != NULL) || (mcs != strchr(host, '/')))) {
	*mcs = 0;
#ifdef MULTICAST
	ttl = atoi(mcs);
#endif
    }

    /* If a port number appears in the name, scan it and lop
       it off.	We allow a slash as well as the documented colon
       as the delimiter to avoid confusing users familiar with
       VAT. */

    if ((mcs = strrchr(host, ':')) != NULL ||
        (mcs = strrchr(host, '/')) != NULL) {
	*mcs++ = 0;
#ifdef NAT_LAUNCH
	dataport = atoi(mcs);
	/* The control port can be specified independently of
	   the data port, separated by a comma. */
        if ((mcs = strrchr(host, ',')) != NULL) {
            ctrlport = atoi(mcs);
        } else {
            ctrlport = dataport + 1;
	}
#else
	curport = atoi(mcs);
#endif
    }

    /* If it's a valid IP number, use it.  Otherwise try to look
       up as a host name. */

    if ((naddr = inet_addr(host)) != -1) {
    } else {
	h = gethostbyname(host);
	if (h == 0) {
            fprintf(stderr, "%s: unknown host\n", host);
	    return FALSE;
	}
	bcopy((char *) h->h_addr, (char *) &naddr, sizeof naddr);
    }

#ifdef MULTICAST
    if (!IN_MULTICAST(naddr)) {
	ttl = 0;
    }
#endif

    d = (struct destination *) malloc(sizeof(struct destination));
    d->dnext = NULL;
    d->server = host;
    bcopy((char *) &naddr, (char *) &(d->name.sin_addr), sizeof naddr);
    bcopy((char *) &naddr, (char *) &(d->ctrl.sin_addr), sizeof naddr);
    bcopy(curkey, d->deskey, 9);
    bcopy(currtpkey, d->rtpdeskey, 9);
    bcopy(curvatkey, d->vatdeskey, 9);
    bcopy(curideakey, d->ideakey, 17);
    bcopy(curpgpkey, d->pgpkey, 17);
    d->blowfish_spec = curblowfish_spec;
#ifdef CRYPTO
    bcopy(&curblowfishkey, &(d->blowfishkey), sizeof(BF_KEY));
#endif
    d->aes_spec = curaes_spec;
#ifdef CRYPTO
    bcopy(&curaes_ctx, &(d->aes_ctx), sizeof curaes_ctx);
#endif
    d->otp = curotp;
    d->time_to_live = ttl;
    d->name.sin_family = AF_INET;
#ifdef NAT_LAUNCH
    d->name.sin_port = htons(dataport);
#else
    d->name.sin_port = htons(curport);
#endif
    d->ctrl.sin_family = AF_INET;
#ifdef NAT_LAUNCH
    d->ctrl.sin_port = htons(ctrlport);
#else
    d->ctrl.sin_port = htons(curport + 1);
#endif
    if (dests == NULL) {
	dests = d;
    } else {
	dtail->dnext = d;
    }
    dtail = d;
#ifdef NAT_LAUNCH
    d->dsock = (xnat_socketsharing ? xnat_datasock : sock);
    d->csock = (xnat_socketsharing ? xnat_consock : sock);
#endif

    /* Send initial status and identity message in
       the correct protocol. */

    if (rtp) {
	if (sendrtpctrl(d, (struct sockaddr *) &(d->ctrl),
			sdes, sdesl, TRUE) < 0) {
            perror("sending initial RTCP SDES packet");
	    return FALSE;
	}
    } else if (vat) {
	if (sendrtpctrl(d, (struct sockaddr *) &(d->ctrl),
		       vatid, vatidl, FALSE) < 0) { 
            perror("sending initial VAT ID control packet");
	    return FALSE;
	}
    } else {
	char pid = sdes[0];

	/* Set Speak Freely protocol flag in packet */
	sdes[0] = (sdes[0] & 0x3F) | (1 << 6);
	if (sendrtpctrl(d, (struct sockaddr *) &(d->ctrl),
			sdes, sdesl, TRUE) < 0) {
            perror("sending initial Speak Freely SDES control packet");
	    return FALSE;
	}
	sdes[0] = pid;
    }

#ifdef CRYPTO
    if (pgpsb != NULL) {
	int i;

        /* "What I tell you three times is true".  When we change
	   over to a TCP connection bracketing the UDP sound data
	   transmission, we can send this just once, knowing it has
	   arrived safely. */

	for (i = 0; i < 3; i++) {
	    if (sendto(
#ifdef NAT_LAUNCH
    	    	 d->dsock,
#else
	    	 sock,
#endif
		(char *) pgpsb, (sizeof(struct soundbuf) - BUFL) + pgpsbl,
		0, (struct sockaddr *) &(d->name), sizeof d->name) < 0) {
                perror("sending public key encrypted session key");
		break;
	    }
	}
/* No need to sleep once ack from far end confirms key decoded. */
sleep(7);
    }
#endif
    return TRUE;
}

/*  TIMERTICK  --  Timer tick signal-catching function.  */

static void timertick()
{
    struct destination *d;

    for (d = dests; d != NULL; d = d->dnext) {
#ifdef MULTICAST
	if (IN_MULTICAST(d->name.sin_addr.s_addr)) {
	    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
		(char *) &(d->time_to_live), sizeof d->time_to_live);
	    if (debugging) {
                fprintf(stderr, "Multicasting ID/SDES with scope of %d to %s.\n",
		    d->time_to_live, inet_ntoa(d->name.sin_addr));
	    }
	}
#endif
	if (rtp) {
	    if (sendrtpctrl(d, (struct sockaddr *) &(d->ctrl),
			 sdes, sdesl, TRUE) < 0) {
                perror("sending RTCP SDES packet");
	    }
	} else if (vat) {
	    if (sendrtpctrl(d, (struct sockaddr *) &(d->ctrl),
			vatid, vatidl, FALSE) < 0) { 
                perror("sending VAT ID control packet");
	    }
	} else {
	    char pid = sdes[0];

	    /* Set Speak Freely protocol flag in packet */
	    sdes[0] = (sdes[0] & 0x3F) | (1 << 6);
	    if (sendrtpctrl(d, (struct sockaddr *) &(d->ctrl),
			    sdes, sdesl, TRUE) < 0) {
                perror("sending Speak Freely SDES control packet");
	    }
	    sdes[0] = pid;
	}
    }

    ularm(TimerStep);
    signal(SIGALRM, timertick);       /* Reset signal to handle timeout */
}

/*  SENDPKT  --  Send a message to all active destinations.  */

static int sendpkt(struct soundbuf *sb)
{
    struct destination *d;
    int pktlen;

    if (gsmcompress && !toasted) {
	gsmcomp(sb);
    }

    if (adpcmcompress && !toasted) {
	adpcmcomp(sb);
    }

    if (lpccompress && !toasted) {
	lpccomp(sb);
    }

    if (lpc10compress && !toasted) {
	lpc10comp(sb);
    }
    
    if (celpcompress && !toasted) {
    	celpcomp(sb);
    }

    if (hasFace) {
	sb->compression |= fFaceOffer;
    }

    pktlen = sb->buffer.buffer_len + (sizeof(struct soundbuf) - BUFL);

    if (vat) {
	pktlen = vatout(sb, 0L, timestamp, spurt);
	timestamp += sound_packet;
    }

    if (rtp) {
	pktlen = rtpout(sb, ssrc, timestamp, seq, spurt);
	seq++;
	timestamp += sound_packet;
    }

    spurt = FALSE;		      /* Not the start of a talk spurt */

    for (d = dests; d != NULL; d = d->dnext) {
#ifdef MULTICAST
	if (IN_MULTICAST(d->name.sin_addr.s_addr)) {
	    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
		(char *) &(d->time_to_live), sizeof d->time_to_live);
	    if (debugging) {
                fprintf(stderr, "Multicasting with scope of %d to %s.\n",
		    d->time_to_live, inet_ntoa(d->name.sin_addr));
	    }
	}
#endif

#ifdef CRYPTO
	if (d->deskey[0] || d->ideakey[0] || d->blowfish_spec ||
	    d->aes_spec || d->pgpkey[0] || d->otp != NULL) {
	    soundbuf ebuf;
	    int i;
	    LONG slen;

	    bcopy(sb, &ebuf, pktlen);
	    slen = ebuf.buffer.buffer_len;

	    /* DES encryption. */

	    if (d->deskey[0]) {
		if (rtp || vat) {
		    int vlen = pktlen;
		    des_key_schedule sched;
		    des_cblock ivec;

                    /* If we're DES encrypting we must round the size of
		       the data to be sent to be a multiple of 8 so that
		       the entire DES frame is sent.  If this is an RTP
		       packet, we may have to set the Pad bit in the
		       header and include a count of pad bytes at the end
		       of the packet. */

		    bzero(ivec, 8);
		    pktlen = (pktlen + 7) & (~7);
		    if (debugging) {
                        fprintf(stderr, "Encrypting %d VAT/RTP bytes with DES key.\r\n",
				pktlen);
		    }
		    if (pktlen > vlen) {
			bzero(((char *) &ebuf) + vlen, pktlen - vlen);
		    }
		    if (rtp && (pktlen > vlen)) {
			char *p = (char *) &ebuf;

			p[0] |= 0x20; /* Set pad bytes present bit */
			p[pktlen - 1] = pktlen - vlen; /* Set pad count at end */
		    }
		    des_set_key((des_cblock *) ((rtp ? d->rtpdeskey : d->vatdeskey) + 1), sched);
		    des_ncbc_encrypt((des_cblock *) &ebuf,
			(des_cblock *) &ebuf, pktlen, sched,
			(des_cblock *) ivec, DES_ENCRYPT);
		    if (vat) {
			pktlen = vlen;
		    }
		} else {
		    setkey_sf(d->deskey + 1);

                    /* If we're DES encrypting we must round the size of
		       the data to be sent to be a multiple of 8 so that
		       the entire DES frame is sent. */

		    slen = (slen + 7) & (~7);
		    pktlen = slen + (sizeof(struct soundbuf) - BUFL);
		    if (debugging) {
                        fprintf(stderr, "Encrypting %ld bytes with DES key.\r\n", slen);
		    }
		    for (i = 0; i < slen; i += 8) {

			/* Apply cipher block chaining within the packet. */

			if (i > 0) {
			    int j;

			    for (j = 0; j < 8; j++) {
				ebuf.buffer.buffer_val[(i + j)] ^=
				    ebuf.buffer.buffer_val[(i + j) - 8];
			    }
			}
			endes(ebuf.buffer.buffer_val + i);
		    }
		    ebuf.compression |= fEncDES;
		}
	    }

	    /* IDEA encryption. */

	    if (d->ideakey[0]) {
		unsigned short iv[4];

		bzero(iv, sizeof(iv));
		initcfb_idea(iv, (unsigned char *) (d->ideakey + 1), FALSE);

                /* If we're IDEA encrypting we must round the size of
		   the data to be sent to be a multiple of 8 so that
		   the entire IDEA frame is sent. */

		slen = (slen + 7) & (~7);
		pktlen = slen + (sizeof(struct soundbuf) - BUFL);
		if (debugging) {
                    fprintf(stderr, "Encrypting %ld bytes with IDEA key.\r\n", slen);
		}
		ideacfb((unsigned char *) ebuf.buffer.buffer_val, slen);
		close_idea();
		ebuf.compression |= fEncIDEA;
	    }

	    /* Blowfish encryption. */

	    if (d->blowfish_spec) {
		unsigned char iv[8];

		bzero(iv, sizeof(iv));

                /* If we're Blowfish encrypting we must round the size of
		   the data to be sent to be a multiple of 8 so that
		   the entire Blowfish frame is sent. */

		slen = (slen + 7) & (~7);
		pktlen = slen + (sizeof(struct soundbuf) - BUFL);
		if (debugging) {
                    fprintf(stderr, "Encrypting %ld bytes with Blowfish key.\r\n", slen);
		}
		BF_cbc_encrypt((unsigned char *) ebuf.buffer.buffer_val,
			       (unsigned char *) ebuf.buffer.buffer_val,
			       slen, &(d->blowfishkey), iv, BF_ENCRYPT);
		ebuf.compression |= fEncBF;
	    }

	    /* AES encryption. */

	    if (d->aes_spec) {
		unsigned char iv[8];

		bzero(iv, sizeof(iv));

                /* If we're AES encrypting we must round the size of
		   the data to be sent to be a multiple of 16 so that
		   the entire AES block is sent. */

		slen = (slen + 15) & (~15);
		pktlen = slen + (sizeof(struct soundbuf) - BUFL);
		if (debugging) {
                    fprintf(stderr, "Encrypting %ld bytes with AES key.\r\n", slen);
		}
		AES_cbc_encrypt((unsigned char *) ebuf.buffer.buffer_val,
			        (unsigned char *) ebuf.buffer.buffer_val,
			        slen, &(d->aes_ctx));
		ebuf.compression |= fEncAES;
	    }

	    /* PGP encryption. */

	    if (d->pgpkey[0]) {
		unsigned short iv[4];

		bzero(iv, sizeof(iv));
		initcfb_idea(iv, (unsigned char *) (d->pgpkey + 1), FALSE);

                /* If we're PGP IDEA encrypting we must round the size of
		   the data to be sent to be a multiple of 8 so that
		   the entire IDEA frame is sent. */

		slen = (slen + 7) & (~7);
		pktlen = slen + (sizeof(struct soundbuf) - BUFL);
		if (debugging) {
                    fprintf(stderr, "Encrypting %ld bytes with public-key session key.\r\n", slen);
		}
		ideacfb((unsigned char *) ebuf.buffer.buffer_val, slen);
		close_idea();
		ebuf.compression |= fEncPGP;
	    }

	    /* Key file encryption. */

	    if (d->otp != NULL) {
		if (debugging) {
                    fprintf(stderr, "Encrypting %ld bytes with key file.\r\n", slen);
		}
		for (i = 0; i < slen; i ++) {
		    ebuf.buffer.buffer_val[i] ^= d->otp[i];
		}
		ebuf.compression |= fEncOTP;
	    }

	    if (!rtp && !vat) {
	    	/*  For LPC10 and CELP, "stuff" the last 16 bytes of the audio data
		    into the obsolete "hostname" field.  Packets compressed with
		    these codecs are sufficiently small that this is a substantial
		    optimisation.  */
		if (lpc10compress && !toasted) {
		    pktlen = lpc10stuff(&ebuf, pktlen);
		}
		if (celpcompress && !toasted) {
		    pktlen = celpstuff(&ebuf, pktlen);
		}

		/*  If we're sending in Speak Freely protocol and robust transmission
	    	    if enabled, include a packet sequence number, modulo 256, in the
		    most significant 8 bits of the buffer_len field.  This is used
		    on the receiving end to discard duplicates and re-order shuffled
		    packets. The fCompRobust flag is set in the compression field to
		    inform the receiver it should examine the sequence number.  */
		    
		if (robust > 1) {
		    ebuf.compression |= fCompRobust;
    	    	    ebuf.buffer.buffer_len |= rseq << 24;
		    rseq = (rseq + 1) & 0xFF;
		}
		ebuf.compression = htonl(ebuf.compression);
		ebuf.buffer.buffer_len = htonl(ebuf.buffer.buffer_len);
	    }
	    
	    /*	Transmit the Speak Freely protocol packet.  We dispatch as many
	    	identical copies of it as indicated by "robust".  */
	    
	    for (i = 0; i < robust; i++) {
		if (sendto(
#ifdef NAT_LAUNCH
    	    	    d->dsock,
#else
		    sock,
#endif
		    (char *) &ebuf, pktlen,
		    0, (struct sockaddr *) &(d->name), sizeof d->name) < 0) {
                    perror("sending encrypted audio");
		    return FALSE;
		}
	    }
	} else
#endif
	{
	    int i, wasf = FALSE;

	    if (!vat && !rtp) {
		wasf = TRUE;
		if (lpc10compress && !toasted) {
		    pktlen = lpc10stuff(sb, pktlen);
		}
		if (celpcompress && !toasted) {
		    pktlen = celpstuff(sb, pktlen);
		}
		if (robust > 1) {
		    sb->compression |= fCompRobust;
		    sb->buffer.buffer_len |= ((long) rseq) << 24;
		    rseq = (rseq + 1) & 0xFF;
		}
		sb->compression = htonl(sb->compression);
		sb->buffer.buffer_len = htonl(sb->buffer.buffer_len);
	    }

	    for (i = 0; i < robust; i++) {
		if (sendto(
#ifdef NAT_LAUNCH
    	    	    d->dsock,
#else		
		    sock,
#endif
		    (char *) sb, pktlen,
		    0, (struct sockaddr *) &(d->name), sizeof d->name) < 0) {
                    perror("sending unencrypted audio");
		    return FALSE;
		}
	    }
	    if (wasf) {
		sb->compression = ntohl(sb->compression);
		sb->buffer.buffer_len = ntohl(sb->buffer.buffer_len);
		if (sb->compression & fCompRobust) {
		    sb->compression &= ~fCompRobust;
		    sb->buffer.buffer_len &= 0xFFFFFFL;
		}
		if ((sb->compression & fCompLPC10) &&
		    (sb->buffer.buffer_len > 16)) {
		    pktlen += sizeof(sb->sendinghost);
		}
		/* NOTE--If you implement support for CELP in RTP, you'll need to add
		   similar un-stuffing of the packet for CELP here. */
	    }
	}
    }
    return TRUE;
}

/*  GETAUDIO  --  Open audio input.  If audio hardware is half duplex,
		  we may have to mute output in progress.  */

static int getaudio(void)
{
    int i;

#ifdef HALF_DUPLEX
    struct soundbuf hdreq;
    struct sockaddr_in name;

    /* If we're afflicted with half-duplex hardware, make several
       attempts to mute the output device and open audio input.
       We send a mute even if the output isn't active as a courtesy
       to the output program; if a later attempt to acquire output
       fails, he'll know it was a result of being muted. */

    for (i = 5; i > 0; i--) {
	hdreq.compression = htonl(fProtocol | fHalfDuplex | fHalfDuplexMute);
	strcpy(hdreq.sendinghost, hostname);
	hdreq.buffer.buffer_len = 0;

	bcopy((char *) &localhost, (char *) &(name.sin_addr), sizeof localhost);
	name.sin_family = AF_INET;
	name.sin_port = htons(Internet_Port);
	if (debugging) {
            fprintf(stderr,"Sending fHalfDuplex | fHalfDuplexMute Request\n");
	}
	if (sendto(sock, &hdreq, (sizeof(struct soundbuf) - BUFL),
		   0, (struct sockaddr *) &name, sizeof name) < 0) {
            perror("sending half-duplex mute request");
	}
	sf_usleep(50000L);
	if (soundinit(O_RDONLY)) {
	    halfDuplexMuted = TRUE;
	    break;
	}
    }

    /* If we failed to initialise, send a resume just in case
       one of our mute requests made it through anyway. */

    if (i <= 0) {
	hdreq.compression = htonl(fProtocol | fHalfDuplex | fHalfDuplexResume);
	if (sendto(sock, &hdreq, (sizeof(struct soundbuf) - BUFL),
		   0, (struct sockaddr *) &name, sizeof name) < 0) {
            perror("sending half-duplex resume request");
	}
    }
#else
    i = soundinit(O_RDONLY);
#endif

    if (i <= 0) {
        fprintf(stderr, "%s: unable to initialise audio.\n", progname);
	return FALSE;
    }
    havesound = TRUE;
    if (agc) {
	soundrecgain(rgain);	  /* Set initial record level */
    }
    return TRUE;
}

/*  FREEAUDIO  --  Release sound input device, sending a resume to
                   the output device if we've muted it due to
		   half-duplex hardware.  */

static void freeaudio(void)
{
    if (havesound) {
	if (debugging) {
            fprintf(stderr, "Restoring audio modes at release.\n");
	}
	soundterm();
	havesound = FALSE;
    }

#ifdef HALF_DUPLEX

    /* When bailing out, make sure we don't leave the output
       muted. */

    if (halfDuplexMuted) {
	struct soundbuf hdreq;
	struct sockaddr_in name;

	hdreq.compression = htonl(fProtocol | fHalfDuplex | fHalfDuplexResume);
	strcpy(hdreq.sendinghost, hostname);
	hdreq.buffer.buffer_len = 0;

	bcopy((char *) &localhost, (char *) &(name.sin_addr), sizeof localhost);
	name.sin_family = AF_INET;
	name.sin_port = htons(Internet_Port);
	if (sendto(sock, &hdreq, (sizeof(struct soundbuf) - BUFL),
		   0, (struct sockaddr *) &name, sizeof name) < 0) {
            perror("sending half-duplex resume request");
	}
	halfDuplexMuted = FALSE;
    }
#endif
}

/*  EXITING  --  Catch as many program termination signals as
		 possible and restore initial audio modes before
		 we exit.  */

static void exiting()
{
    struct destination *d;

    freeaudio();

    if (audioDumpFile != NULL) {
	fclose(audioDumpFile);
    }

#ifdef PUSH_TO_TALK
    if (rawmode) {
        fprintf(stderr, "\r      \r");
#ifdef USE_CURSES
	nocbreak();
	echo();
	endwin();
	fcntl(fileno(stdin), F_SETFL, 0);
#else
	tty_normode();
	fcntl(fileno(stdin), F_SETFL, 0);
#endif
    }
#endif

    if (rtp) {
	for (d = dests; d != NULL; d = d->dnext) {
	    char v[1024];
	    int l;

            l = rtp_make_bye((unsigned char *) v, ssrc, "Exiting Speak Freely", TRUE);
	    if (sendrtpctrl(d, (struct sockaddr *) &(d->ctrl),
			 v, l, TRUE) < 0) {
                perror("sending RTCP BYE packet");
	    }
	}
    } else if (vat) {
	for (d = dests; d != NULL; d = d->dnext) {
	    char v[16];
	    int l;

	    l = makevatdone(v, 0L);
	    if (sendrtpctrl(d, (struct sockaddr *) &(d->ctrl),
			 v, l, FALSE) < 0) {
                perror("sending VAT DONE control packet");
	    }
	}
    } else {
	for (d = dests; d != NULL; d = d->dnext) {
	    char v[1024];
	    int l;

            l = rtp_make_bye((unsigned char *) v, ssrc, "Exiting Speak Freely", TRUE);
	    v[0] = (v[0] & 0x3F) | (1 << 6);
	    if (sendrtpctrl(d, (struct sockaddr *) &(d->ctrl),
			 v, l, TRUE) < 0) {
                perror("sending Speak Freely BYE packet");
	    }
	}
    }

    exit(0);
}

/*  CHANGEMODE	--  Reset state when talk/pause/quiet mode changes.  */

static void changemode(void)
{
    if (talking) {
	spurt = TRUE;
	/* Discard all backlog sound input. */
	soundflush();
    } else {
	sqwait = 0;		      /* Reset squelch delay */
    }
}

/*  TERMCHAR  --  Check for special characters from console when
		  in raw mode.	*/

static void termchar(int ch)	
{
#define Ctrl(x) ((x) - '@')
    if (ch == 'q' || ch == 'Q' || ch == 27 ||
        ch == Ctrl('C') || ch == Ctrl('D')) {
	exiting();
    }
}

/*  CHATCHAR  --  Test for text chat request character.  */

static int chatchar(int ch)
{
    if ((!rtp && !vat) && ch == '.') {
	char s[133];

	if (rawmode) {
#ifdef USE_CURSES
	    nocbreak();
	    echo();
	    endwin();
#else
	    tty_normode();
#endif
	}
	fcntl(fileno(stdin), F_SETFL, 0);
	if (push) {
	    talking = 0;
	}

        fprintf(stderr, "\rChat:  ");
	while (TRUE) {
	    if (fgets(s, (sizeof s) - 1, stdin) != NULL) {
		struct destination *d;

		while (strlen(s) > 0 && isspace(s[strlen(s) - 1])) {
		    s[strlen(s) - 1] = 0;
		}
		for (d = dests; d != NULL; d = d->dnext) {
		    char v[256];
		    int l;

		    l = rtp_make_app((unsigned char *) v, ssrc, TRUE, RTCP_APP_TEXT_CHAT, s);
		    /* Set Speak Freely protocol flag in packet */
		    v[0] = (v[0] & 0x3F) | (1 << 6);
		    if (sendrtpctrl(d, (struct sockaddr *) &(d->ctrl),
				 v, l, TRUE) < 0) {
                        perror("sending text chat packet");
		    }
		}
		fprintf(stderr, talking ? (squelch > 0 ?
                        "\rQuiet: " : "\rTalk:   ") : "\rPause: ");

		if (rawmode) {
#ifdef USE_CURSES
		    initscr();
		    noecho();
		    cbreak();
#else
		    tty_rawmode();
#endif
		}
		changemode();
		break;
	    }
	}
	fcntl(fileno(stdin), F_SETFL, talking ? O_NDELAY : 0);
	return TRUE;
    }
    return FALSE;
}

/*  SENDFILE  --  Send a file or, if the file name is NULL or a
		  single period, send real-time sound input. */

static int sendfile(char *f)
{ 
    soundbuf netbuf;
#define buf netbuf.buffer.buffer_val
    int bread, nread, lread;
    FILE *afile = NULL;
    static int firstTime = TRUE;

    if (firstTime) {
	firstTime = FALSE;
	signal(SIGHUP, exiting);     /* Set signal to handle termination */
	signal(SIGINT, exiting);     /* Set signal to handle termination */
	signal(SIGTERM, exiting);    /* Set signal to handle termination */
	signal(SIGALRM, timertick);  /* Set signal to handle timer */
	ularm(TimerStep);
    }

    /* Compute the number of sound samples needed to fill a
       packet of TINY_PACKETS bytes. */

    if (rtp) {
	sound_packet = (gsmcompress | lpccompress | adpcmcompress) ? (160 * 4)
		       : 320;
    } else if (vat) {
	sound_packet = (gsmcompress | lpccompress | adpcmcompress) ? (160 * 4)
		       : 320;
    } else {
	sound_packet = ((TINY_PACKETS - ((sizeof(soundbuf) - BUFL))) *
			(compressing ? 2 : 1));
	if (gsmcompress) {
	    sound_packet = compressing ? 3200 : 1600;
	} else if (adpcmcompress) {
	    sound_packet *= 2;
	    sound_packet -= 4;		  /* Leave room for state at the end */
	} else if (lpccompress) {
	    sound_packet = (compressing ? 2 : 1) * (10 * LPC_FRAME_SIZE);
	} else if (lpc10compress) {
	    sound_packet = compressing ? 3600 : 1800;
	} else if (celpcompress) {
	    sound_packet = compressing ? 3840 : 1920;
	}
    }

#ifdef SHOW_PACKET_SIZE
    printf("Samples per packet = %d\n", sound_packet);
#endif
    lread = sound_packet;

    strcpy(netbuf.sendinghost, hostname);
    if (f != NULL && (strcmp(f, ".") != 0)) {
	char magic[4];

        afile = fopen(f, "r");
	if (afile == NULL) {
            fprintf(stderr, "Unable to open sound file %s.\n", f);
	    return 2;
	}

	toasted = FALSE;
        if ((strlen(f) > 4) && (strcmp(f + (strlen(f) - 4), ".gsm") == 0)) {
	    toasted = TRUE;
	    lread = 33 * ((rtp || vat) ? 4 : 10);
	    sound_packet = (160 * lread) / 33;
	} else {

	    /* If the file has a Sun .au file header, skip it.
	       Note that we still blithely assume the file is
	       8-bit ISDN u-law encoded at 8000 samples per
	       second. */

	    fread(magic, 1, sizeof(long), afile);
            if (bcmp(magic, ".snd", 4) == 0) {
		long startpos;

		fread(&startpos, sizeof(long), 1, afile);
		fseek(afile, ntohl(startpos), 0);
	    } else {
		fseek(afile, 0L, 0);
	    }
	}
    }

    /* Send a file */

    if (afile) {
	int tlast = FALSE;
#ifdef BSD_like
	struct timeb t1, t2, tl;
#else
	struct timeval t1, t2, tl;
#endif
	long et, corr = 0;

	while (
#ifdef BSD_like
	    ftime(&t1),
#else
	    gettimeofday(&t1, NULL),
#endif

	    (bread = nread =
		fread(buf + (toasted ? 2 : 0), 1,
				  lread,
				  afile)) > 0) {
	    netbuf.compression = fProtocol | (ring ? (fSetDest | fDestSpkr) : 0);
	    ring = FALSE;
	    netbuf.compression |= debugging ? fDebug : 0;
	    netbuf.compression |= loopback ? fLoopBack : 0;
	    if (toasted) {
		*((short *) netbuf.buffer.buffer_val) = htons((short)(bread = (160 * nread) / 33));
		netbuf.buffer.buffer_len = lread + sizeof(short);
		netbuf.compression |= fCompGSM;
	    } else {
		if (compressing) {
		    int is = nread, os = nread / 2;

		    rate_flow((unsigned char *) buf, (unsigned char *) buf, &is, &os);
		    nread = os;
		    netbuf.compression |= fComp2X;
		}
		netbuf.buffer.buffer_len = nread;
	    }
	    if (!sendpkt(&netbuf)) {
		fclose(afile);
		toasted = FALSE;
		exiting();
	    }

            /* The following code is needed because when we're reading
	       sound from a file, as opposed to receiving it in real
	       time from the CODEC, we must meter out the samples
	       at the rate they will actually be played by the destination
	       machine.  For a 8000 samples per second, this amounts
	       to 125 microseconds per sample, minus the time we spent
	       compressing the data (which is substantial for GSM) and
	       encrypting the data.

               What we're trying to accomplish here is to maintain a
	       predictable time between packets equal to the sample
	       rate.  This is, of course, very difficult since system
	       performance and instantaneous load changes the relationship
	       between the compute time per packet and the length of
	       the sound it represents.  What the code below does is
	       measure the actual time between packets, compare it to
	       the desired time, then use the error signal as input
	       to an exponentially smoothed moving average with P=0.1
	       which adjusts the time, in microseconds, we wait between
	       packets.  This quickly adapts to the correct wait time
	       based on system performance and options selected, while
	       minimsing perturbations due to momentary changes in
	       system load.  Due to incompatibilities between System V
	       and BSD-style gettimeofday(), we do this differently
	       for the two styles of system.  If the system conforms
	       to neither convention, we just guess based on the number
	       of samples in the packet and a fudge factor.
	       
	       If you set OVERDRIVE_SOUND_FILE to a value other than
	       the default of 1 (the value may be a floating point number),
	       the rate at which sound is sent will be adjusted by the
	       the given factor.  A setting of 2, for example, will dispatch
	       packets twice as fast as the time computed to play them.  This
	       comes in handy when debugging adaptive rate adjustment intended
	       to avoid long delays when a continuously transmitting site
	       is sending faster than the receiver is playing the audio.  */

/* #define OVERDRIVE_SOUND_FILE	1.1 */

#define MICROSECONDS_PER_SAMPLE (1000000 / 8000)
#ifdef OVERDRIVE_SOUND_FILE
#define DELAY_PER_SAMPLE ((int) ((MICROSECONDS_PER_SAMPLE / OVERDRIVE_SOUND_FILE)))
#else
#define DELAY_PER_SAMPLE MICROSECONDS_PER_SAMPLE
#endif
#define kOverhead   8000

#ifdef BSD_like
	    ftime(&t2);
	    if (tlast) {
		long dt =  1000 * (((t2.time - tl.time) * 1000) +
				   (t2.millitm - tl.millitm)),
			   atime = bread * DELAY_PER_SAMPLE, error;
		error = atime - dt;
		if (error > atime / 2) {
		    error = atime / 2;
		} else if (error < -(atime / 2)) {
		    error = -(atime / 2);
		}
		corr = corr - 0.1 * error;
		et = atime - corr;
#ifdef DEBUG_SOUNDFILE_WAIT
printf("Packet time %d Delta t = %d Error = %d, Corr = %d, Wait = %d\n",
	atime, dt, error, corr, et);
#endif
	    } else {
#ifdef __FreeBSD__
		corr = 80000;
#else
		corr = 8000;
#endif
		et = ((bread * DELAY_PER_SAMPLE) -
		      1000 * (((t2.time - t1.time) * 1000) +
			       (t2.millitm - t1.millitm))) - kOverhead;
	    }
	    tl = t2;
	    tlast = TRUE;
#else
	    gettimeofday(&t2, NULL);
	    if (tlast) {
		long dt = (t2.tv_sec - tl.tv_sec) * 1000000 +
				(t2.tv_usec - tl.tv_usec),
		     atime = bread * DELAY_PER_SAMPLE, error;
		error = atime - dt;
		if (error > atime / 2) {
		    error = atime / 2;
		} else if (error < -(atime / 2)) {
		    error = -(atime / 2);
		}
		corr = corr - 0.1 * error;
		et = atime - corr;
#ifdef DEBUG_SOUNDFILE_WAIT
printf("Packet time %d Delta t = %d Error = %d, Corr = %d, Wait = %d\n",
	atime, dt, error, corr, et);
#endif
	    } else {
		et = ((bread * DELAY_PER_SAMPLE) -
			      (t2.tv_sec - t1.tv_sec) * 1000000 +
			      (t2.tv_usec - t1.tv_usec)) - kOverhead;
	    }
	    tl = t2;
	    tlast = TRUE;
#endif

	    if (et > 0) {
		sf_usleep(et);
	    }
	    if (debugging && !vat && !rtp) {
                fprintf(stderr, "Sent %d samples from %s in %ld bytes.\r\n",
			bread, f, netbuf.buffer.buffer_len);
	    }
	}

	if (debugging) {
            fprintf(stderr, "Sent sound file %s.\r\n", f);
	}
	fclose(afile);
	toasted = FALSE;
    } else {

	    /* Send real-time audio. */

#ifdef PUSH_TO_TALK
	if (push) {
	    char c;
	    int l;

	    fprintf(stderr,
                "Space bar switches talk/pause, Esc or \"q\" to quit, \".\" for chat text\nPause: ");
	    fflush(stderr);
#ifdef USE_CURSES
	    initscr();
	    noecho();
	    cbreak();
#else
	    tty_rawmode();
#endif
	    rawmode = TRUE;
	    while (TRUE) {
		while (TRUE) {
		    if (((l = read(fileno(stdin), &c, 1)) == 1) || (errno != EINTR)) {
			break;
		    }
		}   
		if (l == 1 && !chatchar(c)) {
		    break;
		}
	    }
	    if (l != 1) {
                perror("waiting for first Pause/Talk character");
	    }
	    termchar(c);
            fprintf(stderr, squelch > 0 ? "\rQuiet:  " : "\rTalk:   ");
	    fcntl(fileno(stdin), F_SETFL, O_NDELAY);
	    talking = TRUE;
	    spurt = TRUE;
	}
#endif

	/* Send real-time sound. */

#ifdef Solaris
	setaubufsize(sound_packet);
#endif

	if (!getaudio()) {
            fprintf(stderr, "Unable to initialise audio.\n");
	    return 2;
	}
	changemode();
	while (TRUE) {
	    int soundel = 0;
	    unsigned char *bs = (unsigned char *) buf;

	    if (havesound) {

                /* Obtain a packet's worth of real-time audio from
		   the audio input hardware. */

		soundel = soundgrab(buf, sound_packet);

		/* If a raw audio dump file (-w option) is specified,
		   dump whatever we got from the audio input to the
		   designated file.  This can come in handy when
                   debugging audio drivers which don't provide samples
		   in the expected 8 bit mu-law mode. */

		if ((audioDumpFile != NULL) && (soundel > 0)) {
		    fwrite(buf, 1, soundel, audioDumpFile);
		}
	    }

#ifdef PUSH_TO_TALK
	    while (push) {
		char c;
		int rlen;

		if ((rlen = read(fileno(stdin), &c, 1)) > 0
#ifdef HEWLETT_PACKARD
		    || HPTermHookChar
#endif
		   ) {
#ifdef HEWLETT_PACKARD
		    c = HPTermHookChar;
#endif
		    if (rlen > 0 && chatchar(c)) {
			continue;
		    }
		    termchar(c);
		    talking = !talking;
		    fflush(stderr);
#ifdef HALF_DUPLEX

		    /* For half-duplex, acquire and release the
		       audio device at each transition.  This lets
		       us mute the output only while in Talk mode. */

		    if (talking) {
			if (!getaudio()) {
                            fprintf(stderr, "Audio device busy.\n");
			    talking = FALSE;
			}
		    } else {
			freeaudio();
		    }
#endif
		    fprintf(stderr, talking ? (squelch > 0 ?
                            "\rQuiet: " : "\rTalk:   ") : "\rPause: ");
		    fcntl(fileno(stdin), F_SETFL, talking ? O_NDELAY : 0);
		    changemode();
		    break;
		} else {
		    if (rlen == -1 && errno == EINTR) {
			continue;
		    }
		    break;
		}
	    }
#endif

	    if (soundel > 0) {
		register unsigned char *start = bs;
		register int j;
		int squelched = (squelch > 0), osl = soundel;

		/* If entire buffer is less than squelch, ditch it.  If
                   we haven't received sqdelay samples since the last
		   squelch event, continue to transmit. */

		if (sqdelay > 0 && sqwait > 0) {
		    if (debugging) {
                        printf("Squelch countdown: %d samples left.\n",
			    sqwait);
		    }
		    sqwait -= soundel;
		    squelched = FALSE;
#ifdef PUSH_TO_TALK
		    if (sqwait <= 0 && push) {
                        fprintf(stderr, "\rQuiet: ");
		    }
#endif
		} else if (squelch > 0) {
		    for (j = 0; j < soundel; j++) {
#ifdef USQUELCH
			if (((*start++ & 0x7F) ^ 0x7F) > squelch)
#else
			int samp = audio_u2s(*start++);

			if (samp < 0) {
			    samp = -samp;
			}
			if (samp > squelch)
#endif
			{
			    squelched = FALSE;
#ifdef PUSH_TO_TALK
			    if (sqwait <= 0 && push) {
                               fprintf(stderr, "\rTalk:  ");
			    }
#endif
			    sqwait = sqdelay;
			    break;
			}
		    }
		}

		if (squelched) {
		    if (debugging) {
                        printf("Entire buffer squelched.\n");
		    }
		    spurt = TRUE;
		} else {
		    netbuf.compression = fProtocol | (ring ? (fSetDest | fDestSpkr) : 0);
		    netbuf.compression |= debugging ? fDebug : 0;
		    netbuf.compression |= loopback ? fLoopBack : 0;

		    /* If automatic gain control is enabled,
		       ride the gain pot to fill the dynamic range
		       optimally. */

		    if (agc) {
			register unsigned char *start = bs;
			register int j;
			long msamp = 0;

			for (j = 0; j < soundel; j++) {
			    int s = audio_u2s(*start++);

			    msamp += (s < 0) ? -s : s;
			}
			msamp /= soundel;
			if (msamp < 6000) {
			    if (rgain < 100) {
				soundrecgain(++rgain);
			    }
			} else if (msamp > 7000) {
			    if (rgain > 1) {
				soundrecgain(--rgain);
			    }
			}
		    }

		    ring = FALSE;
		    if (compressing) {
			int is = soundel, os = soundel / 2;

			rate_flow((unsigned char *) buf, (unsigned char *) buf, &is, &os);
			soundel = os;
			netbuf.compression |= fComp2X;
		    }
		    netbuf.buffer.buffer_len = soundel;
		    if (!sendpkt(&netbuf)) {
			exiting();
		    }
		    if (debugging && !vat && !rtp) {
                        fprintf(stderr, "Sent %d audio samples in %ld bytes.\r\n",
				osl, netbuf.buffer.buffer_len);
		    }
		}
	    } else {
		sf_usleep(100000L);  /* Wait for some sound to arrive */
	    }
	}
    }
    return 0;
}

/*  MAKESESSIONKEY  --	Generate session key with optional start
			key.  If mode is TRUE, the key will be
			translated to a string, otherwise it is
			returned as 16 binary bytes.  */

static void makeSessionKey(char *key, char *seed, int mode)
{
    int j, k;
    struct MD5Context md5c;
    unsigned char md5key[16], md5key1[16];
#define SESSIONKEYBUF 1024
    char s[SESSIONKEYBUF];
#ifdef HAVE_DEV_RANDOM
    int random_fd, wanted_random_bytes, random_bytes;
    char rand[SESSIONKEYBUF];
#endif

    s[0] = 0;
    if (seed != NULL) {
	strcat(s, seed);
    }

    /* The following creates a seed for the session key generator
       based on a collection of volatile and environment-specific
       information unlikely to be vulnerable (as a whole) to an
       exhaustive search attack.  If one of these items isn't
       available on your machine, replace it with something
       equivalent or, if you like, just delete it. */

    sprintf(s + strlen(s), "%lu", (unsigned long) getpid());
    sprintf(s + strlen(s), "%lu", (unsigned long) getppid());
    V getcwd(s + strlen(s), 256);
    sprintf(s + strlen(s), "%lu", (unsigned long) clock());
    sprintf(s + strlen(s), "%lu", (unsigned long) time(NULL));
#ifdef Solaris
    sysinfo(SI_HW_SERIAL,s + strlen(s), 12);
#else
    sprintf(s + strlen(s), "%lu", (unsigned long) gethostid());
#endif
    getdomainname(s + strlen(s), 256);
    gethostname(s + strlen(s), 256);
    sprintf(s + strlen(s), "%u", getuid());
    sprintf(s + strlen(s), "%u", getgid());

#ifdef HAVE_DEV_RANDOM
    /* Some systems a random device which offers data as long as the system
       has still enough "randomness" collected.  We use it here. */
 
    /* Leave room for the terminator and to append data for second hash. */
    wanted_random_bytes = SESSIONKEYBUF - (strlen(s) - 12);

    if (wanted_random_bytes > 1) {

        random_fd = open ("/dev/random",O_RDONLY,O_NONBLOCK);
        if (random_fd > 0) { /* Do not complain if open fails */
	    random_bytes = read(random_fd, rand, wanted_random_bytes);

	    if  (random_bytes  > 0) { /* Again do not complain on read errors */

	        if (random_bytes <= wanted_random_bytes) { /* Should be */
		    /* String termination will be lost after memcpy */
		    char *startcopy =  s + strlen(s); 
		    memcpy(startcopy, rand, random_bytes);
		    startcopy[random_bytes + 1] = 0; /* Terminate string */
	        }
 	    }
	    close(random_fd);
	}
    }
#endif

    MD5Init(&md5c);
    MD5Update(&md5c, s, strlen(s));
    MD5Final(md5key, &md5c);
    /*	If you append additional information here, verify that the
    	computation of wanted_random_bytes above leaves adequate room after
	the /dev/random data for its maximum length.  */
    sprintf(s + strlen(s), "%lu", (unsigned long) ((time(NULL) + 65121) ^ 0x375F));
    MD5Init(&md5c);
    MD5Update(&md5c, s, strlen(s));
    MD5Final(md5key1, &md5c);
#define nextrand    (md5key[j] ^ md5key1[j])
    if (mode) {
	for (j = k = 0; j < 16; j++) {
	    unsigned char rb = nextrand;

/* #define Rad16(x) ((x) + 'A') */
    /* We don't count on choosing where in the alphabet to
       start based on the hashes to stir in any more entropy
       than the fundamental 4 bits from the generated key
       (although it may), but primarily to spread out the
       letter distribution across the entire alphabet rather
       than just the first 16 letters. */
#define Rad16(x)  ((((((md5key1[j] >> 1) ^ (md5key[15 - j] >> 2)) % 26) + (x)) % 26) + 'A')
	    key[k++] = Rad16((rb >> 4) & 0xF);
	    key[k++] = Rad16(rb & 0xF);
#undef Rad16
	    if (j & 1) {
                key[k++] = '-';
	    }
	}
	key[--k] = 0;
    } else {
	for (j = 0; j < 16; j++) {
	    key[j] = nextrand;
	}
    }
#undef nextrand
}

/*  PROG_NAME  --  Extract program name from argv[0].  */

static char *prog_name(char *arg)
{
    char *cp = strrchr(arg, '/');

    return (cp != NULL) ? cp + 1 : arg;
}

/*  USAGE  --  Print how-to-call information.  */

static void usage(void)
{
    V fprintf(stderr, "%s  --  Speak Freely sound sender.\n", progname);
    V fprintf(stderr, "            %s.\n", Relno);
    V fprintf(stderr, "\n");
    V fprintf(stderr, "Usage: %s hostname[:port] [options] [ file1 / . ]...\n", progname);
    V fprintf(stderr, "Options: (* indicates defaults)\n");
#ifdef PUSH_TO_TALK
    V fprintf(stderr, "           -A         Always transmit unless squelched\n");
    V fprintf(stderr, "     *     -B         Push to talk using keyboard\n");
#endif
#ifdef CRYPTO
    V fprintf(stderr, "           -BAkey     AES encrypt with text key (128 bit encryption)\n");
    V fprintf(stderr, "           -BFkey     Blowfish encrypt with key\n");
    V fprintf(stderr, "           -BXkey     AES encrypt with hexadecimal key (128-256 bit encryption)\n");
#endif
    V fprintf(stderr, "           -C         Compress subsequent sound\n");
    V fprintf(stderr, "           -CELP      CELP compression\n");
    V fprintf(stderr, "           -D         Enable debug output\n");
#ifdef CRYPTO
    V fprintf(stderr, "           -E[key]    Emit session key string\n");
#endif
    V fprintf(stderr, "           -F         ADPCM compression\n");
    V fprintf(stderr, "           -G         Automatic gain control\n");
#ifdef CRYPTO
    V fprintf(stderr, "           -Ikey      IDEA encrypt with key\n");
    V fprintf(stderr, "           -Kkey      DES encrypt with key\n");
#endif
    V fprintf(stderr, "           -L         Remote loopback\n");
    V fprintf(stderr, "           -LPC       LPC compression\n");
    V fprintf(stderr, "           -LPC10Rn   LPC-10 compression, n copies\n");
    V fprintf(stderr, "     *     -M         Manual record gain control\n");
    V fprintf(stderr, "           -N         Do not compress subsequent sound\n");
#ifdef NAT_LAUNCH
    V fprintf(stderr, "           -NATcsock,dsock  Inherit NAT socket file descriptors\n");
#endif
#ifdef CRYPTO
    V fprintf(stderr, "           -Ofile     Use file as key file\n");
#endif
    V fprintf(stderr, "           -Phostname[:port] Party line, add host to list\n");
    V fprintf(stderr, "     *     -Q         Disable debug output\n");
    V fprintf(stderr, "           -ROBUSTn   Robust transmission: n copies of each packet\n");
    V fprintf(stderr, "           -R         Ring--force volume, output to speaker\n");
    V fprintf(stderr, "           -RTP       Use Internet Real-Time Protocol\n");
    V fprintf(stderr, "           -Sn,t      Squelch at level n (0-32767), timeout t milliseconds\n");
 
    V fprintf(stderr, "     *     -T         Telephone (GSM) compression\n");
#ifdef BYTE_SWAP_DEBUG
    V fprintf(stderr, "           -TD        Debug: send GSM with swapped byte order\n");
#endif
    V fprintf(stderr, "           -U         Print this message\n");
    V fprintf(stderr, "           -VAT       Use VAT protocol\n");
    V fprintf(stderr, "           -Wfile     Write raw audio input to file\n");
#ifdef AUDIO_DEVICE_FILE
    V fprintf(stderr, "           -Yindev[:ctldev] Override default audio device file name or specify open #fd\n");
#endif
#ifdef CRYPTO
    V fprintf(stderr, "           -Z\"user..\" Send public-key session key for user(s)\n");
#endif
    V fprintf(stderr, "\n");
    V fprintf(stderr, "by John Walker\n");
    V fprintf(stderr, "   http://www.fourmilab.ch/\n");
#ifndef CRYPTO
    V fprintf(stderr, "\n");
    V fprintf(stderr, "Note: This version of Speak Freely was built with\n");
    V fprintf(stderr, "      encryption code removed to permit redistribution\n");
    V fprintf(stderr, "      without concern for export control and regulations\n");
    V fprintf(stderr, "      concerning encryption technology in some jurisdictions.\n");
    V fprintf(stderr, "      You can download a version of Speak Freely with\n");
    V fprintf(stderr, "      full encryption from:\n");
    V fprintf(stderr, "             http://www.fourmilab.ch/speakfree/unix/\n");
#endif
}

/*  Main program.  */

int main(int argc, char *argv[])
{
    int i, j, k, l, sentfile = 0;
    FILE *fp;
    struct MD5Context md5c;
    static lpcstate_t lpcc;
    char md5key[16];
    char s[1024];

    progname = prog_name(argv[0]);
    if (gethostname(hostname, sizeof hostname) == -1) {
        strcpy(hostname, "localhost");
    } else {
	if (strlen(hostname) > 15) {
	    hostname[15] = 0;
	}
    }
    
#ifdef OVERDRIVE_SOUND_FILE
    fprintf(stderr, "Warning: %s compiled with OVERDRIVE_SOUND_FILE set to nonstandard value %g.\n",
    	progname, OVERDRIVE_SOUND_FILE);
#endif

#ifdef LINUX_FPU_FIX
    __setfpucw(_FPU_IEEE);	      /* Mask floating point interrupts to
					 enable standard IEEE processing.
					 Current libc releases do this by
					 default, so this is needed only on
					 older libraries. */
#endif
    rate_start(SAMPLE_RATE, SAMPLE_RATE / 2);
    gsmh = gsm_create();

    /* Initialise LPC encoding. */

    if (!lpc_start()) {
        fprintf(stderr, "Cannot allocate LPC decoding memory.\n");
	return 1;
    }
    lpc_init(&lpcc);
    lpc10init();
	
    /* Initialise CELP encoding. */
	
    celp_init(FALSE);

#ifdef sgi
    usticks = 1000000 / CLK_TCK;
#endif

#ifdef HALF_DUPLEX
    {
#ifndef INADDR_LOOPBACK
	/* The standard local host loopback address is supposed to be
	   defined in <netinet/in.h>, but just in case some screwball
           system doesn't, let's include our own definition here. */
#define INADDR_LOOPBACK  0x7F000001
#endif
	localhost.s_addr = htonl(INADDR_LOOPBACK);
#ifdef HDX_DEBUG
        fprintf(stderr, "%s: local host %s\n", progname,
		inet_ntoa(localhost));
#endif
    }
#endif

    /* Create the socket used to send data. */

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("opening datagram socket");
	return 1;
    }

    /* Initialise randomised packet counters. */

    makeSessionKey(md5key, NULL, FALSE);
    bcopy(md5key, (char *) &ssrc, sizeof ssrc);
    bcopy(md5key + sizeof ssrc, (char *) &timestamp,
	  sizeof timestamp);
    bcopy(md5key + sizeof ssrc + sizeof timestamp,
	  (char *) &seq, sizeof seq);
    bcopy(md5key + sizeof ssrc + sizeof timestamp +
	  sizeof seq, &rtpdesrand, sizeof rtpdesrand);
    sdesl = rtp_make_sdes(&sdes, ssrc, -1, TRUE);

    /* See if there's a face image file for this user.  If
       so, we'll offer our face in sound packets we send. */

    {   char *cp = getenv("SPEAKFREE_FACE");
	FILE *facefile;

	if (cp != NULL) {
            if ((facefile = fopen(cp, "r")) == NULL) {
                fprintf(stderr, "%s: cannot open SPEAKFREE_FACE file %s\n",
		    progname, cp);
	    } else {
		fclose(facefile);
		hasFace = TRUE;
	    }
	}
    }

    /*	Process command line options.  */

    for (i = 1; i < argc; i++) {
	char *op, opt;

#ifdef NAT_LAUNCH
        if (strlen(argv[i]) == 0) {
            continue;
	}
#endif
	op = argv[i];
        if (*op == '-') {
	    opt = *(++op);
	    if (islower(opt)) {
		opt = toupper(opt);
	    }

	    switch (opt) {

#ifdef PUSH_TO_TALK
                case 'A':             /* -A  --  Always transmit (no push to talk) */
		    push = FALSE;
		    break;

                case 'B':             /* -B  -- Push to talk (button) */
#ifdef CRYPTO
				      /* -BAkey  -- Set AES key from ASCII string  */ 
                    if (ucase(op[1]) == 'A') {
			if (strlen(op + 2) == 0) {
			    curaes_spec = FALSE;
			} else {
			    struct MD5Context md5c;
			    unsigned char md5sig[32];
			    int klen = 16;
			    char *sep = NULL;

			    curaes_spec = TRUE;
			    if ((strlen(op + 2) > 2) && ((sep = strchr(op + 3, '+')) != NULL)) {
			    	if ((sep - (op + 2)) < (strlen(op + 2) - 1)) {
				    *sep++ = 0;
				    klen = 32;
				} else {
				    sep = NULL;
    	    	    	    	}
    			    }
			    MD5Init(&md5c);
			    MD5Update(&md5c, op + 2, strlen(op + 2));
			    MD5Final(md5sig, &md5c);
			    if (klen == 32) {
				MD5Init(&md5c);
				MD5Update(&md5c, sep, strlen(sep));
				MD5Final(md5sig + 16, &md5c);
			    }
			    curaes_ctx.n_rnd = curaes_ctx.n_blk = 0;
			    aes_enc_key(md5sig, klen, &curaes_ctx);
			    if (debugging) {
                                fprintf(stderr, "AES key:");
				for (j = 0; j < klen; j++) {
                                    fprintf(stderr, " %02X", (md5sig[j] & 0xFF));
				}
                                fprintf(stderr, " (%d bits)\n", klen * 8);
			    }
			}

				      /* -BXkey  -- Set AES key from hexadecimal value  */ 
                    } else if (ucase(op[1]) == 'X') {
			if (strlen(op + 2) == 0) {
			    curaes_spec = FALSE;
			} else {
			    unsigned char aesk[32];
			    int kb = 0, kn = 0, p = 2;
			    
			    bzero(aesk, sizeof aesk);
			    while (op[p] != 0) {
			    	char ch = ucase(op[p]);
				int v;
				
				if ((ch >= '0') && (ch <= '9')) {
				    v = ch - '0';
				} else if ((ch >= 'A') && (ch <= 'F')) {
				    v = 10 + (ch - 'A');
				} else {
				    fprintf(stderr, "Error: non-hexadecimal digit %c in -BX key specification.\n", ch);
				    return 2;
				}
				if (kb >= (sizeof aesk)) {
				    fprintf(stderr, "Error: -BX key specification exceeds 32 bytes (256 bits).\n");
				    return 2;
				}
				if (kn == 0) {
				    aesk[kb] = (v << 4);
				    kn++;
				} else {
				    aesk[kb++] |= v;
				    kn = 0;
				}
				
				p++;
			    }
			    
			    /*	Round up key bytes specified to next valid multiple.
			    	Note that vacant key bits have already been filled
				with zeroes.  */
				
    	    	    	    if (kn != 0) {
				kb++;
			    }
			    kb = (kb <= 16) ? 16 :
			    	    ((kb <= 24) ? 24 : 32);
			    
			    curaes_spec = TRUE;
			    curaes_ctx.n_rnd = curaes_ctx.n_blk = 0;
			    aes_enc_key(aesk, kb, &curaes_ctx);
			    if (debugging) {
                                fprintf(stderr, "AES key:");
				for (j = 0; j < kb; j++) {
                                    fprintf(stderr, " %02X", (aesk[j] & 0xFF));
				}
                                fprintf(stderr, " (%d bits)\n", kb * 8);
			    }
			}
				      /* -BFkey  -- Set Blowfish key */ 
                    } else if (ucase(op[1]) == 'F') {
			if (strlen(op + 2) == 0) {
			    curblowfish_spec = FALSE;
			} else {
			    unsigned char bfvec[16];

			    curblowfish_spec = TRUE;
			    MD5Init(&md5c);
			    MD5Update(&md5c, op + 2, strlen(op + 2));
			    MD5Final(bfvec, &md5c);
			    BF_set_key(&(curblowfishkey), 16, bfvec);
			    if (debugging) {
                                fprintf(stderr, "Blowfish key:");
				for (j = 0; j < 16; j++) {
                                    fprintf(stderr, " %02X", (bfvec[j] & 0xFF));
				}
                                fprintf(stderr, "\n");
			    }
			}
		    } else {
#endif
			if (isatty(fileno(stdin))) {
			    push = TRUE;
			}
#ifdef CRYPTO
		    }
#endif
		    break;
#endif

                case 'C':             /* -C  -- Compress sound samples */
				      /* -CELP  -- CELP compression */ 
                    if (ucase(op[1]) == 'E') {
		    	celpcompress = TRUE;
			adpcmcompress = gsmcompress = lpccompress = lpc10compress = FALSE;
		    } else {
		    	compressing = TRUE;
		    }
		    break;

                case 'D':             /* -D  --  Enable debug output  */
		    debugging = TRUE;
		    break;

#ifdef CRYPTO
                case 'E':             /* -E  --  Emit session key and exit */
		    makeSessionKey(s, op + 1, TRUE);
                    printf("%s\n", s);
		    return 0;
#endif

                case 'F':             /* -F -- ADPCM compression */
		    adpcmcompress = TRUE;
		    gsmcompress = lpccompress = lpc10compress = celpcompress = FALSE;
		    break;

                case 'G':             /* -G  --  Automatic gain control */
		    agc = TRUE;
		    break;

#ifdef CRYPTO
                case 'I':             /* -Ikey  --  Set IDEA key */
		    if (strlen(op + 1) == 0) {
			curideakey[0] = FALSE;
		    } else {
			MD5Init(&md5c);
			MD5Update(&md5c, op + 1, strlen(op + 1));
			MD5Final(curideakey + 1, &md5c);
			curideakey[0] = TRUE;
			if (debugging) {
                            fprintf(stderr, "IDEA key:");
			    for (j = 1; j < 17; j++) {
                                fprintf(stderr, " %02X", (curideakey[j] & 0xFF));
			    }
                            fprintf(stderr, "\n");
			}
		    }
		    break;

                case 'K':             /* -Kkey  --  Set DES key */
		    desinit(1);       /* Initialise the DES library */
		    if (strlen(op + 1) == 0) {
			curkey[0] = currtpkey[0] = curvatkey[0] = FALSE;
		    } else {
			char algorithm[16];

			MD5Init(&md5c);
			MD5Update(&md5c, op + 1, strlen(op + 1));
			MD5Final(md5key, &md5c);
			for (j = 0; j < 8; j++) {
			    curkey[j + 1] = (char)
					  ((md5key[j] ^ md5key[j + 8]) & 0x7F);
			}
			if (debugging) {
                            fprintf(stderr, "DES key:");
			    for (j = 0; j < 8; j++) {
                                fprintf(stderr, " %02X", (curkey[j + 1] & 0xFF));
			    }
                            fprintf(stderr, "\n");
			}
			curkey[0] = TRUE;
			des_string_to_key(op + 1, (des_cblock *) (curvatkey + 1));
			string_DES_key(op + 1, (unsigned char *) (currtpkey + 1), algorithm);
                        if (strcmp(algorithm, "DES-CBC") != 0) {
                            fprintf(stderr, "Unsupported encryption algorithm: %s.  Only DES-CBC is available.\n",
				    algorithm);
			    return 2;
			}
			currtpkey[0] = curvatkey[0] = TRUE;
			if (debugging) {
                            fprintf(stderr, "RTP key:");
			    for (j = 0; j < 8; j++) {
                                fprintf(stderr, " %02X", (currtpkey[j + 1] & 0xFF));
			    }
                            fprintf(stderr, "\n");
			}
			if (debugging) {
                            fprintf(stderr, "VAT key:");
			    for (j = 0; j < 8; j++) {
                                fprintf(stderr, " %02X", (curvatkey[j + 1] & 0xFF));
			    }
                            fprintf(stderr, "\n");
			}
		    }
		    break;
#endif

                case 'L':             /* -L      --  Remote loopback */
				      /* -LPC	 --  LPC compress sound */
				      /* -LPC10  --  LPC10 compress sound */
                    if (ucase(op[1]) == 'P') {
                        if (strlen(op) > 3 && op[3] == '1') {
			    lpc10compress = TRUE;
			    gsmcompress = adpcmcompress = lpccompress = celpcompress = FALSE;
			    robust = 1;
                            if (strlen(op) > 5 && ucase(op[5]) == 'R') {
				robust = 2;
				if (strlen(op) > 6 && isdigit(op[6])) {
				    robust = atoi(op + 6);
				    if (robust <= 0) {
					robust = 1;
				    } else if (robust > 4) {
					robust = 4;
				    }
				}
			    }
			} else {
			    lpccompress = TRUE;
			    gsmcompress = adpcmcompress = lpc10compress = celpcompress = FALSE;
			}
		    } else {
			loopback = TRUE;
		    }
		    break;

                case 'M':             /* -M  --  Manual record gain control */
		    agc = FALSE;
		    break;

                case 'N':             /* -N  --  Do not compress sound samples */
#ifdef NAT_LAUNCH
    	    	    	    	      /* -NATdsock,csock  --  Import file descriptors for sockets */
		    if ((ucase(op[1]) == 'A') && (ucase(op[2]) == 'T')) {
		    	char *delim = strchr(op + 3, ',');
			
			if (delim == NULL) {
			    fprintf(stderr, "Error: -NATdata,control option missing control socket FD.\n");
			    return 2;
			}
			xnat_socketsharing = TRUE;
			xnat_datasock = atoi(op + 3);
			xnat_consock = atoi(delim + 1);
			if (debugging) {
			    fprintf(stderr,"Using -NAT file descriptors.  Data: %d  Control: %d\n",
			    	    	    xnat_datasock, xnat_consock);
			}
		    } else {
#endif
			compressing = FALSE;
			gsmcompress = FALSE;
			lpccompress = FALSE;
			lpc10compress = FALSE;
			adpcmcompress = FALSE;
			celpcompress = FALSE;
#ifdef NAT_LAUNCH
		    }
#endif
		    break;

#ifdef CRYPTO
                case 'O':             /* -Ofile -- Use file as key file */
		    if (op[1] == 0) {
			curotp = NULL; /* Switch off key file */
		    } else {
                        fp = fopen(op + 1, "r");
			if (fp == NULL) {
                            perror("Cannot open key file");
			    return 2;
			}
			curotp = malloc(BUFL);
			if (curotp == NULL) {
                            fprintf(stderr, "Cannot allocate key file buffer.\n");
			    return 2;
			}
			l = fread(curotp, 1, BUFL, fp);
			if (l == 0) {
                            /* Idiot supplied void key file.  Give 'im
			       what he asked for: no encryption. */
			    curotp[0] = 0;
			    l = 1;
			}
			fclose(fp);
			/* If the file is shorter than the maximum buffer
			   we may need to encrypt, replicate the key until
			   the buffer is filled. */
			j = l;
			k = 0;
			while (j < BUFL) {
			    curotp[j++] = curotp[k++];
			    if (k >= l) {
				k = 0;
			    }
			}
		    }
		    break;
#endif

                case 'P':             /* -Phost  --  Copy output to host  */
		    if (!addest(op + 1)) {
			return 1;
		    }
		    break;

                case 'Q':             /* -Q  --  Disable debug output  */
		    debugging = FALSE;
		    break;

                case 'R':             /* -R    --  Ring: divert output to speaker */
				      /* -RTP  --  Use RTP to transmit */
				      /* -ROBUSTn  --  Send n copies of each packet with sequence number */
                    if (ucase(op[1]) == 'T') {
			rtp = TRUE;
                    } else if (ucase(op[1]) == 'O') {
			robust = 1;
			if (strlen(op) > 6 && isdigit(op[6])) {
			    robust = atoi(op + 6);
			    if (robust <= 0) {
				robust = 1;
			    } else if (robust > 8) {
				robust = 8;
			    }
			}
		    } else {
			ring = TRUE;
		    }
		    break;

                case 'S':             /* -Sn,d  --  Squelch at level n,
						    delay d milliseconds */
		    if (strlen(op + 1) == 0) {
			squelch = DEFAULT_SQUELCH; /* Default squelch */
		    } else {
			char *cp;

                        if (op[1] != ',') {
			    squelch = atoi(op + 1);
			} else {
			    squelch = DEFAULT_SQUELCH;
			}
                        cp = strchr(op + 1, ',');
			if (cp != NULL) {
			    sqdelay = 8 * atoi(cp + 1);
			}
		    }
		    break;

                case 'T':             /* -T -- Telephone (GSM) compression */
		    gsmcompress = TRUE;
		    lpccompress = lpc10compress = adpcmcompress = celpcompress = FALSE;
#ifdef BYTE_SWAP_DEBUG
		    /* This can be used if you absolutely have to communicate
		       with somebody running a version prior to 6.1e on little-endian
		       hardware.  This brakes the byte order of GSM packets to
		       compensate for a bug in earlier versions.  Note that you
		       should only set this for compatibility with old Unix
		       versions--the problem this works around was never in
		       the Windows version. */
                    if (ucase(op[1]) == 'D') {
			gsm_byte_order_debug = TRUE;
		    }
#endif
		    break;

                case 'U':             /* -U  --  Print usage information */
                case '?':             /* -?  --  Print usage information */
		    usage();
		    return 0;

                case 'V':             /* -V  --  Use VAT protocol */
		    vat = TRUE;
		    if (vatid == NULL) {
			vatidl = makeVATid(&vatid, 0L);
		    }
		    break;

                case 'W':             /* -Wfile -- Write raw audio to file */
                    audioDumpFile = fopen(op + 1, "w");
		    if (audioDumpFile == NULL) {
                        perror("Cannot create raw audio dump (-w option) file");
			return 2;
		    }
		    break;

#ifdef AUDIO_DEVICE_FILE
                case 'Y':             /* -Yindev:[ctldev] -- Specify audio input and control device file names
							     or #open_fd. */
		    devAudioInput = op + 1;
                    if (strchr(op + 1, ':') != NULL) {
                        devAudioControl = strchr(op + 1, ':') + 1;
		    }
		    break;
#endif

#ifdef CRYPTO
                case 'Z':             /* -Z"user1 user2..."  --  Send PGP/GPG encrypted session key to
						                 named users */
		    if (op[1] == 0) {
			curpgpkey[0] = FALSE;
		    } else {
			char c[80], f[40];
			FILE *kfile;
			FILE *pipe;
			long flen;
    	    	    	char *uniqueName = NULL;
			
    	    	    	kfile = create_tempfile_in_tempdir(".SF_skey_XXXXXXXXXXX", &uniqueName, 077, FALSE);
			if (kfile != NULL) {
#ifdef GPG_KEY_EXCHANGE
    	    	    	    /* For GPG, we must prefix each recipient with "-r".  We
			       allocate a temporary buffer into which the list is
			       expanded. */
			       
    	    	    	    char *rlist;
			    int t, u, ns = 1;
			    
			    for (t = 0; t < strlen(op + 1); t++) {
			    	if (isspace(op[t + 1])) {
				    ns++;
    	    	    	    	}
			    }
			    rlist = (char *) malloc(strlen(op + 1) + (ns * 4) + 1);
			    if (rlist == NULL) {
			    	fprintf(stderr, "Cannot allocate GPG recipient list buffer.\n");
				return 2;
			    }
			    t = 1;
			    rlist[0] = 0;
			    while (op[t] != 0) {
			    	if (!isspace(op[t])) {
				    strcat(rlist, "-r ");
				}
				u = strlen(rlist);
				while ((op[t] != 0) && (!isspace(op[t]))) {
				    rlist[u++] = op[t++];
				}
				if (op[t] == 0) {
				    rlist[u] = 0;
				    break;
				}
				rlist[u++] = ' ';
				rlist[u] = 0;
				t++;
				while ((op[t] != 0) && isspace(op[t])) {
				    t++;
				}
			    }
                            sprintf(c, "gpg --encrypt --output - --quiet %s >%s", rlist, uniqueName);
			    free(rlist);
#else
                            sprintf(c, "pgp -fe +nomanual +verbose=0 +armor=off %s >%s", op + 1, uniqueName);
#endif
#ifdef PGP_DEBUG
                            fprintf(stderr, "Encoding session key with: %s\n", c);
#endif
                            pipe = popen(c, "w");
			    if (pipe == NULL) {
                        	fprintf(stderr, "Unable to open pipe to: %s\n", c);
				return 2;
			    } else {
				makeSessionKey(curpgpkey + 1, NULL, FALSE);
#ifdef PGP_DEBUG
				{	
				    int i;

                                    fprintf(stderr, "Session key:");
				    for (i = 0; i < 16; i++) {
                                	fprintf(stderr, " %02X", curpgpkey[i + 1] & 0xFF);
				    }
                                    fprintf(stderr, "\n");
				}
#endif
				/* The reason we start things off right with
                        	   "Special K" is to prevent the session key
				   (which can be any binary value) from
                        	   triggering PGP's detection of the file as
				   one already processed by PGP.  This causes an
				   embarrassing question when we are attempting
				   to run silent. */
                        	curpgpkey[0] = 'K';
				fwrite(curpgpkey, 17, 1, pipe);
				curpgpkey[0] = FALSE;
				fflush(pipe);
				pclose(pipe);
			    }
    	    	    	    rewind(kfile);
			}
			if (kfile == NULL) {
                            fprintf(stderr, "Cannot open key file %s\n", f);
			} else {
			    fseek(kfile, 0L, 2);
			    flen = ftell(kfile);
			    rewind(kfile);
#ifdef PGP_DEBUG
                            fprintf(stderr, "Public key buffer length: %ld\n", flen);
			    if (flen > (TINY_PACKETS - (sizeof(soundbuf) - BUFL))) {
                                fprintf(stderr, "Warning: Public key message exceeds %d packet size.\n", TINY_PACKETS);
			    }
#endif
			    if (pgpsb != NULL) {
				free(pgpsb);
			    }
			    pgpsb = (soundbuf *) malloc(((unsigned) flen) +
				      (TINY_PACKETS - (sizeof(soundbuf) - BUFL)));
			    if (pgpsb == NULL) {
                                fprintf(stderr, "Cannot allocate public key sound buffer.\n");
				fclose(kfile);
				unlink(f);
				return 2;
			    }
			    fread(pgpsb->buffer.buffer_val, (int) flen, 1, kfile);
			    pgpsbl = flen;
			    pgpsb->buffer.buffer_len = htonl(flen);
			    pgpsb->compression = htonl(fProtocol | fKeyPGP);
#ifdef SENDMD5
			    MD5Init(&md5c);
			    MD5Update(&md5c, pgpsb->buffer.buffer_val, pgpsb->buffer.buffer_len);
			    MD5Final(pgpsb->sendinghost, &md5c);
#else
			    strcpy(pgpsb->sendinghost, hostname);
#endif
			    fclose(kfile);
    	    	    	    unlink(uniqueName);
			    free(uniqueName);
			    curpgpkey[0] = TRUE;
			}
			unlink(f);
		    }
		    break;
#endif
	    }
	} else {

	    /*	Check for conflicting options.	*/

	    if (rtp && vat) {
                fprintf(stderr, "Cannot choose both -RTP and -VAT protocols simultaneously.\n");
		return 2;
	    }

	    if (rtp || vat) {
#ifdef CRYPTO
		if (curideakey[0] || curpgpkey[0] ||
		    curaes_spec || curblowfish_spec || curotp != NULL) {
                    fprintf(stderr, "AES, Blowfish, IDEA, Public Key, and Key File encryption cannot be used with -RTP or -VAT protocols.\n");
		    return 2;
		}
#endif
		if (compressing) {
                    fprintf(stderr, "Simple (-C) compression cannot be used with -RTP or -VAT protocols.\n");
		    return 2;
		}
		if (lpc10compress) {
                    fprintf(stderr, "LPC-10 (-LPC10) compression cannot be used with -RTP or -VAT protocols.\n");
		    return 2;
		}
		if (celpcompress) {
                    fprintf(stderr, "CELP (-CELP) compression cannot be used with -RTP or -VAT protocols.\n");
		    return 2;
		}
		if (robust > 1) {
                    fprintf(stderr, "Robust (-ROBUSTn) transmission cannot be used with -RTP or -VAT protocols.\n");
		    return 2;
		}
		if (ring) {
                    fprintf(stderr, "Warning: ring not implemented in -RTP and -VAT protocols.\n");
		}
	    }

	    if (dests == NULL) {
		if (!addest(op)) {
		    return 1;
		}
	    } else {
		int ok = sendfile(op);
		if (ok != 0)
		    return ok;
		sentfile++;
	    }
	}
    }

    if (dests == NULL) {
	usage();
    } else {
	if (sentfile == 0) {
	    return sendfile(NULL);
	}
    }

    exiting();
    gsm_destroy(gsmh);
    lpc_end();
#ifdef CRYPTO
    desdone();
#endif
    return 0;
}
