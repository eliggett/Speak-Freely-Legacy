/*

			Speak Freely for Unix
			   Echo Back Server

     Designed and implemented in January of 1996 by John Walker.

*/

#include "speakfree.h"
#include "version.h"
#include "vat.h"

#define Echo_Base   0		      /* Echo server offset from speaker port */

#define Echo_Time   10		      /* Echo retransmit time in seconds */

static int debugforce = 0;	      /* Debugging forced / prevented ? */
static int whichport = Internet_Port + Echo_Base; /* Port to listen on (base address) */
static int sock;		      /* Input socket */
static int ssock;		      /* Control socket (RTP/VAT) */
static struct sockaddr_in from;       /* Sending host address */
static struct sockaddr_in name;       /* Address of destination host */
static int fromlen;		      /* Length of sending host address */
static int showhosts = FALSE;	      /* Show host names that connect */
static int hosttimeout = 180 * 1000000L; /* Consider host idle after this time */
static char *prog;		      /* Program name */ 
static long timebase;		      /* Time in seconds at start of program */

static struct sockaddr_in lookhost;   /* Look who's listening host, if any */
static char *sdes = NULL;	      /* RTP SDES packet */
static int sdesl;		      /* RTP SDES packet length */
static unsigned long ssrc;	      /* RTP synchronisation source identifier */
static double lwltimer; 	      /* Time next LWL retransmit scheduled */
static int actives = 0; 	      /* Currently active hosts */

#define LWL_RETRANSMIT	(5 * 60 * 1000000.0) /* Microseconds between LWL updates */

static struct in_addr lwl_sites[LWL_MAX_SITES]; /* LWL site addresses */
static long lwl_ports[LWL_MAX_SITES]; /* Ports for LWL hosts */
static int lwl_nsites = 0;	      /* Number of LWL sites published on */

static int debug_packet_drop = 0;     /* Percent of packets to randomly drop for debugging */
static int debug_packet_shuffle = 0;  /* Percent of packets to randomly shuffle for debugging */
static int debug_packet_shuffle_depth = 10; /* Maximum depth in queue to shuffle packets */

/* Open connection state. */

#define connection e_connection

struct connection {
    struct connection *con_next;      /* Next connection */
    struct in_addr con_addr;	      /* Host Internet address */
    char pgpkey[17];		      /* PGP key for connection */
    char keymd5[16];		      /* Digest of key file */
    double con_timeout; 	      /* Connection timeout time */
    char con_hostname[264];	      /* Host name */
    char *con_compmodes;	      /* Last compression modes */
    int con_control;		      /* Control packet seen ? */
    int con_bye;		      /* BYE packet received ? */

    char face_filename[300];	      /* Face temporary file name */
    FILE *face_file;		      /* Face file, when open */
    pid_t face_viewer;		      /* Face file viewer PID */
    int face_stat;		      /* Face retrieval status */
    long face_address;		      /* Address of current block request */
    int face_retry;		      /* Timeout retry count */
    int face_timeout;		      /* Timeout interval */
};

struct connection *conn = NULL;       /* Chain of current connections */

#define Debug	    (debugforce != 0) /* Generate debug output */

#define TickTock    (10 * 1000000L)   /* Alarm interval in microseconds */
#define TockTock    (60 * 1000000L)   /* Alarm interval when no connections open */

struct queuedPacket {
    struct queuedPacket *next;	      /* Next packet in chain */
    double when;		      /* When packet should be sent */
    struct sockaddr_in where;	      /* Where packet should be sent */
    int pktlen; 		      /* Packet length */
    char pktdata[2];		      /* Packet data */
};

struct queuedPacket *qph = NULL,      /* Queue of packets waiting to be echoed */
		    *qptail = NULL;
static int crit = FALSE;	      /* Queue critical section lock */
static int clash = FALSE;	      /* Critical section clash retry flag */


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
			 compression employed in this buffer.  We
			 attempt to distinguish RTP and VAT
                         messages from our own here.  This isn't
			 100% reliable in the case of VAT, but
			 nothing really terrible will happen if
			 we guess wrong.  We only test a packet for
                         VAT if given the clue that we've seen a
			 packet arrive on the control channel. */

static char *compressionType(soundbuf *msg, int clue)
{
    unsigned char *p = (unsigned char *) msg;

    if (((p[0] >> 6) & 3) == RTP_VERSION) {
	char *r;

	switch (p[1] & 0x7F) {
	    case 0:
                r = "RTP PCMU";
		break;

	    case 1:
                r = "RTP 1016";
		break;

	    case 2:
                r = "RTP G721";
		break;

	    case 3:
                r = "RTP GSM";
		break;

	    case 5:
	    case 6:
                r = "RTP DVI4";
		break;

	    case 7:
                r = "RTP LPC";
		break;

	    case 8:
                r = "RTP PCMA";
		break;

	    case 9:
                r = "RTP G722";
		break;

	    case 10:
	    case 11:
                r = "RTP L16";
		break;

	    default:
                r = "RTP Unknown";
		break;
	}
	return r;
    }

    /* Hokey attempt to detect VAT packets.  We can't tell
       VAT PCMU packets from unmarked Speak Freely packets, so
       we let them fall through. */

    if (clue && (p[0] & 0xC0) == 0) {
	char *r = NULL;

	switch (p[1] & 0x7F) {
	    case VAT_AUDF_GSM:
                r = "VAT GSM";
		break;

	    case VAT_AUDF_LPC4:
                r = "VAT LPC";
		break;

	    case VAT_AUDF_IDVI:
                r = "VAT IDVI";
		break;

	    case VAT_AUDF_L16_16:
	    case VAT_AUDF_L16_44:
                r = "VAT L16";
		break;
	}
	if (r != NULL) {
	    return r;
	}
    }

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
    struct MD5Context md5c;
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
    MD5Final(key, &md5c);
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

/*  SENDLWLMESSAGE  --	If enabled, send a message identifying us
                        to each selected Look Who's Listening server.  */

static void sendLwlMessage(int dobye)
{
    int i, sock;

    for (i = 0; i < lwl_nsites; i++) {
	if (lwl_ports[i] >= 0) {
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
		} else {
		    if (send(sock, (char *) sdes, sdesl, 0) < 0) {
                        perror("sending look who's listening source ID message");
		    }
		}
	    } else {
                perror("connecting look who's listening socket");
	    }
    	    close(sock);
	}
    }
    lwltimer = LWL_RETRANSMIT;
}

/*  RELEASE  --  Alarm signal-catching function to retransmit
		 packets at the correct time and to time out
		 idle hosts.  */

static void release()
{
    struct connection *c, *l, *n;

    if (crit) {
	clash = TRUE;
	ularm(60000000L);
    } else {
	struct timeval tp;
	struct timezone tzp;
	struct queuedPacket *qp;
	double td;
	long timerStep;

	gettimeofday(&tp, &tzp);
	td = (tp.tv_sec - timebase) * 1000000.0 + tp.tv_usec;

	/* Transmit any queued packets scheduled to go out within
	   epsilon of the current time. */

#define Timer_Epsilon	4000	      /* Timer epsilon in microseconds */
#define Timer_Latency	2000	      /* Dispatch latency adjustment */

#if (Timer_Latency >= Timer_Epsilon)
        error = "Timer epsilon must be greater than latency.";
#endif

	while (qph != NULL && ((qph->when - td) < Timer_Epsilon)) {
	
	    /* If enabled, shuffle a randomly chosen percentage of packets,
	       swapping the packet at the head of the queue with a packet
	       randomly chosen with a maximum depth in the queue given by
	       debug_packet_shuffle_depth. */

/* #define SHUFFLE_DEBUG */	    	    	/* Ridiculously detailed debug information for packet shuffling */
    	    if ((debug_packet_shuffle > 0) && ((random() % 100) <= debug_packet_shuffle)) {
	    	if (qph->next == NULL) {
#ifdef SHUFFLE_DEBUG
		    if (Debug) {
                	fprintf(stderr, "%s: Suppressing shuffle because only one packet in queue.\n",
			    prog);
		    }
#endif
		} else {
		    int sdepth = random() % debug_packet_shuffle_depth, n;
		    struct queuedPacket *qpp = qph, *qpl = NULL, *qpt;
		    double swhen;

		    for (n = 0; (n <= sdepth) && (qpp->next != NULL); n++) {
		    	    qpl = qpp;
			    qpp = qpl->next;
		    }
		    if (Debug) {
                	fprintf(stderr, "%s: Shuffling with packet %d (wanted %d).\n",
			    prog, n , sdepth + 1);
		    }
		    
		    /* At this point qph is the packet to push back in
		       the queue, qpp is the one we're going to shuffle
		       to the head of the queue, and qpl is the packet
		       which precedes qpp.  Now we have to re-splice all
		       the links.  *NOTE* that qpl can be the same as
		       qph! */
		       
#ifdef SHUFFLE_DEBUG
		    assert(qpl != NULL);
		    assert(qpp != NULL);
		    assert(qpp != qph);
#endif
		    swhen = qpp->when;  	/* Set time for pushed packet to one we're
		    	    	    	    	   sending in its stead */
		    qpp->when = qph->when;
		    qph->when = swhen;
		    
		    if (n == 1) {
		    	/* If we're exchanging the first two packets, we need to
			   relink them to avoid linking the second packet to itself
			   in a simple swap.  Oh, for a doubly linked list here.... */
		    	qph->next = qpp->next;
			qpp->next = qph;
		    } else {
			qpt = qph->next;  	    /* Swap next links of packets we're exchanging */
			qph->next = qpp->next;
			qpp->next = qpt;
		    }
#ifdef SHUFFLE_DEBUG
		    assert(qpp->next != qpp);
		    assert(qph->next != qph);
#endif
		    
		    /* If we're swapping a packet to the end of the queue,
		       update the queue tail pointer. */
		       
		    if (qph->next == NULL) {
		    	qptail = qph;
#ifdef SHUFFLE_DEBUG
			if (Debug) {
                	    fprintf(stderr, "%s: Adjusting qptail.\n", prog);

			}
#endif
		    }
		    
		    /* Change next link of packet preceding the one we're
		       shuffling to the head to point to its replacement. */
    	    	    if (qpl != qph) {
		    	qpl->next = qph;
		    } else {
#ifdef SHUFFLE_DEBUG
			if (Debug) {
                	    fprintf(stderr, "%s: First packet tweak.\n", prog);

			}
#endif
		    }
		    qph = qpp;	    	    	/* Place replacement packet at head of queue */
#ifdef SHUFFLE_DEBUG
		    assert(qph->next != qph);
#endif
    	    	}
	    }

#ifdef SHUFFLE_DEBUG
    	    /* Check for shuffling messing up the time sequence of packets. */	    
	    if (qph != NULL) {
	    	struct queuedPacket *qpp = qph, *qpl;
		
		for (; qpp->next != NULL; ) {
		    qpl = qpp;
		    qpp = qpp->next;
		    if (qpl->when > qpp->when) {
		    	fprintf(stderr, "%s: ** Out of sequence time after packet shuffle.\n", prog);
		    }
		}
	    }
#endif

	    if (qph->pktlen > 0) {
		if (sendto(sock, qph->pktdata, qph->pktlen,
		    0, (struct sockaddr *) &(qph->where),
		    sizeof(struct sockaddr_in)) < 0) {
                    perror("sending datagram message");
		}
		if (Debug) {
                    fprintf(stderr, "%s: returning %d bytes to %s/%d.\n",
			prog, qph->pktlen, inet_ntoa(qph->where.sin_addr),
			ntohs(qph->where.sin_port));
		}
	    }
	    qp = qph;
	    qph = qp->next;
	    if (qph == NULL) {
		qptail = NULL;
	    }
	    free(qp);
	}

        /* Mark idle any hosts that haven't sent us anything recently. */

	c = conn;
	l = NULL;
	actives = 0;
	while (c != NULL) {
	    n = c->con_next;
	    if (!c->con_bye && (td < (c->con_timeout + hosttimeout))) {
		actives++;
		l = c;
	    } else {
		if (showhosts) {
                    fprintf(stderr, "%s: %s %s idle\n", prog, etime(), c->con_hostname);
		}
		if (l == NULL) {
		    conn = n;
		} else {
		    l->con_next = n;
		}
		free(c);
	    }
	    c = n;
	}

        /* Update our Look Who's Listening information if the
	   timeout has expired. */

	if (sdes != NULL && td >= lwltimer) {
	    sendLwlMessage(FALSE);
	    lwltimer = td + LWL_RETRANSMIT;
	}

	/* Reset the time to the next event in the queue. */

	if (qph != NULL) {
	    timerStep = (long) ((qph->when - td) - Timer_Latency);
	} else {
	    if (actives > 0) {
		timerStep = TickTock;
	    } else {
		timerStep = TockTock;
	    }
	}
	ularm(timerStep);
	if (Debug) {
            fprintf(stderr, "Tick: %.2f...\n", timerStep / 1000000.0);
	}
    }
    signal(SIGALRM, release);	      /* Set signal to handle timeout */
}

/*  EXITING  --  Catch as many program termination signals as
		 possible and clean up before exit.  */

static void exiting()
{

    if (sdes) {
	sendLwlMessage(TRUE);
    }
    exit(0);
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
    V fprintf(stderr, "%s  --  Speak Freely echo server.\n", prog);
    V fprintf(stderr, "            %s.\n", Relno);
    V fprintf(stderr, "\n");
    V fprintf(stderr, "Usage: %s [options]\n", prog);
    V fprintf(stderr, "Options:\n");
    V fprintf(stderr, "           -D               Enable debug output\n");
    V fprintf(stderr, "           -Pport           Listen on given port\n");
    V fprintf(stderr, "           -U               Print this message\n");
    V fprintf(stderr, "           -Vtimeout        Show hostnames that connect\n");
    V fprintf(stderr, "           -Zdrop,shuffle,depth  Simulate errors: drop%% packets, shuffle%% with depth\n");
    V fprintf(stderr, "\n");
    V fprintf(stderr, "by John Walker\n");
    V fprintf(stderr, "   http://www.fourmilab.ch/\n");
}

/*  Main program.  */

int main(int argc, char *argv[])
{
    int i, length;
    struct soundbuf sb;
    struct connection *c;
    char *cp;
    int newconn;
    struct auhdr {		      /* .au file header */
	char magic[4];
	long hsize, dsize, emode, rate, nchan;
    };
    struct timeval tp;
    struct timezone tzp;
    struct queuedPacket *qp;

    prog = prog_name(argv[0]);
    gettimeofday(&tp, &tzp);
    timebase = tp.tv_sec;

    /* First pass option processing.  We have to first scan
       the options to handle any which affect creation of the
       socket.	One the second pass we can assume the socket
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

                case 'P':             /* -Pport  --  Port to listen on */
		    whichport = atoi(op + 1);
		    break;

                case 'U':             /* -U  --  Print usage information */
                case '?':             /* -?  --  Print usage information */
		    usage();
		    return 0;
	    }
	} else {
	    usage();
	    return 2;
	}
    }

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
		    debugforce = 1;
		    break;

                case 'V':             /* -V  --  Show hostnames that connect */
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
		    
		case 'Z':   	    /* -Zdrop,shuffle,depth  --  Set packet drop and shuffle
		    	    	    	    percentage, maximum packet shuffle depth in queue */
		    sscanf(op + 1, "%d,%d,%d", &debug_packet_drop, &debug_packet_shuffle,
		    	    &debug_packet_shuffle_depth);
#define Constrain(x, vmin, vmax) ((x) < (vmin) ? (vmin) : ((x) > (vmax) ? (vmax) : (x)))
		    debug_packet_drop = Constrain(debug_packet_drop, 0, 100);
		    debug_packet_shuffle = Constrain(debug_packet_shuffle, 0, 100);
		    debug_packet_shuffle_depth = Constrain(debug_packet_shuffle_depth, 1, 1000);
		    break;
	    }
	}
    }
    
    /* If debug packet error simulation is configured, always report it to
       standard error.  This is unconditional since we don't want a public
       echo server accidentally operating in this mode.  */
       
    if (debug_packet_drop > 0) {
    	fprintf(stderr, "%s: **WARNING: Dropping %d%% of packets for debugging.\n",
	    prog, debug_packet_drop);
    }
    if (debug_packet_shuffle > 0) {
    	fprintf(stderr, "%s: **WARNING: Shuffling %d%% of packets for debugging: maximum depth %d.\n",
	    prog, debug_packet_shuffle, debug_packet_shuffle_depth);
    }

    /* Find assigned port value and print it. */

    length = sizeof(name);
    if (getsockname(sock, (struct sockaddr *) &name, &length) < 0) {
        perror("getting socket name");
	return 1;
    }
#ifdef SHOW_SOCKET
    fprintf(stderr, "%s: socket port #%d\n", prog, ntohs(name.sin_port));
#endif

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

    signal(SIGHUP, exiting);	      /* Set signal to handle termination */
    signal(SIGINT, exiting);	      /* Set signal to handle termination */
    signal(SIGTERM, exiting);	      /* Set signal to handle termination */
    signal(SIGALRM, release);	      /* Set signal to handle timeout */
    ularm(TockTock);		      /* Start periodic timer */

    /* Read from the socket. */

    while (TRUE) {
	int rll;
	double pktime;		      /* Time packet is to be echoed */
	fd_set fdset;
	int wsock, control;
	unsigned char *p = (unsigned char *) &sb;

	FD_ZERO(&fdset);
	FD_SET(sock, &fdset);
	FD_SET(ssock, &fdset);
	if (select(FD_SETSIZE, &fdset, NULL, NULL, NULL) <= 0) {
	    continue;
	}
	wsock = FD_ISSET(sock, &fdset) ? sock :
		    (FD_ISSET(ssock, &fdset) ? ssock : -1);
	if (wsock < 0) {
	    continue;
	}
	control = wsock == ssock;
	fromlen = sizeof(from);
	if ((rll = recvfrom(wsock, (char *) &sb, sizeof sb, 0, (struct sockaddr *) &from, &fromlen)) < 0) {
	    if (errno != EINTR) {
                perror(!control ? "receiving data packet" :
                                  "receiving control packet");
	    }
	    continue;
	}
	
	gettimeofday(&tp, &tzp);      /* Get time packet received */
	pktime = ((tp.tv_sec + Echo_Time) - timebase) * 1000000.0 + tp.tv_usec;
#ifndef SHOW_SOCKET
	if (Debug) {
            fprintf(stderr, "%s: %d bytes read from %s socket.\n", prog, rll,
                    control ? "control" : "data");
	}
#endif

    	if ((debug_packet_drop > 0) && ((random() % 100) <= debug_packet_drop)) {
	    if (Debug) {
        	fprintf(stderr, "Dropping packet at random.\n");
	    }
	    continue;
	}

	/* See if this connection is active.  If not, initialise a new
	   connection. */

	newconn = FALSE;
	crit = TRUE;		      /* Set connection list critical section lock */
	c = conn;
	while (c != NULL) {
	    if (memcmp(&from.sin_addr, &(c->con_addr),
		       sizeof(struct in_addr)) == 0) {
		break;
	    }
	    c = c->con_next;
	}
	if (c == NULL) {
	    c = (struct connection *) malloc(sizeof(struct connection));
	    if (c != NULL) {
		struct hostent *h;

		newconn = TRUE;
		c->con_next = conn;
		c->pgpkey[0] = FALSE;
		bzero(c->keymd5, 16);
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
	    c->con_compmodes = NULL;
	    c->con_control = FALSE;
	    c->con_bye = FALSE;
	}

	if (c != NULL) {
	    /* Reset connection timeout. */
	    c->con_timeout = pktime;
	    if (newconn || c->con_bye) {
		if (showhosts) {
                    fprintf(stderr, "%s: %s %s connect\n", prog, etime(), c->con_hostname);
		}
		c->con_bye = FALSE;
	    }
	} else {
	    crit = FALSE;
	    if (clash) {
		release();
	    }
	    continue;
	}

        /* Detect BYE packets and don't retransmit them, as they'll
	   reopen the connection on the other end. */

	if (control && ((isValidRTCPpacket((unsigned char *) &sb, rll) &&
			 isRTCPByepacket((unsigned char *) &sb, rll)) ||
			(((p[0] & 0xC0) == 0) && (p[1] == 2)))) {
	    struct queuedPacket *qp;

	    if (showhosts) {
                fprintf(stderr, "%s: %s %s bye\n", prog, etime(), c->con_hostname);
	    }
	    c->con_bye = TRUE;

	    /* Discard any packets queued to this host. */

	    qp = qph;
	    while (qp != NULL) {
		if (memcmp(&(qp->where.sin_addr), &(c->con_addr),
			   sizeof(struct in_addr)) == 0) {
		    if (Debug) {
                        fprintf(stderr, "%s: discarding %d bytes to %s/%d.\n",
			    prog, qph->pktlen, inet_ntoa(qph->where.sin_addr),
			    ntohs(qph->where.sin_port));
		    }
		    qp->pktlen = 0;
		}
		qp = qp->next;
	    }
	    crit = FALSE;
	    if (clash) {
		release();
	    }
	    continue;
	}

	/* Add the packet to the queue of packets awaiting
	   retransmission. */

	qp = (struct queuedPacket *) malloc(sizeof(struct queuedPacket) + rll);
	if (qp != NULL) {
	    qp->next = NULL;
	    qp->when = pktime;
	    bcopy((char *) &from, (char *) &(qp->where),
		sizeof(struct sockaddr_in));
	    qp->where.sin_port = htons(whichport + (control ? 1 : 0));
	    qp->pktlen = rll;
	    bcopy((char *) &sb, qp->pktdata, rll);
	    if (qptail == NULL) {
		qph = qptail = qp;
		ularm(Echo_Time * 1000000L);
	    } else {
		qptail->next = qp;
		qptail = qp;
	    }
	}

	c->con_control |= control;
	sb.compression = ntohl(sb.compression);
	if (showhosts && !control) {
	    char *cmodes;

	    cmodes = compressionType(&sb, c->con_control);
	    if (c->con_compmodes != cmodes) {
		c->con_compmodes = cmodes;
                fprintf(stderr, "%s: %s sending %s.\n", prog, c->con_hostname,
			cmodes);
	    }
	}
	if (Debug) {
            fprintf(stderr, "%s: echoing %d %s bytes from %s.\n",
		    prog, rll,
                    control ? "RTP/VAT control" :
			      compressionType(&sb, c->con_control),
		    c->con_hostname);
	}
	sb.compression = htonl(sb.compression);

	crit = FALSE;
	if (clash) {
	    release();
	}
    }
#ifdef MEANS_OF_EXIT
    close(sock);
    exiting();
    return 0;
#endif
}
