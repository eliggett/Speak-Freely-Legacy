#ifndef _audio_descr_h
#define _audio_descr_h

typedef enum {
  AE_PCMU, AE_PCMA, AE_G721, AE_IDVI, AE_G723, AE_GSM,
  AE_1016, AE_LPC, 
  AE_L8, AE_L16, AE_L24, AE_L32,
  AE_G728, AE_TRUE,
  AE_MAX
} audio_encoding_t;

/* common audio description for files and workstation */
typedef struct {
  audio_encoding_t encoding;  /* type of encoding (differs) */
  unsigned sample_rate;       /* sample frames per second */
  unsigned channels;          /* number of interleaved channels */
} audio_descr_t;
#endif
