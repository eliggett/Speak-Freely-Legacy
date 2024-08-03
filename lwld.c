/*

			Speak Freely for Unix
                     Look Who's Listening Server

*/

#include "speakfree.h"
#include "version.h"

#ifdef THREADS
#include <pthread.h>
#define Lock(x) pthread_mutex_lock(&(x))
#define Unlock(x) pthread_mutex_unlock(&(x))
#define LockConn()  Lock(connLock)
#define UnlockConn() Unlock(connLock)
#else
#define LockConn()
#define UnlockConn()
#endif

#define TockTock    120 	      /* Timeout check frequency, seconds */
#define TimeoutTime 15 * 60	      /* No-response timeout interval, seconds */
#define ServiceThreadTimeout	60    /* Service thread timeout interval, seconds  */

#define MaxReplyPacket 512	      /* Maximum length of reply data */

static int sock;		      /* Our socket */
static char *prog;		      /* Program name */ 

static int lwlport = Internet_Port + 2; /* Look Who's Listening port */
static int debugging = FALSE;	      /* Debug mode enabled */
static int verbose = FALSE;	      /* Show connections/disconnects */
static int prolix = FALSE;	      /* Extremely verbose (show queries) */
#ifdef HEXDUMP
static int hexdump = FALSE;	      /* Dump received packets in hex ? */
#endif
static char *htmlFile = NULL;	      /* HTML file name base */
static char *htmlPrivateFile = NULL;  /* HTML private directory file name base */
static int htmlTime = 1 * 60;	      /* HTML file update time */
static time_t htmlLast = 0;	      /* HTML last update time */
static int htmlChange = TRUE;	      /* Change since last HTML update ? */
static int htmlRefresh = 0;	      /* HTML client-pull refresh interval */
#define HTML_REFRESH    "<meta http-equiv=\"Refresh\" content=\"%d\">\n"
static char *message = NULL;	      /* Server information message */
static int messagel;		      /* Length of server information message */

#ifdef THREADS

#define ForwarderSleep	30	      /* How often to update forwarded hosts */
#define ForwarderMaxQ	15	      /* Maximum items in forward queue */

struct lwl_queueitem {
    struct lwl_queueitem *next;       /* Next item in LWL queue */
    int q_rll;			      /* Length of message */
    char q_pkt[2];		      /* Message body */
};

struct forwarder {
    struct forwarder *next;	      /* Next forwarder in chain */
    int status; 		      /* Site status (0 = normal) */
    char *sitename;		      /* Site name specification */
    struct in_addr site;	      /* Internet address of target host */
    long port;			      /* Destination port on that host */
    pthread_t thread;		      /* ID of forwarder thread */
    int queuelen;		      /* Length of queue for forwarder */
    struct lwl_queueitem *head, *tail;/* Queue of messages to be forwarded */
};

static pthread_attr_t detached;       /* Attributes for our detached threads */
static pthread_mutex_t fwdlistLock = PTHREAD_MUTEX_INITIALIZER;
static struct forwarder *fwdlist = NULL; /* List of forwarding destinations */

#define lwl_nsites 1		      /* Threaded version driven from list */

struct ballOstring {
    struct ballOstring *next, *prev;  /* Forward and back queue links */
    pthread_t thread;		      /* Service thread ID */
    time_t startTime;		      /* When did thread start ? */
    struct in_addr site;	      /* Internet address of requesting site */
    struct service_arg *sa;	      /* Service thread argument */
};

static struct ballOstring bshead;     /* Ball of string queue head */
static pthread_mutex_t bsLock = PTHREAD_MUTEX_INITIALIZER;

#else
static struct sockaddr_in lookhost;   /* Look who's listening host, if any */
static struct in_addr lwl_sites[LWL_MAX_SITES]; /* LWL site addresses */
static long lwl_ports[LWL_MAX_SITES]; /* Ports for LWL hosts */
static int lwl_nsites = 0;	      /* Number of LWL sites published on */
#endif

struct lwl {
    struct lwl *next;		      /* Next connection */
    time_t ltime;		      /* Time of last update */
    unsigned long ssrc; 	      /* Session source descriptor */
    long naddr; 		      /* Internet address */
    short port; 		      /* Port address */
    char *cname;		      /* Canonical name */ 
    char *name; 		      /* User name */
    char *email;		      /* Electronic mail address */
    char *phone;		      /* Telephone number */
    char *loc;			      /* Geographic location */
    char *tool; 		      /* Application name */
};

#ifdef THREADS
				      /* Connection chain critical section */
static pthread_mutex_t connLock = PTHREAD_MUTEX_INITIALIZER;
#endif
static struct lwl *conn = NULL;       /* Chain of current connections */

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

/*  ESTIME  --	Edit short time.  */

static char *estime(void)
{
    struct tm *t;
    time_t clock;
    static char s[20];

    time(&clock);
    t = localtime(&clock);
    sprintf(s, "%02d:%02d", t->tm_hour, t->tm_min);
    return s;
}

/*  DUPSDESITEM  --  Make a copy of an SDES item and advance the pointer
		     past it.  */

static char *dupSdesItem(char **cp)
{
    char *ip = *cp, *bp;
    int l = ip[1] & 0xFF;

    bp = malloc(l + 1);
    if (bp != NULL) {
	bcopy(ip + 2, bp, l);
	bp[l] = 0;
    }
    *cp = ip + l + 2;
    return bp;
}

/*  DESTROYLWL	--  Release storage associated with an LWL list item.  */

static void destroyLwl(struct lwl *lw)
{
    if (lw->cname != NULL) {
	free(lw->cname);
    }
    if (lw->name != NULL) {
	free(lw->name);
    }
    if (lw->email != NULL) {
	free(lw->email);
    }
    if (lw->phone != NULL) {
	free(lw->phone);
    }
    if (lw->loc != NULL) {
	free(lw->loc);
    }
    if (lw->tool != NULL) {
	free(lw->tool);
    }
    free(lw);
}

/*  DUMPLWL  --  Dump an LWL on the specified stream.  */

static void dumpLwl(FILE *fo, struct lwl *lw)
{
    struct sockaddr_in s;

    s.sin_addr.s_addr = lw->naddr;
    fprintf(fo, "SSRC = %lX IP = %s Port %d %s", lw->ssrc, inet_ntoa(s.sin_addr),
		lw->port, ctime(&(lw->ltime)));
    if (lw->cname != NULL) {
        fprintf(fo, "    CNAME = %s\n", lw->cname);
    }
    if (lw->name != NULL) {
        fprintf(fo, "     NAME = %s\n", lw->name);
    }
    if (lw->email != NULL) {
        fprintf(fo, "    EMAIL = %s\n", lw->email);
    }
    if (lw->phone != NULL) {
        fprintf(fo, "    PHONE = %s\n", lw->phone);
    }
    if (lw->loc != NULL) {
        fprintf(fo, "      LOC = %s\n", lw->loc);
    }
    if (lw->tool != NULL) {
        fprintf(fo, "     TOOL = %s\n", lw->tool);
    }
}

#ifdef NEEDED

/*  DUMPLWLCHAIN  --  Dump all current connections.  */

static void dumpLwlChain(FILE *fo)
{
    struct lwl *lw;

    LockConn();
    lw = conn;
    fprintf(fo, "\n==========  %s  ==========\n", etime(FALSE));
    while (lw != NULL) {
        fprintf(fo, "\n");
	dumpLwl(fo, lw);
	lw = lw->next;
    }
    UnlockConn();
}
#endif

/*  LOGLWL  --	Generate log entry for LWL event.  */

static void logLwl(struct lwl *lw, char *event)
{
    char ipport[40], deef[256];
    struct sockaddr_in u;

    u.sin_addr.s_addr = lw->naddr;
    sprintf(ipport, "%s:%d", inet_ntoa(u.sin_addr), lw->port);
    sprintf(deef, "%s %s", ipport, lw->email ? lw->email : lw->cname);
    if (lw->name) {
        sprintf(deef + strlen(deef), " (%s)", lw->name);
    }
    printf("%s %s%s\n", estime(), event, deef);
}

/*  GARDOL  -- Validity-check an RTCP packet to make sure we don't
	       crash if some bozo sends us garbage.  */

static int gardol(unsigned char *p, int len)
{
    unsigned char *end;

    if ((((p[0] >> 6) & 3) != RTP_VERSION) ||	   /* Version incorrect ? */
	((p[0] & 0x20) != 0) || 		   /* Padding in first packet ? */
	((p[1] != RTCP_SR) && (p[1] != RTCP_RR) &&
	 (p[1] != RTCP_SDES) && (p[1] != RTCP_BYE) &&
	 (p[1] != RTCP_APP))) { 		   /* Item type valid ? */
	return FALSE;
    }
    end = p + len;

    do {
	/* Advance to next subpacket */
	p += (ntohs(*((short *) (p + 2))) + 1) * 4;
    } while (p < end && (((p[0] >> 6) & 3) == RTP_VERSION));

    return p == end;
}

/*  MAKEHTML  --  Create an HTML file showing current connections.  If
                  "private" is set, exact-match names are included
		  in the HTML file; this allows creation of a non-exported
                  active site file for the system manager's use.  */

static void makeHTML(char *fname, int private)
{
    FILE *of;
    char f[132], fn[132];

    strcpy(f, fname);
    strcat(f, ".new");
    of = fopen(f, "w");
    if (of != NULL) {
	struct lwl *lw;

#define P(x) fprintf(of, x)

	LockConn();
	lw = conn;
        P("<html>\n<head>\n");
	if (htmlRefresh > 0) {
	    fprintf(of, HTML_REFRESH, htmlRefresh);
	}
        P("<title>\nSpeak Freely: Active Sites\n");
        P("</title>\n</head>\n\n<body>\n<center>\n<h1>");
        P("Speak Freely: Active Sites</h1>\n<h2>");
        fprintf(of, "As of %s UTC</h2>\n</center>\n<p>\n", etime(TRUE));

	if (lw == NULL) {
            P("<h2>No sites active.</h2>\n");
	} else {
	    int i = 0;

	    while (lw != NULL) {
                if (private || ((lw->email == NULL || lw->email[0] != '*') &&
                    (lw->cname[0] != '*'))) {
		    i++;
		}
		lw = lw->next;
	    }
	    lw = conn;
	    if (i == 0) {
		/* Fib about number of sites active if all active sites
                   don't want a directory listing. */
                P("<h2>No sites active.</h2>\n");
	    } else {
                fprintf(of, "<h2>%d site%s active.</h2>\n",
                    i, i == 1 ? "" : "s");
                P("<pre>\n");
		while (lw != NULL) {
                    if (private || ((lw->email == NULL || lw->email[0] != '*') &&
                        (lw->cname[0] != '*'))) {
			char ipport[40];
			struct tm *lt;
			struct sockaddr_in u;
			char s[384];

			lt = gmtime(&lw->ltime);
			u.sin_addr.s_addr = lw->naddr;
                        sprintf(ipport, "%s:%d", inet_ntoa(u.sin_addr), lw->port);
                        sprintf(s, "\n%-24s %-48s %02d:%02d\n", ipport, lw->cname,
			    lt->tm_hour, lt->tm_min);
			outHTML(of, s);
			if (lw->name != NULL) {
                            sprintf(s, "%25s%s\n", "", lw->name);
			    outHTML(of, s);
			}
			if (lw->loc != NULL) {
                            sprintf(s, "%25s%s\n", "", lw->loc);
			    outHTML(of, s);
			}
			if (lw->phone != NULL) {
                            sprintf(s, "%25sPhone:  %s\n", "", lw->phone);
			    outHTML(of, s);
			}
			if (lw->email != NULL) {
                            sprintf(s, "%25sE-mail: %s\n", "", lw->email);
			    outHTML(of, s);
			}
		    }
		    lw = lw->next;
		}
                P("</pre>\n");
	    }
	}
	UnlockConn();

        P("</body>\n</html>\n");
	fclose(of);
	strcpy(fn, fname);
        strcat(fn, ".html");
	rename(f, fn);
	if (debugging) {
            fprintf(stderr, "%s: updated %s\n", prog, fn);
	}
    }
}

/*  UPDHTML  --  Update HTML if necessary.  */

static void updHTML(void)
{
    time_t now = time(NULL);

    if (((htmlFile != NULL) || (htmlPrivateFile != NULL)) && htmlChange && 
	((htmlTime <= 0) || ((now - htmlLast) > htmlTime))) {
	htmlLast = now;
	htmlChange = FALSE;
	if (htmlFile != NULL) {
	    makeHTML(htmlFile, FALSE);
	}
	if (htmlPrivateFile != NULL) {
	    makeHTML(htmlPrivateFile, TRUE);
	}
    }
}

/*  CHANGED  --  Indicate a change which may require updating
		 the HTML file.  */

static void changed(void)
{
    htmlChange = TRUE;
#ifndef THREADS
    updHTML();
#endif
}

/*  LCASE  --  Convert a string to lower case.	*/

static void lcase(char *s)
{
    while (*s) {
	if (isupper(*s)) {
	    *s = tolower(*s);
	}
	s++;
    }
}

#ifdef THREADS

/*  FORWARDER_THREAD  --  Thread which manages forwarding to a given
			  destination.	*/

static void *forwarder_thread(void *arg)
{
    struct forwarder *f = (struct forwarder *) arg;
    struct hostent *h;
    long iadr;

#ifdef DBTHREAD
fprintf(stderr, "Started forwarder thread for %s\n", f->sitename);
#endif
    if (isdigit(f->sitename[0]) && (iadr = inet_addr(f->sitename)) != -1) {
	bcopy((char *) &iadr, (char *) (&(f->site)),
	      sizeof iadr);
	f->status = 0;
    } else {
	h = gethostbyname(f->sitename);
	if (h != NULL) {
	    bcopy((char *) (h->h_addr), 
		  (char *) (&(f->site)),
		  sizeof(unsigned long));
	    f->status = 0;
	} else {
            fprintf(stderr, "%s: warning, forward destination %s unknown.\n",
		prog, f->sitename);
	    f->status = -2;
	    return NULL;
	}
    }
    if (debugging && f->status == 0) {
        fprintf(stderr, "%s: forwarding to LWL server %s: %s.\n", prog,
	    inet_ntoa(f->site), f->sitename);
    }

    while (f->status == 0) {
	struct lwl_queueitem *q;
	int sock = -1;

	while ((q = f->head) != NULL) {
	    int cstat;
	    struct sockaddr_in lookhost;

	    sock = socket(AF_INET, SOCK_STREAM, 0);
	    if (sock < 0) {
                perror("opening forwarding socket");
		break;
	    }

	    lookhost.sin_port = htons(f->port);
	    bcopy((char *) (&(f->site)), (char *) &lookhost.sin_addr.s_addr,
		  sizeof lookhost.sin_addr.s_addr);

	    errno = 0;
	    do {
		cstat = connect(sock, (struct sockaddr *) &(lookhost), sizeof lookhost);
		if (cstat >= 0) {
		    break;
		}
	    } while (errno == EINTR);
	    if (cstat >= 0) {
		if (send(sock, q->q_pkt, q->q_rll, 0) < 0) {
                    perror("forwarding look who's listening source ID message");
		    break;
		} else {
		    Lock(fwdlistLock);
		    f->head = q->next;
		    if (f->head == NULL) {
			f->tail = NULL;
		    }
		    free(q);
		    f->queuelen--;
		    if (f->queuelen < 0) {
                        fprintf(stderr, "%s: forward queue underflow to %s.\n", 
			    prog, f->sitename);
			f->queuelen = 0;
		    }
		    Unlock(fwdlistLock);
		    if (debugging) {
                        fprintf(stderr, "%s: forwarded packet to server %s. Queue length now %d.\n",
			    prog, f->sitename, f->queuelen);
		    }
		}
	    } else {

		/* As LWL servers go up and down, failures to connect
		   to forwarding destinations are not uncommon.  Further,
		   while a server is down, we get here on every packet
		   we attempt to forward.  To avoid blithering all
		   over the log, we only note such failure if in
		   prolix or debugging mode. */

		if (prolix || debugging) {
                    perror("connecting forwarding socket");
		}
		break;
	    }
	    close(sock);
	    sock = -1;
	}

	/* If we bailed out of the above loop due to an error in
	   forwarding the packet, make sure the socket is closed. */

	if (sock >= 0) {
	    close(sock);
	    sock = -1;
	}
	sleep(ForwarderSleep);
    }

#ifdef DBTHREAD
fprintf(stderr, "Exited forwarder thread for %s\n", f->sitename);
#endif
    f->status = -1;		      /* Forwarder terminated */
    return NULL;
}
#endif

/*  FORWARDLIST  --  Parse a list of servers to forward packets to. */

static void forwardList(char *cp)
{
#ifdef THREADS
    struct forwarder *f;
    char *ep, *np;

    while (TRUE) {
	while (*cp != 0 && isspace(*cp)) {
	    cp++;
	}
	if (*cp == 0) {
	    break;
	}
        if ((np = strchr(cp, ',')) != NULL) {
	    *np++ = 0;
	}
	f = (struct forwarder *) malloc(sizeof(struct forwarder));
	if (f != NULL) {
	    bzero(f, sizeof(struct forwarder));
	    f->status = 1;
	    f->sitename = cp;
	    f->port = Internet_Port + 2;
            if ((ep = strchr(cp, ':')) != NULL) {
		*ep = 0;
		f->port = atoi(ep + 1);
	    }
	    f->next = fwdlist;
	    fwdlist = f;
	    if (pthread_create(&(f->thread), &detached, forwarder_thread, f) != 0) {
                fprintf(stderr, "%s: Cannot launch forwarder thread for %s: %s",
		    prog, cp, strerror(errno));
		free(f);
	    }
	} else {
            fprintf(stderr, "%s: out of memory allocating forwarder for %s\n",
		prog, cp);
	    break;
	}
	if (np == NULL) {
	    break;
	}
	cp = np;
    }
#else
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
                fprintf(stderr, "%s: warning, forward destination %s unknown.\n",
		    prog, cp);
	    }
	}
	if (debugging && lwl_nsites > n) {
            fprintf(stderr, "%s: forwarding to LWL server %s: %s.\n", prog,
		inet_ntoa(lwl_sites[n]), cp);
	}
	if (np == NULL) {
	    break;
	}
	cp = np;
    }
#endif
}

/*  FORWARDLWLMESSAGE  --  If forwarding is enabled, pass this message
			   on to hosts in our forward list, changing the
			   packet protocol header to indicate the packet
			   was forwarded and appending the IP address
			   of the originating host to the end of the packet. */

static void forwardLwlMessage(char *zp, int rll, struct in_addr addr)
/*  	zp  	Packet address
    	rll 	Total packet length (starting at zp)
    	addr	IP address of originator of this packet */
{
#ifndef THREADS
    int i, sock;
#endif

    zp[0] |= 0x40;		/* Set forwarded packet flag bit */
    bcopy(&addr, zp + rll, sizeof(struct in_addr));
    rll += sizeof(struct in_addr);
#ifdef THREADS
    {
	struct forwarder *f = fwdlist;

	while (f != NULL) {
	    if (f->status == 0) {
		if (f->queuelen > ForwarderMaxQ) {
                    fprintf(stderr, "%s: %s queue too long; forward message discarded.\n",
			prog, f->sitename);
		} else {
		    struct lwl_queueitem *q = (struct lwl_queueitem *) malloc(
			sizeof(struct lwl_queueitem) + rll);

		    if (q != NULL) {
			q->next = NULL;
			q->q_rll = rll;
			bcopy(zp, q->q_pkt, rll);
			Lock(fwdlistLock);
			f->queuelen++;
			if (f->head == NULL) {
			    f->head = f->tail = q;
			} else {
			    f->tail->next = q;
			    f->tail = q;
			}
			Unlock(fwdlistLock);
		    }
		}
	    }
	    f = f->next;
	}
    }
#else
    for (i = 0; i < lwl_nsites; i++) {
	if (lwl_ports[i] >= 0) {
	    int cstat;

	    sock = socket(AF_INET, SOCK_STREAM, 0);
	    if (sock < 0) {
                perror("opening forwarding socket");
		continue;
	    }

	    lookhost.sin_port = htons(lwl_ports[i]);
	    bcopy((char *) (&lwl_sites[i]), (char *) &lookhost.sin_addr.s_addr,
		  sizeof lookhost.sin_addr.s_addr);

	    errno = 0;
	    do {
		cstat = connect(sock, (struct sockaddr *) &(lookhost), sizeof lookhost);
		if (cstat >= 0) {
		    break;
		}
	    } while (errno == EINTR);
	    if (cstat >= 0) {
		if (send(sock, zp, rll, 0) < 0) {
                    perror("forwarding look who's listening source ID message");
		} else {
		    if (debugging) {
                        fprintf(stderr, "%s: forwarded packet to server %s.\n",
			    prog, inet_ntoa(lookhost.sin_addr));
		    }
		}
	    } else {

		/* As LWL servers go up and down, failures to connect
		   to forwarding destinations are not uncommon.  Further,
		   while a server is down, we get here on every packet
		   we attempt to forward.  To avoid blithering all
		   over the log, we only note such failure if in
		   prolix or debugging mode. */

		if (prolix || debugging) {
                    perror("connecting forwarding socket");
		}
	    }
	    close(sock);
	}
    }
#endif
}

/*  QUERYMATCH	--  Determing if query matches a given item.  */

static int queryMatch(char *q, struct lwl *l)
{
    char ts[1024];
    int exact = FALSE;
    char *s;

    s = ts;

    /* We give preference to an E-mail address, if given, to a
       canonical address since it's the user's best known published
       identity on the net.  This, of course, runs the risk of
       spoofing, but since anybody can send us an RTCP packet with
       whatever cname they like, there's no added security in insisting
       on using it. */

    strcpy(s, (l->email == NULL) ? l->cname : l->email);
    lcase(s);

    if (*q == '*') {
	exact = TRUE;
	q++;
    }
    if (*s == '*') {
	exact = TRUE;
	s++;
    }
    /* Even if we're using the E-mail name, allow an asterisk on the
       canonical name to require an exact match. */
    if (l->cname[0] == '*') {
	exact = TRUE;
    }

    if (exact) {
	return strcmp(s, q) == 0;
    }

    if (strstr(s, q)) {
	return TRUE;
    }
    if (l->name != NULL) {
	strcpy(s, l->name);
	lcase(s);
	if (strstr(s, q)) {
	   return TRUE;
	}
    }
    return FALSE;
}

/*  RELEASE  --  Check for connections who haven't send us an update
		 once in the timeout interval and close them.  They
		 probably went away without having the courtesy to
		 say good-bye.	*/

static void release()
{
    struct lwl *lw, *llr = NULL, *lf;
    time_t now = time(NULL);

    LockConn();
    lw = conn;
    while (lw != NULL) {
	if ((now - lw->ltime) > TimeoutTime) {
	    lf = lw;
	    if (llr == NULL) {
		conn = lw->next;
		lw = conn;
	    } else {
		llr->next = lw->next;
		lw = lw->next;
	    }
/*
fprintf(stderr, "\nTiming out:\n");
dumpLwl(stderr, lf);
*/
	    if (verbose) {
                logLwl(lf, "Timeout: ");
	    }
	    htmlChange = TRUE;
	    destroyLwl(lf);
	} else {
	    llr = lw;
	    lw = lw->next;
	}
    }
    UnlockConn();
#ifndef THREADS
    updHTML();                        /* Update HTML if something's changed recently */
    signal(SIGALRM, release);	      /* Reset signal to handle timeout (Sys V) */
    alarm(TockTock);		      /* Reset the timer */
#endif
}

/*  PLUMBER  --  Catch SIGPIPE signal which occurs when remote user
		 disconnects before we try to send the reply.  */

static void plumber()
{
#ifdef DBTHREAD
fprintf(stderr, "Caught SIGPIPE--continuing.\n");
#endif
    signal(SIGPIPE, plumber);	      /* Reset signal just in case */
}

/*  EXITING  --  Release socket in case of termination.  */

static void exiting()
{
#ifdef DBTHREAD
fprintf(stderr, "Exiting.\n");
#endif
    shutdown(sock, 2);
    close(sock);
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
    V fprintf(stderr, "%s  --  Speak Freely: Look Who's Listening Server.\n", prog);
#ifdef THREADS
    V fprintf(stderr, "               (Multi-threaded version)\n");
#endif
    V fprintf(stderr, "               %s.\n", Relno);
    V fprintf(stderr, "\n");
    V fprintf(stderr, "Usage: %s [options]\n", prog);
    V fprintf(stderr, "Options:\n");
    V fprintf(stderr, "           -D         Debugging output on stderr\n");
    V fprintf(stderr, "           -Fserv,... Forward to listed servers\n");
    V fprintf(stderr, "           -Hpath     Write HTML status on base path\n");
    V fprintf(stderr, "           -Insec     Interval between HTML updates\n");
    V fprintf(stderr, "           -Mfile     Load server message from file\n");
    V fprintf(stderr, "           -Pport     Listen on given port\n");
    V fprintf(stderr, "           -Rnsec     Client-pull HTML refresh every nsec seconds\n");
    V fprintf(stderr, "           -U         Print this message\n");
    V fprintf(stderr, "           -V[V]      List connections and disconnects [-VV: all packets]\n");
#ifdef HEXDUMP
    V fprintf(stderr, "           -X         Dump packets in hex\n");
#endif
    V fprintf(stderr, "           -Zpath     Write HTML private directory on base path\n");
    V fprintf(stderr, "\n");
    V fprintf(stderr, "by John Walker\n");
    V fprintf(stderr, "   http://www.fourmilab.ch/\n");
}

/*  ADD_SDES_ITEM  --  Include an SDES item in a reply packet,
		       pruning illegal characters and content,
		       checking for packet overflow, and updating
		       the packet pointer.  */

#define addSDES(item, text) add_sdes_item(item, text, &ap)

static void add_sdes_item(int item, char *text, char **app)
{
    int l, state;
    char prune[258];
    char *ap = *app, *hp, *op = prune;

    /* Copy the text to the "prune" buffer using a simple
       state machine parser, filtering extraneous
       information. */

    state = 0;

    while (*text) {
	unsigned char ch = (unsigned char) *text++;

	if (item == RTCP_SDES_PRIV && op == prune) {
	    /* Allow PRIV item binary prefix field to pass. */
	    *op++ = (char) ch;
	    continue;
	}

	/* Discard non-graphic characters (ISO 8859 standard). */

        if (!isspace(ch) && (ch < ' ' || (ch > '~' && ch < 161))) {
	    continue;
	}

	switch (state) {

	    case 0:		      /* Skipping initial white space */
		if (!isspace(ch)) {
		    state = 1;
		} else {
		    break;
		}
		/* Note fall-through */

	    case 1:		      /* Transcribing to output */
s1:             if (ch == '<') {
                    if ((hp = strchr(text, '>')) != NULL) {
			text = hp + 1;
			if (op > prune && isspace(op[-1])) {
			    state = 2;
			}
			continue;
		    }
		}
		if (isspace(ch)) {
                    ch = ' ';         /* Convert all white space into blanks */
		    state = 2;
		}
		*op++ = (char) ch;
		break;

	    case 2:		      /* Collapsing multiple white space */
		if (!isspace(ch)) {
		    state = 1;
		    goto s1;
		}
		break;
	}
    }
    *op = 0;

    /* Drop trailing blanks. */

    while (strlen(prune) > 0 && isspace(op[-1])) {
	*(--op) = 0;
    }

    l = strlen(prune);
    if (l == 0) {
	return;
    } else if (l > 48) {
	int n;

	/* String is too long.	Try to trim it at a word boundary,
	   but if that fails just brutally lop it off. */

	for (n = 47; n > 31; n--) {
	    if (isspace(prune[n])) {
		prune[n] = 0;
		n = 0;
		break;
	    }
	}
	if (n > 0) {
	    prune[47] = 0;
	}
    }

    *ap++ = item;

    *ap++ = l = strlen(prune);
    bcopy(prune, ap, l);
    ap += l;

    *app = ap;
}

/*  SERVICEPACKET  --  Process a packet from a connection.  The "from"
    	    	       argument is a sockaddr_in structure giving the
		       address of the originating host.  */

static void servicePacket(int csock, struct sockaddr_in from)
{
    int rll, forward, pvalid;
    char zp[1024];
    rtcp_t *rp;
    short srp;
    struct lwl *lw;		  /* Parsed packet */
    char *p, *pend;
    struct sockaddr_in fwdh;

    forward = FALSE;
    rll = recv(csock, zp, sizeof zp, 0);
#ifdef HEXDUMP
    if (hexdump) {
        fprintf(stderr, "%s: %d bytes read from socket.\n", prog, rll);
	xd(stderr, zp, rll, TRUE);
    }
#endif

    /* If packet was forwarded from another server, extract the
       embedded original source address and make the packet
       look like it came directly from the sender. */

    if (rll > 4 && (zp[0] & 0xC0) == 0xC0) {
	forward = TRUE;
	zp[0] &= ~0x40;
	bcopy(zp + (rll - 4), &fwdh.sin_addr, 4);
	rll -= 4;
	if (debugging) {
            fprintf(stderr, "%s: received forwarded packet from %s.\n",
		prog, inet_ntoa(from.sin_addr));
	}
	from.sin_addr = fwdh.sin_addr ;
    }

    /* Some version of Speak Freely for Windows don't always
       pad request packets to a multiple of four bytes.  If
       this is the case, insert the pad ourselves. */

    while (((zp[1] & 0xFF) == RTCP_APP) && ((rll & 3) != 0)) {
	zp[rll++] = 0;
    }

    /* Validate this packet.  If it fails the validity check,
       ignore it do it can't crash the code below. */

    if (!gardol((unsigned char *) zp, rll)) {
	if (debugging) {
            fprintf(stderr, "%s: discarded invalid RTCP packet from %s.\n",
		prog, inet_ntoa(from.sin_addr));
	}
	close(csock);
	return;
    }

    /* Walk through the individual items in a possibly composite
       packet until we locate an item we're interested in.  This
       allows us to accept packets that comply with the RTP standard
       that all RTCP packets begin with an SR or RR.  */

    p = zp;
    pend = p + rll;
    pvalid = FALSE;

    while ((p < pend) && (p[0] >> 6 & 3) == RTP_VERSION) {
	int pt = p[1] & 0xFF;

	if (pt == RTCP_SDES || pt == RTCP_BYE || pt == RTCP_APP) {
	    pvalid = TRUE;
	    break;
	}
	/* If not of interest to us, skip to next subpacket. */
	p += (ntohs(*((short *) (p + 2))) + 1) * 4;
    }

    if (!pvalid) {
	if (debugging) {
            fprintf(stderr, "%s: discarded RTCP packet with unknown payload from %s.\n",
		prog, inet_ntoa(from.sin_addr));
	}
	close(csock);
	return;
    }

    /* Examine the packet to see what kind of request it is.
       Note that since all the packets we're interested in can
       be parsed without knowing their actual length in bytes, we
       can ignore the possible presence of padding. */

    rp = (rtcp_t *) p;
    srp = ntohs(*((short *) p));
    if (
#ifdef RationalWorld
	rp->common.version == RTP_VERSION && /* Version ID correct */
	rp->common.count == 1
#else
	(((srp >> 14) & 3) == RTP_VERSION) &&
	(((srp >> 8) & 0x1F) == 1)
#endif
       ) {

	switch (
#ifdef RationalWorld
		rp->common.pt
#else
		srp & 0xFF
#endif
	       ) {

	    /*	SDES packet.  This is a notification by a listening
                              that it's just started or is still
			      listening.  If the host was previously
			      active we replace the old parameters
			      with the new in case something has
			      changed.	The timeout is reset.  Identity
			      of host is by canonical name, not SSRC,
			      since the host may have restarted with
			      a new SSRC without having sent us a BYE.	*/

	    case RTCP_SDES:

		/* Parse the fields out of the SDES packet. */

		{
		    char *cp = (char *) rp->r.sdes.item,
			 *lp = cp + (ntohs(rp->common.length) * 4);
		    struct lwl *lr, *llr;

		    lw = (struct lwl *) malloc(sizeof(struct lwl));
		    if (lw != NULL) {
			bzero((char *) lw, sizeof(struct lwl));

			lw->ssrc = rp->r.sdes.src;
			lw->naddr = from.sin_addr.s_addr;
			lw->port = from.sin_port;
			lw->ltime = time(NULL);
			if (prolix) {
                            printf("%s: %s SDES %08lX\n", prog,
				inet_ntoa(from.sin_addr), lw->ssrc);
			}
			while (cp < lp) {
			    switch ((*cp) & 0xFF) {

				case RTCP_SDES_CNAME:
				    lw->cname = dupSdesItem(&cp);
				    break;

				case RTCP_SDES_NAME:
				    lw->name = dupSdesItem(&cp);
				    break;

				case RTCP_SDES_EMAIL:
				    lw->email = dupSdesItem(&cp);
				    break;

				case RTCP_SDES_PHONE:
				    lw->phone = dupSdesItem(&cp);
				    break;

				case RTCP_SDES_LOC:
				    lw->loc = dupSdesItem(&cp);
				    break;

				case RTCP_SDES_TOOL:
				    lw->tool = dupSdesItem(&cp);
				    break;

				case RTCP_SDES_PRIV:
				    {
					char *zp = dupSdesItem(&cp);

					if (zp != NULL) {
					    if (zp[0] == 1 &&
                                                zp[1] == 'P') {
						lw->port = atoi(zp + 2);
					    }
					    free(zp);
					}
				    }
				    break;

				case RTCP_SDES_END:
				    cp = lp;
				    break;

				default:
				    {
					char *zp = dupSdesItem(&cp);

					if (zp != NULL) {
					    free(zp);
					}
				    }
				    break;
			    }
			}
			if (debugging) {
			    dumpLwl(stderr, lw);
			}

			/* Search chain and see if a user with this
			   name is already know.  If so, replace the
			   entry with this one. */

			if (lw->cname != NULL) {
			    lr = conn;
			    llr = NULL;

			    while (lr != NULL) {
				char *p = lw->cname, *q = lr->cname;

                                if (*p == '*') {
				    p++;
				}
                                if (*q == '*') {
				    q++;
				}
				if (strcmp(p, q) == 0) {
				    lw->next = lr->next;
				    if (llr == NULL) {
					conn = lw;
				    } else {
					llr->next = lw;
				    }
				    destroyLwl(lr);
				    lw = NULL;
				    break;
				}
				llr = lr;
				lr = lr->next;
			    }

                            /* If we didn't find an entry already in the
			       chain, link in the new entry.  */

			    if (lw != NULL) {
				lw->next = conn;
				conn = lw;
				if (verbose) {
                                    logLwl(lw, "Connect: ");
				}
				changed();
			    }
			    if (!forward && lwl_nsites > 0) {
				forwardLwlMessage(zp, rll, from.sin_addr);
			    }
			} else {
			    /* Bogus item with no CNAME -- discard. */
			    if (debugging || verbose) {
                                fprintf(stderr, "Bogus SDES with no CNAME.\n");
				if (!debugging) {
				    dumpLwl(stderr, lw);
				}
			    }
			    destroyLwl(lw);
			}
		    }
		}
		break;

	    /*	BYE packet.  This is sent when a listening host is
			     ceasing to listen in an orderly manner.
			     Identity here is by SSRC, since the
			     host is assumed to have properly announced
                             itself.  Besides, that's the only ID in
			     the RTCP BYE packet, so it had darned
			     well be sufficient.  */

	    case RTCP_BYE:
		{
		    struct lwl *lr = conn, *llr = NULL;

		    if (prolix) {
                        printf("%s: %s BYE %08lX\n", prog,
			    inet_ntoa(from.sin_addr), rp->r.bye.src[0]);
		    }
		    while (lr != NULL) {
			if (rp->r.bye.src[0] == lr->ssrc) {
			    if (llr == NULL) {
				conn = lr->next;
			    } else {
				llr->next = lr->next;
			    }
			    if (debugging) {
                                fprintf(stderr, "Releasing:\n");
				dumpLwl(stderr, lr);
			    }
			    if (verbose) {
                                logLwl(lr, "Bye:     ");
			    }
			    changed();
			    destroyLwl(lr);
			    break;
			}
			llr = lr;
			lr = lr->next;
		    }
		    if (!forward && lwl_nsites > 0) {
			forwardLwlMessage(zp, rll, from.sin_addr);
		    }
		}
		break;

	    /*	Application request packets.  The following application
		extensions implement the various queries a client can make
		regarding the current state of the listening host list.  */

	    case RTCP_APP:
		if (prolix) {
                    printf("%s: %s APP %.4s\n", prog,
			inet_ntoa(from.sin_addr), p + 8);
		}

		/*  SFlq  --  Pattern match in cname and name and
			      return matches, up to the maximum
			      packet size.  If either the query string
			      or the canonical name begins with an
			      asterisk, the asterisk(s) is(/are) ignored
			      and a precise match with the canonical
			      name is required.  */

                if (bcmp(p + 8, "SFlq", 4) == 0) {
		    struct lwl *lr = conn;
		    char b[1500];
		    rtcp_t *rp = (rtcp_t *) b;
		    char *ap, *pap;
		    int l, scandex = 0;

#ifdef RationalWorld
		    rp->common.version = RTP_VERSION;
		    rp->common.p = 0;
		    rp->common.count = 0;
		    rp->common.pt = RTCP_SDES;
#else
		    *((short *) rp) = htons((RTP_VERSION << 14) |
					     RTCP_SDES | (0 << 8));
#endif

		    ap = (char *) &(rp->r.sdes.src);

		    if (prolix) {
                        printf("%s: %s query \"%s\"\n", prog, inet_ntoa(from.sin_addr), p + 12);
		    }
		    lcase(p + 12);
		    while (lr != NULL) {
			if (queryMatch(p + 12, lr)) {
			    char s[20];

			    pap = ap;
			    *((unsigned long *) ap) = lr->ssrc;
			    ap += sizeof(unsigned long);
			    addSDES(RTCP_SDES_CNAME, lr->cname +
                                (lr->cname[0] == '*' ? 1 : 0));

			    if (lr->name != NULL) {
				addSDES(RTCP_SDES_NAME, lr->name);
			    }

			    if (lr->email != NULL) {
				addSDES(RTCP_SDES_EMAIL, lr->email +
                                  (lr->email[0] == '*' ? 1 : 0));
			    }

			    if (lr->phone != NULL) {
				addSDES(RTCP_SDES_PHONE, lr->phone);
			    }

			    if (lr->loc != NULL) {
				addSDES(RTCP_SDES_LOC, lr->loc);
			    }

			    if (lr->tool != NULL) {
				addSDES(RTCP_SDES_TOOL, lr->tool);
			    }

                            sprintf(s, "\001P%d", lr->port);
			    addSDES(RTCP_SDES_PRIV, s);

			    {
				struct sockaddr_in u;

				u.sin_addr.s_addr = lr->naddr;
                                sprintf(s, "\001I%s", inet_ntoa(u.sin_addr));
				addSDES(RTCP_SDES_PRIV, s);
			    }

                            sprintf(s, "\001T%lu", lr->ltime);
			    addSDES(RTCP_SDES_PRIV, s);

#ifdef NEEDED
                            /* If we're over the packet size limit,
                               let the user know there's more to be
			       retrieved starting at the given offset. */

			    if ((ap - b) > MaxReplyPacket) {
                                sprintf(s, "\001M%d", scandex);
				addSDES(RTCP_SDES_PRIV, s);
			    }
#endif
			    *ap++ = RTCP_SDES_END;

			    /* Pad to next 32 bit boundary. */

			    while (((ap - b) & 3) != 0) {
				*ap++ = RTCP_SDES_END;
			    }
			    if ((ap - b) > MaxReplyPacket) {
				ap = pap;
				break;
			    }
#ifdef RationalWorld
			    rp->common.count++;
#else
			    (((unsigned char *) rp)[0])++;
#endif
			}
			lr = lr->next;
			scandex++;
		    }

		    l = ap - b;

		    rp->common.length = htons(((l + 3) / 4) - 1);
		    l = (ntohs(rp->common.length) + 1) * 4;
#ifdef HEXDUMP
		    if (hexdump) {
                        fprintf(stderr, "%s: %d bytes sent to socket.\n", prog, l);
			xd(stderr, b, l, TRUE);
		    }
#endif
		    if (send(csock, b, l, 0) < 0) {
                        perror("sending query match reply");
		    }
		}

                /*  SFms  --  Retrieve the server's information
                              message, if any.  If the server doesn't
			      publish a message, a null string is
			      returned in the reply packet.  */

                else if (bcmp(p + 8, "SFms", 4) == 0) {
#ifdef HEXDUMP
		    if (hexdump) {
                        fprintf(stderr, "%s: %d bytes sent to socket.\n",
				prog, messagel);
			xd(stderr, message, messagel, TRUE);
		    }
#endif
		    if (prolix) {
                        printf("%s: %s server message request\n", prog, inet_ntoa(from.sin_addr));
		    }
		    if (send(csock, message, messagel, 0) < 0) {
                        perror("sending server message");
		    }
		}
		break;

	    default:
		if (debugging || verbose) {
                    fprintf(stderr, "Bogus payload type %d\n",
#ifdef RationalWorld
				  rp->common.pt
#else
				  ((unsigned char *) rp)[1]
#endif
			   );
		}
		break;
	}
    }
    close(csock);
}

#ifdef THREADS

/*  Thread processing functions.  */

#ifdef DBTHREAD

/*  EDIT_THREAD  --  Edit information about a service thread from
		     its packet in the ballOstring chain.  */

static char *edit_thread(struct ballOstring *b)
{
    static char s[80];

    sprintf(s, "%d %s %s", b->thread, inet_ntoa(b->site), ctime(&b->startTime));
    s[strlen(s) - 1] = 0;
    return s;
}
#endif

/*  TIMEOUT_THREAD  --	Handle timeout of sites that go away without
                        sending a "Bye" message.  */

static void *timeout_thread(void *arg)
{
    long ct;
    struct ballOstring *b;

#ifdef DBTHREAD
fprintf(stderr, "Timeout thread running\n");
#endif
    while (TRUE) {
	sleep(120);		      /* Wait for next awake interval */
	time(&ct);
#ifdef DBTHREAD
fprintf(stderr, "Timeout thread scanning connections\n");
#endif
	Lock(bsLock);
	b = bshead.next;
	while (b != &bshead) {
#ifdef DBTHREAD
fprintf(stderr, "    Thread %s\n", edit_thread(b));
#endif
	    if ((b->startTime + ServiceThreadTimeout) < ct) {
#ifdef DBTHREAD
fprintf(stderr, "               Kill sent due to timeout.\n");
#endif
		pthread_kill(b->thread, SIGUSR1);
	    }
	    b = b->next;
	}
	Unlock(bsLock);
	release();
    }
    /*NOTREACHED*/
}

/*  HTML_THREAD  --  Update the HTML database(s) if something has
		     changed since the last time.  */

static void *html_thread(void *arg)
{
#ifdef DBTHREAD
fprintf(stderr, "HTML update thread running\n");
#endif
    while (TRUE) {
#ifdef DBTHREAD
fprintf(stderr, "HTML update thread examining connections\n");
#endif
	updHTML();

	/* In the threaded version, consider every 15 seconds as
           equivalent to "immediate update on every change".
	   We could accomplish immediate update with a
	   semaphore signalling mechanism, but that would
	   cause the threaded and non-threaded versions to
	   diverge much more.  Besides, nobody runs in immediate
	   update mode anyway, as far as I can determine. */

	sleep((unsigned int) (htmlTime <= 0 ? 15 : htmlTime));
    }
    /*NOTREACHED*/
}

/*  SERVICE_THREAD  --	Thread to process user packet.	One of
			these is created every time a connection is
			accepted.  */

struct service_arg {
    int sa_csock;
    struct sockaddr_in sa_from;
    struct ballOstring *sa_b;
};

static void killserv()
{
    pthread_t us = pthread_self();
    struct ballOstring *b;
#ifdef DBTHREAD
fprintf(stderr, "Gaaaak!!!  Killserv received by thread %d.\n", us);
#endif

    /* We're being killed by the timeout handler.  Search for
       our entry in the ballOstring chain. */

    Lock(bsLock);
    b = bshead.next;
    while (b != &bshead) {
	if (b->thread == us) {

	    /* Dechain from active thread list. */

	    b->prev->next = b->next;
	    b->next->prev = b->prev;
	    break;
	}
	b = b->next;
    }
    Unlock(bsLock);
    if (b != &bshead) {
	if (b->sa->sa_csock != -1) {
	    close(b->sa->sa_csock);
	}
#ifdef DBTHREAD
fprintf(stderr, "Thread killed by timeout: %s\n", edit_thread(b));
#endif
	free(b->sa);
	free(b);
	pthread_exit(NULL);
    } else {
#ifdef DBTHREAD
fprintf(stderr, "Oops!!!  Timeout kill request to thread %d, not in ballOstring.\n", us);
#endif
    }
}

static void *service_thread(void *arg)
{
    struct service_arg *sa = arg;

#ifdef DBTHREAD
fprintf(stderr, "Server thread %d for host %s launched\n", pthread_self(), inet_ntoa(sa->sa_from.sin_addr));
#endif
    signal(SIGUSR1, killserv);	       /* Set signal to handle timeout */
    sa->sa_b->thread = pthread_self();
    servicePacket(sa->sa_csock, sa->sa_from);
#ifdef DBTHREAD
fprintf(stderr, "Server thread %d for host %s exiting\n", pthread_self(), inet_ntoa(sa->sa_from.sin_addr));
#endif

    /* Dechain this thread's item from the ballOstring list. */

#ifdef DBTHREAD
fprintf(stderr, "Thread exiting: %s\n", edit_thread(sa->sa_b));
#endif
    Lock(bsLock);
    sa->sa_b->prev->next = sa->sa_b->next;
    sa->sa_b->next->prev = sa->sa_b->prev;
    free(sa->sa_b);
    Unlock(bsLock);
    free(sa);			      /* Release request packet */
    pthread_exit(NULL);
}
#endif

/*  Main program.  */

int main(int argc, char *argv[])
{
    int i, length;
    struct sockaddr_in name;
#ifdef THREADS
    pthread_t timeout_tid, html_tid;

    pthread_attr_init(&detached);
    pthread_attr_setdetachstate(&detached, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&detached, (size_t) 65536);
    bshead.next = bshead.prev = &bshead;
#endif

    /*	Process command line options.  */

    prog = prog_name(argv[0]);

#ifdef PRUNEFACE
    {	/* Interctive debugging of text pruning logic. */
	char s[132], p[132];
	char *zap;

	while (TRUE) {
            printf("--> ");
	    if (fgets(s, sizeof s, stdin) == NULL) {
		break;
	    }
	    zap = p;
	    add_sdes_item(1, s, &zap);
	    *zap = 0;
            printf(    "\"%s\"\n", p + 2);
	}
	exit(0);
    }
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

                case 'D':             /* -D  --  Debug output to stderr */
		    debugging = TRUE;
		    break;

                case 'F':             /* -Fserv,...  --  Forward to listed servers. */
		    forwardList(op + 1);
		    break;

                case 'H':             /* -Hname  --  HTML file name base path */
		    htmlFile = op + 1;
		    break;

                case 'I':             /* -Isec  --  Interval between HTML updates */
		    htmlTime = atoi(op + 1);
		    break;

                case 'M':             /* -Mfile  --  Load server message from file */
		    {
                        FILE *fp = fopen(op + 1, "r");
			long fl;

			if (fp == NULL) {
                            fprintf(stderr, "%s: can't open server message file %s\n", prog, op + 1);
			    return 2;
			}
			fseek(fp, 0L, 2);
			fl = ftell(fp);
			rewind(fp);
			messagel = ((int) fl) + 13;
			message = (char *) malloc(messagel);
			if (message != NULL) {
			    rtcp_t *rp = (rtcp_t *) message;

#ifdef RationalWorld
			    rp->common.version = RTP_VERSION;
			    rp->common.p = 0;
			    rp->common.count = 1;
			    rp->common.pt = RTCP_APP;
#else
			    *((short *) rp) = htons((RTP_VERSION << 14) |
						    RTCP_APP | (1 << 8));
#endif
			    rp->r.sdes.src = 0;
                            bcopy("SFmr", message + 8, 4);
			    fread(message + 12, (int) fl, 1, fp);
			    message[messagel - 1] = 0;
			}
			fclose(fp);
		    }
		    break;

                case 'P':             /* -Pport  --  Port to listen on */
		    lwlport = atoi(op + 1);
		    break;

                case 'R':             /* -Rnsec  --  Interval between HTML client-pull updates */
		    htmlRefresh = atoi(op + 1);
		    break;

                case 'U':             /* -U  --  Print usage information */
                case '?':             /* -?  --  Print usage information */
		    usage();
		    return 0;

                case 'V':             /*  -V  -- Show connects/disconnects */
		    verbose = TRUE;
                    if (op[1] == 'v' || op[1] == 'V') {
			prolix = TRUE;
		    }
		    break;

#ifdef HEXDUMP
                case 'X':             /* -X  --  Dump packets in hex */
		    hexdump = TRUE;
		    break;
#endif

                case 'Z':             /* -Zname  --  HTML private file name base path */
		    htmlPrivateFile = op + 1;
		    break;
	    }
	} else {
	    usage();
	    return 2;
	}
    }

    /* If no server message has been loaded, create a void one. */

    if (message == NULL) {
	rtcp_t *rp;

	messagel = 13;
	message = (char *) malloc(messagel);
	rp = (rtcp_t *) message;

#ifdef RationalWorld
	rp->common.version = RTP_VERSION;
	rp->common.p = 0;
	rp->common.count = 1;
	rp->common.pt = RTCP_APP;
#else
	*((short *) rp) = htons((RTP_VERSION << 14) | RTCP_APP | (1 << 8));
#endif
	rp->r.sdes.src = 0;
        bcopy("SFmr", message + 8, 4);
	message[messagel - 1] = 0;
    }


    /* Create the socket from which to read */

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("opening stream socket");
	return 1;
    }

    /* Create name with wildcards. */

    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(lwlport);
    if (bind(sock, (struct sockaddr *) &name, sizeof name) < 0) {
        perror("binding stream socket");
	return 1;
    }

    signal(SIGHUP, exiting);	      /* Set signal to handle termination */
    signal(SIGINT, exiting);	      /* Set signal to handle termination */
    signal(SIGTERM, exiting);	      /* Set signal to handle termination */

    signal(SIGPIPE, plumber);         /* Catch "broken pipe" signals from disconnects */

    if (listen(sock, 25) < 0) {
        perror("calling listen for socket");
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

#ifdef THREADS
    pthread_create(&timeout_tid, &detached, timeout_thread, NULL);
    if (htmlFile != NULL || htmlPrivateFile != NULL) {
	pthread_create(&html_tid, &detached, html_thread, NULL);
    }
#else
    signal(SIGALRM, release);	      /* Set signal to handle timeout */
    alarm(TockTock);		      /* Set alarm clock to purge idle hosts */
#endif
    changed();			      /* Create initial HTML file */
    setlinebuf(stdout); 	      /* Set stdout to line buffering for log */

    /* Process requests from the socket. */

    while (TRUE) {
	int csock;
	struct sockaddr_in from;      /* Sending host address */
	int fromlen;		      /* Length of sending host address */
#ifdef THREADS
	struct service_arg *sap;
	pthread_t service_tid;
#endif

	errno = 0;
	do {
	    fromlen = sizeof from;
	    csock = accept(sock, (struct sockaddr *) &from, &fromlen);
#ifdef DBTHREAD
fprintf(stderr, "Accept %d\n", csock);
#endif
	    if (csock >= 0) {
		break;
	    }
	} while (errno == EINTR);
	if (csock < 0) {
            perror("accepting connection to socket");
	    continue;
	}
	if (prolix) {
            printf("%s: %s accept\n", prog, inet_ntoa(from.sin_addr));
	}
#ifdef THREADS
	sap = (struct service_arg *) malloc(sizeof(struct service_arg));
	if (sap != NULL) {
	    struct ballOstring *b;

	    sap->sa_csock = csock;
	    sap->sa_from = from;
	    b = (struct ballOstring *) malloc(sizeof(struct ballOstring));
	    if (b != NULL) {
		sap->sa_b = b;
		b->sa = sap;
		Lock(bsLock);
		b->next = bshead.next;
		b->prev = &bshead;
		bshead.next = b;
		b->next->prev = b;
		b->thread = -1;
		time(&(b->startTime));
		b->site = from.sin_addr;
		Unlock(bsLock);
		if (pthread_create(&service_tid, &detached, service_thread, sap) != 0) {
                    fprintf(stderr, "%s: Cannot launch server thread for request from %s: %s",
			prog, inet_ntoa(from.sin_addr), strerror(errno));
		    close(csock);
		}
	    } else {
		free(sap);
                fprintf(stderr, "Unable to allocate memory for ballOstring packet.\n");
		close(csock);
	    }
	} else {
            fprintf(stderr, "Unable to allocate memory for service_arg packet.\n");
	    close(csock);
	}
#else
	servicePacket(csock, from);
#endif
    }
#ifdef MEANS_OF_EXIT
    close(sock);
    return 0;
#endif
}
