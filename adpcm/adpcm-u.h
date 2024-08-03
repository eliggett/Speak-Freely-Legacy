/*
** adpcm.h - include file for adpcm coder.
**
** Version 1.0, 7-Jul-92.
*/

struct adpcm_state {
    short       valprev;        /* Previous output value */
    char        index;          /* Index into stepsize table */
};

void adpcm_coder_u(unsigned char indata[], char outdata[], int len, struct adpcm_state *state);
void adpcm_decoder_u(char indata[], unsigned char outdata[], int len, struct adpcm_state *state);
