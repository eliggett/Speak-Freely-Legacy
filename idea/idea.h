
/* idea.h - header file for idea.c

   The types defined within idea.c are expanded here
   so this file can be included in a program which uses
   the IDEA functions without the need to adhere to its
   type nomenclature.
   
*/

extern void initkey_idea(unsigned char key[16], int decryp);
extern void idea_ecb(unsigned short *inbuf, unsigned short *outbuf);
extern void initcfb_idea(unsigned short iv0[4], unsigned char key[16],
    	    	    	 int decryp);
extern void ideacfb(unsigned char *buf, int count);
extern void close_idea(void);
extern void init_idearand(unsigned char key[16], unsigned char seed[8],
    	    	    	  unsigned long tstamp);
extern unsigned char idearand(void);
extern void close_idearand(void);
