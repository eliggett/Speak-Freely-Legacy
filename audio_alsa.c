/*
 * Sound interface for Speak Freely for Unix
 * for ALSA 0.9 on Linux
 *
 * All sound is 8000 Hz mono ulaw encoded.
 * by Wolfgang Oertl <wolfgang.oertl@gmx.at>.
 * This file is public domain.
 *
 * 06-11-2002	first version.
 * 07-11-2002	honour devAudioInput/devAudioOutput, some small fixes, reduce
 *		amount of debugging output.  Mixer setting (soundrecgain,
 *		soundplayvol)
 */

#ifdef LINUX_ALSA

#include "speakfree.h"
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>


/*
 * Interface functions:
 *
extern int soundinit(int iomode);
extern void soundterm(void);
extern void sound_open_file_descriptors(int *audio_io, int *audio_ctl);
extern void soundplay(int len, unsigned char *buf);
extern void soundplayvol(int value);
extern void soundrecgain(int value);
extern void sounddest(int where);
extern int soundgrab(char *buf, int len);
extern void soundflush(void);

*/


/* --- module's public variables --- */
char *devAudioInput = "plughw:0,0";
char *devAudioOutput = "plughw:0,0";
char *devAudioControl = "default";	/* the card */

/* the mixer elements modified by soundrecgain and soundplayvol */
static char *capture_mixer_elem = "Capture";
static char *playback_mixer_elem = "PCM";


/* --- module's private variables --- */
/* - setup - */
static int snd_rate = SAMPLE_RATE;
static int snd_format = SND_PCM_FORMAT_MU_LAW;
static int snd_channels = 1;
static int verbose = 0;		/* DEBUG! */
static int quiet_mode = 0;	/* Show info when suspending.  Not relevant as
				   this application doesn't suspend. */

static unsigned buffer_time = 800 * 1000;	/* Total size of buffer: 800 ms */
static unsigned period_time = 200 * 1000;	/* a.k.a. fragment size: 200 ms */
static int sleep_min = 0;
static int avail_min = -1;
static int start_delay = 0;
static int stop_delay = 0;

/* - record/playback - */
static snd_pcm_t *pcm_handle = NULL;
static snd_pcm_uframes_t chunk_size;
static size_t bits_per_sample, bits_per_frame;
static size_t chunk_bytes;

/* - mixer - */
static int mixer_failed = 0;
static snd_mixer_t *mixer = NULL;
static snd_mixer_elem_t *capture_elem = NULL;
static snd_mixer_elem_t *playback_elem = NULL;
static int mixer_capture_failed=0, mixer_playback_failed=0;
static long reclevel_min, reclevel_max;
static long playlevel_min, playlevel_max;

/* - misc - */
static snd_output_t *log;



/*
 * Open the sound peripheral and initialise for access.  Returns TRUE if
 * successful, FALSE otherwise.
 *
 * iomode: O_RDONLY (sfmike) or O_WRONLY (sfspeaker) or O_RDWR (sflaunch).
 */
static int soundinit_2(int iomode)
{
	snd_pcm_stream_t stream;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	snd_pcm_uframes_t xfer_align, buffer_size, start_threshold, stop_threshold;
	size_t n;
	char *device_file;

	// might be used in case of error even without verbose.
	snd_output_stdio_attach(&log, stderr, 0);

	if (iomode == O_RDONLY) {
		stream = SND_PCM_STREAM_CAPTURE;
		start_delay = 1;
		device_file = devAudioInput;
	} else {
		stream = SND_PCM_STREAM_PLAYBACK;
		device_file = devAudioOutput;
	}

	if (snd_pcm_open(&pcm_handle, device_file, stream, 0) < 0) {
		fprintf(stderr, "Error opening PCM device %s\n", device_file);
		return FALSE;
	}

	snd_pcm_hw_params_alloca(&hwparams);
	if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
		fprintf(stderr, "Can't configure the PCM device %s\n",
			devAudioInput);
		return FALSE;
	}


	/* now try to configure the device's hardware parameters */
	if (snd_pcm_hw_params_set_access(pcm_handle, hwparams,
		SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		fprintf(stderr, "Error setting interleaved access mode.\n");
		return FALSE;
	}

	/* Here we request mu-law sound format.  ALSA can handle the
	 * conversion to linear PCM internally, if the device used is a
	 * "plughw" and not "hw". */
	if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, snd_format) < 0) {
		fprintf(stderr, "Error setting PCM format\n");
		return FALSE;
	}

	if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, snd_channels) < 0) {
		fprintf(stderr, "Error setting channels to %d\n",
			snd_channels);
		return FALSE;
	}

	if (snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &snd_rate, NULL) < 0) {
		fprintf(stderr, "The rate %d Hz is not supported.  "
			"Try a plughw device.\n", snd_rate);
		return FALSE;
	}

// buffer and period size can be set in bytes, or also in time.  I use the
// time approach here.
//	if (snd_pcm_hw_params_set_buffer_size(pcm_handle, hwparams, bufsize) < 0) {
//		fprintf(stderr, "Error setting buffer size to %d\n", bufsize);
//		return FALSE;
//	}
//	if (snd_pcm_hw_params_set_periods(pcm_handle, hwparams, periods, 0) < 0) {
//		fprintf(stderr, "Error setting periods.\n");
//		return FALSE;
//	}

	if (snd_pcm_hw_params_set_buffer_time_near(pcm_handle, hwparams, &buffer_time, 0) < 0) {
		fprintf(stderr, "Error setting buffer time to %d\n", buffer_time);
		return FALSE;
	}

	if (snd_pcm_hw_params_set_period_time_near(pcm_handle, hwparams, &period_time, 0) < 0) {
		fprintf(stderr, "Error setting period time to %d\n", period_time);
		return FALSE;
	}

	if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
		fprintf(stderr, "Error setting hardware parameters.\n");
		return FALSE;
	}

	/* check the hw setup */
	snd_pcm_hw_params_get_period_size(hwparams, &chunk_size, 0);
	snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size);
	if (chunk_size == buffer_size) {
		fprintf(stderr, "Can't use period equal to buffer size (%lu)\n",
			chunk_size);
		return FALSE;
	}
	if (verbose)
		printf("Period size %lu, Buffer size %lu\n\r",
			chunk_size, buffer_size);

	/* now the software setup */
	/* This is from aplay, and I don't really understand what it's good
	 * for... */
	snd_pcm_sw_params_alloca(&swparams);
	snd_pcm_sw_params_current(pcm_handle, swparams);

	if (snd_pcm_sw_params_get_xfer_align(swparams, &xfer_align) < 0) {
		fprintf(stderr, "Unable to obtain xfer align\n");
		return FALSE;
	}
	if (sleep_min)
		xfer_align = 1;
	if (snd_pcm_sw_params_set_sleep_min(pcm_handle, swparams, sleep_min) < 0) {
		fprintf(stderr, "Unable to set sleep_min to %d\n", sleep_min);
		return FALSE;
	}

	if (avail_min < 0)
		n = chunk_size;
	else
		n = (double) snd_rate * avail_min / 1000000;
	if (snd_pcm_sw_params_set_avail_min(pcm_handle, swparams, n) < 0) {
		fprintf(stderr, "Can't set avail_min to %d\n", n);
		return FALSE;
	}

	/* round up to closest transfer boundary */
	n = (buffer_size / xfer_align) * xfer_align;
	if (start_delay <= 0) {
		start_threshold = n + (double) snd_rate * start_delay / 1000000;
	} else
		start_threshold = (double) snd_rate * start_delay / 1000000;
	if (start_threshold < 1)
		start_threshold = 1;
	if (start_threshold > n)
		start_threshold = n;
	if (verbose)
		printf("Start threshold would be %lu\n\r", start_threshold);
#if 0
	// NOTE: this makes playback not work.  Dunno why.
	if (snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams, start_threshold) < 0) {
		fprintf(stderr, "Can't set start threshold\n");
		return FALSE;
	}
#endif

	if (stop_delay <= 0) 
		stop_threshold = buffer_size + (double) snd_rate * stop_delay / 1000000;
	else
		stop_threshold = (double) snd_rate * stop_delay / 1000000;
	if (snd_pcm_sw_params_set_stop_threshold(pcm_handle, swparams, stop_threshold) < 0) {
		fprintf(stderr, "Can't set stop threshold\n");
		return FALSE;
	}

	if (snd_pcm_sw_params_set_xfer_align(pcm_handle, swparams, xfer_align) < 0) {
		fprintf(stderr, "Can't set xfer align\n");
		return FALSE;
	}

	if (snd_pcm_sw_params(pcm_handle, swparams) < 0) {
		fprintf(stderr, "unable to install sw params:\n");
		snd_pcm_sw_params_dump(swparams, log);
		return FALSE;
	}


	/* ready to enter the SND_PCM_STATE_PREPARED status */
	if (snd_pcm_prepare(pcm_handle) < 0) {
		fprintf(stderr, "Can't enter prepared state\n");
		return FALSE;
	}

	if (verbose)
		snd_pcm_dump(pcm_handle, log);

	bits_per_sample = snd_pcm_format_physical_width(snd_format);
	bits_per_frame = bits_per_sample * snd_channels;
	chunk_bytes = chunk_size * bits_per_frame / 8;

	if (verbose)
		printf("Audio buffer size should be %d bytes\n\r", chunk_bytes);

//	audiobuf = realloc(audiobuf, chunk_bytes);
//	if (audiobuf == NULL) {
//		error("not enough memory");
//		exit(EXIT_FAILURE);
//	}

	return TRUE;
}

/* Helper function.  If the sound setup fails, release the device, because if
 * we try to open it twice, the application will block */
int soundinit(int iomode)
{
	int rc;

	rc = soundinit_2(iomode);
	if (rc != TRUE && pcm_handle) {
		soundterm();
	}

	return rc;
}

/* Close the audio device and the mixer */
void soundterm(void)
{
	snd_pcm_close(pcm_handle);
	pcm_handle = NULL;
	if (mixer) {
		snd_mixer_close(mixer);
		mixer = NULL;
	}
}

/* This is a hack to open the audio device once with O_RDWR, and then give the
 * descriptors to sfmike/sfspeaker.  Use this when opening the same device
 * twice, once read only and once write only, doesn't work.  AFAIK, not an
 * issue with ALSA.
 */
void sound_open_file_descriptors(int *audio_io, int *audio_ctl)
{
	return;
}


/* I/O error handler */
/* grabbed from alsa-utils-0.9.0rc5/aplay/aplay.c */
/* what: string "overrun" or "underrun", just for user information */
static void xrun(char *what)
{
	snd_pcm_status_t *status;
	int res;
	
	snd_pcm_status_alloca(&status);
	if ((res = snd_pcm_status(pcm_handle, status))<0) {
		fprintf(stderr, "status error: %s\n", snd_strerror(res));
		exit(EXIT_FAILURE);
	}
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
		struct timeval now, diff, tstamp;
		gettimeofday(&now, 0);
		snd_pcm_status_get_trigger_tstamp(status, &tstamp);
		timersub(&now, &tstamp, &diff);
		fprintf(stderr, "Buffer %s!!! (at least %.3f ms long)\n",
			what,
			diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
		if (verbose) {
			fprintf(stderr, "Status:\n");
			snd_pcm_status_dump(status, log);
		}
		if ((res = snd_pcm_prepare(pcm_handle))<0) {
			fprintf(stderr, "xrun: prepare error: %s\n",
				snd_strerror(res));
			exit(EXIT_FAILURE);
		}
		return;		/* ok, data should be accepted again */
	}
	fprintf(stderr, "read/write error\n");
	exit(EXIT_FAILURE);
}

/* I/O suspend handler */
/* grabbed from alsa-utils-0.9.0rc5/aplay/aplay.c */
static void suspend(void)
{
	int res;

	if (!quiet_mode)
		fprintf(stderr, "Suspended. Trying resume. "); fflush(stderr);
	while ((res = snd_pcm_resume(pcm_handle)) == -EAGAIN)
		sleep(1);	/* wait until suspend flag is released */
	if (res < 0) {
		if (!quiet_mode)
			fprintf(stderr, "Failed. Restarting stream. "); fflush(stderr);
		if ((res = snd_pcm_prepare(pcm_handle)) < 0) {
			fprintf(stderr, "suspend: prepare error: %s\n", snd_strerror(res));
			exit(EXIT_FAILURE);
		}
	}
	if (!quiet_mode)
		fprintf(stderr, "Done.\n");
}


/*
 * Play a sound.  Buf contains ULAW encoded audio, 8000 Hz, mono, signed bytes
 * grabbed from alsa-utils-0.9.0rc5/aplay/aplay.c
 *
 * buf: where the samples are, if stereo then interleaved
 * len: number of samples (not bytes).  Doesn't matter for mono 8 bit.
 */
void soundplay(int len, unsigned char *buf)
{
	int rc;

	/* the function expects the number of frames, which is equal to bytes
	 * in this case */
	while (len > 0) {
		rc = snd_pcm_writei(pcm_handle, buf, len);
		if (rc == -EAGAIN || (rc >= 0 && rc < len)) {
			snd_pcm_wait(pcm_handle, 1000);
		} else if (rc == -EPIPE) {
			/* Experimental: when a buffer underrun happens, then
			 * wait some extra time for more data to arrive on the
			 * network.  The one skip will be longer, but less
			 * buffer underruns will happen later.  Or so he
			 * thought... */
			usleep(10000);
			xrun("underrun");
		} else if (rc == -ESTRPIPE) {
			suspend();
		} else if (rc < 0) {
			fprintf(stderr, "Write error: %s\n", snd_strerror(rc));
			return;
		}

		if (rc > 0) {
			len -= rc;
			buf += rc * bits_per_frame / 8;
		}
	}
}

/* Try to open the mixer */
/* returns FALSE on failure */
static int mixer_open_2()
{
	int rc;

	if ((rc=snd_mixer_open(&mixer, 0)) < 0) {
		fprintf(stderr, "Can't open mixer: %s\n", snd_strerror(rc));
		return FALSE;
	}

	if ((rc=snd_mixer_attach(mixer, devAudioControl)) < 0) {
		fprintf(stderr, "Mixer attach error to %s: %s\n",
			devAudioControl, snd_strerror(rc));
		return FALSE;
	}

	if ((rc=snd_mixer_selem_register(mixer, NULL, NULL)) < 0) {
		fprintf(stderr, "Mixer register error: %s\n", snd_strerror(rc));
		return FALSE;
	}

	if ((rc=snd_mixer_load(mixer)) < 0) {
		fprintf(stderr, "Mixer load error: %s\n", snd_strerror(rc));
		return FALSE;
	}

	return TRUE;
}

static snd_mixer_elem_t *get_mixer_elem(char *name, int index)
{
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, index);
	snd_mixer_selem_id_set_name(sid, name);

	elem= snd_mixer_find_selem(mixer, sid);
	if (!elem) {
		fprintf(stderr, "Control '%s',%d not found.\n",
			snd_mixer_selem_id_get_name(sid),
			snd_mixer_selem_id_get_index(sid));
	}
	return elem;
}


/* Try to open the mixer, but only once. */
static int mixer_open()
{
	if (mixer_failed)
		return FALSE;

	if (mixer)
		return TRUE;

	if (mixer_open_2() == TRUE)
		return TRUE;

	if (mixer) {
		snd_mixer_close(mixer);
		mixer = NULL;
	}
	mixer_failed ++;
	return FALSE;
}

/* Set the playback volume from 0 (silence) to 100 (full on). */
void soundplayvol(int value)
{
	long vol;

	if (mixer_open() != TRUE)
		return;
	if (mixer_playback_failed)
		return;
	if (!playback_elem) {
		playback_elem = get_mixer_elem(playback_mixer_elem, 0);
		if (!playback_elem) {
			mixer_playback_failed = 1;
			return;
		}
		snd_mixer_selem_get_playback_volume_range(playback_elem,
			&playlevel_min, &playlevel_max);
	}

	vol = playlevel_min + (playlevel_max - playlevel_min) * value / 100;
	snd_mixer_selem_set_playback_volume(playback_elem, 0, vol);
	snd_mixer_selem_set_playback_volume(playback_elem, 1, vol);
}


/* Set recording gain from 0 (minimum) to 100 (maximum). */
void soundrecgain(int value)
{
	long vol;

	if (mixer_open() != TRUE)
		return;
	if (mixer_capture_failed)
		return;
	if (!capture_elem) {
		capture_elem = get_mixer_elem(capture_mixer_elem, 0);
		if (!capture_elem) {
			mixer_capture_failed = 1;
			return;
		}
		snd_mixer_selem_get_capture_volume_range(capture_elem,
			&reclevel_min, &reclevel_max);
	}

	// maybe unmute, or enable "rec" switch and so forth...
	vol = reclevel_min + (reclevel_max - reclevel_min) * value / 100;
	snd_mixer_selem_set_capture_volume(capture_elem, 0, vol);
	snd_mixer_selem_set_capture_volume(capture_elem, 1, vol);
}

/* select the output - speaker, or audio output jack.  Not implemented */
void sounddest(int where)
{
}

/* Record some audio (as much as fits into the given buffer) */
int soundgrab(char *buf, int len)
{
	ssize_t r;
	size_t result = 0;
	size_t count = len;


/*  I don't care about the chunk size.  We just read as much as we need here.
 *  Seems to work.
	if (sleep_min == 0 && count != chunk_size) {
		fprintf(stderr, "Chunk size should be %lu, not %d\n\r",
			chunk_size, count);
		count = chunk_size;
	}
*/

	// Seems not to be required.
//	int rc = snd_pcm_state(pcm_handle);
//	if (rc == SND_PCM_STATE_PREPARED)
//		snd_pcm_start(pcm_handle);

	while (count > 0) {
		r = snd_pcm_readi(pcm_handle, buf, count);
//		printf("Recording %d bytes of %d\n\r", r, count);
		if (r == -EAGAIN || (r >= 0 && r < count)) {
			snd_pcm_wait(pcm_handle, 1000);
		} else if (r == -EPIPE) {
			xrun("overrun");
		} else if (r == -ESTRPIPE) {
			suspend();
		} else if (r < 0) {
			if (r == -4) {
				/* This is "interrupted system call, which
				 * seems to happen quite frequently, but does
				 * no harm.  So, just return whatever has been
				 * already read. */
				return result;
			}
			fprintf(stderr, "read error: %s (%d); state=%d\n\r",
				snd_strerror(r), r, snd_pcm_state(pcm_handle));
			return -1;
		}
		if (r > 0) {
			result += r;
			count -= r;
			buf += r * bits_per_frame / 8;
		}
	}

	return result;
}


/* This is called *before* starting to record.  Flush pending input.
 * I'd like better a call when recording stops, so I can properly call
 * snd_pcm_stop() */
void soundflush(void)
{
//	printf("SOUND FLUSH\n\r");
	snd_pcm_drop(pcm_handle);	/* this call makes the state go to "SETUP" */
	snd_pcm_prepare(pcm_handle);	/* and now go to "PREPARE" state, ready for "RUNNING" */
}

#endif

