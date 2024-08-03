/*

			Speak Freely for Unix
                     Look Who's Listening Client

*/

#include "speakfree.h"
#include "version.h"

static int sock;		      /* Our socket */
static char *prog;		      /* Program name */ 

static struct sockaddr_in lookhost;   /* Look who's listening host, if any */
static int lwlport = Internet_Port + 2; /* Look Who's Listening port */
static int debugging = FALSE;	      /* Debug mode enabled */
static int showmessage = FALSE;       /* Show server message */

struct lwl {
    struct lwl *next;		      /* Next connection */
    long ltime; 		      /* Time of last update */
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

/*  PROG_NAME  --  Extract program name from argv[0].  */

static char *prog_name(char *arg)
{
    char *cp = strrchr(arg, '/');

    return (cp != NULL) ? cp + 1 : arg;
}

/*  USAGE  --  Print how-to-call information.  */

static void usage(void)
{
    V fprintf(stderr, "%s  --  Speak Freely: Look Who's Listening Lookup.\n", prog);
    V fprintf(stderr, "               %s.\n", Relno);
    V fprintf(stderr, "\n");
    V fprintf(stderr, "Usage: %s [options]\n", prog);
    V fprintf(stderr, "Options:\n");
    V fprintf(stderr, "           -C          Output sfmike connection address(es)\n");
    V fprintf(stderr, "           -D          Debugging output on stderr\n");
    V fprintf(stderr, "           -Hhost:port Query designated host\n");
    V fprintf(stderr, "           -L          List table of matches\n");
    V fprintf(stderr, "           -M          Show server message\n");
    V fprintf(stderr, "           -U          Print this message\n");
    V fprintf(stderr, "\n");
    V fprintf(stderr, "by John Walker\n");
    V fprintf(stderr, "   http://www.fourmilab.ch/\n");
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

/*  Main program.  */

int main(int argc, char *argv[])
{
    int argie, length, nret = 0, connex = FALSE,
	listing = FALSE, deefault = TRUE;
    struct sockaddr_in name;
    char *hostspec = NULL, *cp;

    /*	Process command line options.  */

    prog = prog_name(argv[0]);
    for (argie = 1; argie < argc; argie++) {
	char *op, opt;

	op = argv[argie];
        if (*op == '-') {
	    opt = *(++op);
	    if (islower(opt)) {
		opt = toupper(opt);
	    }

	    switch (opt) {
                case 'C':             /* -C  --  Output connect addresses */
		    connex = TRUE;
		    deefault = FALSE;
		    break;

                case 'D':             /* -D  --  Debug output to stderr */
		    debugging = TRUE;
		    break;

                case 'H':             /* -Hhost:port  --  Host/port to query */
		    hostspec = op + 1;
		    break;

                case 'L':             /* -L  --  List table of finds  */
		    listing = TRUE;
		    deefault = FALSE;
		    break;

                case 'M':             /* -M  --  Show server message */
		    showmessage = TRUE;
		    break;

                case 'U':             /* -U  --  Print usage information */
                case '?':             /* -?  --  Print usage information */
		    usage();
		    return 0;
	    }
	} else {
	    break;
	}
    }

    /* Try to locate host to contact for information. */

    if ((cp = hostspec) == NULL) {
        cp = getenv("SPEAKFREE_LWL_ASK");
	if (cp == NULL || strlen(cp) == 0) {
	    char *np;

            cp = getenv("SPEAKFREE_LWL_TELL");
	    if (cp == NULL) {
                fprintf(stderr, "%s: no host specified.\n", prog);
                fprintf(stderr, "%s: use -Hhost:port option or SPEAKFREE_LWL_ASK\n", prog);
                fprintf(stderr, "%s: or SPEAKFREE_LWL_TELL environment variables.\n", prog);
		return 2;
	    }

	    /* Multiple hosts may have been specified on the
	       SPEAKFREE_LWL_TELL statement.  If so, query only the
	       first. */

            if ((np = strchr(cp, ',')) != NULL) {
		*np = 0;
	    }
	}
    }

    if (cp != NULL) {
	struct hostent *h;
	char *ep;

        if ((ep = strchr(cp, ':')) != NULL) {
	    *ep = 0;
	    lwlport = atoi(ep + 1);
	}
	h = gethostbyname(cp);
	if (h != NULL) {
	    bcopy((char *) (h->h_addr), (char *) &lookhost.sin_addr.s_addr,
		sizeof lookhost.sin_addr.s_addr);
	    lookhost.sin_family = AF_INET;
	    lookhost.sin_port = htons(lwlport);
	} else {
            fprintf(stderr, "%s: server %s is unknown.\n", prog, cp);
	    return 2;
	}
    }

    /* Now loop through the command line arguments and pose
       each as a query to the server, then print whatever
       reply we get. */

    for (; showmessage || (argie < argc); argie++) {
	char p[1500];
	rtcp_t *rp = (rtcp_t *) p;
        static char query[] = "SFlq";
	int l;
	char *cp;

	/* Create the socket */

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
            perror("opening stream socket");
	    return 1;
	}

	if (connect(sock, (struct sockaddr *) &lookhost, sizeof lookhost) < 0) {
            perror("connecting stream socket");
	    return 1;
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

#ifdef RationalWorld
	rp->common.version = RTP_VERSION;
	rp->common.p = 0;
	rp->common.count = 1;
	rp->common.pt = RTCP_APP;
#else
	*((short *) rp) = htons((RTP_VERSION << 14) | RTCP_APP | (1 << 8));
#endif
	rp->r.sdes.src = 0;
	if (showmessage) {
	    argie--;
            bcopy("SFms", p + 8, 4);
	    rp->common.length = htons(2);
	    if (send(sock, p, 12, 0) < 0) {
                perror("sending server message request");
	    }
	} else {
	    bcopy(query, p + 8, 4);
	    strcpy(p + 12, argv[argie]);
	    cp = p + 13 + strlen(p + 12);
	    while ((cp - p) & 3) {
		*cp++ = 0;
	    }
	    rp->common.length = htons(((cp - p) / 4) - 1);
	    if (send(sock, p, cp - p, 0) < 0) {
                perror("sending look who's listening query message");
	    }
	}

	if ((l = recv(sock, p, sizeof p, 0)) > 0) {
	    if (showmessage) {
		showmessage = FALSE;
		fwrite(p + 12, l - 13, 1, stdout);
	    } else {
		int sdes_count;

#ifdef RationalWorld
		sdes_count = rp->common.count;
#else
		sdes_count = (((unsigned char *) rp)[0]) & 0x1F;
#endif
		cp = (char *) &(rp->r.sdes.src);
		while (sdes_count-- > 0) {
		    struct lwl *lw;

		    lw = (struct lwl *) malloc(sizeof(struct lwl));
		    if (lw != NULL) {
			int parsing = TRUE;
			struct sockaddr_in u;

			bzero((char *) lw, sizeof(struct lwl));

			bcopy(cp, (char *) &(lw->ssrc), 4);
			cp += 4;

			while (parsing) {
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
					    } else if (zp[0] == 1 &&
                                                       zp[1] == 'I') {
						lw->naddr = inet_addr(zp + 2);
					    }  else if (zp[0] == 1 &&
                                                        zp[1] == 'T') {
						lw->ltime = atol(zp + 2);
					    }
					    free(zp);
					}
				    }
				    break;

				case RTCP_SDES_END:
				    cp++;
				    while ((cp - p) & 3) {
					cp++;
				    }
				    parsing = FALSE;
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
			u.sin_addr.s_addr = lw->naddr;

			/* Default output listing: short address/cname (name) list. */

			if (deefault) {
			    char ipport[40], deef[256];

                            sprintf(ipport, "%s:%d", inet_ntoa(u.sin_addr), lw->port);
                            sprintf(deef, "%-24s %s", ipport, lw->cname);
			    if (lw->name) {
                                sprintf(deef + strlen(deef), " (%s)", lw->name);
			    }
                            printf("%s\n", deef);
			}

			/* Output connect IP:port arguments if requested. */

			if (connex) {
                            printf("%s-p%s:%d", nret > 0 ? " " : "",
				inet_ntoa(u.sin_addr), lw->port);
			}

			/* Produce long tabular listing of results.  */

			if (listing) {
			    char ipport[40];
			    struct tm *lt;

			    lt = localtime(&lw->ltime);
                            sprintf(ipport, "%s:%d", inet_ntoa(u.sin_addr), lw->port);
                            printf("\n%-24s %-48s %02d:%02d\n", ipport, lw->cname,
				lt->tm_hour, lt->tm_min);
			    if (lw->name != NULL) {
                                printf("%25s%s\n", "", lw->name);
			    }
			    if (lw->loc != NULL) {
                                printf("%25s%s\n", "", lw->loc);
			    }
			    if (lw->phone != NULL) {
                                printf("%25sPhone:  %s\n", "", lw->phone);
			    }
			    if (lw->email != NULL) {
                                printf("%25sE-mail: %s\n", "", lw->email);
			    }
			}

			nret++;
			destroyLwl(lw);
		    }
		}
	    }
	}
	if (l <= 0) {
            perror("reading reply from socket");
	    return 2;
	}
	close(sock);
    }
    if (connex) {
	if (nret > 0) {
            printf("\n");
	} else {
            fprintf(stderr, "No users found.\n");
	    return 2;
	}
    }
    return 0;
}
