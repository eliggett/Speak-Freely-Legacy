/* #define PGP_DEBUG */
/*

			Speak Freely for Unix
			 Sound Output Server

	Designed and implemented in July of 1991 by John Walker.

*/

#include "speakfree.h"
#include "version.h"

static int audiok = FALSE;	      /* Audio initialised flag */
static int audiotime = 0;	      /* Audio timeout counter */
static int debugging = FALSE;	      /* Debugging enabled */
static int debugforce = FALSE;	      /* Debugging forced / prevented ? */
#ifdef HEXDUMP
static int hexdump = FALSE;	      /* Dump received packets in hex ? */
#endif
static int noring = FALSE;	      /* Disable remote ring requests */
gsm gsmh;			      /* GSM handle */
static char deskey[9] = "";           /* DES key, if any */
static char rtpdeskey[9] = "";        /* RTP DES key, if any */
static char vatdeskey[9] = "";        /* VAT DES key, if any */
static char ideakey[17] = "";         /* IDEA key, if any */
static char blowfish_spec = FALSE;    /* Nonzero if Blowfish key specified */
#ifdef CRYPTO
static BF_KEY blowfishkey;	      /* Blowfish key */
#endif
static char aes_spec = FALSE;         /* Nonzero if AES key specified */
#ifdef CRYPTO
static aes_ctx daes_ctx;  	      /* AES context */
#endif
static char *curotp = NULL;	      /* Key file, if any */
static int whichport = Internet_Port; /* Port to listen on (base address) */
static int sock;		      /* Data socket */
static int ssock;		      /* Control socket (RTP/VAT) */
static struct sockaddr_in from;       /* Sending host address */
static struct sockaddr_in name;       /* Address of destination host */
static int fromlen;		      /* Length of sending host address */
static int rll; 		      /* Length of packet read from socket */
static int showhosts = FALSE;	      /* Show host names that connect */
static int hosttimeout = 180 * 1000000L; /* Consider host idle after this time */
static int jitter = 0;		      /* Jitter compensation delay */
static int jitterwait = 0;	      /* Jitter wait in progress */
static int jitteridle = 0;	      /* Countdown to reset jitter timer */
static int jitteridlet = 0;	      /* Jitter idle timeout interval */
struct jitterbuf {		      /* Jitter queue item */
    struct jitterbuf *next;
    LONG length;
    char value[2];
};
static struct jitterbuf *jithead = NULL, *jittail = NULL; /* Jitter queue links */
static int outRate = SAMPLE_RATE;     /* Output data rate in samples per second */
static int currentRate = SAMPLE_RATE; /* Current output data rate (possibly dynamically adjusted) */
static char *prog;		      /* Program name */ 
static FILE *record = NULL;	      /* Answering machine record file */
static char *pgppass = NULL;	      /* PGP secret key password */
static char *replyfile = NULL;	      /* Reply file pathname */
static char *replycmdexe = NULL;
static char *replycmd = "sfmike -t";  /* Default reply command */
static char *busysignal = "sleep 10; sfmike %s busy.au"; /* Default busy signal command */
static int busytimeout = 60 * 1000000L; /* Busy signal timeout interval */
static int dobusy = FALSE;	      /* Reject calls when busy ? */
#ifdef HALF_DUPLEX
static struct in_addr localhost;      /* Our internet address */
static int halfDuplexMuted = FALSE;   /* Muted by half-duplex transmission */
#endif

static struct sockaddr_in lookhost;   /* Look who's listening host, if any */
static char *sdes = NULL;	      /* RTP SDES packet */
static int sdesl;		      /* RTP SDES packet length */
static unsigned long ssrc;	      /* RTP synchronisation source identifier */
static int lwltimer;		      /* Seconds before next LWL retransmit */
static int lwlonly = FALSE;	      /* LWL broadcast only ? */
static int actives = 0; 	      /* Currently active hosts */
static int faceTransferActive = 0;    /* Face transfers in progress */

#ifdef NAT_LAUNCH

/* NAT socket-sharing related stuff */

static int xnat_passive = FALSE;      /* Operating in NAT passive mode ? */
static int xnat_active_sfmike_pid = 0;/* Process ID of sfmike we launched */

struct mikeopts_t {
    char *crypto;
    char *compression;
    char *squelch;
    int autogain;
};
#define MIKEOPTS_EMPTY {NULL,NULL,NULL,0}

struct xn_conspec_t {
    char* xnat_conaddr;
    struct xn_conspec_t *next;
};

static struct xn_conspec_t *xnat_conspec = NULL;
#endif

#define LWL_RETRANSMIT	(5 * 60 * 1000000L) /* Microseconds between LWL updates */
#define LWL_STRIKEOUT	5	      /* Abandon LWL transmission to site
					 after this number of consecutive
					 failures. */

static struct in_addr lwl_sites[LWL_MAX_SITES]; /* LWL site addresses */
static long lwl_ports[LWL_MAX_SITES]; /* Ports for LWL hosts */
static int lwl_strikes[LWL_MAX_SITES];/* Consecutive failures to send LWL packet */
static int lwl_nsites = 0;	      /* Number of LWL sites published on */

static FILE *facefile = NULL;         /* User's face image */
static int facesDisplayed = 0;	      /* Face viewers currently active */

static struct connection *conn = NULL;/* Chain of current connections */

static int adjustRate = FALSE;	      /* Dynamically adjust output rate ? */
static double timebase;     	      /* Full seconds at program start time */
static double lastPacketTime = 0;     /* Time last packet arrived */
static double averageDataRate = SAMPLE_RATE; /* Average data arrival rate, samples/second */
static long lastPacketLength = 0;     /* Samples received since last packet */
#define DYNRATE_UPDATE_SAMPLE_COUNT SAMPLE_RATE /* Update dynamic rate after this many samples received */
#define DYNRATE_SMOOTHING_FACTOR 0.1  /* Exponential moving average smoothing factor for rate */

/* Audio output and control device file names. */

#ifdef AUDIO_DEVICE_FILE
extern char *devAudioOutput, *devAudioControl;
#endif

#define Debug	((debugforce != 0) ? (debugforce > 0) : debugging)

#define TickTock    (10 * 1000000L)   /* Alarm interval in microseconds */
#define TockTock    (60 * 1000000L)   /* Alarm interval when no connections open */
#define JitterTock  (1000000L / 4)    /* Alarm interval for jitter compensation */

#ifdef AUDIO_RELEASE
#define AudioRelease (AUDIO_RELEASE * 1000000L) /* Release audio when idle this long */
#else
#define AudioRelease (20 * 1000000L)  /* Release audio when idle this long */
#endif

static int timerStep = TockTock;      /* Current timer step */

#define ucase(x)    (islower(x) ? toupper(x) : (x))

#ifdef NAT_LAUNCH

/*  NEW_XNAT_CONSPEC -- Create a new connection specification for
    	    	    	later use in launch_sfmike and store it on the list.
*/

static struct xn_conspec_t *new_xnat_conspec(char *addr)
{
    struct xn_conspec_t *cur, *neue;
    
    neue = (struct xn_conspec_t *) malloc(sizeof(struct xn_conspec_t));
    if (neue == NULL) {
    	fprintf(stderr, "%s: Unable to allocate NAT connection specification.\n", prog);
	exit(2);
    }
    neue->next = NULL;
    if (xnat_conspec) {
	cur = xnat_conspec;
	while (cur->next != NULL) {
	    cur = cur->next;
	}
	cur->next = neue;
    } else {
	xnat_conspec = neue;
    }
    return neue;
}

static void childterm();

/*  LAUNCH_SFMIKE -- For xnat socket sharing. */

static int launch_sfmike(struct connection *c, struct mikeopts_t *mo)
{
    char *comp, *hostspec = NULL, **argv;
    char socks[24];
    struct mikeopts_t mo_empty = MIKEOPTS_EMPTY;
    int argc = 0, dynargs = 0;
    struct xn_conspec_t *cs;
    char *sfmike_progname = "sfmike",
    	 *sfmike_cdprogname = "./sfmike";
    int cpid = -1;

    if (!mo) {
	mo = &mo_empty;
    }
    
    if (c) {
	   /* ask the user if he wants to pick up the call */

	   /* ... */
	argv = (char **) malloc(8 * sizeof(char *));
	if (argv == NULL) {
	    fprintf(stderr, "%s: Unable to allocate sfmike command line.\n", prog);
	}
	argv[argc++] = sfmike_progname;
        comp = ((c->con_compression & fCompADPCM) ? "-f" :
               ((c->con_compression & fComp2X) ? "-c" :
               ((c->con_compression & fCompLPC) ? "-lpc" :
               ((c->con_compression & fCompLPC10) ? "-lpc10" :
               ((c->con_compression & fCompCELP) ? "-celp" :
               ((c->con_compression & fCompGSM) ? "-t" : ""))))));
	if (strlen(comp) > 0) {
	    argv[argc++] = comp;
	}
	sprintf(socks, "-NAT%d,%d", sock, ssock);
	argv[argc++] = socks;
	if (mo->crypto && strlen(mo->crypto) > 0) {
	    argv[argc++] = mo->crypto;
	}
	if (mo->autogain) {
	    argv[argc++] = "-g";
	}
	if (mo->squelch && strlen(mo->squelch) > 0) {
	    argv[argc++] = mo->squelch;
	}
	hostspec = (char *) malloc(strlen(c->con_hostname) + 24);
	if (hostspec == NULL) {
	    fprintf(stderr, "%s: Unable to allocate launch_sfmike host specification.\n", prog);
	}
	sprintf(hostspec,"%s:%d,%d", c->con_hostname, c->con_ports.dport, c->con_ports.cport);
    	dynargs = argc;     	    /* Start of dynamically allocated arguments */
	argv[argc++] = hostspec;
	argv[argc] = NULL;
    	assert(argc <= 8);

    } else {
    	int margc;
	
	if (xnat_conspec == NULL) {
	    fprintf(stderr, "%s: launch_sfmike: no connections specified.\n", prog);
	    return -1;
	}
	cs = xnat_conspec;
	margc = 8;
	while (cs->next != NULL) {
	    cs = cs->next;
	    margc++;
	}
	argv = (char **) malloc(margc * sizeof(char *));
	if (argv == NULL) {
	    fprintf(stderr, "%s: Unable to allocate launch_sfmike argument vector.\n", prog);
	}
	argc = 0;
	argv[argc++] = sfmike_progname;
	sprintf(socks, "-NAT%d,%d", sock, ssock);
	argv[argc++] = socks;
	if (mo->compression && (strlen(mo->compression) > 0)) {
	    argv[argc++] = mo->compression;
	}
	if (mo->crypto && (strlen(mo->crypto) > 0)) {
	    argv[argc++] = mo->crypto;
	}
	if (mo->autogain) {
	    argv[argc++] = "-g";
	}
	if (mo->squelch && (strlen(mo->squelch) > 0)) {
	    argv[argc++] = mo->squelch;
	}

	cs = xnat_conspec;
    	dynargs = argc;     	    /* Start of dynamically allocated arguments */
	while (cs->next !=  NULL) {
	    cs = cs->next;
	    hostspec = (char *) malloc(strlen(cs->xnat_conaddr) + 3);
	    if (hostspec == NULL) {
		fprintf(stderr, "%s: Unable to allocate launch_sfmike host name buffer.\n", prog);
	    }
	    strcpy(hostspec, "-p");
	    strcat(hostspec, cs->xnat_conaddr);
	    argv[argc++] = hostspec;
	}
	hostspec = (char *) malloc(strlen(xnat_conspec->xnat_conaddr) + 3);
	if (hostspec == NULL) {
	    fprintf(stderr, "%s: Unable to allocate launch_sfmike host name buffer.\n", prog);
	}
	strcpy(hostspec, "-p");
	strcat(hostspec, xnat_conspec->xnat_conaddr);
	argv[argc++] = hostspec;
	argv[argc] = NULL;
	assert(argc <= margc);
    }

    signal(SIGCHLD, childterm);
    if ((cpid = fork()) != 0) {
    	/* We are in the parent process here. */
	if (cpid == -1) {
	    perror("launch_sfmike::fork()");
	}
	
	/*  Release dynamically allocated arguments (host names) and then
	    release the argument vector itself.  */
	while (argv[dynargs] != NULL) {
	    free(argv[dynargs++]);
	}
	free(argv);
	return cpid;
    }
    
    /* Child process */
    signal(SIGCHLD, SIG_DFL);

    if (Debug) {
	fprintf(stderr, "%s: Launching sfmike with: execvp(", prog);
	for (argc = 0; argv[argc] != NULL; argc++) {
	    fprintf(stderr, "<%s> ", argv[argc]);
	}
    	fprintf(stderr, ")\n");
    }
    if ((execvp(sfmike_progname, argv) == -1) && (execvp(sfmike_cdprogname, argv) == -1)) {
	perror("launch_sfmike::execvp()");
    }
    exit(1);

    /*NOTREACHED*/
    return -1;
}
#endif

/*  GSMDECOMP  --  Uncompress the contents of a sound buffer using GSM.  */

static void gsmdecomp(struct soundbuf *sb)
{
    gsm_signal dst[160];
    int i, j, l = 0;
    char *dpx = ((char *) sb->buffer.buffer_val) + sizeof(short);
    static char dcb[BUFL];
    short declen = ntohs(*((short *) sb->buffer.buffer_val));

    /* Since some old releases forget to convert this field
       from network byte order, kludge it back to sanity
       if this happens.  First we try swapping the bytes.  If
       that doesn't work, force the standard buffer length.
       Note that we have to write this out, as ntohs will be
       a no-op on a big-endian machine. */

    if (declen <= 0 || declen > 1600) {
	declen = ((declen & 0xFF) << 8) | ((declen >> 8) & 0xFF);
    }
    if (declen <= 0 || declen > 1600) {
	declen = 1600;
    }
    for (i = 0; i < sb->buffer.buffer_len - sizeof(short);
		i += sizeof(gsm_frame)) {
	gsm_decode(gsmh, (gsm_byte *) dpx, dst);
	dpx += sizeof(gsm_frame);
	for (j = 0; j < 160; j++) {
	    dcb[l++] = audio_s2u(dst[j]);
	}
    }
    bcopy(dcb, sb->buffer.buffer_val, declen);
    sb->buffer.buffer_len = declen;
}

/*  LPCDECOMP  --  Uncompress the contents of a sound buffer using LPC.  */

static void lpcdecomp(struct soundbuf *sb, struct connection *c)
{
    int i, l = 0;
    char *dpx = ((char *) sb->buffer.buffer_val) + sizeof(short);
    char dcb[BUFL];
    short declen = ntohs(*((short *) sb->buffer.buffer_val));

    if (declen <= 0 || declen > (10 * LPC_FRAME_SIZE)) {
	declen = 10 * LPC_FRAME_SIZE;
    }
    for (i = 0; l < declen;
		i += LPCRECSIZE) {
	lpc_synthesize(dcb + l, (lpcparams_t *) (dpx + i), &c->lpcc);
	l += LPC_FRAME_SIZE;
    }
    bcopy(dcb, sb->buffer.buffer_val, declen);
    sb->buffer.buffer_len = declen;
}

/*  LPC10DECOMP  --  Uncompress the contents of a sound buffer using LPC10.  */

static void lpc10decomp(struct soundbuf *sb, struct connection *c)
{
    int j;
    unsigned char *dpx = ((unsigned char *) sb->buffer.buffer_val);
    unsigned char dcb[BUFL];

    j = lpc10decode(dpx, dcb, sb->buffer.buffer_len);
    bcopy(dcb, sb->buffer.buffer_val, j);
    sb->buffer.buffer_len = j;
}

/*  CELPDECOMP  --  Uncompress the contents of a sound buffer encoded in CELP.  */

static void celpdecomp(struct soundbuf *sb)
{
    short dst[240];
    int i, j, l = 0;
    char *dpx = ((char *) sb->buffer.buffer_val) + sizeof(short);
    static char dcb[BUFL];
    short declen = ntohs(*((short *) sb->buffer.buffer_val));

    if (declen <= 0 || declen > 1920) {
	declen = 1920;
    }
    for (i = 0; i < sb->buffer.buffer_len - sizeof(short);
		i += 18) {
	celp_decode(dpx, dst);
	dpx += 18;
	for (j = 0; j < 240; j++) {
	    dcb[l++] = audio_s2u(dst[j]);
	}
    }
    bcopy(dcb, sb->buffer.buffer_val, declen);
    sb->buffer.buffer_len = declen;
}

/*  AES_cbc_decrypt  --  Decrypt buffer with AES.  */

static void AES_cbc_decrypt(unsigned char *in,
			    unsigned char *out,
			    int len, aes_ctx *ctx)
{
    int i, j;
    unsigned char feedback[AES_BLOCK_SIZE], fbnext[AES_BLOCK_SIZE];
    
    /*	Initially zero the feedback buffer.  */
    bzero(feedback, AES_BLOCK_SIZE);
    assert((len % AES_BLOCK_SIZE) == 0);  	/* Attempt to encrypt non-multiple of block length */
    
    /*	Loop over encryption blocks in the buffer.  */
    for (i = 0; i < len; i += AES_BLOCK_SIZE) {
    	/*  Save encrypted block as feedback for the following block.  */
    	bcopy(in, fbnext, AES_BLOCK_SIZE);
	/*  Decrypt the current block.  */
	aes_dec_blk(in, out, ctx);
	/*  XOR it with the encrypted previous block saved in feedback.  */
    	for (j = 0; j < AES_BLOCK_SIZE; j++) {
    	    out[j] ^= feedback[j];
	}
	/*  Move saved encrypted present block to feedback buffer.  */
	bcopy(fbnext, feedback, AES_BLOCK_SIZE);
	/*  Advance input and output pointers to next block.  */
	in += AES_BLOCK_SIZE;
	out += AES_BLOCK_SIZE;
    }
}

/*  ETIME  --  Edit time and date for log messages.  */

static char *etime(void)
{
    struct tm *t;
    time_t clock;
    static char s[20];

    time(&clock);
    t = localtime(&clock);
    sprintf(s, "%02d-%02d %02d:%02d", t->tm_mon + 1, t->tm_mday,
	       t->tm_hour, t->tm_min);
    return s;
}

/*  COMPRESSIONTYPE  --  Return a string describing the type of
			 compression employed in this buffer.  Assumes
			 this *is*, in fact, a sound buffer and not
			 an RTP packet, face data, etc.  */

static char *compressionType(soundbuf *msg)
{
    return ((msg->compression & (fComp2X | fCompGSM)) ==
				(fComp2X | fCompGSM)) ?
                                "GSM+2X compressed" :
	   ((msg->compression & (fComp2X | fCompADPCM)) ==
				(fComp2X | fCompADPCM)) ?
                                "ADPCM+2X compressed" :
	   ((msg->compression & (fComp2X | fCompLPC)) ==
				(fComp2X | fCompLPC)) ?
                                "LPC+2X compressed" :
	   ((msg->compression & (fComp2X | fCompLPC10)) ==
				(fComp2X | fCompLPC10)) ?
                                "LPC10+2X compressed" :
	   ((msg->compression & (fComp2X | fCompCELP)) ==
				(fComp2X | fCompCELP)) ?
                                "CELP+2X compressed" :
	   ((msg->compression & (fComp2X | fCompVOX)) ==
				(fComp2X | fCompVOX)) ?
                                "VOX+2X compressed" :
           ((msg->compression & fCompADPCM) ? "ADPCM compressed" :
           ((msg->compression & fCompLPC) ? "LPC compressed" :
           ((msg->compression & fCompLPC10) ? "LPC10 compressed" :
           ((msg->compression & fCompCELP) ? "CELP compressed" :
           ((msg->compression & fComp2X) ? "2X compressed" :
           ((msg->compression & fCompGSM) ? "GSM compressed" :
           ((msg->compression & fCompVOX) ? "VOX compressed" :
                                            "uncompressed")))))));
}

/*  MAKESESSIONKEY  --	Generate session key.  */

static void makeSessionKey(char *key)
{
    int j;
    struct MD5Context md5c;
    unsigned char md5key[16], md5key1[16];
    char s[1024];

    s[0] = 0;
    sprintf(s + strlen(s), "%u", getpid());
    sprintf(s + strlen(s), "%u", getppid());
    V getcwd(s + strlen(s), 256);
    sprintf(s + strlen(s), "%lu", (unsigned long) clock());
    sprintf(s + strlen(s), "%lu", (unsigned long) time(NULL));
#ifdef Solaris
    sysinfo(SI_HW_SERIAL, s + strlen(s), 12);
#else
    sprintf(s + strlen(s), "%lu", (unsigned long) gethostid());
#endif
    getdomainname(s + strlen(s), 256);
    gethostname(s + strlen(s), 256);
    sprintf(s + strlen(s), "%u", getuid());
    sprintf(s + strlen(s), "%u", getgid());
    MD5Init(&md5c);
    MD5Update(&md5c, s, strlen(s));
    MD5Final(md5key, &md5c);
    sprintf(s + strlen(s), "%lu", (time(NULL) + 65121) ^ 0x375F);
    sprintf(s + strlen(s), "%lu", (unsigned long) (getppid() ^ clock()));
    MD5Init(&md5c);
    MD5Update(&md5c, s, strlen(s));
    MD5Final(md5key1, &md5c);
    for (j = 0; j < 16; j++) {
	key[j] = md5key[j] ^ md5key1[j];
    }
}

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

/*  WINDTIMER  --  Reset alarm timer to appropriate value
                   depending on what's going on at the moment.  */

static void windtimer(void)
{
    if (faceTransferActive > 0) {
	/* There is an implicit assumption here that FaceFetchInterval
	   is <= JitterTock.  If this is not the case, jitter compensation
	   may be too coarse during a face transfer. */
	timerStep = FaceFetchInterval;
    } else if (audiok || (actives > 0)) {
	timerStep = (jitteridle > 0) ? JitterTock : TickTock;
    } else {
	timerStep = TockTock;
    }
    ularm(timerStep);
}

/*  ISBUSY  --	Test if a connection is active (item on connection
		chain with a protocol other than PROTOCOL_UNKNOWN).  */

static int isBusy(void)
{
    struct connection *c;

    c = conn;
    while (c != NULL) {
	if (c->con_protocol != PROTOCOL_UNKNOWN) {
	    return dobusy;
	}
	c = c->con_next;
    }
    return FALSE;
}

/*  SENDLWLMESSAGE  --	If enabled, send a message identifying us
                        to each selected Look Who's Listening server.  */

static void sendLwlMessage(int dobye)
{
    int i, sock;

    for (i = 0; i < lwl_nsites; i++) {
	if (lwl_ports[i] >= 0) {
	    if (Debug) {
                fprintf(stderr, "%s: updating LWL data at %s\n",
		    prog, inet_ntoa(lwl_sites[i]));
	    }
	    sock = socket(AF_INET, SOCK_STREAM, 0);
	    if (sock < 0) {
                perror("opening look who's listening socket");
		sdes = NULL;
		return;
	    }

	    lookhost.sin_port = htons(lwl_ports[i]);
	    bcopy((char *) (&lwl_sites[i]), (char *) &lookhost.sin_addr.s_addr,
		  sizeof lookhost.sin_addr.s_addr);

	    if (connect(sock, (struct sockaddr *) &(lookhost), sizeof lookhost) >= 0) {
		if (dobye) {
		    unsigned char v[1024];
		    int l;

                    l = rtp_make_bye(v, ssrc, "Exiting sfspeaker", FALSE);
		    if (send(sock, v, l, 0) < 0) {
                        perror("sending look who's listening BYE packet");
		    }
                    /* No point in clearing strikes if we're exiting. */
		} else {
		    if (send(sock, (char *) sdes, sdesl, 0) < 0) {
                        perror("sending look who's listening source ID message");
		    } else {
			lwl_strikes[i] = 0; /* Success--clear strikes */
		    }
		}
	    } else {
                perror("connecting look who's listening socket");

		/* If we get a connection refused reply, increment the number
		   of strikes against the server.  If the server strikes out,
		   remove it from the list of server to which we send our
		   LWL information. */

		if (errno == ECONNREFUSED) {
		    if ((++lwl_strikes[i]) >= LWL_STRIKEOUT) {
			if (Debug) {
                            fprintf(stderr, "%s: abandoning LWL transmission to %s\n",
				prog, inet_ntoa(lwl_sites[i]));
			}
			lwl_ports[i] = -1;
		    }
		}
	    }
    	    close(sock);
	}
    }
    lwltimer = LWL_RETRANSMIT;
}

/*  OBTAINAUDIO  --  Attempt to obtain the audio output device.  */

static void obtainaudio(void)
{
    if (!audiok) {
	if (!soundinit(O_WRONLY)) {
            perror("opening audio output device");
            fprintf(stderr, "A common cause of this error is a sound board or\n");
            fprintf(stderr, "driver which cannot run in full duplex mode (some\n");
            fprintf(stderr, "boards which are physically capable of full duplex\n");
            fprintf(stderr, "have drivers which cannot run them in this mode).\n\n");
            fprintf(stderr, "Try uncommenting the line:\n\n");
            fprintf(stderr, "    DUPLEX = -DHALF_DUPLEX\n\n");
            fprintf(stderr, "in the Makefile, then \"make clean\", then \"make\".\n");
            fprintf(stderr, "This will operate your board in half-duplex mode\n");
            fprintf(stderr, "and avoid the conflict which probably caused the\n");
            fprintf(stderr, "error opening audio output.\n");
            return;                   /* Can't acquire sound device */
	}
	audiok = TRUE;
	if (Debug) {
            fprintf(stderr, "%s: opening audio device.\n", prog);
	}
	windtimer();		      /* Reset timer, if necessary */
    }
}

/*  FLUSHJITTER  --  Flush any buffers waiting in the jitter compensation
		     queue.  */

static void flushjitter(void)
{
    struct jitterbuf *jb;

    /* If any samples are pending in the jitter queue, flush
       them to the audio output.  */

    if ((jb = jithead) != NULL) {
	jithead = jittail = NULL;

	if (Debug) {
            fprintf(stderr, "Flushing jitter queue.\n");
	}
	obtainaudio();
	while (jb != NULL) {
	    struct jitterbuf *jo = jb;

	    if (Debug) {
		fprintf(stderr,
                        "Playing %ld samples from jitter queue.\n", jb->length);
	    }
	    if (audiok) {
		audiotime = 0;
		soundplay(jb->length, (unsigned char *) jb->value);
	    }
	    jb = jb->next;
	    free(jo);
	}
    }
}

/*  RELEASE  --  Signal-catching function which releases the audio
                 device if we haven't received anything to play in
		 the last minute. */

static void release()
{
    struct connection *c, *l, *n;

    /* Mark idle any hosts that haven't sent us anything recently. */

    c = conn;
    l = NULL;
    actives = 0;
    while (c != NULL) {
	if ((c->con_timeout >= 0) && (c->con_timeout < hosttimeout)) {
	    c->con_timeout += timerStep;
	    actives++;
	}

	/* If the busy signal timer is running, decrement it.
	   Once it expires the host becomes eligible to attempt
	   to connect once again.  In essence, the connection
	   is held open until the busy signal timeout expires, and
	   is then forced to go idle with the regular timeout
	   mechanism, making it eligible once again to attempt
	   connection. */

	if (c->con_busy > 0) {
	    c->con_busy -= timerStep;
	    if (c->con_busy <= 0) {
		c->con_busy = 0;
		c->con_timeout = hosttimeout;
		if (Debug) {
                    fprintf(stderr, "%s: %s %s busy signal timeout expired\n", prog, etime(), c->con_hostname);
		}
	    }
	}
	n = c->con_next;
	if (c->con_timeout >= hosttimeout) {
	    actives--;		      /* Oops--just went inactive */
	    c->con_timeout = -1;      /* Mark inactive */
	    if (showhosts) {
                fprintf(stderr, "%s: %s %s idle\n", prog, etime(), c->con_hostname);
	    }
	    c->con_protocol = PROTOCOL_UNKNOWN;
	    c->con_uname[0] = c->con_email[0] = 0;
	    c->con_rseq = -1;

	    /* If a face file transfer is in progress, shut it down and
	       discard the incomplete face file. */

	    if (c->face_stat == FSrequest || c->face_stat == FSreply) {
		if (c->face_file != NULL) {
		    fclose(c->face_file);
		    c->face_file = NULL;
		}
		unlink(c->face_filename);
		c->face_filename[0] = 0;
		faceTransferActive--;
	    }

            /* If there's a face viewer active, terminate it and delete
	       the face image file, if any. */

	    if (c->face_viewer > 0) {
		kill(c->face_viewer, SIGHUP);
		c->face_viewer = 0;
		if (facesDisplayed > 0) {
		    facesDisplayed--;
		}
		if (c->face_file != NULL) {
		    fclose(c->face_file);
		    c->face_file = NULL;
		}
		unlink(c->face_filename);
		c->face_filename[0] = 0;
		c->face_file = NULL;
	    }
	    c->face_stat = FSinit;

            /* If there's a PGP session key for this host, don't actually
	       release the connection buffer, as that would lose it.  Issue
	       an idle message, though, and tweak the timeout so we do this
	       only once. */

	    if (c->pgpkey[0] == 0) {
		if (l == NULL) {
		    conn = n;
		} else {
		    l->con_next = n;
		}
		free(c);
	    }
	} else {
	    int makereq = FALSE;

	    /* If a face file transfer is in progress, request
               the next block or, if it's time, re-issue the last
	       request if the timeout has expired. */

	    if (c->face_stat == FSreply) {
		makereq = TRUE;
		c->face_retry = 0;
		if (Debug) {
                    fprintf(stderr, "%s: request face data at %ld from %s\n",
			prog, c->face_address, c->con_hostname);
		}
	    } else if (c->face_stat == FSrequest) {
		c->face_timeout += timerStep;
		if (c->face_timeout >= FaceTimeout) {
		    if (Debug) {
                        fprintf(stderr, "%s: retry %d reissue face data request at %ld from %s\n",
			    prog, c->face_retry, c->face_address, c->con_hostname);
		    }
		    if (c->face_retry > FaceMaxRetries) {
			if (c->face_file != NULL) {
			    fclose(c->face_file);
			    c->face_file = NULL;
			}
			unlink(c->face_filename);
			c->face_filename[0] = 0;
			c->face_stat = FSabandoned;
			faceTransferActive--;
			if (Debug) {
                            fprintf(stderr, "%s: timeout, no face image available for %s\n",
				prog, c->con_hostname);
			}
		    } else {
			makereq = TRUE;
			c->face_retry++;
		    }
		}
	    }

	    if (makereq) {
		sbhead fsb;

		c->face_stat = FSrequest;
		c->face_timeout = 0;
		bcopy((char *) &(c->con_addr), (char *) &(name.sin_addr),
		    sizeof(struct in_addr));
		fsb.compression = htonl(fProtocol | fFaceData | faceRequest);
		fsb.buffer.buffer_len = htonl(c->face_address);
		if (sendto(sock, (char *) &fsb,
		    (int) (sizeof(struct soundbuf) - BUFL),
		    0, (struct sockaddr *) &(name), sizeof name) < 0) {
                    perror("requesting face image data");
		}
	    }

	    l = c;
	}
	c = n;
    }

    /* Release the sound device after two ticks.  This allows
       other programs to use it while we're idle. */

    if (audiok && ((audiotime += timerStep) >= AudioRelease)) {
	soundterm();
	audiok = FALSE;
	if (Debug) {
            fprintf(stderr, "%s: releasing audio device.\n", prog);
	}
	
    	/* When audio goes idle, reset the dynamic data rate computation
	   so the rate from the last connection or session doesn't bias
	   that of a subsequent connection. */
	   
    	lastPacketTime = 0;
    	averageDataRate = SAMPLE_RATE;
    	lastPacketLength = 0;

        /* Flush the record file so if we're rudely terminated it
	   will be up to date as of the last audio idle time. */

	if (record != NULL) {
	    fflush(record);
	}
    }

    /* Update our Look Who's Listening information if the
       timeout has expired. */

    lwltimer -= timerStep;
    if (sdes != NULL && lwltimer <= 0) {
	sendLwlMessage(FALSE);
    }

    /* If sound is waiting in the jitter queue, decrement the
       jitter timer and flush the queue if it decrements to zero. */

    if (jithead != NULL && jitterwait > 0) {
	jitterwait -= timerStep;
	if (jitterwait <= 0) {
	    jitterwait = 0;
	    flushjitter();
	}
    }

    /* Count down the jitter idle timer.  When it's zero, the
       first packets of an audio stream will be buffered in
       the jitter queue. */

    if (jitteridle > 0) {
	jitteridle -= timerStep;
	if (jitteridle <= 0) {
	    jitteridle = 0;
	    if (Debug) {
                fprintf(stderr, "%s: jitter idle\n", prog);
	    }
	}
    }

    /* If we still own the sound device or have a connection
       open, reset the timer. */

    windtimer();
    if (Debug) {
	time_t t = time(NULL);
        fprintf(stderr, "%s: Tick: %.2f... %s", prog,
		timerStep / 1000000.0, ctime(&t));
    }
    signal(SIGALRM, release);	      /* Set signal to handle timeout */
}

/*  EXITING  --  Catch as many program termination signals as
		 possible and clean up before exit.  */

static void exiting()
{
    struct connection *c = conn;

    if (sdes) {
	sendLwlMessage(TRUE);
    }

    /* Terminate any active face viewers. */

    while (c != NULL) {
	if (c->face_viewer > 0) {
	    if (c->face_file != NULL) {
		fclose(c->face_file);
		c->face_file = NULL;
	    }
	    kill(c->face_viewer, SIGHUP);
	    unlink(c->face_filename);
	}
	c = c->con_next;
    }

    exit(0);
}

/*  CHILDTERM	--  Handle termination of a face viewer child
		    process and any sfmikes.  This is necessary both to avoid
		    viewers the user terminates manually becoming
		    zombie processes, and also to delete the
		    face file no longer associated with a viewer. */

static void childterm()
{
    int status, pid;

    while ((pid = 
#ifdef Solaris
		  /* Should work for any System V Unix. */
		  waitpid(0 /* or -1 */,&status, WNOHANG)
#else
		  /* BSD and IRIX implement wait3(). */
		  wait3(&status, WNOHANG, 0)
#endif
	   ) > 0) {
	if (Debug) {
            fprintf(stderr, "Child process %d done\n", pid);
	}
	if (WIFEXITED(status) || WIFSIGNALED(status)) {
	    struct connection *c = conn;

	    while (c != NULL) {
		if (pid == (int) c->face_viewer) {
		    c->face_viewer = 0;
		    if (c->face_file != NULL) {
			fclose(c->face_file);
			c->face_file = NULL;
		    }
		    unlink(c->face_filename);
		    if (Debug) {
                        fprintf(stderr, "Face viewer pid %d terminated.  Deleting %s\n", pid, c->face_filename);
		    }
		    c->face_filename[0] = 0;
		    if (facesDisplayed > 0) {
			facesDisplayed--;
			if (facesDisplayed == 0) {
			}
		    }
		}

#ifdef NAT_LAUNCH
		if (pid == c->sfmike_pid) {
		    if (Debug) {
			if (WIFEXITED(status))
			    fprintf(stderr,  "sfmike terminated with status %d\n",
				    WEXITSTATUS(status));
			if (WIFSIGNALED(status))
			    fprintf(stderr, "sfmike terminated with signal %d\n",
				    WTERMSIG(status));
		    }
		    c->sfmike_pid = 0;
		    printf("Transmission terminated. Continue listening [y/n]? ");
		    fflush(stdout);
		    if (getc(stdin) == 'n') {
			exit(1);
		    }
		}
#endif
		c = c->con_next;
	    }
	}
    }
    signal(SIGCHLD, childterm);     	/* Set signal to handle face viewer termination */
}

/*  PLAYBUFFER	--  Send next buffer to audio output. */

static void playbuffer(soundbuf *msg, struct connection *c)
{
    char *val;
    LONG len;
    unsigned long cmodes;
    char auxbuf[BUFL + 2], bbuf[8], tbuf[8];
    struct timeval tp;
    struct timezone tzp;
    double packetTime = 0;

    if (adjustRate) {
	gettimeofday(&tp, &tzp);	  /* Time stamp this packet's arrival time, in microseconds */
	packetTime = (tp.tv_sec - timebase) * 1000000.0 + tp.tv_usec;
/*fprintf(stderr, "Packet = %.0f, lastPacket = %.0f, delta = %.0f\n", packetTime, lastPacketTime, packetTime - lastPacketTime);*/
    }

    debugging = (msg->compression & fDebug) ? TRUE : FALSE;
    
    audiotime = 0;		      /* Reset timeout counter */
    obtainaudio();		      /* Open audio output if necessary */

    if (showhosts && (c->con_compmodes != (fCompressionModes &
	    msg->compression))) {
	c->con_compmodes = fCompressionModes & msg->compression;
        fprintf(stderr, "%s: %s sending %s.\n", prog, c->con_hostname,
		compressionType(msg));
    }

    len = msg->buffer.buffer_len;
    val = msg->buffer.buffer_val;
    
    /*	Long we get here, we should have already verified that the
    	length in the packet is consistent with the length of
	data actually read from the socket which, in turn, is
	guaranteed to be limited to BUFL--our maximum buffer size.
	Just as a sanity check, and prior to the specific checks
	made below based on details of the protocol, make sure
	something didn't slip by which might cause us to index
	off the end of a buffer.  We allow for worst-case round-up
	of the size in the buffer due to encryption in making
	the comparison.  */
	
    if (((len + 15) & (~15)) >= BUFL) {
    	fprintf(stderr, "%s: ** Sound buffer from %s in %s of %ld bytes\n"
	    	    	"       exceeds maximum size of %d bytes--discarded. **\n",
	    prog, c->con_hostname, compressionType(msg), len, BUFL);
	return;
    }

    if (Debug) {
        fprintf(stderr, "%s: playing %ld %s bytes from %s.\n", prog, len,
		compressionType(msg), c->con_hostname);
    }

    /* If the fSetDest bit is on, use the fDestJack bit to re-route
       the sound.  This is normally used to divert the sound to the
       speaker to get an individual's attention.  This can be
       disabled with the -N option.  */

    if ((msg->compression & fSetDest) && !noring) {
	sounddest((msg->compression & fDestJack) ? 1 : 0);
	if (!(msg->compression & fDestJack)) {
	    if (audiok) {
		soundplayvol(50);     /* Make sure volume high enough */
	    }
	}
    }

#ifdef CRYPTO

    /* If message is encrypted, decrypt. Other than round-up to the
       encryption frame size, which has already been accounted for
       in the sanity check of packet length above, decryption never
       expands the sound buffer, so we needn't worry about additional
       overflow checks here. */

    /*	Key file  */
    if ((msg->compression & fEncOTP) && (curotp != NULL)) {
	int i;
	LONG slen = (len + 15) & (~15);

	if (Debug) {
            fprintf(stderr, "%s: decrypting %ld bytes with key file.\n", prog, len);
	}
	for (i = 0; i < slen; i ++) {
	    val[i] ^= curotp[i];
	}
    }

    /*  PGP (IDEA)  */
    if ((msg->compression & fEncPGP) && c->pgpkey[0]) {
	unsigned short iv[4];
	LONG slen = (len + 7) & (~7);

	bzero(iv, sizeof(iv));
	initcfb_idea(iv, (unsigned char *) (c->pgpkey + 1), TRUE);

	if (Debug) {
            fprintf(stderr, "%s: decrypting %ld bytes with public key session key.\n", prog, slen);
	}
	ideacfb((unsigned char *) val, slen);
	close_idea();
    }

    /*	AES  */
    if ((msg->compression & fEncAES) && aes_spec) {
	LONG slen = (len + 15) & (~15);

	if (Debug) {
            fprintf(stderr, "%s: decrypting %ld bytes with AES key.\n", prog, slen);
	}
	AES_cbc_decrypt((unsigned char *) val, (unsigned char *) val,
		    slen, &daes_ctx);
    }

    /*	Blowfish  */
    if ((msg->compression & fEncBF) && blowfish_spec) {
	unsigned char iv[8];
	LONG slen = (len + 7) & (~7);

	bzero(iv, sizeof(iv));
	if (Debug) {
            fprintf(stderr, "%s: decrypting %ld bytes with Blowfish key.\n", prog, slen);
	}
	BF_cbc_encrypt((unsigned char *) val, (unsigned char *) val,
		       slen, &blowfishkey, iv, BF_DECRYPT);
    }

    /*	IDEA  */
    if ((msg->compression & fEncIDEA) && ideakey[0]) {
	unsigned short iv[4];
	LONG slen = (len + 7) & (~7);

	bzero(iv, sizeof(iv));
	initcfb_idea(iv, (unsigned char *) (ideakey + 1), TRUE);

	if (Debug) {
            fprintf(stderr, "%s: decrypting %ld bytes with IDEA key.\n", prog, slen);
	}
	ideacfb((unsigned char *) val, slen);
	close_idea();
    }

    /*	DES  */
    if ((msg->compression & fEncDES) && deskey[0]) {
	int i;
	LONG slen = (len + 7) & (~7);

	setkey_sf(deskey + 1);

	if (Debug) {
            fprintf(stderr, "%s: decrypting %ld bytes with DES key.\n", prog, slen);
	}
	for (i = 0; i < slen; i += 8) {
	    bcopy(val + i, tbuf, 8);
	    dedes(val + i);

	    /* Reverse cipher block chaining. */

	    if (i > 0) {
		int j;

		for (j = 0; j < 8; j++) {
		    val[(i + j)] ^= bbuf[j];
		}
	    }
	    bcopy(tbuf, bbuf, 8);
	}
    }
#else
    if ((msg->compression & (fEncDES | fEncOTP | fEncIDEA | fEncPGP | fEncBF | fEncAES)) &&
	!(c->con_crypt_warning)) {
	c->con_crypt_warning = TRUE;
        fprintf(stderr, "%s: Warning: host %s is sending encrypted audio\n",
		prog, c->con_hostname);
        fprintf(stderr, "   which cannot be decoded by this version of\n");
        fprintf(stderr, "   Speak Freely, which was built with encryption\n");
        fprintf(stderr, "   removed to permit redistribution without concern\n");
        fprintf(stderr, "   for export control and other regulations.\n");
        fprintf(stderr, "   You can download a version of Speak Freely which\n");
        fprintf(stderr, "   includes full encryption from:\n");
        fprintf(stderr, "       http://www.fourmilab.ch/speakfree/unix/\n");
    }
#endif

    /* With the exception of 2X compression, which may be selected
       by itself or combined with any other mode, compression modes
       are mutually exclusive.  Verify that this is the case in the
       packet before proceeding with decompression.  Any possible
       damage which may be caused by this will be averted by the
       length checks in the decoding sequence below, but identifying
       the source of the error before we blither into the
       decoders makes diagnosis easier, which is important since
       packets specifying multiple compression modes are an obvious
       way malicious users may try to provoke buffer overflows. */
       
    cmodes = msg->compression & (fCompressionModes ^ fComp2X);
    if (cmodes != 0) {
    	while ((cmodes & 1) == 0) {
	    cmodes >>= 1;
	}
	if (cmodes != 1) {
	    fprintf(stderr, "%s: ** Audio packet from %s specifies multiple mutually "
	    	    	    "exclusive compression modes: %08lX.  Dropped. **\n",
			    prog, c->con_hostname, msg->compression);
	    return;
	}
    }

    /* If message is compressed, decompress appropriately.  Here we get
       quite strict enforcing packet size constraints.  Regardless of the
       provenance of the packet, depending on its compression and protocol
       there is a maximum packet size which will never be exceeded by any
       valid message, and these sizes are well below the threshold at which
       a buffer overflow might occur.  Based on the compression mode, we
       check against the maximum and discard the packet if the contents
       are too long to be valid.  For some protocols we can also test that
       the payload is an even number of encoded frames in the specified
       compression mode.  Notwithstanding these checks, the CODECs still must
       be invulnerable to garbage data which passes these length tests.
       
       Maximum packet payload lengths, by compression type and protocol,
       are as follows.  Protocols with a fixed frame size give the frame
       size after the protocol name.  Many compression types embed the
       actual decompressed length in the first two bytes of the payload,
       accounting for the total being two bytes greater than an even number
       of frames.  ADPCM appends its 3 byte state to the end of the compressed
       audio stream.

            	    	    Speak Freely    	RTP 	    VAT
			    
    	Uncompressed	    	488 	    	320 	    320
       
    	Simple	    	    	488
       
    	GSM 	[33]	    	332 	    	134 	    134
    	    +Simple 	    	332
	 
    	ADPCM	    	    	489 	    	323 	    323
	    +Simple 	    	490
	
	LPC 	[14] 	    	142 	    	 58 	     58
	    +Simple 	    	142
	    
	LPC10	 [7]	    	 70 
	    +Simple 	    	 70
	    
	CELP	[18]	    	146
	    +Simple 	    	146
	    
    */

#define Checklen(n, t) 	if (msg->buffer.buffer_len > (n)) { \
    	    fprintf(stderr, "%s: ** Payload (%ld) bytes from %s exceeds maximum of %d bytes for " \
	    	    	    t " compression. **\n", prog, msg->buffer.buffer_len, c->con_hostname, n); \
	    return; }

#define Checkframe(f, p, t)    if (((msg->buffer.buffer_len - (p)) % (f)) != 0) { \
    	    fprintf(stderr, "%s: ** Payload (%ld+%d pad) bytes from %s is not an even number of %d byte frames for " \
	    	    	    t " compression. **\n", prog, msg->buffer.buffer_len - (p), p, c->con_hostname, f); \
	    return; }

    if (msg->compression & fCompVOX) {
    	Checklen(332, "VOX/GSM");
	Checkframe(33, 2, "VOX/GSM");
	vox_gsmdecomp(msg);
	len = msg->buffer.buffer_len;
    }

    if (msg->compression & fCompGSM) {
    	Checklen(332, "GSM");
	Checkframe(33, 2, "GSM");
	gsmdecomp(msg);
	len = msg->buffer.buffer_len;
    }

    if (msg->compression & fCompADPCM) {
    	Checklen(490, "ADPCM");
	adpcmdecomp(msg);
	len = msg->buffer.buffer_len;
    }

    if (msg->compression & fCompLPC) {
    	Checklen(142, "LPC");
	Checkframe(14, 2, "LPC");
	lpcdecomp(msg, c);
	len = msg->buffer.buffer_len;
    }

    if (msg->compression & fCompLPC10) {
    	Checklen(70, "LPC10");
	Checkframe(7, 0, "LPC10");
	lpc10decomp(msg, c);
	len = msg->buffer.buffer_len;
    }

    if (msg->compression & fCompCELP) {
    	Checklen(146, "CELP");
	Checkframe(18, 2, "CELP");
	celpdecomp(msg);
	len = msg->buffer.buffer_len;
    }

    if (msg->compression & fComp2X) {
	int is = len, os = len * 2;

    	/*  The following Checklen must be sufficiently large to handle
	    the maximum result of any of the above decoders.  At the moment,
	    this is the 1920 bytes resulting from CELP+2X compression.  */
    	Checklen(1920, "2X");
	rate_start(SAMPLE_RATE / 2, SAMPLE_RATE);
	rate_flow((unsigned char *) val, (unsigned char *) auxbuf, &is, &os);
    	bcopy(auxbuf, val, os);
	len = os;
    }
    
    /* Update the dynamic sample arrival rate and the exponentially
       smoothed moving average of the same.  To avoid short-term jitter,
       we first average the arrival rate by computing it only after a
       total of DYNRATE_UPDATE_SAMPLE_COUNT samples have arrived (in
       however many packets it takes).  These individual measures
       are then smoothed with an exponentially smoothed moving
       average with smoothing factor DYNRATE_SMOOTHING_FACTOR to
       determine the average arrival rate. */
       
    lastPacketLength += len;
    if (adjustRate && (lastPacketLength > DYNRATE_UPDATE_SAMPLE_COUNT)) {
    	if (lastPacketTime != 0) {
	    int oldRate;
    	    double idrate = lastPacketLength / ((packetTime - lastPacketTime) / 1e6);	/* Instantaneous data rate */
	    
	    if ((idrate > ((SAMPLE_RATE * 9) / 10)) && (idrate < ((SAMPLE_RATE * 11) / 10))) {
/*
fprintf(stderr, "lastPacketLength = %ld, elapsedTime = %.6f\n", lastPacketLength, (packetTime - lastPacketTime) / 1e6);
*/
		averageDataRate = averageDataRate + ((idrate - averageDataRate) * DYNRATE_SMOOTHING_FACTOR);
    		if (Debug) {
		    fprintf(stderr, "%s: data rate (samples/sec): instantaneous %.0f, average %.0f\n", prog,
	    		idrate, averageDataRate);
		}
		
		/* Now adjust the output rate to dispose of data at the
		   smoothed arrival rate of input.  Note that we only adjust
		   the rate if it is greater than the nominal SAMPLE_RATE; this
		   creates the bias which prevents secular growth in the output
		   queue.  Further, the rate we compute is based on outRate,
		   which can be adjusted by the -fx.x option to compensate for
		   a clock on the audio hardware which has a bias with regard to
		   the CPU clock we use to time sample arrival rate. */
		   
		oldRate = currentRate;

		currentRate = (averageDataRate <= SAMPLE_RATE) ? SAMPLE_RATE :
		    	    	((outRate * SAMPLE_RATE) / averageDataRate);
		if (Debug && (currentRate != oldRate)) {
		    fprintf(stderr, "%s: dynamically adjusting output rate to %d samples/sec.\n", prog, currentRate);
		}
	    }
/*
else {
fprintf(stderr, "Ignored ridiculous instantaneous data rate of %.0f\n", idrate);
}
*/
	}
    	lastPacketTime = packetTime;
	lastPacketLength = 0;
    }
    
    /* If output rate adjustment is requested, re-sample the data
       in the sound buffer to the requested rate.  If the requested
       rate would cause the output buffer to overflow, the length is
       limited to that of the buffer, truncating any additional
       audio in the packet.  In practice, this only happens with
       absurdly large rate reductions which would render the audio
       incomprehensible in any case. */
           
    if (currentRate != SAMPLE_RATE) {
	int is = len, os = ((len * currentRate) + 7999) / SAMPLE_RATE;

    	if (os > BUFL) {
	    os = BUFL;	    	/* Constrain length to output buffer size */
	}
	rate_start(SAMPLE_RATE, currentRate);
	rate_flow((unsigned char *) val, (unsigned char *) auxbuf, &is, &os);
	bcopy(auxbuf, val, os);
	len = os;
	if (Debug) {
            fprintf(stderr, "%s: resampled to %ld samples at %d samples/sec.\n", prog, len, currentRate);
	}	
    }

    /* If the jitter idle time has elapsed, reset the jitter
       wait to queue the start of this packet sequence. */

    if (jitter > 0 && jitteridle == 0 && jitterwait == 0) {
	jitterwait = jitter;
	if (Debug) {
            fprintf(stderr, "%s: starting %ld millisecond jitter delay.\n", prog, jitterwait / 1000L);
	}
    }

    /* Receipt of any sound buffer resets the jitter idle timer. */

    jitteridle = jitteridlet;	      /* Reset jitter idle countdown */

    /* If the jitter wait has not yet expired, add the samples to
       the queue waiting to be played at the end of the jitter
       wait.  */

    if (jitterwait > 0) {
	struct jitterbuf *jb;

	jb = (struct jitterbuf *) malloc(sizeof(struct jitterbuf) + len);
	if (jb != NULL) {
	    bcopy(val, jb->value, len);
	    jb->length = len;
	    jb->next = NULL;
	    if (jittail != NULL) {
		jittail->next = jb;
	    }
	    jittail = jb;
	    if (jithead == NULL) {
		jithead = jb;
		windtimer();
	    }
	    if (Debug) {
                fprintf(stderr, "%s: adding %ld samples to jitter queue.\n",
			prog, len);
	    }
	}
    } else {
	flushjitter();
	if (audiok) {
	    soundplay(len, (unsigned char *) val);
	}
    }
    if (record != NULL) {
	fwrite(val, len, 1, record);
    }
}

/*  PROGNAME  --  Extract program name from argv[0].  */

static char *progname(char *arg)
{
    char *cp = strrchr(arg, '/');

    return (cp != NULL) ? cp + 1 : arg;
}

/*  USAGE  --  Print how-to-call information.  */

static void usage(void)
{
    V fprintf(stderr, "%s  --  Speak Freely sound receiver.\n", prog);
    V fprintf(stderr, "               %s.\n", Relno);
    V fprintf(stderr, "\n");
    V fprintf(stderr, "Usage: %s [options]\n", prog);
    V fprintf(stderr, "Options:\n");
    V fprintf(stderr, "           -A\"file command\" Create answer command in file\n");
    V fprintf(stderr, "           -B\"command\"      Execute command for busy signal\n");
#ifdef CRYPTO
    V fprintf(stderr, "           -BAkey           AES decrypt with key (128 bit encryption)\n");
    V fprintf(stderr, "           -BFkey           Blowfish decrypt with key\n");
    V fprintf(stderr, "           -BXkey           AES encrypt with hexadecimal key (128-256 bit encryption)\n");
#endif
    V fprintf(stderr, "           -D               Force debug output\n");
    V fprintf(stderr, "           -E\"command\"      Execute command on incoming call\n");
    V fprintf(stderr, "           -Ffactor         Increase output rate by factor (+/-, floating point)\n");
    V fprintf(stderr, "           -FA              Auto-adjust output rate to match incoming data rate\n");
#ifdef NAT_LAUNCH
    V fprintf(stderr, "           -G               Passed to sfmike by -NAT (autogain)\n");
#endif
#ifdef CRYPTO
    V fprintf(stderr, "           -Ikey            IDEA decrypt with key\n");
#endif
    V fprintf(stderr, "           -Jwait,idle      Jitter delay wait and idle in milliseconds\n");
#ifdef CRYPTO
    V fprintf(stderr, "           -Kkey            DES decrypt with key\n");
#endif
#ifdef MULTICAST
    V fprintf(stderr, "           -Mhost/ip        Join multicast to given name or IP address\n");
#endif
    V fprintf(stderr, "           -N               Disable remote ring requests\n");
#ifdef NAT_LAUNCH
    V fprintf(stderr, "           -NAT[host[:port]]  Initiate shared-socket connection to host (use with NAT firewall)\n");
    V fprintf(stderr, "           -NClpc|lpc10[rn]|celp|f[c]|t]  Specify compression modes for NAT-launched connections\n");
#endif
#ifdef CRYPTO
    V fprintf(stderr, "           -Ofile           Use file as key file\n");
#endif
    V fprintf(stderr, "           -Pport           Listen on given port\n");
    V fprintf(stderr, "           -Q               Prevent debug output\n");
    V fprintf(stderr, "           -R[+]file        Record [append] sound in file\n");
#ifdef NAT_LAUNCH
    V fprintf(stderr, "           -Slevel[,timeout] Passed to sfmike by -NAT (squelch)\n");
#endif
    V fprintf(stderr, "           -U               Print this message\n");
    V fprintf(stderr, "           -Vtimeout        Show hostnames that connect\n");
    V fprintf(stderr, "           -W               Publish LWL information for sfvod\n");
#ifdef HEXDUMP
    V fprintf(stderr, "           -X               Dump packets in hex\n");
#endif
#ifdef AUDIO_DEVICE_FILE
    V fprintf(stderr, "           -Youtdev[:ctldev] Override default audio device file name or specify open #fd\n");
#endif
#ifdef CRYPTO
    V fprintf(stderr, "           -Z\"phrase\"       Set public key exchange secret key pass phrase\n");
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
    int i, j, k, l, length;
    struct soundbuf sb;
    FILE *fp;
    struct connection *c;
    char *cp;
    int newconn, wasrtp;
    struct auhdr {		      /* .au file header */
	char magic[4];
	long hsize, dsize, emode, rate, nchan;
    };
    static struct auhdr afh = { ".snd", sizeof(struct auhdr), 1, 1, 8000, 1 };
#ifdef NAT_LAUNCH
    struct mikeopts_t *mikeopts = NULL;
    struct xn_conspec_t *cur_xnat_conspec = NULL;
#define Gbuf(ptr, type, size) if ((ptr = (type) malloc(size)) == NULL) { \
    fprintf(stderr, "%s: Unable to allocate NAT sfmike options buffer, size %d.\n", prog, size); \
    return 2; }
#endif

    prog = progname(argv[0]);

    /* Save the number of full seconds at progam start time.  We subtract this
       from packet arrival times measured in microseconds to minimise floating
       point round-off in subsequent computations. */
    {
	struct timeval tp;
	struct timezone tzp;
	gettimeofday(&tp, &tzp);
	timebase = tp.tv_sec;
    }

    /* First pass option processing.  We have to first scan
       the options to handle any which affect creation of the
       socket.	On the second pass we can assume the socket
       already exists, allowing us to join multicast groups,
       etc. */

    for (i = 1; i < argc; i++) {
	char *op, opt;

	op = argv[i];
        if (*op == '-') {
	    opt = *(++op);
	    if (islower(opt)) {
		opt = toupper(opt);
	    }

	    switch (opt) {

                case 'D':             /* -D  --  Force debug output */
		    debugforce = TRUE;
		    break;

                case 'P':             /* -Pport  --  Port to listen on */
		    if (op[1] != 0) {
			whichport = atoi(op + 1);
		    }
		    break;

                case 'U':             /* -U  --  Print usage information */
                case '?':             /* -?  --  Print usage information */
		    usage();
		    return 0;

                case 'W':           /* -Wport  --  Publish LWL information for sfvod */
		    lwlonly = TRUE;
		    if (op[1] != 0) {
			whichport = atoi(op + 1);
		    }
		    break;
	    }
	} else {
	    usage();
	    return 2;
	}
    }

    if (!lwlonly) {

	/* Create the sockets from which to read */

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
            perror("opening data socket");
	    return 1;
	}

	ssock = socket(AF_INET, SOCK_DGRAM, 0);
	if (ssock < 0) {
            perror("opening control socket");
	    return 1;
	}

	/* Create name with wildcards. */

	name.sin_family = AF_INET;
	name.sin_addr.s_addr = INADDR_ANY;
	name.sin_port = htons(whichport);
	if (bind(sock, (struct sockaddr *) &name, sizeof name) < 0) {
            perror("binding data socket");
	    return 1;
	}
	name.sin_port = htons(whichport + 1);
	if (bind(ssock, (struct sockaddr *) &name, sizeof name) < 0) {
            perror("binding control socket");
	    return 1;
	}
	name.sin_port = htons(whichport);

	/*  Process command line options.  */

        pgppass = getenv("PGPPASS");

#if Internet_Port != 2074
	/* This is a reminder to the author who frequently tests with
	   non-standard ports to avoid inadvertently releasing a
	   production version configured for the wrong port. */
        fprintf(stderr, "%s: warning, listening on non-standard Internet port %d\n",
	    prog, Internet_Port);
#endif

	for (i = 1; i < argc; i++) {
	    char *op, opt;

	    op = argv[i];
            if (*op == '-') {
		opt = *(++op);
		if (islower(opt)) {
		    opt = toupper(opt);
		}

		switch (opt) {

                    case 'A':             /* -A"file command"  --  Create reply command on connect */
			{
                            char *cp = strchr(op + 1, ' ');

			    if (cp != NULL) {
				replycmd = cp + 1;
				*cp = 0;
			    }
			    replyfile = op + 1;
			}
			break;		  

                    case 'B':             /* -B"command"  --  Execute command for busy signal. */
#ifdef CRYPTO
					  /* -BAkey  -- Set AES key */ 
                        if (ucase(op[1]) == 'A') {
			    if (strlen(op + 2) == 0) {
				aes_spec = FALSE;
			    } else {
			    	struct MD5Context md5c;
			    	unsigned char md5sig[32];
			    	int klen = 16;
			    	char *sep = NULL;

				aes_spec = TRUE;
#ifdef NAT_LAUNCH
				if (mikeopts == NULL) {
				    Gbuf(mikeopts, struct mikeopts_t *, sizeof(struct mikeopts_t));
				}
				Gbuf(mikeopts->crypto, char *, strlen(op + 1) + 3);
				strcpy(mikeopts->crypto, "-b");
				strcat(mikeopts->crypto, op + 1);
#endif
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
			    	daes_ctx.n_rnd = daes_ctx.n_blk = 0;
			    	aes_dec_key(md5sig, klen, &daes_ctx);
				if (Debug) {
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
				aes_spec = FALSE;
			    } else {
				unsigned char aesk[32];
				int kb = 0, kn = 0, p = 2;

#ifdef NAT_LAUNCH
				if (mikeopts == NULL) {
				    Gbuf(mikeopts, struct mikeopts_t *, sizeof(struct mikeopts_t));
				}
				Gbuf(mikeopts->crypto, char *, strlen(op + 1) + 3);
				strcpy(mikeopts->crypto, "-b");
				strcat(mikeopts->crypto, op + 1);
#endif
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

				/*  Round up key bytes specified to next valid multiple.
			    	    Note that vacant key bits have already been filled
				    with zeroes.  */

    	    	    	    	if (kn != 0) {
				    kb++;
				}
				kb = (kb <= 16) ? 16 :
			    		((kb <= 24) ? 24 : 32);

				aes_spec = TRUE;
				daes_ctx.n_rnd = daes_ctx.n_blk = 0;
				aes_dec_key(aesk, kb, &daes_ctx);
				if (Debug) {
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
				blowfish_spec = FALSE;
			    } else {
				struct MD5Context md5c;
				unsigned char bfvec[16];

				blowfish_spec = TRUE;
#ifdef NAT_LAUNCH
				if (mikeopts == NULL) {
				    Gbuf(mikeopts, struct mikeopts_t *, sizeof(struct mikeopts_t));
				}
				Gbuf(mikeopts->crypto, char *, strlen(op + 1) + 3);
				strcpy(mikeopts->crypto, "-b");
				strcat(mikeopts->crypto, op + 1);
#endif
				MD5Init(&md5c);
				MD5Update(&md5c, op + 2, strlen(op + 2));
				MD5Final(bfvec, &md5c);
				BF_set_key(&blowfishkey, 16, bfvec);
				if (Debug) {
                                    fprintf(stderr, "Blowfish key:");
				    for (j = 0; j < 16; j++) {
                                        fprintf(stderr, " %02X", (bfvec[j] & 0xFF));
				    }
                                    fprintf(stderr, "\n");
				}
			    }
			} else
#endif
			{
			    if (strlen(op + 1) > 0) {
                                if (!strstr(op + 1, "%s")) {
				    fprintf(stderr,
  "%s: invalid -B option command: must contain %%s for IP address insertion.\n",
				      prog);
				} else {
				    busysignal = op + 1;
				    dobusy = TRUE;
				}
			    } else {
				dobusy = TRUE;
			    }
			}
			break;

                    case 'E':             /* -E"command"  --  Execute command on connect */
			replycmdexe = op + 1;
			break;
			
		    case 'F':	    	  /* -Ffactor  -- Adjust output rate by specified factor */
		    	    	    	  /* -FA       -- Automatically adjust output rate to incoming rate */
		    	if (ucase(op[1]) == 'A') {
			    adjustRate = TRUE;
		    	} else {
			    int zop = 1, facok = FALSE;
			    
			    if ((op[1] == '+') || (op[1] == '-')) {
			    	zop++;
			    }
			    if (isdigit(op[zop]) || ((op[zop] == '.') && isdigit(op[zop + 1]))) {
				outRate = SAMPLE_RATE - (SAMPLE_RATE * (atof(op + 1) / 100));
				if (outRate > 0) {
				    facok = TRUE;
				    if (Debug) {
				    	fprintf(stderr, "Output will be rate adjusted to %d samples per second.\n",
					    outRate);
				    }
    	    	    	    	}
			    }
			    if (!facok) {
			    	fprintf(stderr, "%s: invalid -F option argument: negative output rate; ignored.\n",
				    prog);
				outRate = SAMPLE_RATE;
			    }
			    currentRate = outRate;
			}
			break;

#ifdef CRYPTO
                    case 'I':             /* -Ikey  --  Set IDEA key */
			if (strlen(op + 1) == 0) {
			    ideakey[0] = FALSE;
			} else {
			    struct MD5Context md5c;

#ifdef NAT_LAUNCH
			    if (mikeopts == NULL) {
				Gbuf(mikeopts, struct mikeopts_t *, sizeof(struct mikeopts_t));
			    }
			    Gbuf(mikeopts->crypto, char *, strlen(op + 1) + 3);
			    strcpy(mikeopts->crypto, "-i");
			    strcat(mikeopts->crypto, op + 1);
#endif
			    MD5Init(&md5c);
			    MD5Update(&md5c, op + 1, strlen(op + 1));
			    MD5Final(ideakey + 1, &md5c);
			    ideakey[0] = TRUE;
			    if (Debug) {
                                fprintf(stderr, "IDEA key:");
				for (j = 0; j < 16; j++) {
                                    fprintf(stderr, " %02X", (ideakey[j + 1] & 0xFF));
				}
                                fprintf(stderr, "\n");
			    }
			}
			break;
#endif

                    case 'J':             /* -Jwait,idle --  Set jitter wait and idle times */
			if (isdigit(*(op + 1))) {
                            char *cp = strchr(op + 1, ',');

			    if (cp != NULL) {
				*cp = 0;
			    }
			    jitter = atoi(op + 1) * 1000L;
			    jitteridlet = (cp == NULL) ? (jitter * 2) :
				(atoi(cp + 1) * 1000L);
			    if (jitter < 250000) {
				fprintf(stderr,
                     "%s: invalid jitter wait %d--reset to 250 milliseconds.\n", 
				    prog, jitter / 1000);
				jitter = 250000;
			    }
			    if (jitteridlet < 250000) {
				fprintf(stderr,
                     "%s: invalid jitter idle time %d--reset to 250 milliseconds.\n", 
				    prog, jitteridlet / 1000);
				jitteridlet = 250000;
			    }
			} else {
			    jitteridlet = 2 * (jitter = 1000000L);
			}
			break;

#ifdef CRYPTO
                    case 'K':             /* -Kkey  --  Set DES key */
			desinit(1);	  /* Initialise the DES library */
			if (strlen(op + 1) == 0) {
			    deskey[0] = rtpdeskey[0] = vatdeskey[0] = FALSE;
			} else {
			    struct MD5Context md5c;
			    char md5key[16], algorithm[16];

#ifdef NAT_LAUNCH
			    if (mikeopts == NULL) {
			    	Gbuf(mikeopts, struct mikeopts_t *, sizeof(struct mikeopts_t));
			    }
			    Gbuf(mikeopts->crypto, char *, strlen(op) + 2);
			    strcpy(mikeopts->crypto, "-");
			    strcat(mikeopts->crypto, op);
#endif
			    MD5Init(&md5c);
			    MD5Update(&md5c, op + 1, strlen(op + 1));
			    MD5Final(md5key, &md5c);
			    for (j = 0; j < 8; j++) {
				deskey[j + 1] = (char)
					      ((md5key[j] ^ md5key[j + 8]) & 0x7F);
			    }
			    deskey[0] = TRUE;
			    des_string_to_key(op + 1, (des_cblock *) (vatdeskey + 1));
			    string_DES_key(op + 1, (unsigned char *) (rtpdeskey + 1), algorithm);
                            if (strcmp(algorithm, "DES-CBC") != 0) {
                                fprintf(stderr, "Unsupported encryption algorithm: %s.  Only DES-CBC is available.\n",
					algorithm);
				return 2;
			    }
			    rtpdeskey[0] = vatdeskey[0] = TRUE;
			    if (Debug) {
                                fprintf(stderr, "DES key:");
				for (j = 0; j < 8; j++) {
                                    fprintf(stderr, " %02X", (deskey[j + 1] & 0xFF));
				}
                                fprintf(stderr, "\n");
                                fprintf(stderr, "RTP key:");
				for (j = 0; j < 8; j++) {
                                    fprintf(stderr, " %02X", (rtpdeskey[j + 1] & 0xFF));
				}
                                fprintf(stderr, "\n");
                                fprintf(stderr, "VAT key:");
				for (j = 0; j < 8; j++) {
                                    fprintf(stderr, " %02X", (vatdeskey[j + 1] & 0xFF));
				}
                                fprintf(stderr, "\n");
			    }
			}
			break;
#endif

#ifdef MULTICAST
                    case 'M':             /* -Mhost/ip -- Join multicast to name/IP number */
			{
			    struct ip_mreq m;
			    struct hostent *h;
			    long naddr;

                            /* If it's a valid IP number, use it.  Otherwise try to look
			       up as a host name. */

			    if ((naddr = inet_addr(op + 1)) == -1) {
				h = gethostbyname(op + 1);
				if (h == 0) {
                                    fprintf(stderr, "%s: unknown multicast group\n", op + 1);
				    return 2;
				}
				bcopy((char *) h->h_addr, (char *) &naddr, sizeof naddr);
			    }
			    m.imr_multiaddr.s_addr = naddr;
			    m.imr_interface.s_addr = htons(INADDR_ANY);
			    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
				(char *) &m, sizeof m) < 0) {
                                perror("joining multicast group");
				return 2;
			    }
			}
			break;
#endif

                    case 'N':             /* -N  --  Disable remote ring requests */
#ifdef NAT_LAUNCH
    	    	    	    	    	  /* -NAThost[:port]  --  Connect to NAT host */
		    	if ((ucase(op[1]) == 'A') && (ucase(op[2]) == 'T')) {
			    if (strlen(op + 3) > 0) {
				cur_xnat_conspec = new_xnat_conspec(op + 3);
				Gbuf(cur_xnat_conspec->xnat_conaddr, char *, strlen(op + 3) + 1);
				strcpy(cur_xnat_conspec->xnat_conaddr, op + 3);
			    } else {
				xnat_passive = TRUE;
			    }
			    	    	  /* -NCmode  --  Specify compression mode for NAT connection */
			} else if (ucase(op[1]) == 'C') {
			    if (strlen(op + 2) > 0) {
				if (mikeopts == NULL) {
				    Gbuf(mikeopts, struct mikeopts_t *, sizeof(struct mikeopts_t));
				}
				Gbuf(mikeopts->compression, char *, strlen(op + 2) + 2);
				strcpy(mikeopts->compression, "-");
				strcat(mikeopts->compression, op + 2);
			    }
			} else {
#endif
			    noring = TRUE;
#ifdef NAT_LAUNCH
			}
#endif
			break;

#ifdef CRYPTO
                    case 'O':             /* -Ofile -- Use file as key file */
                        fp = fopen(op + 1, "r");
			if (fp == NULL) {
                            perror("Cannot open key file");
			    return 2;
			}
			curotp = malloc(BUFL);
			if (curotp == NULL) {
                            fprintf(stderr, "%s: Cannot allocate key file buffer.\n", prog);
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
			break;
#endif

                    case 'Q':             /* -Q  --  Prevent debug output */
			debugforce = -1;
			break;

                    case 'R':             /* -Rfile  --  Answering machine record */
			{
			    char *fn = op + 1;
			    int append = FALSE;

                            if (*fn == '+') {
				fn++;
				append = TRUE;
			    }
                            record = fopen(fn, append ? "a" : "w");
			    if (record == NULL) {
                                perror("Cannot create answering machine file");
				return 2;
			    }
			    fseek(record, 0L, 2);
			    if (ftell(record) == 0) {
				fwrite(&afh, sizeof afh, 1, record);
			    }
			    fflush(record);
			}
			break;
			
#ifdef NAT_LAUNCH
		    case 'S':
			if (strlen(op + 1) != 0) {
			    if (mikeopts == NULL) {
			    	Gbuf(mikeopts, struct mikeopts_t *, sizeof(struct mikeopts_t));
			    }
			    Gbuf(mikeopts->squelch, char *, strlen(op) + 3);
			    strcpy(mikeopts->squelch, "-s");
			    strcat(mikeopts->squelch, op + 1);
			} else {
			    fprintf(stderr, "%s: -S without level specified.\n", prog);
			    usage();
			    return 1;
			}
			break;
#endif

                    case 'V':             /* -Vtimeout  --  Show hostnames that connect */
			showhosts = TRUE;
			if (op[1] != 0) {
			    int t = atoi(op + 1) * 1000000L;

			    if (t > 0) {
				if (t < (TickTock + 1)) {
				    t = TickTock + 1;
				}
				hosttimeout = (t / TickTock) * TickTock;
			    }
			}
			break;

#ifdef HEXDUMP
                    case 'X':             /* -X  --  Dump packets in hex */
			hexdump = debugforce = TRUE;
			break;
#endif

#ifdef AUDIO_DEVICE_FILE
                case 'Y':             /* -Youtdev:[ctldev] -- Specify audio
						     output and control device
						     file names or #open_fd. */
		    devAudioOutput = op + 1;
                    if (strchr(op + 1, ':') != NULL) {
                        devAudioControl = strchr(op + 1, ':') + 1;
		    }
		    break;
#endif

#ifdef CRYPTO
                    case 'Z':             /* -Z"phrase"  --  Set public key pass phrase */
			if (op[1] != 0) {
			    pgppass = strdup(op + 1);
			    op = argv[i];
                            /* Zap the pass phrase in memory so "ps" etc. don't see it. */
			    while (*op != 0) {
                                *op++ = ' ';
			    }
			} else {
			    char s[256];

 			    fflush(stderr);
			    initscr();
    	    	    	    move(12, 0);
			    printw("%s secret key pass phrase: ",
#ifdef GPG_KEY_EXCHANGE
    	    	    	    	    	"GPG"
#else
    	    	    	    	    	"PGP"
#endif
			    	  );
			    noecho();
			    getstr(s);
			    pgppass = strdup(s);
			    echo();
			    endwin();
			}
#ifdef PGP_DEBUG
    	    	    	fprintf(stderr, "Public key exchange secret key passphrase: \"%s\"\n", pgppass);
#endif
			break;
#endif
		}
	    }
	}

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
	    if (Debug) {
                fprintf(stderr, "%s: local host %s\n", prog,
		       inet_ntoa(localhost));
	    }
#endif
	}
#endif

#ifdef LINUX_FPU_FIX
    __setfpucw(_FPU_IEEE);	      /* Mask floating point interrupts to
					 enable standard IEEE processing.
					 Current libc releases do this by
					 default, so this is needed only on
					 older libraries. */
#endif

	/* Initialise GSM decoding. */

	gsmh = gsm_create();

	/* Initialise LPC decoding. */

	if (!lpc_start()) {
            fprintf(stderr, "Cannot allocate LPC decoding memory.\n");
	    return 1;
	}
	lpc10init();
	
	/* Initialise CELP decoding. */
	
	celp_init(FALSE);

	/* Find assigned port value and print it. */

	length = sizeof(name);
	if (getsockname(sock, (struct sockaddr *) &name, &length) < 0) {
            perror("getting socket name");
	    return 1;
	}
#ifdef SHOW_SOCKET
        fprintf(stderr, "%s: socket port #%d\n", prog, ntohs(name.sin_port));
#endif
    }

    /* Contact look who's listening host, if requested. */

    cp = getenv("SPEAKFREE_LWL_TELL");
    if (cp != NULL) {
	struct hostent *h;
	char md5key[16];
	char *ep, *np;
	long iadr;
	int n;

	makeSessionKey(md5key);
	bcopy(md5key, (char *) &ssrc, sizeof ssrc);
	sdesl = rtp_make_sdes(&sdes, ssrc, whichport, FALSE);
	lookhost.sin_family = AF_INET;

	while (lwl_nsites < LWL_MAX_SITES) {
	    n = lwl_nsites;
	    while (*cp != 0 && isspace(*cp)) {
		cp++;
	    }
	    if (*cp == 0) {
		break;
	    }
            if ((np = strchr(cp, ',')) != NULL) {
		*np++ = 0;
	    }
	    lwl_ports[lwl_nsites] = Internet_Port + 2;
	    lwl_strikes[lwl_nsites] = 0; /* Clear strikeout counter */
            if ((ep = strchr(cp, ':')) != NULL) {
		*ep = 0;
		lwl_ports[lwl_nsites] = atoi(ep + 1);
	    }
	    if (isdigit(*cp) && (iadr = inet_addr(cp)) != -1) {
		bcopy((char *) &iadr, (char *) (&lwl_sites[lwl_nsites]),
		      sizeof iadr);
		lwl_nsites++;
	    } else {
		h = gethostbyname(cp);
		if (h != NULL) {
		    bcopy((char *) (h->h_addr), 
			  (char *) (&lwl_sites[lwl_nsites]),
			  sizeof(unsigned long));
		    lwl_nsites++;
		} else {
                    fprintf(stderr, "%s: warning, SPEAKFREE_LWL_TELL host %s unknown.\n",
			prog, cp);
		}
	    }
	    if (Debug && lwl_nsites > n) {
                fprintf(stderr, "%s: publishing on LWL server %s: %s.\n", prog,
		    inet_ntoa(lwl_sites[n]), cp);
	    }
	    if (np == NULL) {
		break;
	    }
	    cp = np;
	}
	if (lwl_nsites > 0) {
	    sendLwlMessage(FALSE);
	}
    }

    /*	See if the user has specified a face file.  If so,
	try to open it.  */

    if (!lwlonly) {
        char *cp = getenv("SPEAKFREE_FACE");

	if (cp != NULL) {
            if ((facefile = fopen(cp, "r")) == NULL) {
                fprintf(stderr, "%s: cannot open SPEAKFREE_FACE file %s\n",
		    prog, cp);
	    }
	}
    }

    signal(SIGHUP, exiting);	      /* Set signal to handle termination */
    signal(SIGINT, exiting);	      /* Set signal to handle termination */
    signal(SIGTERM, exiting);	      /* Set signal to handle termination */
    signal(SIGALRM, release);	      /* Set signal to handle timeout */
    signal(SIGCHLD, childterm);       /* Set signal to handle face viewer termination */
    windtimer();		      /* Set periodic status update  */

    /* If all we're doing is broadcasting a periodic LWL update
       for sfvod, simply sit in a timer loop forever and let the
       SIGALRM handler take care of business. */

    while (lwlonly) {
	if (lwl_nsites == 0) {
	    /* -W option but no valid sites found.  Exit with error
	       status. */
	    return 2;
	}
	pause();
    }
    
#ifdef NAT_LAUNCH
    /* Launch sfmike if -NAThost was specified. */
    if (xnat_conspec) {
	xnat_active_sfmike_pid = launch_sfmike(NULL, mikeopts);
    }
#endif

    /* Read from the socket. */

    while (TRUE) {
	fd_set fdset;
	int wsock, control, busyreject;

	FD_ZERO(&fdset);
	FD_SET(sock, &fdset);
	FD_SET(ssock, &fdset);
	
	/*  Wait here for a packet to arrive on either the control or
	    audio socket.  */
	    
	if (select(FD_SETSIZE, &fdset, NULL, NULL, NULL) <= 0) {
	    continue;
	}
	wsock = FD_ISSET(sock, &fdset) ? sock :
		    (FD_ISSET(ssock, &fdset) ? ssock : -1);
	if (wsock < 0) {
	    continue;
	}
	
	/*  A packet appears to have arrived.  Read it from the proper socket.  */
	
	control = wsock == ssock;
	fromlen = sizeof(from);
	if ((rll = recvfrom(wsock, (char *) &sb, sizeof sb, 0, (struct sockaddr *) &from, &fromlen)) < 0) {
	    if (errno != EINTR) {
                perror(!control ? "receiving data packet" :
                                  "receiving control packet");
	    }
	    continue;
	}
#ifdef HEXDUMP
	if (hexdump) {
            fprintf(stderr, "%s: %d bytes read from %s socket.\n", prog, rll,
                    control ? "control" : "data");
	    xd(stderr, &sb, rll, TRUE);
	}
#endif

	/* See if this connection is active.  If not, initialise a new
	   connection. */

	busyreject = FALSE;
	newconn = FALSE;
	c = conn;
	while (c != NULL) {
	    if (memcmp(&from.sin_addr, &(c->con_addr),
		       sizeof(struct in_addr)) == 0) {
		break;
	    }
	    c = c->con_next;
	}
	
	/*  If we didn't find a connection from the sender's address
	    already active, open a new one and insert it in the list
	    of connections.  */
	    
	if (c == NULL) {
	    c = (struct connection *) malloc(sizeof(struct connection));
	    if (c != NULL) {
		struct hostent *h;

		newconn = TRUE;
		c->con_next = conn;
		c->pgpkey[0] = FALSE;
		bzero(c->keymd5, 16);
		c->con_uname[0] = c->con_email[0] = 0;
#ifndef CRYPTO
		c->con_crypt_warning = FALSE;
#endif
		conn = c;
		bcopy(&from.sin_addr, &(c->con_addr),
		    sizeof(struct in_addr));
		h = gethostbyaddr((char *) &from.sin_addr, sizeof(struct in_addr),
				  AF_INET);
		if (h == NULL) {
		    strcpy(c->con_hostname, inet_ntoa(from.sin_addr));
		} else {
		    strcpy(c->con_hostname, h->h_name);
		}
#ifdef NAT_LAUNCH
		if (control) {
		    c->con_ports.cport = ntohs(from.sin_port);
		    c->con_ports.dport = 0;
		} else {
		    c->con_compression = sb.compression;
		    c->con_ports.dport = ntohs(from.sin_port);
		    c->con_ports.cport = 0;
		}
		c->sfmike_pid = (xnat_conspec ? xnat_active_sfmike_pid : 0);
#endif
	    }
	} else if (c->con_timeout == -1) {
	    newconn = TRUE;
	}

	/* Initialise fields in connection.  Only fields which need to
	   be reinitialised when a previously idle host resumes activity
	   need be set here. */

	if (newconn) {
	    c->face_file = NULL;
	    c->face_filename[0] = 0;
	    c->face_viewer = 0;
	    c->face_stat = FSinit;
	    c->face_address = 0L;
	    c->face_retry = 0;
#ifdef NAT_LAUNCH
    	    c->sfmike_pid = 0;
#endif
	    c->con_compmodes = -1;
	    c->con_protocol = PROTOCOL_UNKNOWN;
	    c->con_rseq = -1;
	    c->con_reply_current = FALSE;
	    c->con_busy = 0;
            bcopy("\221\007\311\201", c->con_session, 4);
	    lpc_init(&c->lpcc);
	    busyreject = isBusy();
	}

	if (c != NULL) {
	    /* Reset connection timeout. */
	    c->con_timeout = 0;

            /* If we're rejecting this with a busy signal, start
	       the busy timeout running and dispatch the busy
	       signal to the host. */

	    if (busyreject && c->con_busy == 0) {
		pid_t cpid;

		c->con_busy = busytimeout;

		if (audiok) {
		    soundterm();
		    audiok = FALSE;
		    if (Debug) {
                         fprintf(stderr, "%s: releasing audio before busy signal fork().\n", prog);
		    }
		}
		cpid = fork();
		if (cpid == 0) {
		    char s[256];

		    signal(SIGHUP, SIG_DFL);
		    signal(SIGINT, SIG_DFL);
		    signal(SIGTERM, SIG_DFL);
		    signal(SIGALRM, SIG_DFL);
		    signal(SIGCHLD, SIG_DFL);

		    /* Now we need to close any shared resources
		       that might have been inherited from the parent
		       process to avoid their being locked up for the
		       duration of the child process execution. */

		    close(sock);
		    if (record != NULL) {
			fclose(record);
		    }
		    if (facefile != NULL) {
			fclose(facefile);
		    }
		    sprintf(s, busysignal, inet_ntoa(from.sin_addr));
		    if (Debug) {
                        fprintf(stderr, "%s: sending busy signal with %s\n",
			    prog, s);
		    }
		    system(s);
		    exit(0);
		} else if (cpid == (pid_t) -1) {
                    perror("creating busy signal process");
		}
	    }

	    if (newconn) {
		if (showhosts) {
                    fprintf(stderr, "%s: %s %s %s\n", prog, etime(),
			    c->con_hostname,
                            busyreject ? "sending busy signal" : "connect");
		}
	    }
	    if (busyreject) {
		continue;
	    }

	    /* Request face data from the other end, starting with
	       block zero.  If the connection was created itself
               by a face data request, don't request the face from
	       the other end; wait, instead, for some sound to
	       arrive.	We use face_stat to decide when to make the
	       request rather than newconn, since a connection may
	       have been created by a face request from the other
               end, which didn't trigger a reciprocal request by
	       us. */

	    if (!control && (c->con_protocol == PROTOCOL_SPEAKFREE) &&
		    (c->face_stat == FSinit) &&
		    isSoundPacket(ntohl(sb.compression)) &&
		    (ntohl(sb.compression) & fFaceOffer)) {
		c->face_address = 0;
		c->face_timeout = 0;
		c->face_retry = 0;
		c->face_stat = FSreply;   /* Activate request from timeout */
		faceTransferActive++;	  /* Mark face transfer underway */
		if (faceTransferActive == 1) {
		    windtimer();	  /* Set timer to fast cadence */
		}
	    }
	    
#ifdef NAT_LAUNCH
	    if (xnat_passive && (c->sfmike_pid == 0)) {
		if (control && (c->con_ports.cport == 0)) {
		    c->con_ports.cport = ntohs(from.sin_port);
		}
		if ((!control) && (c->con_ports.dport == 0)) {
		    c->con_ports.dport = ntohs(from.sin_port);
		    c->con_compression = sb.compression;
		}
		if ((c->con_ports.dport > 0) && c->con_ports.cport) {
		    c->sfmike_pid = launch_sfmike(c, mikeopts);
		}
	    }
#endif
	} else {
	    continue;
	}

        /* If we've sent the connection a busy signal, ignore all
	   packets from it until the busy timeout expires. */

	if (c->con_busy > 0) {
	    if (Debug) {
                fprintf(stderr, "%s: busy--discarding packet from %s.\n", prog, 
			c->con_hostname);
	    }
	    continue;
	}

	wasrtp = FALSE;

#ifdef CRYPTO

        /* If a DES key is present and we're talking RTP or VAT
	   protocol we must decrypt the packet at this point.
	   We decrypt the packet if:

		1.  A DES key was given on the command line, and
		    either:

		    a)	The packet arrived on the control port
			(and hence must be from an RTP/VAT client), or

		    b)	The protocol has already been detected as
			RTP or VAT by reception of a valid control
			port message.  */

	if ((control || (c->con_protocol == PROTOCOL_RTP) ||
			(c->con_protocol == PROTOCOL_VAT)) &&
	     rtpdeskey[0]) {

	    /* One more little twist.  If this packet arrived on the
	       control channel, see if it passes all the tests for a
               valid RTCP packet.  If so, we'll assume it isn't
	       encrypted.  RTP utilities have the option of either
	       encrypting their control packets or sending them in
	       the clear, so a hack like this is the only way we have
	       to guess whether something we receive is encrypted. */

	    if (!isValidRTCPpacket((unsigned char *) &sb, rll)) {
		des_key_schedule sched;
		des_cblock ivec;
		int drll = rll;
		char *whichkey;
		static int toggle = 0;

		bzero(ivec, 8);
		drll = (rll + 7) & (~7);
		if (Debug) {
                    fprintf(stderr, "Decrypting %d VAT/RTP bytes with DES key.\r\n",
			    drll);
		}
		if (drll > rll) {
		    /* Should only happen for VAT protocol.  Zero the rest of
		       the DES encryption block to guarantee consistency. */
		    bzero(((char *) &sb) + rll, drll - rll);
		}

		/* If the protocol is unknown, toggle back and forth
		   between the RTP and VAT DES keys until we crack the
		   packet and set the protocol. */

		if (c->con_protocol == PROTOCOL_UNKNOWN || 
		    c->con_protocol == PROTOCOL_VATRTP_CRYPT ||
		    c->con_protocol == PROTOCOL_SPEAKFREE) {
		    whichkey = toggle == 0 ? vatdeskey :
			       (toggle == 1 ? rtpdeskey : NULL);
		    toggle = (toggle + 1) % 3;
		} else {
		    whichkey = c->con_protocol == PROTOCOL_VAT ?
				vatdeskey : rtpdeskey;
		}
		if (whichkey != NULL) {
		    des_set_key((des_cblock *) (whichkey + 1), sched);
		    des_ncbc_encrypt((des_cblock *) &sb,
			(des_cblock *) &sb, rll, sched,
			(des_cblock *) ivec, DES_DECRYPT);

		    /* Just one more thing.  In RTP (unlike VAT), when
		       an RTCP control packet is encrypted, 4 bytes of
		       random data are prefixed to the packet to prevent
		       known-plaintext attacks.  We have to strip this
		       prefix after decrypting. */

		    if (control && ((*(((char *) &sb) + 4) & 0xC0) == 0x80)) {
			rll -= 4;
			bcopy(((char *) &sb) + 4, (char *) &sb, rll);
		    }
		}
#ifdef HEXDUMP
		if (hexdump) {
		    xd(stderr, &sb, rll, TRUE);
		}
#endif
	    }
	}
#endif

	/* If this packet arrived on the session control port, dispatch
	   it to the appropriate handler for its protocol. */

	if (control) {
	    short protocol = PROTOCOL_VATRTP_CRYPT;
	    unsigned char *p = (unsigned char *) &sb;
	    unsigned char *apkt;
	    int proto = (p[0] >> 6) & 3;

	    if (proto == 0) {	      /* VAT */
		/* To avoid spoofing by bad encryption keys, require
                   a proper ID message be seen before we'll flip into
		   VAT protocol. */
		if (((p[1] == 1) || (p[1] == 3)) ||
		    ((c->con_protocol == PROTOCOL_VAT) && (p[1] == 2))) {
		    protocol = PROTOCOL_VAT;
		    bcopy(p + 2, c->con_session, 2);  /* Save conference ID */

		    if (p[1] == 1 && showhosts) {
			char uname[256];
			int unl = rll - 4;

    	    	    	if (unl > ((sizeof uname) - 1)) {
			    unl = (sizeof uname) - 1;
			}
			bcopy(p + 4, uname, unl);
			uname[unl] = 0;
			if (strcmp(uname, c->con_uname) != 0) {
			    strcpy(c->con_uname, uname);
                            fprintf(stderr, "%s: %s sending from %s.\n", prog,
				    c->con_uname, c->con_hostname);
			}
		    }

		    /* Handling of VAT IDLIST could be a lot more elegant
		       than this. */

		    if (p[1] == 3 && showhosts) {
			char *uname;

			uname = (char *) malloc(rll);
			if (uname != NULL) {
			    unsigned char *bp = p, *ep = p + rll;
			    int i = bp[4];

			    bp += 8;
			    uname[0] = 0;
			    *ep = 0;
			    while (--i >= 0 && bp < ep) {
				bp += 4;
                                strcat(uname, "\t");
				strcat(uname, (char *) bp);
				while (isspace(uname[strlen(uname) - 1])) {
				    uname[strlen(uname) - 1] = 0;
				}
                                strcat(uname, "\n");
				bp += (strlen((char *) bp) + 3) & ~3;
			    }
			    if (strncmp(uname, c->con_uname, (sizeof c->con_uname - 1)) != 0) {
				strncpy(c->con_uname, uname, sizeof c->con_uname);
				if (strlen(uname) > ((sizeof c->con_uname) - 1)) {
				    c->con_uname[((sizeof c->con_uname) - 1)] = 0;
				}
                                fprintf(stderr, "%s: now in conference at %s:\n%s", prog,
					c->con_hostname, uname);
			    }
			    free(uname);
			}
		    }

                    /* If it's a DONE packet, reset protocol to unknown. */

		    if (p[1] == 2) {
			c->con_protocol = protocol = PROTOCOL_UNKNOWN;
			c->con_timeout = hosttimeout - 1;
			c->con_uname[0] = c->con_email[0] = 0;
			if (showhosts) {
                            fprintf(stderr, "%s: %s VAT connection closed.\n",
				    prog, c->con_hostname);
			}
		    }
		}

	    } else if (proto == RTP_VERSION || proto == 1) { /* RTP */
		if (isValidRTCPpacket((unsigned char *) &sb, rll)) {
		    protocol = (proto == 1) ? PROTOCOL_SPEAKFREE : PROTOCOL_RTP;
		    bcopy(p + 4, c->con_session, 4);  /* Save SSRC */

                    /* If it's a BYE packet, reset protocol to unknown. */

		    if (isRTCPByepacket((unsigned char *) &sb, rll)) {
			c->con_protocol = protocol = PROTOCOL_UNKNOWN;
			c->con_timeout = hosttimeout - 1;
			c->con_uname[0] = c->con_email[0] = 0;
			if (showhosts) {
                            fprintf(stderr, "%s: %s %s connection closed.\n",
				    prog, c->con_hostname,
                                    proto == 1 ? "Speak Freely" : "RTP");
			}

                    /* If it's a text chat message, print it. */

		    } else if (isRTCPAPPpacket((unsigned char *) &sb, rll,
				    RTCP_APP_TEXT_CHAT, &apkt) && apkt != NULL) {
			char *ident = c->con_hostname;

			/* To identify the sender, get successively more
			   personal depending on the information we have at
			   hand, working down from hostname (which may just
                           be an IP address if we couldn't resolve the host,
			   through E-mail address, to user name. */

			if (c->con_email[0] != 0) {
			    ident = c->con_email;
			}
			if (c->con_uname[0] != 0) {
			    ident = ident = c->con_uname;
			}

                        printf("%s: %s\n", ident, (char *) (apkt + 12));

                    /* Otherwise, it's presumably an SDES, from which we
		       should update the user identity information for the
		       connection. */

		    } else {
			struct rtcp_sdes_request rp;

			rp.nitems = 4;
			rp.item[0].r_item = RTCP_SDES_CNAME;
			rp.item[1].r_item = RTCP_SDES_NAME;
			rp.item[2].r_item = RTCP_SDES_EMAIL;
			rp.item[3].r_item = RTCP_SDES_TOOL;
			if (parseSDES((unsigned char *) &sb, &rp)) {
			    char uname[256], email[256];

			    uname[0] = email[0] = 0;
			    if (rp.item[1].r_text != NULL) {
				copySDESitem(rp.item[1].r_text, uname);
				if (rp.item[2].r_text != NULL) {
				    copySDESitem(rp.item[2].r_text, email);
				} else if (rp.item[2].r_text != NULL) {
				    copySDESitem(rp.item[0].r_text, email);
				}
			    } else if (rp.item[2].r_text != NULL) {
				copySDESitem(rp.item[2].r_text, uname);
			    } else if (rp.item[0].r_text != NULL) {
				copySDESitem(rp.item[0].r_text, uname);
			    }
			    if (strcmp(uname, c->con_uname) != 0 ||
				strcmp(email, c->con_email) != 0) {
				strcpy(c->con_uname, uname);
				strcpy(c->con_email, email);
				if (showhosts && uname[0]) {
                                    fprintf(stderr, "%s: %s", prog, uname);
				    if (email[0]) {
                                      fprintf(stderr, " (%s)", email);
				    }
                                    fprintf(stderr, " sending from %s.\n",
					    c->con_hostname);
				}
			    }
			}
		    }
		} else {
		    if (Debug) {
                        fprintf(stderr, "Invalid RTCP packet received.\n");
		    }
		}
	    } else {
		if (Debug) {
                    fprintf(stderr, "Bogus protocol 3 control message.\n");
		}
	    }

	    /* If protocol changed, update in connection and, if appropriate,
	       update the reply command. */

	    if (protocol != c->con_protocol) {
		static char *pname[] = {
                    "Speak Freely",
                    "VAT",
                    "RTP",
                    "VAT/RTP encrypted",
                    "Unknown"
		};

		c->con_protocol = protocol;
		if (showhosts) {
                    fprintf(stderr, "%s: %s sending in %s protocol.\n",
			    prog, c->con_hostname, pname[protocol]);
		}
		c->con_reply_current = FALSE;
	    }
	    continue;
	}

	/* If this message is tagged with our Speak Freely protocol
	   bit, force protocol back to Speak Freely.  This allows us
	   to switch back to Speak Freely after receiving packets in
	   VAT.  We can still get confused if we receive a packet from
           an older version of Speak Freely that doesn't tag. */

	if (c->con_protocol == PROTOCOL_VAT ||
	    c->con_protocol == PROTOCOL_VATRTP_CRYPT) {
	    unsigned char *p = (unsigned char *) &sb;

	    if (((p[0] >> 6) & 3) == 1) {
		c->con_protocol = PROTOCOL_SPEAKFREE;
	    }
	}

	/* If this is a VAT packet, translate it into a sound buffer. */

	if (((c->con_protocol == PROTOCOL_VAT)) &&
	    (bcmp(((unsigned char *) &sb) + 2, c->con_session, 2) == 0) &&
	    isvat((unsigned char *) &sb, rll)) {
	    if (sb.buffer.buffer_len == 0) {
		if (Debug) {
                    fprintf(stderr, "Ignoring unparseable VAT packet.\n");
		}
		continue;
	    }
	    wasrtp = TRUE;

	/* If this is an RTP packet, transmogrify it into a sound
	   buffer we can understand. */

	} else if ((c->con_protocol == PROTOCOL_RTP) &&
		 (bcmp(((unsigned char *) &sb) + 8, c->con_session, 4) == 0) &&
		 isrtp((unsigned char *) &sb, rll)) {
	    if (sb.buffer.buffer_len == 0) {
		if (Debug) {
                    fprintf(stderr, "Ignoring unparseable RTP packet.\n");
		}
		continue;
	    }
	    wasrtp = TRUE;
	}

	if (!wasrtp) {
	    long xbl;

	    /* Convert relevant fields from network to host
	       byte order, if necessary. */

	    sb.compression = ntohl(sb.compression);
	    sb.buffer.buffer_len = ntohl(sb.buffer.buffer_len);

	    if (sb.compression & fCompRobust) {
		int aseq = (sb.buffer.buffer_len >> 24) & 0xFF, pdist, mdist, cdist;

    	    	/* Robust mode sends multiple copies of each sound buffer
		   with a sequence number modulo 256 hidden in the most
		   significant byte of the buffer length field.  If
		   multipath has caused packets to arrive out of order,
		   we want do discard any packets prior one we've played;
		   otherwise we'd shuffle the sound with the result being
		   gibberish.  The fact that the sequence number is modular
		   complicates this test a tad since the sequence numbers
		   wrap around at 255.  Consequently, we compute the
		   distance in modular space between the sequence number
		   in the packet we've just received and that of the last
		   packet we played.  */

		if (aseq >= c->con_rseq) {
		    pdist = aseq - c->con_rseq;
		    mdist = (c->con_rseq + 256) - aseq;
		} else {
		    mdist = c->con_rseq - aseq;
		    pdist = (aseq + 256) - c->con_rseq;
		}
		cdist = (pdist <= mdist) ? pdist : -mdist;

    	    	if ((cdist <= 0) && (cdist > -16)) {
		    if (Debug) {
			fprintf(stderr, "Robust REJECTING   seq %3d   last seq %3d   distance %4d\n",
			    aseq, c->con_rseq, cdist);
		    }
		    continue;
		}
		if (Debug) {
		    fprintf(stderr, "Robust PLAYING     seq %3d   last seq %3d   distance %4d\n",
			aseq, c->con_rseq, cdist);
		}
		c->con_rseq = aseq;
		sb.buffer.buffer_len &= 0xFFFFFFL;
	    }

	    /* Now if this is a valid Speak Freely packet (as
	       opposed to a VAT packet masquerading as one, or
               an encrypted VAT or RTP packet we don't have the
	       proper key to decode), the length received from the
	       socket will exactly equal the buffer length plus
	       the size of the header.	This is a reasonably
	       good validity check, well worth it considering the
	       horrors treating undecipherable garbage as a sound
	       buffer could inflict on us. */

	    xbl = sb.buffer.buffer_len + (sizeof(struct soundbuf) - BUFL);

	    /* If this packet is encrypted with an algorithm which requires
	       padding the packet to an 8- or 16-byte boundary, adjust the actual
	       content length to account for the padding. */

	    if (!(sb.compression & (fFaceOffer | fFaceData))) {
		if ((sb.compression & (fEncDES | fEncIDEA | fEncBF | fEncPGP)) != 0) {
		    xbl = ((sb.buffer.buffer_len + 7) & (~7)) +
			  (sizeof(struct soundbuf) - BUFL);
		}

		if ((sb.compression & fEncAES) != 0) {
		    xbl = ((sb.buffer.buffer_len + 15) & (~15)) +
			  (sizeof(struct soundbuf) - BUFL);
		}

                /* If packet is compressed with LPC-10 or CELP, compensate for "packet
                   stuffing". */

		if ((sb.compression & (fCompLPC10 | fCompCELP)) &&
		    (sb.buffer.buffer_len >= 16)) {
		    xbl -= 16;
		}
		if (xbl != rll) {
		    if (Debug) {
			fprintf(stderr,
                                "%s: Sound buffer from %s length %ld doesn't match %d byte packet.  Hdr=%08lX\n",
				prog, c->con_hostname, xbl, rll, sb.compression);
		    }
		    if (showhosts && c->con_protocol != PROTOCOL_UNKNOWN) {
                        fprintf(stderr, "%s: %s sending in unknown protocol or encryption.\n",
				prog, c->con_hostname);
		    }
		    c->con_protocol = PROTOCOL_UNKNOWN;
		    continue;
		}
	    } else if (!(sb.compression & fHalfDuplex)) {
	    
	    	/* This packet purports to be a face request.  Due to an
		   ill-considered design decision by yours truly, the
		   file address in the face file was embedded in the
		   packet length field, precluding our normal sanity check in
		   the above branch of the if.  But not to despair--these
		   packets conform to a rigid structure which we will
		   now verify. */
		   
		if ((((sb.compression & (fProtocol | fFaceData | faceRequest)) == (fProtocol | fFaceData)) ||
		     ((sb.compression & (fProtocol | fFaceData | faceRequest)) == (fProtocol | faceRequest))) &&
		     (rll > ((sizeof(struct soundbuf) - BUFL) + 512))) {
			fprintf(stderr,
                                "%s: Face request from %s length %d exceeds maximum %d byte face data packet size.  Hdr=%08lX\n",
				prog, c->con_hostname, rll, (sizeof(struct soundbuf) - BUFL) + 512, sb.compression);
			continue;
		}
	    }
	    /* The only packets which remain are half-duplex mute requests,
	       and these contain no payload. */

	    /* It does appear to be a genuine Speak Freely sound
	       buffer.	On that basis, set the protocol to Speak Freely
               even if the buffer isn't explicitly tagged. */

	    if (c->con_protocol != PROTOCOL_SPEAKFREE) {
		c->con_protocol = PROTOCOL_SPEAKFREE;
		if (showhosts) {
                    fprintf(stderr, "%s: %s sending in Speak Freely protocol.\n", prog, c->con_hostname);
		}
		c->con_reply_current = FALSE;
	    }
	}
	
	if ((replycmdexe != NULL && !c->con_reply_current)) {
            char *rcmd = StrReplace(replycmdexe, "%s", c->con_hostname);

	    if (rcmd != NULL) {
		if (Debug) {
                    fprintf(stderr, "Executing: %s\n", rcmd);
		}
		system(rcmd);
		c->con_reply_current = TRUE;
		if (rcmd != replycmdexe) {
		    free(rcmd);
		}
	    }
	}

        /* If this is the first sound packet we've seen from this host 
	   in this protocol, create (or update) the reply command file. */

	if (replyfile != NULL && !c->con_reply_current) {

	    /* If a reply file is requested, create an executable
	       shell script to reply to the host that just connected
	       using either the default command or whatever command
	       the user specifies on the -A option. */

            FILE *rfp = fopen(replyfile, "w");
	    static char *popt[] = {
                "",
                "-vat ",
                "-rtp ",
                "-rtp ",
                ""
	    };

	    if (rfp != NULL) {
                fprintf(rfp, "#! /bin/sh\n");
		/* $* allows specifying options on reply file call. */
                fprintf(rfp, "%s %s$* %s\n", replycmd,
			      popt[c->con_protocol], c->con_hostname);
		/* Make the file executable. */
		fchmod(fileno(rfp), 0755);
		fclose(rfp);
	    }
	    c->con_reply_current = TRUE;
	}

#ifdef HALF_DUPLEX

	/* If this is a half duplex mute request, immediately release
	   the audio output device if we have it.  We verify the
           sender's address and accept mute requests only frome
	   localhost; nobody else has any business telling us
	   to shut up! */

	if (sb.compression & fHalfDuplex) {
#ifdef HDX_DEBUG
	    if (Debug) {
                static char *hdxreq[4] = { "Bogus(0)", "Mute", "Resume",
                                           "Bogus(3)" };

                fprintf(stderr, "%s: half-duplex %s request from %s.\n",
		    prog, hdxreq[sb.compression & 3], inet_ntoa(from.sin_addr));
	    }
#endif
	    if (memcmp(&from.sin_addr, &localhost, sizeof localhost) == 0) {
		if (sb.compression & fHalfDuplexMute) {
		    if (audiok) {
			soundterm();
			audiok = FALSE;
			if (Debug) {
                            fprintf(stderr, "%s: half-duplex releasing audio output.\n", prog);
			}
		    }
		    halfDuplexMuted = TRUE;
		} else if (sb.compression & fHalfDuplexResume) {
		    halfDuplexMuted = FALSE;
		}
	    }
	    continue;		      /* Done with packet */
	}
#endif

	/* If this is a face request and we have a face file open,
	   respond to it.  Note that servicing of face file data requests
	   is stateless. */

	if (sb.compression & fFaceData) {
	    if (sb.compression & faceRequest) {
		long l;

		/* Request for face data. */

		if (facefile != NULL) {
		    fseek(facefile, sb.buffer.buffer_len, 0);
		    *((long *) sb.buffer.buffer_val) = htonl(sb.buffer.buffer_len);
		    l = fread(sb.buffer.buffer_val + sizeof(long),
			1, 512 - (sizeof(long) + (sizeof(soundbuf) - BUFL)), facefile);
		    sb.compression = fProtocol | fFaceData | faceReply;
		    if (Debug) {
                        fprintf(stderr, "%s: sending %ld bytes of face data at %ld to %s\n",
			    prog, l, (long) ntohl(*((long *) sb.buffer.buffer_val)), c->con_hostname);
		    }
		    l += sizeof(long);
		} else {
		    /* No face file.  Shut down requestor. */
		    sb.compression = fProtocol | fFaceData | faceLess;
		    l = 0;
		}
		bcopy((char *) &(from.sin_addr), (char *) &(name.sin_addr),
		    sizeof(struct in_addr));

		sb.compression = htonl(sb.compression);
		sb.buffer.buffer_len = htonl(l);
		if (sendto(sock, (char *) &sb,
		    (int) ((sizeof(struct soundbuf) - BUFL) + l),
		    0, (struct sockaddr *) &(name), sizeof name) < 0) {
                    perror("sending face image data");
		}
	    } else if (sb.compression & faceReply) {

		/* Face data packet received from remote server. */

		if ((c->face_file == NULL) && (sb.buffer.buffer_len > 0)) {
    	    	    char *faceFilename = NULL;
		    
		    c->face_file = create_tempfile_in_tempdir(".SF_FaceFile_XXXXXXXXXXX", &faceFilename, 077, FALSE);
		    if (c->face_file != NULL) {
		    	strcpy(c->face_filename, faceFilename);
			free(faceFilename);
		    }
		}
		if (c->face_file != NULL) {
		    if (sb.buffer.buffer_len > sizeof(long)) {
			long lp =  ntohl(*((long *) sb.buffer.buffer_val));

			if (lp == c->face_address) {
			    fseek(c->face_file, lp, 0);
			    fwrite(sb.buffer.buffer_val + sizeof(long),
				    sb.buffer.buffer_len - sizeof(long), 1,
				    c->face_file);
			    if (Debug) {
                                fprintf(stderr, "%s: writing %ld bytes at %ld in face file %s\n",
				    prog, sb.buffer.buffer_len - sizeof(long),
				    lp, c->face_filename);
			    }
			    c->face_address += sb.buffer.buffer_len - sizeof(long);
			    /* Timeout will make next request after the
			       configured interval. */
			    c->face_stat = FSreply;
			    c->face_retry = 0;
			} else {
			    if (Debug) {
                                fprintf(stderr, "%s: discarded %ld bytes for %ld in face file %s, expected data for %ld\n",
				    prog, sb.buffer.buffer_len - sizeof(long),
				    lp, c->face_filename, c->face_address);
			    }
			}
		    } else {
			pid_t cpid;

			if (Debug) {
                            fprintf(stderr, "%s: closing face file %s\n",
				prog, c->face_filename);
			}
    	    	    	rewind(c->face_file);
			c->face_stat = FScomplete;
			faceTransferActive--;

			/* Start viewer to display face.  We terminate
			   audio output (if active) before doing this since
                           we don't know the nature of the audio output
                           resource.  If it's an open file handle which
			   would be inherited by the child process, that
			   would hang the audio device as long as the
			   viewer is active.  */

			if (audiok) {
			    soundterm();
			    audiok = FALSE;
			    if (Debug) {
                                fprintf(stderr, "%s: releasing audio before viewer fork().\n", prog);
			    }
			}
			cpid = fork();
			if (cpid == 0) {
			    char geom[30], *gp1 = NULL, *gp2 = NULL;

#ifdef NEEDED
			    /* These should be reset by the execlp(). */
			    signal(SIGHUP, SIG_DFL);
			    signal(SIGINT, SIG_DFL);
			    signal(SIGTERM, SIG_DFL);
			    signal(SIGALRM, SIG_DFL);
			    signal(SIGCHLD, SIG_DFL);
#endif

			    /* Now we need to close any shared resources
			       that might have been inherited from the parent
			       process to avoid their being locked up for the
                               duration of the viewer's execution. */

			    close(sock);
			    if (record != NULL) {
				fclose(record);
			    }
			    if (facefile != NULL) {
				fclose(facefile);
			    }
#ifdef FACE_SET_GEOMETRY
			    /* Attempt to reasonably place successive face windows
                               on the screen to avoid the user's having to place
			       them individually (for window managers with
			       interactivePlacement enabled). */

#define faceInterval 120    /* Interval, in pixels, between successive faces */
                            sprintf(geom, "-0+%d", facesDisplayed * faceInterval);
                            gp1 = "-geometry";
			    gp2 = geom;
#endif
                            execlp("xv",  "xv", c->face_filename, gp1, gp2, (char *) 0);
                            perror("launching face image viewer");
			    facesDisplayed--;
			    exit(0);
			    /* Leave face image around, for the moment, so the user can
			       try to view it manually. */
			} else if (cpid == (pid_t) -1) {
                            perror("creating face image viewer process");
			} else {
			    c->face_viewer = cpid;
			    facesDisplayed++;
			}
		    }
		}
	    } else if (sb.compression & faceLess) {
		if (c->face_file != NULL) {
		    fclose(c->face_file);
		    unlink(c->face_filename);
		}
		c->face_stat = FSabandoned;
		faceTransferActive--;
		if (Debug) {
                    fprintf(stderr, "%s: no face image available for %s\n",
			prog, c->con_hostname);
		}
	    }
	    continue;		      /* Done with packet */
	}

	/* If the packet requests loop-back, immediately dispatch it
	   back to the host who sent it to us.	To prevent an infinite
	   loop-back cycle, we clear the loop-back bit in the header
	   before sending the message.	We leave the host of origin
	   unchanged, allowing the sender to identify the packet as
	   one he originated. */

	if (sb.compression & fLoopBack) {
	    bcopy((char *) &(from.sin_addr), (char *) &(name.sin_addr),
		sizeof(struct in_addr));
	    sb.compression &= ~fLoopBack;    /* Prevent infinite loopback */

	    sb.compression = htonl(sb.compression);
	    sb.buffer.buffer_len = htonl(sb.buffer.buffer_len);
	    if (sendto(sock, (char *) &sb, rll,
		0, (struct sockaddr *) &(name), sizeof name) < 0) {
                perror("sending datagram message");
	    }
	    sb.compression = ntohl(sb.compression);
	    sb.buffer.buffer_len = ntohl(sb.buffer.buffer_len);
	}

#ifdef HALF_DUPLEX

        /* If we're muted by a transmission in progress on half-duplex
	   audio hardware, this is the end of line for this sound
	   packet. */

	if (halfDuplexMuted) {
	    if (Debug) {
                fprintf(stderr, "%s: %s packet lost by half-duplex muting.\n",
				prog, c->con_hostname);
	    }
	    continue;
	}
#endif

        /* If this packet has been "stuffed" for maximum efficiency,
	   un-stuff it at this point. */

	if ((sb.compression & (fCompLPC10 | fCompCELP)) &&
	    (sb.buffer.buffer_len >= 16)) {
	    bcopy(sb.sendinghost, (char *) &sb + rll,
		  sizeof sb.sendinghost);
	    rll += sizeof sb.sendinghost;
	}

#ifdef CRYPTO
	if ((sb.compression & fKeyPGP)) {
	    char cmd[256], kmd[16];
	    FILE *kfile;
	    FILE *pipe;
	    struct MD5Context md5c;

	    MD5Init(&md5c);
	    MD5Update(&md5c, sb.buffer.buffer_val, sb.buffer.buffer_len);
	    MD5Final(kmd, &md5c);

	    if (memcmp(c->keymd5, kmd, 16) != 0) {
    	    	char *uniqueName = NULL;
		
		bcopy(kmd, c->keymd5, 16);
    	    	kfile = create_tempfile_in_tempdir(".SF_skey_XXXXXXXXXXX", &uniqueName, 077, FALSE);
		if (kfile == NULL) {
                    fprintf(stderr, "Cannot open encrypted session key file.\n");
		} else {
		    fwrite(sb.buffer.buffer_val, sb.buffer.buffer_len, 1, kfile);
    	    	    rewind(kfile);
#ifdef GPG_KEY_EXCHANGE
    	    	    sprintf(cmd, "%s%s%sgpg %s--quiet --output - --decrypt %s",
		    	pgppass ? "echo \"" : "",
			pgppass ? pgppass : "",
			pgppass ? "\" | " : "",
		    	pgppass ? "--passphrase-fd 0 " : "",
		    	uniqueName);
#else
                    sprintf(cmd, "pgp -f +nomanual +verbose=0 +armor=off %s%s%s <%s",
                        pgppass ? "-z\"" : "", pgppass ? pgppass : "",
                        pgppass ? "\" " : "", uniqueName);
#endif
#ifdef PGP_DEBUG
                    fprintf(stderr, "Decoding session key with: %s\n", cmd);
#else
		    if (Debug) {
                       fprintf(stderr, "%s: decoding session key.\n", prog);
		    }
#endif
                    pipe = popen(cmd, "r");
		    if (pipe == NULL) {
                        fprintf(stderr, "Unable to open pipe to: %s\n", cmd);
		    } else {
			int lr;

			/* Okay, explanation time again.  On some systems
			   (Silicon Graphics, for example), the timer tick
			   alarm signal can cause the pending read from the
                           PGP key pipe to return an "Interrupted system
                           call" status (EINTR) with (as far as I've ever
                           seen and I sincerely hope it's always) zero bytes
			   read.  This happens frequently when the timer is
			   running and the user takes longer to enter the
			   secret key pass phrase than the timer tick.	So,
			   if this happens we keep on re-issuing the pipe
			   read until the phrase allows PGP to finish the
			   job. */

			while ((lr = fread(c->pgpkey, 1, 17, pipe)) != 17 &&
			       (errno == EINTR)) ;
			if (lr == 17) {
			    c->pgpkey[0] = TRUE;
#ifdef PGP_DEBUG
			    {	
				int i;

                                fprintf(stderr, "Session key for %s:", c->con_hostname);
				for (i = 0; i < 16; i++) {
                                    fprintf(stderr, " %02X", c->pgpkey[i + 1] & 0xFF);
				}
                                fprintf(stderr, "\n");
			    }
#else
			    if (Debug) {
                               fprintf(stderr, "%s: session key decoded.\n", prog);
			    }
#endif
			} else {
			    c->pgpkey[0] = FALSE;
                            fprintf(stderr, "%s: Error decoding session key.\n", prog);
#ifdef PGP_DEBUG
                            fprintf(stderr, "Read status from pipe: %d\n", lr);
                            perror("reading decoded PGP key from pipe");
#endif
			}
			pclose(pipe);
		    }
    	    	    fclose(kfile);
		    unlink(uniqueName);
		    free(uniqueName);
		}
	    }
	} else
#endif
	{
	    playbuffer(&sb, c);
	}
    }
#ifdef MEANS_OF_EXIT
    close(sock);
#ifdef CRYPTO
    desdone();
#endif
    gsm_destroy(gsmh);
    lpc_end();
    exiting();
    if (record != NULL) {
	fclose(record);
    }
    if (facefile != NULL) {
	fclose(facefile);
    }
    return 0;
#endif
}
