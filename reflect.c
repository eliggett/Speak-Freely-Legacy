/*

			Speak Freely for Unix
			 Conference Reflector

      Designed and implemented in April of 1998 by John Walker.

    Note: this reflector currently works by simple packet replication;
    it does not examine the contents of packets or attempt to merge
    arriving audio streams in any intelligent fashion.	Because it
    appears to a connected user as a single connection in Speak
    Freely protocol, users must connect in Speak Freely protocol
    in order for their audio to be successfully decoded by others.
    I've left in complete logic for VAT and RTP connections,
    however, in order to make it easier to support those protocols
    if, in the future, the reflector is extended to add additional
    reflector and/or mixer functionality.

*/

#include "speakfree.h"
#include "version.h"
#include "vat.h"

#define SHOW_SOCKET

#define Reflect_Base 2000	      /* Reflector offset from default speaker port */

static int debugforce = 0;	      /* Debugging forced / prevented ? */
static int whichport = Internet_Port + Reflect_Base; /* Port to listen on (base address) */
static int sock;		      /* Input socket */
static int ssock;		      /* Control socket (RTP/VAT) */
static struct sockaddr_in from;       /* Sending host address */
static struct sockaddr_in name;       /* Address of destination host */
static int fromlen;		      /* Length of sending host address */
static int showhosts = FALSE;	      /* Show host names that connect */
static int hosttimeout = 180;	      /* Consider host idle after this number of seconds */
static char *prog;		      /* Program name */ 

static struct sockaddr_in lookhost;   /* Look who's listening host, if any */
static char *sdes = NULL;	      /* RTP SDES packet */
static int sdesl;		      /* RTP SDES packet length */
static unsigned long ssrc;	      /* RTP synchronisation source identifier */
static long lwltimer;		      /* Time next LWL retransmit scheduled */
static int actives = 0; 	      /* Currently active hosts */

#define LWL_RETRANSMIT	(5 * 60)      /* Seconds between LWL updates */

static struct in_addr lwl_sites[LWL_MAX_SITES]; /* LWL site addresses */
static int lwl_ports[LWL_MAX_SITES];  /* Ports for LWL hosts */
static int lwl_nsites = 0;	      /* Number of LWL sites published on */

static int monitor = 0; 	      /* Audio monitor ? */
static struct in_addr monitor_site;   /* Audio monitor site */
static int monitor_port;	      /* Audio monitor port */

static struct in_addr local_site;     /* IP address running reflector */
static int local_site_known = 0;      /* Nonzero if local IP address known */
#ifndef INADDR_LOOPBACK 	      /* Should be defined in netinet/in.h, but just in case */
#define INADDR_LOOPBACK (unsigned long) 0x7F000001L
#endif

static char *htmlFile = NULL;	      /* HTML file name base */
static int htmlTime = 1 * 60;	      /* HTML file update time */
static time_t htmlLast = 0;	      /* HTML last update time */
static int htmlChange = TRUE;	      /* Change since last HTML update ? */
static int htmlRefresh = 0;	      /* HTML client-pull refresh interval */
#define HTML_REFRESH    "<meta http-equiv=\"Refresh\" content=\"%d\">\n"

#define connection r_connection

/* Open connection state. */

struct connection {
    struct connection *con_next;      /* Next connection */
    struct in_addr con_addr;	      /* Host Internet address */
    long con_timeout;		      /* Connection timeout */
    char con_hostname[264];	      /* Host name */
    short con_protocol; 	      /* Transmission protocol */
    char con_session[4];	      /* VAT/RTP session identifier */

    char con_uname[4096];	      /* User name */
    char con_email[256];	      /* User E-mail address, if known */

    /* Special fields for reflector. */

    char *con_compmodes;	      /* Last compression modes */
    int con_control;		      /* Control packet seen ? */
    int con_bye;		      /* BYE packet received ? */

    char con_cname[256];	      /* RTCP unique name identifier */
    char con_phone[256];	      /* User phone number */
    char con_loc[1024]; 	      /* User location */
    time_t con_ltime;		      /* Last update time */
};

static struct connection *conn = NULL; /* Chain of current connections */

#define Debug	    (debugforce != 0) /* Generate debug output */

#define TickTock    7		      /* Alarm interval in seconds */
#define TockTock    60		      /* Alarm interval when no connections open */

static int crit = FALSE;	      /* Queue critical section lock */
static int clash = FALSE;	      /* Critical section clash retry flag */

/*  ETIME  --  Edit time and date for log messages.  */

static char *etime(int gmt)
{
    struct tm *t;
    time_t clock;
    static char s[20];

    time(&clock);
    if (gmt) {
	t = gmtime(&clock);
    } else {
	t = localtime(&clock);
    }
    sprintf(s, "%d-%02d-%02d %02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
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

    /* If you get compile or link errors on references to any of the
       following functions, simply comment out the offending lines and
       try again.  This sequence of code uses a variety of information
       about the instantaneous execution environment of the program to
       construct a unique session key (or SSRC) to uniquely identify
       this sender in RTP packets.  The intent is to stir as many
       items into the cauldron as possible so that the resulting brew,
       cranked through MD5, will have as close a probability as
       possible to 1 in 2^32 of duplicating that of another user.
       Note that RTP does not guarantee SSRCs are unique.  If a
       duplication should occur, the result may be confusing but not
       disastrous.  */

    sprintf(s + strlen(s), "%u", getpid());
    sprintf(s + strlen(s), "%u", getppid());
    V getcwd(s + strlen(s), 256);
    sprintf(s + strlen(s), "%lu", clock());
    sprintf(s + strlen(s), "%lu", time(NULL));
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

                    l = rtp_make_bye(v, ssrc, "Exiting sfreflect", FALSE);
		    if (send(sock, v, l, 0) < 0) {
                        perror("sending look who's listening BYE packet");
		    }
		    if (Debug) {
                        fprintf(stderr, "%s: sent LWL BYE message to %s/%d.\n", prog,
			    inet_ntoa(lwl_sites[i]), lwl_ports[i]);
		    }
		} else {
		    if (send(sock, (char *) sdes, sdesl, 0) < 0) {
                        perror("sending look who's listening source ID message");
		    }
		    if (Debug) {
                        fprintf(stderr, "%s: sent LWL message to %s/%d.\n", prog,
			    inet_ntoa(lwl_sites[i]), lwl_ports[i]);
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

/*  MAKEHTML  --  Create an HTML file showing current connections.  If
                  "private" is set, exact-match names are included
		  in the HTML file.  */

static void makeHTML(char *fname, int private)
{
    FILE *of;
    char f[132], fn[132];

    strcpy(f, fname);
    strcat(f, ".new");
    of = fopen(f, "w");
    if (of != NULL) {
	struct connection *lw;

#define P(x) fprintf(of, x)

	lw = conn;
        P("<html>\n<head>\n");
	if (htmlRefresh > 0) {
	    fprintf(of, HTML_REFRESH, htmlRefresh);
	}
        P("<title>\nSpeak Freely: Sites Active on Reflector\n");
        P("</title>\n</head>\n\n<body>\n<center>\n<h1>");
        P("Speak Freely: Sites Active on Reflector</h1>\n<h2>");
        fprintf(of, "Last change: %s UTC</h2>\n</center>\n<p>\n", etime(TRUE));

	if (lw == NULL) {
            P("<h2>No sites active.</h2>\n");
	} else {
	    int i = 0;

	    while (lw != NULL) {
                if (!lw->con_bye && (private || ((lw->con_email[0] != '*') &&
                    (lw->con_cname[0] != '*')))) {
		    i++;
		}
		lw = lw->con_next;
	    }
	    lw = conn;
	    if (i == 0) {
		/* Fib about number of sites active if all active sites
                   don't want a directory listing or are in termination. */
                P("<h2>No sites active.</h2>\n");
	    } else {
                fprintf(of, "<h2>%d site%s active.</h2>\n",
                    i, i == 1 ? "" : "s");
                P("<pre>\n");
		while (lw != NULL) {
                    if (!lw->con_bye && (private || ((lw->con_email[0] != '*') &&
                        (lw->con_cname[0] != '*')))) {
			char ipport[40];
			struct tm *lt;
			char s[384];

			lt = gmtime(&lw->con_ltime);
                        sprintf(ipport, "%s:%d", inet_ntoa(lw->con_addr), whichport);
                        sprintf(s, "\n%-24s %-48s %02d:%02d\n", ipport, lw->con_cname,
			    lt->tm_hour, lt->tm_min);
			outHTML(of, s);
			if (lw->con_uname[0] != 0) {
                            sprintf(s, "%25s%s\n", "", lw->con_uname);
			    outHTML(of, s);
			}
			if (lw->con_loc[0] != 0) {
                            sprintf(s, "%25s%s\n", "", lw->con_loc);
			    outHTML(of, s);
			}
			if (lw->con_phone[0] != 0) {
                            sprintf(s, "%25sPhone:  %s\n", "", lw->con_phone);
			    outHTML(of, s);
			}
			if (lw->con_email[0] != 0) {
                            sprintf(s, "%25sE-mail: %s\n", "", lw->con_email);
			    outHTML(of, s);
			}
		    }
		    lw = lw->con_next;
		}
                P("</pre>\n");
	    }
	}

        P("</body>\n</html>\n");
	fclose(of);
	strcpy(fn, fname);
        strcat(fn, ".html");
	rename(f, fn);
	if (Debug) {
            fprintf(stderr, "%s: updated %s\n", prog, fn);
	}
    }
}

/*  UPDHTML  --  Update HTML if necessary.  */

static void updHTML(void)
{
    time_t now = time(NULL);

    if ((htmlFile != NULL) && htmlChange && 
	((htmlTime <= 0) || ((now - htmlLast) > htmlTime))) {
	htmlLast = now;
	htmlChange = FALSE;
	if (htmlFile != NULL) {
	    /* The reflector always includes names of connected users
               in its HTML summary, even if they've flagged their
               names not to appear in sflwld's public HTML output.
	       The logic is that connecting to a reflector is
	       equivalent to connecting to another user, in which
               case one's identity is shown.  If you want to permit
               "stealth conference members", change the TRUE in the
	       following line to FALSE. */
	    makeHTML(htmlFile, TRUE);
	}
    }
}

/*  CHANGED  --  Indicate a change which may require updating
		 the HTML file.  */

static void changed(void)
{
    htmlChange = TRUE;
    updHTML();
}

/*  RELEASE  --  Alarm signal-catching function to retransmit
		 packets at the correct time and to time out
		 idle hosts.  */

static void release()
{
    struct connection *c, *l, *n;

    if (crit) {
	clash = TRUE;
	alarm(60);
	if (Debug) {
            fprintf(stderr, "%s: Critical section clash in release()\n", prog);
	}
    } else {
	int timerStep;
	long tdate;
	char *hdes = NULL;
	int hdesl = 0;

	time(&tdate);

	/* Mark idle any hosts that have sent us a BYE message or
           haven't sent any packets in the last hosttimeout seconds. */

	c = conn;
	l = NULL;
	actives = 0;
	while (c != NULL) {
	    n = c->con_next;
	    if (!c->con_bye && (tdate < (c->con_timeout + hosttimeout))) {
		actives++;
		l = c;
		if (Debug) {
                    fprintf(stderr, "Host %s %ld seconds from timeout.\n",
			c->con_hostname, (c->con_timeout + hosttimeout) - tdate);
		}
	    } else {
		if (showhosts) {
                    fprintf(stderr, "%s: %s %s idle\n", prog, etime(FALSE), c->con_hostname);
		}
		if (l == NULL) {
		    conn = n;
		} else {
		    l->con_next = n;
		}
		free(c);
		htmlChange = TRUE;    /* Indicate HTML file update needed */
	    }
	    c = n;
	}

        /* Update our Look Who's Listening information if the
	   timeout has expired. */

	if (sdes != NULL && tdate >= lwltimer) {
	    sendLwlMessage(FALSE);
	    lwltimer = tdate + LWL_RETRANSMIT;
	}

	/* If any hosts are active, send them all a heartbeat
	   identifying the reflector. */

	if (actives > 0) {
	    struct connection *rc;

	    hdesl = rtp_make_sdes(&hdes, ssrc, -1, TRUE);
	    /* Set Speak Freely protocol flag in packet */
	    hdes[0] = (hdes[0] & 0x3F) | (1 << 6);
	    rc = conn;
	    while (rc != NULL) {

		/* Once again we have to worry about the case where the
		   reflector is being accessed from the same machine on
                   which it is running.  In that case, we'll have a 
		   connection in the chain for the local host, since sfmike
                   is sending us heartbeats and sound, but we don't want
                   to send heartbeats since they'd loop right back and
		   wind up in our own input queue.  So, test the address
                   against the local site and skip the heartbeat if they're
		   the same.  The actual heartbeats to the copy of sfspeaker
		   on the local host are sent in the section of code below
		   which deals with the monitoring host. */

		if (!rc->con_bye &&
		    (ntohl(rc->con_addr.s_addr) != INADDR_LOOPBACK) &&
		    (!local_site_known ||
		     (memcmp(&local_site, &(rc->con_addr), sizeof(struct in_addr)) != 0))) {
		    struct sockaddr_in pto;

		    pto.sin_family = AF_INET;
		    pto.sin_port = htons(whichport + 1);
		    bcopy(&(rc->con_addr), &pto.sin_addr, sizeof(struct in_addr));
		    if (sendto(sock, hdes, hdesl,
			0, (struct sockaddr *) &pto,
			sizeof(struct sockaddr_in)) < 0) {
                        perror("sending heartbeat");
		    }
		    if (Debug) {
                        fprintf(stderr, "%s: sending heartbeat to %s/%d (%s).\n",
			    prog, inet_ntoa(rc->con_addr),
			    whichport + 1, rc->con_hostname);
		    }
		}
		rc = rc->con_next;
	    }
	}

	/* If there is a monitor host, send it a heartbeat also.  */

	if (monitor) {
	    struct sockaddr_in pto;

	    if (hdes == NULL) {
		hdesl = rtp_make_sdes(&hdes, ssrc, -1, TRUE);
		/* Set Speak Freely protocol flag in packet */
		hdes[0] = (hdes[0] & 0x3F) | (1 << 6);
	    }

	    pto.sin_family = AF_INET;
	    pto.sin_port = htons(monitor_port + 1);
	    bcopy(&(monitor_site), &pto.sin_addr, sizeof(struct in_addr));
	    if (sendto(sock, hdes, hdesl,
		0, (struct sockaddr *) &pto,
		sizeof(struct sockaddr_in)) < 0) {
                perror("sending heartbeat");
	    }
	    if (Debug) {
                fprintf(stderr, "%s: sending heartbeat to monitor host %s/%d.\n",
		    prog, inet_ntoa(monitor_site),
		    monitor_port + 1);
	    }
	}
	if (hdes != NULL) {
	    free(hdes);
	}

	/* Reset the timer based on whether anybody is active. */

	if (actives > 0) {
	    timerStep = TickTock;
	} else {
	    timerStep = TockTock;
	}
	alarm(timerStep);
	if (Debug) {
	    time_t t;
	    struct tm *ltd;

	    time(&t);
	    ltd = localtime(&t);
            fprintf(stderr, "Tick: %2d:%02d %d...\n", ltd->tm_hour, ltd->tm_min, timerStep);
	}
    }
    updHTML();                        /* Update HTML if something's changed recently */
    signal(SIGALRM, release);	      /* Reset signal to handle timeout */
}

/*  EXITING  --  Catch as many program termination signals as
		 possible and clean up before exit.  */

static void exiting()
{
    if (sdes) {
	/* De-list from LWL server. */
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
    V fprintf(stderr, "%s  --  Speak Freely conference reflector.\n", prog);
    V fprintf(stderr, "               %s.\n", Relno);
    V fprintf(stderr, "\n");
    V fprintf(stderr, "Usage: %s [options]\n", prog);
    V fprintf(stderr, "Options:\n");
    V fprintf(stderr, "           -D               Enable debug output\n");
    V fprintf(stderr, "           -Hpath           Write HTML active site list on base path\n");
    V fprintf(stderr, "           -Insec           Interval between HTML updates\n");
    V fprintf(stderr, "           -Mhost[:port]    Monitor audio on host and port\n");
    V fprintf(stderr, "           -Pport           Listen on given port\n");
    V fprintf(stderr, "           -Rnsec           Client-pull HTML refresh every nsec seconds\n");
    V fprintf(stderr, "           -U               Print this message\n");
    V fprintf(stderr, "           -Vtimeout        Show hostnames that connect\n");
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
    char md5key[16];

    prog = prog_name(argv[0]);

    /* First pass option processing.  We have to first scan
       the options to handle any which affect creation of the
       socket.	*/

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

    /* Create the sockets from which to read audio and control packets. */

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

                case 'H':             /* -Hname  --  HTML file name base path */
		    htmlFile = op + 1;
		    break;

                case 'I':             /* -Isec  --  Interval between HTML updates */
		    htmlTime = atoi(op + 1);
		    break;

                case 'M':             /* -Mhost:port  --  Monitor on given host and port */
		    {
			char *ep, *cp = op + 1;
			long iadr;
			struct hostent *h;

			monitor_port = Internet_Port;
                        if ((ep = strchr(cp, ':')) != NULL || (ep = strchr(cp, '/')) != NULL) {
			    *ep = 0;
			    monitor_port = atoi(ep + 1);
			}
			if (isdigit(*cp) && (iadr = inet_addr(cp)) != -1) {
			    bcopy((char *) &iadr, (char *) (&monitor_site),
				  sizeof iadr);
			    monitor = 1;
			} else {
			    h = gethostbyname(cp);
			    if (h != NULL) {
				bcopy((char *) (h->h_addr), 
				      (char *) (&monitor_site),
				      sizeof(unsigned long));
				monitor = 1;
			    } else {
                                fprintf(stderr, "%s: warning, monitor host %s unknown.\n",
				    prog, cp);
			    }
			}
			if (Debug && monitor) {
                            fprintf(stderr, "%s: sending monitor audio to %s/%d: %s.\n", prog,
				inet_ntoa(monitor_site), monitor_port, cp);
			}
		    }
		    break;

                case 'R':             /* -Rnsec  --  Interval between HTML client-pull updates */
		    htmlRefresh = atoi(op + 1);
		    break;

                case 'V':             /* -V  --  Show hostnames that connect */
		    showhosts = TRUE;
		    if (op[1] != 0) {
			int t = atoi(op + 1);

			if (t > 0) {
			    if (t < (TickTock + 1)) {
				t = TickTock + 1;
			    }
			    hosttimeout = (t / TickTock) * TickTock;
			}
		    }
		    break;
	    }
	}
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

    /* Build the RTCP SDES packet identifying the reflector.
       This packet is used both to identify the reflector to
       those who connect and, if SPEAKFREE_LWL_TELL is defined,
       to register the reflector with the designated look who's
       listening hosts.  Note that we do *not* include a list
       of the identities of currently-connected users in our
       SDES, as a pure RTP reflector would do.	We rely instead
       on an HTML file, which can be accessed by anybody with
       a Web browser and does not require special support in
       the audio client program. */

    makeSessionKey(md5key);
    bcopy(md5key, (char *) &ssrc, sizeof ssrc);
    sdesl = rtp_make_sdes(&sdes, ssrc, whichport, FALSE);

    /* Contact look who's listening host, if requested. */

    cp = getenv("SPEAKFREE_LWL_TELL");
    if (cp != NULL) {
	struct hostent *h;
	char *ep, *np;
	long iadr;
	int n;

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

    /* Determine the IP address of the host running the
       reflector.  We need this to avoid duplicating packets
       when the reflector is both running and being accessed
       from the same machine. */

    {
	struct hostent *h;
	char hname[256];

	if (gethostname(hname, sizeof hname) == 0) {
	    h = gethostbyname(hname);
	    if (h != NULL) {
		bcopy((char *) (h->h_addr), (char *) (&local_site), sizeof(unsigned long));
		local_site_known = 1;
		if (Debug) {
                    fprintf(stderr, "%s: running on host %s (%s)\n", prog,
			inet_ntoa(local_site), hname);
		}
	    }
	}
    }

    signal(SIGHUP, exiting);	      /* Set signal to handle termination */
    signal(SIGINT, exiting);	      /* Set signal to handle termination */
    signal(SIGTERM, exiting);	      /* Set signal to handle termination */
    signal(SIGALRM, release);	      /* Set signal to handle timeout */
    alarm(TockTock);		      /* Start periodic timer */

    /* Read packets from the sockets. */

    while (TRUE) {
	int rll;
	long pkrtime;		      /* Time packet received */
	fd_set fdset;
	int wsock, control;
	struct connection *rc = conn;
	unsigned char *rchat = NULL;
	int rchatl = 0;

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
	time(&pkrtime);
#ifdef SHOW_SOCKET
	if (Debug) {
            fprintf(stderr, "%s: %d bytes read from %s socket from %s.\n", prog, rll,
                    control ? "control" : "data", inet_ntoa(from.sin_addr));
	}
#endif

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

		memset(c, 0, sizeof(struct connection));  /***** Added by Rod *****/
		newconn = TRUE;
		c->con_next = conn;
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
	    c->con_compmodes = NULL;
	    c->con_control = FALSE;
	    c->con_bye = FALSE;
	    c->con_ltime = time(NULL);

	    /* If this is the first connection after a period of
	       inactivity, make sure the timer loop is running at
	       the faster rate we used when one or more connections
	       are active. */

	    if (actives == 0) {
		alarm(TickTock);
	    }
	}

	if (c != NULL) {
	    /* Reset connection timeout. */
	    c->con_timeout = pkrtime;
	    if (newconn || c->con_bye) {
		if (showhosts) {
                    fprintf(stderr, "%s: %s %s connect\n", prog, etime(FALSE), c->con_hostname);
		}
		c->con_bye = FALSE;
		changed();		 /******* Added by Rod *******/
	    }
	} else {
	    crit = FALSE;
	    if (clash) {
		release();
	    }
	    continue;
	}

	/*  If this is a control channel packet, examine it to
	    determine if this is a new connection, protocol
	    change, end of connection and update the connection
	    list accordingly.  */

	if (control) {
	    short protocol = PROTOCOL_VATRTP_CRYPT;
	    unsigned char *p = (unsigned char *) &sb;
	    unsigned char *apkt;
	    int proto = (p[0] >> 6) & 3;

	    if (proto == 0) {
		/* To avoid spoofing by bad encryption keys, require
                   a proper ID message be seen before we'll flip into
		   VAT protocol. */
		if (((p[1] == 1) || (p[1] == 3)) ||
		    ((c->con_protocol == PROTOCOL_VAT) && (p[1] == 2))) {
		    protocol = PROTOCOL_VAT;
		    bcopy(p + 2, c->con_session, 2);  /* Save conference ID */

		    if (p[1] == 1) {
			char uname[256];

			bcopy(p + 4, uname, rll - 4);
			uname[rll - 4] = 0;
			if (strcmp(uname, c->con_uname) != 0) {
			    strcpy(c->con_uname, uname);
			    if (showhosts) {
                                fprintf(stderr, "%s: %s sending from %s.\n", prog,
					c->con_uname, c->con_hostname);
			    }
			    changed();
			}
		    }

		    /* Handling of VAT IDLIST could be a lot more elegant
		       than this. */

		    if (p[1] == 3) {
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
				if (showhosts) {
                                    fprintf(stderr, "%s: now in conference at %s:\n%s", prog,
					    c->con_hostname, uname);
				}
				changed();
			    }
			    free(uname);
			}
		    }

                    /* If it's a DONE packet, reset protocol to unknown. */

		    if (p[1] == 2) {
			c->con_protocol = protocol = PROTOCOL_UNKNOWN;
			c->con_timeout = -1;
			c->con_uname[0] = c->con_email[0] = 0;
			c->con_bye = TRUE;
			if (showhosts) {
                            fprintf(stderr, "%s: %s VAT connection closed.\n",
				    prog, c->con_hostname);
			}
			changed();
		    }
		}
	    } else if (proto == RTP_VERSION || proto == 1) {
		if (isValidRTCPpacket((unsigned char *) &sb, rll)) {
		    protocol = (proto == 1) ? PROTOCOL_SPEAKFREE : PROTOCOL_RTP;
		    bcopy(p + 4, c->con_session, 4);  /* Save SSRC */
		    c->con_ltime = time(NULL);
                    /* If it's a BYE packet, reset protocol to unknown. */
		    if (isRTCPByepacket((unsigned char *) &sb, rll)) {
			c->con_protocol = protocol = PROTOCOL_UNKNOWN;
			c->con_timeout = -1;
			c->con_uname[0] = c->con_email[0] = 0;
			c->con_bye = TRUE;
			if (showhosts) {
                            fprintf(stderr, "%s: %s %s connection closed.\n",
				    prog, c->con_hostname,
                                    proto == 1 ? "Speak Freely" : "RTP");
			}
			changed();

                    /* If it's a text chat message, reformat it to
		       be forwarded to all connected hosts. */

		    } else if (isRTCPAPPpacket((unsigned char *) &sb, rll,
				    RTCP_APP_TEXT_CHAT, &apkt) && apkt != NULL) {
			char *ident = c->con_hostname,
			     *echat = NULL;

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

			if (Debug) {
                            printf("%s: Text chat received \"%s: %s\"\n",
				c->con_hostname, ident, (char *) (apkt + 12));
			}

			echat = malloc(strlen(ident) +
				       strlen((char *) (apkt + 12)) + 8);
			if (echat != NULL) {
			    rchat = malloc(strlen(echat) + 256);

			    if (rchat != NULL) {
                                sprintf(echat, "%s: %s", ident, (char *) (apkt + 12));

				rchatl = rtp_make_app(rchat, ssrc, TRUE, RTCP_APP_TEXT_CHAT, echat);
				/* Set Speak Freely protocol flag in packet */
				rchat[0] = (rchat[0] & 0x3F) | (1 << 6);
			    }
			    free(echat);
			}

                    /* Otherwise, it's presumably an SDES, from which we
		       should update the user identity information for the
		       connection. */

		    } else {
			struct rtcp_sdes_request rp;

			rp.nitems = 5;
			rp.item[0].r_item = RTCP_SDES_CNAME;
			rp.item[1].r_item = RTCP_SDES_NAME;
			rp.item[2].r_item = RTCP_SDES_EMAIL;
			rp.item[3].r_item = RTCP_SDES_PHONE;
			rp.item[4].r_item = RTCP_SDES_LOC;
			if (parseSDES((unsigned char *) &sb, &rp)) {
			    char cname[256], uname[4096], email[256], phone[256], location[1024];

			    cname[0] = uname[0] = email[0] = phone[0] = location[0] = 0;
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

			    if (rp.item[3].r_text != NULL) {
				copySDESitem(rp.item[3].r_text, phone);
			    }
			    if (rp.item[4].r_text != NULL) {
				copySDESitem(rp.item[4].r_text, location);
			    }
			    if (rp.item[0].r_text != NULL) {
				copySDESitem(rp.item[0].r_text, cname);
			    }
			    if (strcmp(cname, c->con_cname) != 0 ||
				strcmp(uname, c->con_uname) != 0 ||
				strcmp(email, c->con_email) != 0 ||
				strcmp(phone, c->con_phone) != 0 ||
				strcmp(location, c->con_loc) != 0
				) {
				strcpy(c->con_cname, cname);
				strcpy(c->con_uname, uname);
				strcpy(c->con_email, email);
				strcpy(c->con_phone, phone);
				strcpy(c->con_loc, location);
				if (uname[0] != 0) {
				    if (showhosts) {
                                        fprintf(stderr, "%s: %s", prog, uname);
					if (email[0] != 0) {
                                          fprintf(stderr, " (%s)", email);
					}
                                        fprintf(stderr, " sending from %s.\n",
						c->con_hostname);
				    }
				    changed();
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
	    }

	    /* Done processing control packet.	We never reflect
	       non-chat control packets, just use them to update our own
	       list of connections. */

	    crit = FALSE;
	    if (clash) {
		release();
	    }
	    if (!rchat) {
		continue;
	    }
	}
	crit = FALSE;

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

	/* If this is not a sound packet (show your face packet,
	   PGP key, etc.) discard it.  These point-to-point facilities
           won't work and will probably disrupt a conference. */

	if ((c->con_protocol == PROTOCOL_SPEAKFREE) && (!rchat) &&
	    (!isSoundPacket(ntohl(sb.compression)))) {
	    if (Debug) {
                fprintf(stderr, "%s: discarding %d byte control packet (flags 0x%08lX) from %s/%d.\n",
		    prog, rll, (long) ntohl(sb.compression), inet_ntoa(c->con_addr),
		    whichport);
	    }
	    continue;
	}

        /*  If we're forwarding a text chat packet, copy it into the
	    buffer which we forward.  */

	if (rchat) {
	    rll = rchatl;
	    bcopy(rchat, &sb, rll);
	}

	/*  This is a data channel packet.  Reflect it back to all
	    active hosts, excepting of course the one which sent
	    it.  Packets are not to sent to hosts which have sent
	    us a BYE packet and not affirmatively re-connected.  */

	if (Debug) {
            fprintf(stderr, "%s: reflecting %d bytes from %s/%d (%s).\n",
		prog, rll, inet_ntoa(c->con_addr), whichport, c->con_hostname);
	}
	while (rc != NULL) {

	    /* The test involving local_site in the following statement
	       is to avoid sending a packet to ourselves when the same
	       machine is used to run the reflector and connect to it,
               using the "monitor site" facility.  In this circumstance
	       there will be a connection in the connection chain for
	       the local copy of sfmike sending to the reflector.  But
	       if we send to this address, what will actually happen is
	       that the reflector (namely us) will receive the packet and,
	       unless it originates from the local machine, re-echo it
	       to all the connected hosts, resulted in stut-stut-stuttering
               sound.  Since we know that we're in fact listening to
	       whichport on this host, there is no circumstance in which
	       we should ever send a packet on that port to the local host. */

	    if (rc != c && !(rc->con_bye) &&
		(ntohl(rc->con_addr.s_addr) != INADDR_LOOPBACK) &&
		(!local_site_known ||
		 (memcmp(&local_site, &(rc->con_addr), sizeof(struct in_addr)) != 0))) {
		struct sockaddr_in pto;

		pto.sin_family = AF_INET;
		pto.sin_port = htons(whichport + (rchat ? 1 : 0));
		bcopy(&(rc->con_addr), &pto.sin_addr, sizeof(struct in_addr));
		if (sendto(sock, (char *) &sb, rll,
		    0, (struct sockaddr *) &pto,
		    sizeof(struct sockaddr_in)) < 0) {
                    perror("reflecting sound packet");
		}
		if (Debug) {
                    fprintf(stderr, "%s: ... reflecting to %s/%d (%s).\n",
			prog, inet_ntoa(rc->con_addr), whichport + (rchat ? 1 : 0), rc->con_hostname);
		}
	    }
	    rc = rc->con_next;
	}

	/*  If audio monitoring is requested, send the packet to the
	    monitoring host as well.  If the originating site is the
            same as the monitoring host we don't forward the packet.
	    This case occurs when the same machine is running the
	    reflector and connected to it.  */

	if (monitor &&
	    (ntohl(c->con_addr.s_addr) != INADDR_LOOPBACK) &&
	    (!local_site_known ||
	     (memcmp(&local_site, &(c->con_addr), sizeof(struct in_addr)) != 0))) {
	    struct sockaddr_in pto;

	    pto.sin_family = AF_INET;
	    pto.sin_port = htons(monitor_port + (rchat ? 1 : 0));
	    bcopy(&monitor_site, &pto.sin_addr, sizeof(struct in_addr));
	    if (sendto(sock, (char *) &sb, rll,
		0, (struct sockaddr *) &pto,
		sizeof(struct sockaddr_in)) < 0) {
                perror("echoing sound packet to monitor host");
	    }
	    if (Debug) {
                fprintf(stderr, "%s: ... sending to monitor host to %s/%d.\n",
		    prog, inet_ntoa(monitor_site), monitor_port + (rchat ? 1 : 0));
	    }
	}

	if (rchat) {
	    free(rchat);
	    rchat = NULL;
	}

	/*  If verbose mode is selected, indicate any change in
	    protocol or compression mode by this connection.  */

	c->con_control |= control;
	sb.compression = ntohl(sb.compression);
	if (showhosts && !control) {
	    char *cmodes = compressionType(&sb, c->con_control);

	    if (c->con_compmodes != cmodes) {
		c->con_compmodes = cmodes;
                fprintf(stderr, "%s: %s sending %s.\n", prog, c->con_hostname,
			cmodes);
	    }
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
