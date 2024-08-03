/*

	Sound interface for Speak Freely for Unix

	Designed and implemented in July of 1990 by John Walker

*/

#include "speakfree.h"

#ifdef AUDIO_DEVICE_FILE

#ifdef Solaris
#include <sys/filio.h>
#else
#include <sys/dir.h>
#include <sys/file.h>
#endif

#include <errno.h>

#include <sys/ioctl.h>

#ifdef sun
#include <sys/stropts.h>
#ifdef Solaris
#include <sys/audioio.h>
#else /* !Solaris */
#include <sun/audioio.h>
#endif /* Solaris */
#else /* !sun */
#ifdef LINUX
#include <linux/soundcard.h>
#else /*!LINUX */
#include <machine/soundcard.h>
#endif /* LINUX */
#endif /* sun */

#ifndef sun
#ifndef AUDIO_BLOCKING
static int abuf_size;
#endif
#endif

#ifdef IN_AUDIO_DEV
#define SoundFileIn IN_AUDIO_DEV
#else
#define SoundFileIn     "/dev/audio"
#endif

#ifdef OUT_AUDIO_DEV
#define SoundFileOut OUT_AUDIO_DEV
#else
#define SoundFileOut    "/dev/audio"
#endif

#ifdef sun
#define AUDIO_CTLDEV    "/dev/audioctl"
#else
#define AUDIO_CTLDEV    "/dev/mixer"
#endif

char *devAudioInput = SoundFileIn,    /* Audio device files to open. */
     *devAudioOutput = SoundFileOut,  /* These can overridden by the -y */
     *devAudioControl = AUDIO_CTLDEV; /* option on sfmike and sfspeaker. */

#define MAX_GAIN	100

struct sound_buf {
    struct sound_buf *snext;	      /* Next sound buffer */
    int sblen;			      /* Length of this sound buffer */
    unsigned char sbtext[2];	      /* Actual sampled sound */
};

/*  Local variables  */

static int audiof = -1; 	      /* Audio device file descriptor */
static int stereo=0;		      /* 0=mono, 1=stereo */
static int Audio_fd;		      /* Audio control port */
#ifdef sun
static audio_info_t Audio_info;       /* Current configuration info */
#endif
struct sound_buf *sbchain = NULL,     /* Sound buffer chain links */
		 *sbtail = NULL;
static int neverRelease = FALSE;      /* Never release audio device.  This
					 is set if we inherited our open
					 audio device file descriptor from
					 a parent process.  If we release
                                         it, there's no way we can get it
					 back, so we glom onto it until
					 we exit. */

#ifdef sun
/* Some old SunOS libraries didn't define these. */
#ifndef AUDIO_MIN_GAIN
#define AUDIO_MIN_GAIN 0
#endif
#ifndef AUDIO_MAX_GAIN
#define AUDIO_MAX_GAIN 255
#endif

/* Convert local gain into device parameters */

static unsigned scale_gain(g)
  unsigned g;
{
    return (AUDIO_MIN_GAIN + (unsigned)
	((int) ((((double) (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN)) *
	((double) g / (double) MAX_GAIN)) + 0.5)));
}
#endif

#ifdef Solaris

/*  SETAUBUFSIZE  --  Preset size of internal /dev/audio buffer segments
		      Must be called before soundinit()  */

static int aubufsize = 2048;	      /* Default */

void setaubufsize(int size)
{
    aubufsize = size;
}
#endif


/*  SOUNDINIT  --  Open the sound peripheral and initialise for
		   access.  Return TRUE if successful, FALSE
		   otherwise.  */

int soundinit(int iomode)
{
    int attempts = 3;
#ifdef NEEDED_LINEAR
    int format = AFMT_S16_LE;
#endif  
    int speed = SAMPLE_RATE;
    int channels;
#ifdef LINUX_DSP_SMALL_BUFFER
    int arg = 0x7FFF000B, frag_size;
#ifdef FRAGMENT_BUFSIZE
#ifdef FRAGMENT_BUFPOWER
    /* http://www.opensound.com/pguide/audio2.html
       section "improving real-audio performance":
                     "Selecting buffering parameter"
       buffer is FRAGMENT_BUFSIZE * 2 ^ FRAGMENT_BUFPOWER
       eg.: 128 buffers (0x0080) of 64 (0x0006=2^6) bytes
	    arg = 0x00800006  ->  128 * 64 = 8192 bytes.

       See README.Linux_OSS_bufsize for additional details.

       Code contributed by Walter Haidinger
       (http://members.kstp.at/wh/index.html). */

    arg = ((((FRAGMENT_BUFSIZE < 2 ? 2 : FRAGMENT_BUFSIZE)  & 0xFFFF) << 16) |
	  ((FRAGMENT_BUFPOWER < 4 ? 4 : FRAGMENT_BUFPOWER) & 0xF));
#endif
#endif
#endif

    /* If we've been handed open file descriptors for the
       audio files on a silver platter, we never release
       them and hence don't need to re-open them, even if
       sfspeaker is trying to be nice about releasing
       audio output when it's been idle for a while. */

    if (neverRelease) {
	return TRUE;
    }

    assert(audiof == -1);

    /* When opening the audio device file and control file, we
       check for a specification which begins with a sharp sign
       and, if present, use the integer which follows as the number
       of an already-open file descriptor.  This allows a launcher
       program to open the audio device and then fork two processes
       which invoke sfmike and sfspeaker.  Why go to all this
       trouble?  Because some audio drivers, particularly on
       Linux, don't permit two separate programs to open /dev/audio
       simultaneously, even though function just fine in full
       duplex.	This kludge allows getting aroung around that
       restriction. */

    while (attempts-- > 0) {
	char *adf = (iomode == O_RDONLY) ? devAudioInput : devAudioOutput;

        if (adf[0] == '#') {
	    audiof = atoi(adf + 1);
	    neverRelease = TRUE;
	} else {
	    audiof = open(adf, iomode);
	}
	if (audiof >= 0) {
            if (devAudioControl[0] == '#') {
		Audio_fd = atoi(devAudioControl + 1);
	    } else {
		Audio_fd = open(devAudioControl, O_RDWR);
	    }
	    if (Audio_fd < 0) {
		perror(devAudioControl);
		close(audiof);
		audiof = -1;
		return FALSE;
	    }
    /*fcntl(audiof, F_SETFL, O_NDELAY);*/


#ifdef NEEDED_LINEAR
   channels = 1;
   if (ioctl(audiof, SNDCTL_DSP_CHANNELS, &channels) == -1) {
     perror("SNDCTL_DSP_CHANNELS");
     exit(-1);
   }
   stereo = (channels==1)?0:1;
		{
		   int linearSet = FALSE;

		   if (ioctl(audiof, SNDCTL_DSP_SETFMT, &format) == -1) {
                      perror("SNDCTL_DSP_SETFMT");  
		   } else if (ioctl(audiof, SNDCTL_DSP_STEREO, &stereo) == -1) {
                      perror("SNDCTL_DSP_STEREO");
		   } else if (ioctl(audiof, SNDCTL_DSP_SPEED, &speed) == -1) {
                      perror("SNDCTL_DSP_SPEED");  
		   } else {
		      linearSet = TRUE;
		   }
		   if (!linearSet) {
		      soundterm();
		      return FALSE;
		   }
		}
#endif

#ifdef LINUX_DSP_SMALL_BUFFER
	
	/* Some Linux sound drivers use a large audio input buffer
	   which results in a long delay between the time audio is
	   recorded and when it is delivered to a program.  The
	   following attempts to set the input buffer size to
	   reduce the delay.  You can explicitly set the buffer size
	   by defining FRAGMENT_BUFSIZE and FRAGMENT_BUFPOWER or
	   leave these values undefined, in which case a buffer size
	   of 2048 bytes will be used. */

	if (ioctl(audiof, SNDCTL_DSP_SETFRAGMENT, &arg) == -1) {
	   /* Ignore EINVAL if we were handed an open file descriptor
	      because that just means that sound is already being
	      transferred, which, in turn, means that another process
	      has already configured the device.
	   */
	   if ((errno != EINVAL) || !neverRelease) {
              perror("SNDCTL_DSP_SETFRAGMENT (LINUX_DSP_SMALL_BUFFER defined)");
	   }
           /* This isn't fatal, so keep on going. */
	}
	if (ioctl(audiof, SNDCTL_DSP_GETBLKSIZE, &frag_size) == -1) {
            perror("SNDCTL_DSP_GETBLKSIZE (LINUX_DSP_SMALL_BUFFER defined)");
	    soundterm();
	    return FALSE;
	}
#endif
    
#ifndef sun
#ifndef AUDIO_BLOCKING
	    if (ioctl(audiof, SNDCTL_DSP_NONBLOCK, NULL) < 0) {
                perror("SNDCTL_DSP_NONBLOCK");
		soundterm();
		return FALSE;
	    }
	    if (ioctl(audiof, SNDCTL_DSP_GETBLKSIZE, &abuf_size) < 0) {
                perror("SNDCTL_DSP_GETBLKSIZE");
		soundterm();
		return FALSE;
	    }
#endif
#else
	    AUDIO_INITINFO(&Audio_info);

	    /* We always fill in the information for
	       record and play regardless of the open mode since
	       we may be called from sflaunch to open in O_RDWR. */

	    Audio_info.play.sample_rate = SAMPLE_RATE;
	    Audio_info.play.channels = 1;
	    Audio_info.play.precision = 8;
	    Audio_info.play.encoding = AUDIO_ENCODING_ULAW;
	    Audio_info.record.sample_rate = SAMPLE_RATE;
	    Audio_info.record.channels = 1;
	    Audio_info.record.precision = 8;
	    Audio_info.record.encoding = AUDIO_ENCODING_ULAW;
#ifdef Solaris
	    Audio_info.record.buffer_size = aubufsize;
#endif
	    ioctl(Audio_fd, AUDIO_SETINFO, &Audio_info);
#endif
	    return TRUE;
	}
	if (errno != EINTR) {
	    break;
	}
        fprintf(stderr, "Audio open: retrying EINTR attempt %d\n", attempts);
    }
    return FALSE;
}

/*  SOUNDTERM  --  Close the sound device.  */

void soundterm(void)
{
	if ((!neverRelease) && (audiof >= 0)) {
	    if (close(audiof) < 0) {
                perror("closing audio device");
	    }
	    if (close(Audio_fd) < 0) {
                perror("closing audio control device");
	    }
	    audiof = -1;
	}
}

/*  SOUND_OPEN_FILE_DESCRIPTORS  --  Obtain file descriptors of open
				     audio and audio control device files.  */

void sound_open_file_descriptors(int *audio_io, int *audio_ctl)
{
    *audio_io = audiof;
    *audio_ctl = Audio_fd;
}

/*  SOUNDPLAY  --  Begin playing a sound.  */

void soundplay(int len, unsigned char *buf)
{
    int ios;
#ifdef NEEDED_LINEAR
    int i, p;
    short c;
    unsigned char abuf[BUFL*2];
#endif

    assert(audiof != -1);
#ifdef NEEDED_LINEAR
    p = 0;
    for ( i=0 ; i< len ; i++) { 
	c = (short)ulaw2linear(buf[i]);
	if (stereo) {
	    abuf[p++] = (unsigned char)(c & 0xff); 
	    abuf[p++] = (unsigned char)((c >> 8) & 0xff);
	}
	abuf[p++] = (unsigned char)(c & 0xff); 
	abuf[p++] = (unsigned char)((c >> 8) & 0xff);
    }
    buf = abuf;
    len = p;
#endif
    while (TRUE) {
	ios = write(audiof, buf, len);
	if (ios == -1) {
	    sf_usleep(100000);
	} else {
	    if (ios < len) {
		buf += ios;
		len -= ios;
	    } else {
		break;
	    }
	}
    }
}

/*  SOUNDPLAYVOL  --  Set playback volume from 0 (silence) to 100 (full on). */

void soundplayvol(int value)
{
#ifdef sun
    AUDIO_INITINFO(&Audio_info);
    Audio_info.play.gain = scale_gain(value);
    if (ioctl(Audio_fd, AUDIO_SETINFO, &Audio_info) < 0) {
        perror("Set play volume");
    }
#else
   int arg;

   arg	 = (value << 8) | value;

   if (ioctl(Audio_fd, MIXER_WRITE(SOUND_MIXER_PCM), &arg) < 0)
        perror("SOUND_MIXER_PCM");
#endif
}

/*  SOUNDRECGAIN  --  Set recording gain from 0 (minimum) to 100 (maximum).  */

void soundrecgain(int value)
{
#ifdef sun
    AUDIO_INITINFO(&Audio_info);
    Audio_info.record.gain = scale_gain(value);
    if (ioctl(Audio_fd, AUDIO_SETINFO, &Audio_info) < 0) {
        perror("Set record gain");
    }
#else
    int arg;

    arg   = (value << 8) | value;

    if (ioctl(Audio_fd, SOUND_MIXER_WRITE_RECLEV, &arg) < 0)
        perror("SOUND_MIXER_WRITE_RECLEV");
#endif
}

/*  SOUNDDEST  --  Set destination for generated sound.  If "where"
		   is 0, sound goes to the built-in speaker; if
		   1, to the audio output jack. */

void sounddest(int where)
{
#ifdef sun
    AUDIO_INITINFO(&Audio_info);
    Audio_info.play.port = (where == 0 ? AUDIO_SPEAKER : AUDIO_HEADPHONE);
    if (ioctl(Audio_fd, AUDIO_SETINFO, &Audio_info) < 0) {
        perror("Set output port");
    }
#endif
}

/*  SOUNDGRAB  --  Return audio information in the record queue.  */

int soundgrab(char *buf, int len)
{
    long read_size = len;
    int c;

#ifdef NEEDED_LINEAR
    int i, j, result;
    static short buf2[BUFL*2]; 
    read_size *= 2; /* 16-bit samples */
    if (stereo) read_size *= 2; /* stereo 16-bit samples */
#endif

#ifndef sun
#ifndef AUDIO_BLOCKING
    if (read_size > abuf_size) {
	read_size = abuf_size;
    }
#endif
#endif
    while (TRUE) {

#ifdef NEEDED_LINEAR
	c = read(audiof, buf2, read_size);
	result = c/2;
	if (stereo) result = c/4;
	for (i = j = 0; i < result; i++,j++) {
	    buf[i] = linear2ulaw(buf2[j]);
	    if (stereo) j++;
	}
#else
	c = read(audiof, buf, read_size);
#endif
		if (c < 0) {
			if (errno == EINTR) {
				continue;
			} else if (errno == EAGAIN) {
				c = 0;
#ifdef NEEDED_LINEAR
				result = 0;
#endif
			}
		}
		break;
	}

	if (c < 0) {
            perror("soundgrab");
	}
#ifdef NEEDED_LINEAR
	return result;
#else
	return c;
#endif
}

/*  SOUNDFLUSH	--  Flush any queued sound.  */

void soundflush(void)
{
#ifndef sun
#ifndef AUDIO_BLOCKING
    char sb[BUFL];
    int c;
    
    while (TRUE) {
	c = read(audiof, sb, BUFL < abuf_size ? BUFL : abuf_size);
	if (c < 0 && errno == EAGAIN)
	    c = 0;
	if (c < 0)
            perror("soundflush");
	if (c <= 0)
	    break;
    }
#endif
#else
    if (ioctl(audiof, I_FLUSH, FLUSHR)) {
        perror("soundflush");
    }
#endif
}
#endif /* AUDIO_DEVICE_FILE */
