#
# Makefile for adpcm and test programs
# -------------------

CFLAGS = -I..

all:	adpcm-u.o

adpcm-u.o: adpcm-u.c adpcm-u.h

clean:
	rm -f core *.out *.o *.bak idea *.shar

depend:
	makedepend -- $(CFLAGS) -- adpcm-u.c

# DO NOT DELETE


adpcm-u.o: adpcm-u.h ../ulaw2linear.h
