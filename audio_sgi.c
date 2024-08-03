/*

     Silicon Graphics audio drivers adapted from the stand-alone
	  Speak Freely for SGI designed and implemented by:

			    Paul Schurman
			    Espoo, Finland

			     16 July 1995

*/

#include "speakfree.h"

#ifdef sgi
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <dmedia/audio.h>

#include "ulaw2linear.h"

/*  Local variables  */

static ALport audioport;	      /* Audio I/O port */
static long origParams[] = {
    AL_INPUT_RATE, 0,
    AL_OUTPUT_RATE, 0,
    AL_LEFT_SPEAKER_GAIN, 0,
    AL_RIGHT_SPEAKER_GAIN, 0,
    AL_LEFT_INPUT_ATTEN, 0,
    AL_RIGHT_INPUT_ATTEN, 0
};
static long origMute[] = {
    AL_SPEAKER_MUTE_CTL, 0
};

/*  SOUNDINIT  --  Open the sound peripheral and initialise for
		   access.  Return TRUE if successful, FALSE
		   otherwise.  */

int soundinit(int iomode)
{
    ALconfig audioconfig; 
    int err;
    static long params[] = {
	AL_INPUT_RATE, SAMPLE_RATE,
	AL_OUTPUT_RATE, SAMPLE_RATE
    };

    ALseterrorhandler(0);

    /* Get a new audioconfig and set parameters. */
  
    audioconfig = ALnewconfig();   
    ALsetsampfmt(audioconfig, AL_SAMPFMT_TWOSCOMP);
    ALsetwidth(audioconfig, AL_SAMPLE_16);
    ALsetqueuesize(audioconfig, 8000L);  
    ALsetchannels(audioconfig, AL_MONO);   

    /* Save original of all global modes we change. */ 

    ALgetparams(AL_DEFAULT_DEVICE, origMute, 2);
    ALgetparams(AL_DEFAULT_DEVICE, origParams, 12);

    /* Set input and output data rates to 8000 samples/second. */

    ALsetparams(AL_DEFAULT_DEVICE, params, 4);

    /* Open the audioport. */

    audioport = ALopenport((iomode & O_WRONLY) ? "Speaker" : "Mike",
                           (iomode & O_WRONLY) ? "w" : "r", audioconfig);
    if (audioport == (ALport) 0) {	
       err = oserror();      
       if (err == AL_BAD_NO_PORTS) {
          fprintf(stderr, " System is out of audio ports\n"); 
       } else if (err == AL_BAD_DEVICE_ACCESS) { 
          fprintf(stderr, " Couldn't access audio device\n"); 
       } else if (err == AL_BAD_OUT_OF_MEM) { 
          fprintf(stderr, " Out of memory\n"); 
       } 
       return FALSE;
    }  

    if (ALfreeconfig(audioconfig) != 0) {
       err = oserror();
       if (err == AL_BAD_CONFIG) {
          fprintf(stderr, " Config not valid");
	  return FALSE;
       }
    } 

    /* Initialized the audioport okay. */

    return TRUE;
}

/*  SOUNDTERM  --  Close the sound device.  */

void soundterm(void)
{
    ALsetparams(AL_DEFAULT_DEVICE, origParams, 12);
    ALsetparams(AL_DEFAULT_DEVICE, origMute, 2);
    ALcloseport(audioport);
}

/*  SOUNDPLAY  --  Begin playing a sound.  */

void soundplay(int len, unsigned char *buf)
{
    int i;
    short abuf[BUFL];

    for (i = 0; i < len; i++) {
	abuf[i] = audio_u2s(buf[i]);
    }
    ALwritesamps(audioport, abuf, len);
}

/*  SOUNDPLAYVOL  --  Set playback volume from 0 (silence) to 100 (full on). */

void soundplayvol(int value)
{
    long par[] = {
	AL_LEFT_SPEAKER_GAIN, 0,
	AL_RIGHT_SPEAKER_GAIN, 0
    };

    par[1] = par[3] = (value * 255L) / 100;
    ALsetparams(AL_DEFAULT_DEVICE, par, 4);
}

/*  SOUNDRECGAIN  --  Set recording gain from 0 (minimum) to 100 (maximum).  */

void soundrecgain(int value)
{
    long par[] = {
	AL_LEFT_INPUT_ATTEN, 0,
	AL_RIGHT_INPUT_ATTEN, 0
    };

    par[1] = par[3] = ((100 - value) * 255L) / 100;
    ALsetparams(AL_DEFAULT_DEVICE, par, 4);
}

/*  SOUNDDEST  --  Set destination for generated sound.  If "where"
		   is 0, sound goes to the built-in speaker; if
		   1, to the audio output jack. */

void sounddest(int where)
{
    /* Since we can't mute independently, and this is used only
       for ring, always unmute the speaker and headphones. */

    static long par[] = {
	AL_SPEAKER_MUTE_CTL, AL_SPEAKER_MUTE_OFF
    };

    ALsetparams(AL_DEFAULT_DEVICE, par, 2);
}

/*  SOUNDGRAB  --  Return audio information in the record queue.  */

int soundgrab(char *buf, int len)
{
    int i;
    short sb[BUFL];
/*  long filled = ALgetfilled(audioport);

    if (len > filled) {
	len = filled;
    }
len=488;
*/
/*len=1600;*/
    if (len > 0) {
       ALreadsamps(audioport, sb, len);

      
       for (i = 0; i < len; i++) {
	  buf[i] = audio_s2u(sb[i]);
       }
    }
    return len;
}

/*  SOUNDFLUSH	--  Flush any queued sound.  */

void soundflush(void)
{
    short sb[BUFL];

    while (TRUE) {
	long l = ALgetfilled(audioport);

	if (l < 400) {
	    break;
	} else {
	    if (l > BUFL) {
		l = BUFL;
	    }
	    ALreadsamps(audioport, sb, l);
	}
    }
}
#endif /* sgi */
