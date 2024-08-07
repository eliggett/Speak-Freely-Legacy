/**************************************************************************
*
* NAME
*		ldelay
*
* FUNCTION
*		Time delay a bandlimited signal
*		using point-by-point recursion
*		with a long interpolation filter (see delay.c)
*
* SYNOPSIS
*		subroutine ldelay(x, start, n, d, m, y)
*
*   formal 
*			data	I/O
*	name		type	type	function
*	-------------------------------------------------------------------
*	x[n]		float	i	signal input (output in last 60)
*	start		int	i	beginning of output sequence
*	n		int	i	length of input sequence
*	d		float	i	fractional pitch
*	m		int	i	integer pitch
*	y[n]		float	o	delayed input signal
*
***************************************************************************
*
* DESCRIPTION
*
*	Subroutine to time delay a bandlimited signal by resampling the
*	reconstructed data (aka sinc interpolation).  The well known
*	reconstruction formula is:
*
*	       |    M2	    sin[pi(t-nT)/T]    M2
*   y(n) = X(t)| = SUM x(n) --------------- = SUM x(n) sinc[(t-nT)/T]
*	       |   n=M1 	pi(t-nT)/T    n=M1
*	       |t=n+d
*
*	The interpolating (sinc) function is Hamming windowed to bandlimit
*	the signal to reduce aliasing.
*
*	Multiple simultaneous time delays may be efficiently calculated
*	by polyphase filtering.  Polyphase filters decimate the unused
*	filter coefficients.  See Chapter 6 in C&R for details. 
*
***************************************************************************
*
* CALLED BY
*
*	pitchvq   psearch
*
* CALLS
*
*	ham
*
***************************************************************************
*
* REFERENCES
*
*	Crochiere & Rabiner, Multirate Digital Signal Processing,
*	P-H 1983, Chapters 2 and 6.
*
*
*       Kroon & Atal, "Pitch Predictors with High Temporal Resolution,"
*       ICASSP '90, S12.6
*
**************************************************************************/

#define SIZE (M2 - M1 + 1)
#define NFRAC 5
#define M1 -20
#define M2  19

/* five fractional delays calculated over a 40 point interpolation	*/
/* (-20 to 19)								*/

static const float ldelay_frac[NFRAC] = {0.25, 0.33333333, 0.5, 0.66666667, 0.75};
static const int ldelay_twelfths[NFRAC] = {3, 4, 6, 8, 9};

static void ldelay(float x[], int start, int n, float d, int m, float y[])
{
  static float wsinc[SIZE][NFRAC], hwin[12*SIZE+1];
  static int first = TRUE;
  int i, j, k, index;

  /* Generate Hamming windowed sinc interpolating function for each	*/
  /* allowable fraction.  The interpolating functions are stored in	*/
  /* time reverse order (i.e., delay appears as advance) to align	*/
  /* with the adaptive code book v0 array.  The interp filters are:	*/
  /*		wsinc[.,0]	frac = 1/4 (3/12)			*/
  /*		wsinc[.,1]	frac = 1/3 (4/12)			*/
  /*		.		.					*/
  /*		wsinc[.,4]	frac = 3/4 (9/12)			*/


  if (first)
  {
    ham(hwin, 12*SIZE+1);
    for (i = 0; i < NFRAC; i++)
      for (k = M1, j = 0; j < SIZE; j++)
      {
	wsinc[j][i] = sinc(ldelay_frac[i] + k) * hwin[12*j+ldelay_twelfths[i]];
	k++;
      }
    first = FALSE;
  }

  index = qd(d);

  /* *Resample: 						 */

  for (i = 0; i < n; i++)
  {
    x[start+i-1] = 0.0;
    for (k = M1, j = 0; j < SIZE; j++)
    {
      x[start+i-1] += x[start-m+i+k-1] * wsinc[j][index];
      k++;
    }
  }

  /* *The v0 array in psearch.c/pgain.c must be zero above "start"      */
  /* *because of overlap and add convolution techniques used in pgain.	*/

  for (i = 0; i < n; i++)
  {
    y[i] = x[start+i-1];
    x[start+i-1] = 0.0;
  }
}

#undef SIZE
#undef NFRAC
#undef M1
#undef M2
