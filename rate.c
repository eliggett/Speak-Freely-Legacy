/*
 * August 21, 1998
 * Copyright 1998 Fabrice Bellard.
 *
 * [Rewrote completly the code of Lance Norskog And Sundry
 * Contributors with a more efficient algorithm.]
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.  
 */

/*

 * Modified for use in Speak Freely by John Walker: 2003-01-25

 * Sound Tools rate change effect file.
 */

#include "speakfree.h"

/*
 * Linear Interpolation.
 *
 * The use of fractional increment allows us to use no buffer. It
 * avoid the problems at the end of the buffer we had with the old
 * method which stored a possibly big buffer of size
 * lcm(in_rate,out_rate).
 *
 * Limited to 16 bit samples and sampling frequency <= 65535 Hz. If
 * the input & output frequencies are equal, a delay of one sample is
 * introduced.  Limited to processing 32-bit count worth of samples.
 *
 * 1 << FRAC_BITS evaluating to zero in several places.  Changed with
 * an (unsigned long) cast to make it safe.  MarkMLl 2/1/99
 */

#define FRAC_BITS 16

/* Private data */
typedef struct ratestuff {
    unsigned long opos_frac;    	/* Fractional position of the output stream in input stream unit */
    unsigned long opos;

    unsigned long opos_inc_frac;	/* Fractional position increment in the output stream */
    unsigned long opos_inc; 

    unsigned long ipos; 	    	/* Position in the input stream (integer) */

    long ilast; 	    	    	/* Last sample in the input stream */
} rate_t;

static rate_t rt;
static rate_t *rate = &rt;

/*
 * Prepare processing.
 */
void rate_start(int inrate, int outrate)
{
    unsigned long incr;

    rate->opos_frac=0;
    rate->opos=0;

    /* Increment */
    incr = (unsigned long)(((double) inrate) / ((double) outrate) * 
               (double) (((unsigned long) 1) << FRAC_BITS));

    rate->opos_inc_frac = incr & ((((unsigned long) 1) << FRAC_BITS) - 1);
    rate->opos_inc = incr >> FRAC_BITS;

    rate->ipos = 0;
    rate->ilast = 0;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
void rate_flow(unsigned char *ibuf, unsigned char *obuf, 
               int *isamp, int *osamp)
{
    unsigned char *istart, *iend;
    unsigned char *ostart, *oend;
    long ilast, icur, out;
    unsigned long tmp;
    double t;

    ilast = rate->ilast;

    istart = ibuf;
    iend = ibuf + *isamp;

    ostart = obuf;
    oend = obuf + *osamp;

    while (obuf < oend) {

	/* Safety catch to make sure we have input samples.  */
        if (ibuf >= iend) goto the_end;

        /* Read as many input samples so that ipos > opos */

        while (rate->ipos <= rate->opos) {
            ilast = audio_u2s(*ibuf++);
            rate->ipos++;
	    /* See if we finished the input buffer yet */
            if (ibuf >= iend) goto the_end;
        }

        icur = audio_u2s(*ibuf);

        /* Interpolate */
        t = ((double) rate->opos_frac) / (((unsigned long) 1) << FRAC_BITS);
        out = (((double) ilast) * (1.0 - t)) + (((double) icur) * t);

        /* Output sample & increment position */

        *obuf++ = audio_s2u(out);

        tmp = rate->opos_frac + rate->opos_inc_frac;
        rate->opos = rate->opos + rate->opos_inc + (tmp >> FRAC_BITS);
        rate->opos_frac = tmp & ((((unsigned long) 1) << FRAC_BITS) - 1);
    }

the_end:
    *isamp = ibuf - istart;
    *osamp = obuf - ostart;
    rate->ilast = ilast;
}
