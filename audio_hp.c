/*
 * Hewlett-Packard Audio Support - by Marc Kilian
 *
 * All rights granted to John Walker (who then promptly placed them
 * in the public domain--JW).
 *
 * History:
 * 12/12/1995	HP Audio Server version 1
 */

#include "speakfree.h"

#ifdef HEWLETT_PACKARD

/* #define HP_DEBUG */			 /* Enable debug I/O */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <audio/Alib.h>

#define SAMPLE_FORMAT	ADFMuLaw
#define BITS_PER_SAMPLE 8

#define STATE_RECORD_NEXT_TIME	1
#define STATE_EMPTY		2

static Audio* Audio_Dev = NULL; 	/* Pointer to open audio	*/
static SBucket* Sound_Buck = NULL;	/* Sound bucket for audio	*/
static long   Audio_Error;		/* Status value for audio cmds	*/
static SBPlayParams  Play_Params;	/* Play parameters		*/
static SBRecordParams Rec_Params;	/* Record parameters		*/
static AGainEntry Out_Gain_Entries;	/* Play gain entries		*/
static AGainEntry In_Gain_Entries;	/* Record gain entries		*/
static ATransID Trans_Id;		/* Audio transaction ID 	*/
static unsigned long Read_Begin = 0;	/* Current begin in rec bucket	*/
static int Read_State = STATE_RECORD_NEXT_TIME; /* Read state		*/

int HPTermHookChar = 0; 		/* Must hook into mike to do it */
int Term_Char = 0;

/* hpAudioHandler - HP audio exception handler
 */
static long hpAudioHandler(Audio* audio, AErrorEvent* event)
{
    fprintf(stderr, "Audio error %ld\n", event->error_code);
    exit(1);
}

/* soundinit - Initialize sound.
 *
 * iomode is either O_RDONLY for mike or O_WRONLY for speaker
 */
int soundinit(int iomode)
{
    AudioAttrMask attr_mask;
    AudioAttributes audio_attr;

    (void)ASetErrorHandler(hpAudioHandler);

    Audio_Dev = AOpenAudio(NULL, NULL);

    ASetCloseDownMode(Audio_Dev, AKeepTransactions, NULL);

    audio_attr.attr.sampled_attr.sampling_rate	 = SAMPLE_RATE;
    audio_attr.attr.sampled_attr.data_format	 = SAMPLE_FORMAT;
    audio_attr.attr.sampled_attr.bits_per_sample = BITS_PER_SAMPLE;
    audio_attr.attr.sampled_attr.channels	 = 1;

    attr_mask = (ASDataFormatMask | ASBitsPerSampleMask | ASSamplingRateMask |
		 ASChannelsMask);

    Sound_Buck = ACreateSBucket(Audio_Dev, attr_mask, &audio_attr, NULL);

    Out_Gain_Entries.u.o.out_ch = AOCTMono;
    Out_Gain_Entries.u.o.out_dst = AODTMonoIntSpeaker;
    Out_Gain_Entries.gain = AMaxOutputGain(Audio_Dev);

    Play_Params.gain_matrix.type = AGMTOutput;
    Play_Params.gain_matrix.num_entries = 1;
    Play_Params.gain_matrix.gain_entries = &Out_Gain_Entries;
    Play_Params.play_volume = AUnityGain;
    Play_Params.pause_first = False;
    Play_Params.start_offset.type = ATTSamples;
    Play_Params.start_offset.u.samples = 0;
    Play_Params.duration.type = ATTFullLength;
    Play_Params.loop_count = 1;
    Play_Params.priority = APriorityUrgent;
    Play_Params.previous_transaction = 0;
    Play_Params.event_mask = 0;

    In_Gain_Entries.u.i.in_ch = AICTMono;
    In_Gain_Entries.u.i.in_src = AISTMonoMicrophone;
    In_Gain_Entries.gain = AMaxInputGain(Audio_Dev);
    
    Rec_Params.gain_matrix.type = AGMTInput;
    Rec_Params.gain_matrix.num_entries = 1;
    Rec_Params.gain_matrix.gain_entries = &In_Gain_Entries;
    Rec_Params.record_gain = AUnityGain;
    Rec_Params.start_offset.type = ATTSamples;
    Rec_Params.start_offset.u.samples = 0;
    Rec_Params.duration.type = ATTFullLength;
    Rec_Params.event_mask = ATransCompletedMask | ATransStoppedMask;
}

/* soundterm - close the sounddevice.
 */
void soundterm(void)
{
    if (Sound_Buck) {
	ADestroySBucket(Audio_Dev, Sound_Buck, NULL);
	Sound_Buck = NULL;
    }

    if (Audio_Dev) {
	ACloseAudio(Audio_Dev, NULL);
	Audio_Dev = NULL;
    }
}

/* soundplay - begin playing a sound.
 */
void soundplay(int len, unsigned char *buf)
{
    unsigned long bytes_to_put = len;
    unsigned long bytes_put;
    
    while (bytes_to_put) {
	bytes_put = APutSBucketData(Audio_Dev, Sound_Buck, 0, (char*)buf,
				    bytes_to_put, &Audio_Error);
	if (Audio_Error)
	    break;
	bytes_to_put -= bytes_put;
	APlaySBucket(Audio_Dev, Sound_Buck, &Play_Params, NULL);
    }
}

/* soundplayvol - set playback volume from 0 (silence) to 100 (full on).
 */
void soundplayvol(int value)
{
    ;
}

/* soundrecgain - set recording gain from 0 (minimum) to 100 (maximum).
 */
void soundrecgain(int value)
{
    ;
}

/* sounddest - set destination for generated sound.
 *
 * where = 0, goes to built-in speaker
 *	 = 1, goes to the audio output jack
 */
void sounddest(int where)
{
    if (where == 0)
	Out_Gain_Entries.u.o.out_dst = AODTMonoIntSpeaker;
    else
	Out_Gain_Entries.u.o.out_dst = AODTMonoJack;
}

static int WaitForTermination(void)
{
    int stdin_fd = fileno(stdin);
    int max_fd = aConnectionNumber(Audio_Dev);
    int num_ready;
    int c;

    fd_set sel_fds;

    if (stdin_fd > max_fd)
	max_fd = stdin_fd;

    FD_ZERO(&sel_fds);
    FD_SET(stdin_fd, &sel_fds);
    FD_SET(aConnectionNumber(Audio_Dev), &sel_fds);
    
    num_ready = select( max_fd + 1, (int*)&sel_fds, NULL, NULL, NULL);
    if (num_ready < 0) {
        fprintf(stderr, "select failed\n");
	return 0;
    }

    if (FD_ISSET(stdin_fd, &sel_fds))
	Term_Char = getchar();

    if (FD_ISSET(aConnectionNumber(Audio_Dev), &sel_fds)) {
	AEvent Event;
	while (AQLength(Audio_Dev) > 0) {
	    ANextEvent(Audio_Dev, Event, NULL);
#ifdef HP_DEBUG
            printf("Audio event type %ld\n", Event.type);
#endif
	}

	return 1;
    }

    return 0;
}

/* soundgrab - return audio information in the record queue.
 */
int soundgrab(char *buf, int len)
{
    unsigned long bytes_read;

    if (Read_Begin == 0) {

	/* Read buffer is empty for the first time */
	if (Read_State == STATE_EMPTY) {
	    Read_State = STATE_RECORD_NEXT_TIME;
	    HPTermHookChar = 0;
	    return 0;
	}

#ifdef HP_DEBUG
        printf("Recording bucket\n");
#endif
	Trans_Id = ARecordAData(Audio_Dev, Sound_Buck, Rec_Params, NULL);
	if (WaitForTermination() == 0) {
#ifdef HP_DEBUG
            printf("Manually terminated\n");
#endif
	    AStopAudio(Audio_Dev, Trans_Id, ASMThisTrans, NULL, NULL);
	}
    }

    bytes_read = AGetSBucketData(Audio_Dev, Sound_Buck, Read_Begin,
				 buf, (unsigned long)len, NULL);

#ifdef HP_DEBUG
    printf("%ld bytes got from position %ld in bucket\n", bytes_read, Read_Begin);
#endif

    if (bytes_read == 0) {

	/* Ok, here we stop to return something */

	Read_Begin = 0L;
	Read_State = STATE_EMPTY;
	HPTermHookChar = Term_Char;

    } else {
	Read_Begin += bytes_read;
    }

    return (int)bytes_read;
}

/* soundflush - flush any queued sound.
 */
void soundflush(void)
{
    ;
}

/* Brian Abernathy supplied the following emulation of gethostid(),
   which is not implemented on HPUX at present.  */

gethostid(void)
{
    struct utsname utname;

    uname(utname);
    return (unsigned long) atoi(utname.idnumber);
}
#endif
