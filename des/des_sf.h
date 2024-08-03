/*

	Prototypes for non-permuting DES used with Speak Freely protocol
	
*/

extern int desinit(int mode);
extern void desdone(void);
extern void setkey_sf(char *key);
extern void endes(char *block);
extern void dedes(char *block);
