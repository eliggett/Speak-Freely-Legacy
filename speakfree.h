/*

	    Speak Freely for Unix definitions

*/

#define RTP_SUPPORT
#define PUSH_TO_TALK
#define FACE_SET_GEOMETRY
#define NAT_LAUNCH

#if defined(sun) && !defined(Solaris)
#define BSD_like
#endif

#ifdef __FreeBSD__
#define BSD_like
#endif

#ifdef sgi
#define _BSD_SIGNALS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef PUSH_TO_TALK
#include <curses.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef BSD_like
#include <sys/timeb.h>
#endif
#ifdef Solaris
#include <sys/systeminfo.h>
#endif
#ifdef sgi
#include <dmedia/audio.h>
#include <limits.h>
#endif
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef LINUX_FPU_FIX
#include <fpu_control.h>
#endif

#define NOLONGLONG
#include "types.h"

#ifndef NOCRYPTO
#define CRYPTO
#endif

#ifdef CRYPTO
#include "aes.h"
#define AES_BLOCK_SIZE    BLOCK_SIZE
#include "blowfish.h"
#include "des.h"
#include "des_sf.h"
#include "idea.h"
#endif

#include "ulaw2linear.h"
#include "adpcm-u.h"
#include "celp.h"
#include "lpc.h"
#include "gsm.h"
#include "lpc10.h"
#include "md5.h"
#include "rtp.h"
#include "rtpacket.h"
#include "audio_descr.h"
#include "common.h"

#define SAMPLE_RATE 	8000	      /* Audio sample rate in sound packets */

#define TINY_PACKETS	512	      /* Ideal tiny packet size */

#define FACEDIR "/tmp/"               /* Directory where you'd like to keep face files */

#define BUFL	8000

#define LONG	long

/*  AUDIO_DEVICE_FILE enables code for platforms which support
    audio via one or more device files, such as /dev/audio.
    Platforms which do not implement audio in this manner,
    for example SGI and Hewlett-Packard Unix workstations,
    should #undef AUDIO_DEVICE_FILE in the lines below.  */

#define AUDIO_DEVICE_FILE
/* Platform-specific undefines for non-device-file audio. */
#ifdef sgi
#undef AUDIO_DEVICE_FILE
#endif
#ifdef HEWLETT_PACKARD
#undef AUDIO_DEVICE_FILE
#endif
#ifdef LINUX_ALSA
#undef AUDIO_DEVICE_FILE
#endif

/* Binary block and signal functions for fanatic ex-BSD platforms. */

#ifdef Solaris
#define bcmp(a, b, n)	memcmp(a, b, n)
#define bcopy(a, b, n)	memmove(b, a, n)
#define bzero(a, n)	memset(a, 0, n)

typedef void (*signalFUNC)(int);
#define signal(a, b)	sigset(a, (signalFUNC) b)
#define signalFUNCreturn (signalFUNC)
#endif

#ifndef signalFUNCreturn
#define signalFUNCreturn
#endif

/* Auto-configure multicast support */

#ifdef IP_ADD_MEMBERSHIP
#define MULTICAST
#endif

#define LPC_FRAME_SIZE	    160       /* Frame size for LPC compression */

struct soundbuf {
	LONG compression;
	char sendinghost[16];
	struct {
		LONG buffer_len;
		char buffer_val[BUFL];
	} buffer;
};
typedef struct soundbuf soundbuf;

struct sbhead {
	LONG compression;
	char sendinghost[16];
	struct {
		LONG buffer_len;
		char buffer_val[16];
	} buffer;
};
typedef struct sbhead sbhead;

#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

#define V	(void)

#define ELEMENTS(array) (sizeof(array)/sizeof((array)[0]))

/* Packet mode flags. */

#define fComp2X     1		      /* Simple 2 to 1 compression */
#define fDebug	    2		      /* Debug mode */
#define fSetDest    4		      /* Set sound output destination */
#define fDestSpkr   0		      /* Destination: speaker */
#define fDestJack   8		      /* Destination: audio output jack */
#define fLoopBack   16		      /* Loopback packets for testing */
#define fCompGSM    32		      /* GSM compression */
#define fEncDES     64		      /* DES encrypted */
#define fEncOTP     128 	      /* One-time pad encrypted */
#define fEncIDEA    256 	      /* IDEA encrypted */
#define fCompADPCM  512 	      /* ADPCM compressed */
#define fEncPGP     1024	      /* PGP encrypted */
#define fKeyPGP     2048	      /* Buffer contains PGP-encrypted session key */
#define fCompLPC    4096	      /* LPC compressed */
#define fFaceData   8192	      /* Request/reply for face data */
#define fFaceOffer  16384	      /* Offer face image to remote host */
#define fCompVOX    0x10000	      /* VOX compressed */
#define fCompLPC10  0x20000	      /* LPC-10 compressed */
#define fCompRobust 0x40000	      /* Robust packet replication ? */
#define fEncBF	    0x80000	      /* Blowfish encrypted */
#define fCompCELP   0x100000	      /* CELP compressed */
#define fEncAES     0x200000	      /* AES encrypted */
#define fProtocol   0x40000000	      /* Speak Freely protocol flag */

/* Special definitions for face data packets (which have fFaceData set). */

#define faceRequest 1		      /* Face data request */
#define faceReply   2		      /* Face data reply */
#define faceLess    4		      /* No face available */

/* Mask to extract packet modes. */

#define fCompressionModes   (fComp2X | fCompGSM | fCompADPCM | fCompLPC | fCompLPC10 | fCompCELP)
#define isEncrypted(x)	    (((x) & (fEncDES | fEncOTP | fEncOTP | fEncPGP)) != 0)

/* Special definitions for half-duplex control packets. */

#define fHalfDuplex 0x10000000	      /* Local half-duplex transition */
#ifdef HALF_DUPLEX
#define fHalfDuplexMute      1	      /* Mute audio output to allow input */
#define fHalfDuplexResume    2	      /* Resume audio output */
#endif

/* Test if a packet actually contains sound. */

#define isSoundPacket(c)    (((c) & (fFaceData | fKeyPGP | fHalfDuplex)) == 0)

/* Open connection state. */

struct connection {
    struct connection *con_next;      /* Next connection */
    struct in_addr con_addr;	      /* Host Internet address */
    char pgpkey[17];		      /* PGP key for connection */
    char keymd5[16];		      /* Digest of key file */
    long con_timeout;		      /* Connection timeout */
    long con_busy;		      /* Connection busy timeout */
    char con_hostname[264];	      /* Host name */
#ifdef NAT_LAUNCH
    struct {
	int dport;
	int cport;
    } con_ports;    	    	      /* Ports for NAT connected host */
    int sfmike_pid; 	    	      /* PID of launched sfmike for NAT connection */
#endif
    long con_compmodes; 	      /* Last compression modes */
#ifdef NAT_LAUNCH
    long con_compression; 	      /* Last compression modes */
#endif
    short con_protocol; 	      /* Transmission protocol */
    int con_reply_current;	      /* Reply file current ? */
#ifndef CRYPTO
    int con_crypt_warning;	      /* Encryption warning sent ? */
#endif
    char con_session[4];	      /* VAT/RTP session identifier */
    lpcstate_t lpcc;		      /* LPC decoder state */
    int con_rseq;		      /* Robust mode sequence number */

    char con_uname[4096];	      /* User name */
    char con_email[256];	      /* User E-mail address, if known */

    char face_filename[300];	      /* Face temporary file name */
    FILE *face_file;		      /* Face file, when open */
    pid_t face_viewer;		      /* Face file viewer PID */
    int face_stat;		      /* Face retrieval status */
    long face_address;		      /* Address of current block request */
    int face_retry;		      /* Timeout retry count */
    int face_timeout;		      /* Timeout interval */
};

/* Face retrieval status values for face_stat. */

#define FSinit	    0		      /* Nothing requested yet */
#define FSrequest   1		      /* Request sent, awaiting reply */
#define FSreply     2		      /* Reply received, ready for next request */
#define FScomplete  3		      /* Face file reception complete */
#define FSabandoned 4		      /* Face file retrieval abandoned */

/* Face retrieval configuration parameters. */

#define FaceFetchInterval   250000    /* Interval between block requests, usec */
				      /* Resend block request after this time */
#define FaceTimeout	    (FaceFetchInterval * 20)
#define FaceMaxRetries	    10	      /* Maximum retries to obtain face data */

/* Protocol types for con_protocol. */

#define PROTOCOL_SPEAKFREE  0	      /* Speak Freely protocol */
#define PROTOCOL_VAT	    1	      /* VAT protocol */
#define PROTOCOL_RTP	    2	      /* RTP protocol */
#define PROTOCOL_VATRTP_CRYPT 3       /* Probably encrypted VAT or RTP message */
#define PROTOCOL_UNKNOWN    4	      /* No evidence as to protocol yet */

/* RTCP APP packet identifiers. */

#define RTCP_APP_TEXT_CHAT  "SFtc"    /* Text chat message */

/* Look Who's Listening parameters. */

#define LWL_MAX_SITES	    5	      /* Maximum LWL servers a user can publish on */

#ifdef MYDOMAIN
#define getdomainname(s, n)	strncpy(s, MYDOMAIN, n)
#endif

/*  From CODECS.C  */

extern void adpcmdecomp(struct soundbuf *sb);

/*  From COMMON.C  */

extern char *StrReplace(char *Str, char *OldStr, char *NewStr);

/*  From DESKEY.C  */

extern void string_DES_key(char *key, unsigned char des_key[8], char algorithm[16]);

/*  From G711.C  */

#ifdef NEEDED_LINEAR
extern unsigned char linear2alaw(int pcm_val);
extern int alaw2linear(unsigned char a_val);
extern unsigned char linear2ulaw(int pcm_val);
extern int ulaw2linear(unsigned char u_val);
extern unsigned char ulaw2alaw(unsigned char uval);
#endif
extern unsigned char alaw2ulaw(unsigned char aval);

/*  From HTML.C  */

extern void outHTML(FILE *fp, char *s);

/*  From RATE.C  */

extern void rate_start(int inrate, int outrate);
extern void rate_flow(unsigned char *ibuf, unsigned char *obuf, int *isamp, int *osamp);

/*  From RTPACKET.C  */

extern int isrtp(unsigned char *pkt, int len);
extern int isValidRTCPpacket(unsigned char *p, int len);
extern int isRTCPByepacket(unsigned char *p, int len);
extern int isRTCPAPPpacket(unsigned char *p, int len, char *name, unsigned char **app_ptr);
extern int rtp_make_sdes(char **pkt, unsigned long ssrc_i, int port, int strict);
extern int rtp_make_bye(unsigned char *p, unsigned long ssrc_i, char *raison, int strict);
extern int rtp_make_app(unsigned char *p, unsigned long ssrc_i, int strict, char *type, char *content);
extern int rtpout(soundbuf *sb, unsigned long ssrc_i, unsigned long timestamp_i,
    	    	  unsigned short seq_i, int spurt);
extern int parseSDES(unsigned char *packet, struct rtcp_sdes_request *r);
extern void copySDESitem(char *s, char *d);

/*  From SOUNDBYTE.C  */

extern void setaubufsize(int size);
extern int soundinit(int iomode);
extern void soundterm(void);
extern void sound_open_file_descriptors(int *audio_io, int *audio_ctl);
extern void soundplay(int len, unsigned char *buf);
extern void soundplayvol(int value);
extern void soundrecgain(int value);
extern void sounddest(int where);
extern int soundgrab(char *buf, int len);
extern void soundflush(void);

/*  From TEMPFILE.C  */

extern FILE *create_tempfile(char *temp_filename_pattern, const int uMask, const int unLink);
extern FILE *create_tempfile_in_tempdir(const char *tag, char **genName, const int uMask, const int unLink);

/*  From USLEEP.C  */

extern void sf_usleep(unsigned t);

/*  From VATPKT.C  */

extern int isvat(unsigned char *pkt, int len);
extern int vatout(soundbuf *sb, unsigned long ssrc_i, unsigned long timestamp_i, int spurt);
extern int makeVATid(char **vp, unsigned long ssrc_i);
extern int makevatdone(char *v, unsigned long ssrc_i);

/*  From VOX.C  */

#include "vox.h"

/*  From XDSUB.C  */

#ifdef HEXDUMP
extern void xd(FILE *out, void *buf, int bufl, int dochar);
#endif
