# You must select the correct terminal control system to be used to
# turn character echo off when reading passwords.  There a 5 systems
# SGTTY   - the old BSD system
# TERMIO  - most system V boxes
# TERMIOS - SGI (ala IRIX).
# VMS	  - the DEC operating system
# MSDOS   - we all know what it is :-)
# read_pwd.c makes a reasonable guess at what is correct.

OPTS0= -DRAND # -DTERMIO -DNOCONST

# Version 1.94 has changed the strings_to_key function so that it is
# now compatible with MITs when the string is longer than 8 characters.
# If you wish to keep the old version, uncomment the following line.
# This will affect the -E/-D options on des(1).
#OPTS1= -DOLD_STR_TO_KEY

# This #define specifies the use of an alternative D_ENCRYPT macro in
# ecb_encrypt.	The choice of macro can make a %20 difference in the
# speed.  Unfortunatly the choise of the best macro appears to be very
# dependant on the compiler and the machine in question.
# For the following combinations use the ALT_ECB option.
# Sparc 2 (cc -O4), sun 3/260 (cc -O4)
# For the following combinations do not use the ALT_ECB option.
# Sparc 2 (gcc2 -O2), sun 3/260 (cc -O2), mvax2 (cc -O), MSDOS (Turbo Cv2)
# For other machines, experiment with changing the option and run
# ./speed to see which is faster.
# DO NOT TURN THIS OPTION ON WHEN COMPILING THIS CODE ON A 64 BIT MACHINE
#OPTS2= -DDES_USE_PTR

OPTS= $(OPTS0) $(OPTS1) $(OPTS2)

CC=cc
CFLAGS= -O $(OPTS) $(CFLAG)

#CC=gcc
#CFLAGS= -O3 -fomit-frame-pointer $(OPTS) $(CFLAG)

LIBDIR=/usr/local/lib
BINDIR=/usr/local/bin
INCDIR=/usr/local/include
MANDIR=/usr/local/man
MAN1=1
MAN3=3
SHELL=/bin/sh
OBJS=	cbc3_enc.o cbc_cksm.o cbc_enc.o ncbc_enc.o pcbc_enc.o qud_cksm.o \
	cfb64ede.o cfb64enc.o cfb_enc.o ecb3_enc.o ecb_enc.o  ede_enc.o  \
	enc_read.o enc_writ.o fcrypt.o	ofb64ede.o ofb64enc.o ofb_enc.o  \
	rand_key.o	      set_key.o rpc_enc.o  str2key.o supp.o

GENERAL=COPYRIGHT FILES INSTALL Imakefile README VERSION Makefile \
	times vms.com KERBEROS MODES.DES  GNUmakefile des.man \
	DES.pm DES.pod DES.xs Makefile.PL Makefile.uni typemap t \
	des_crypt.man Makefile.ssl des.doc makefile.bc
DES=	des.c
#TESTING=destest.c speed.c rpw.c
HEADERS=version.h des.h des_locl.h podd.h sk.h spr.h rpc_des.h
LIBDES= cbc3_enc.c cbc_cksm.c cbc_enc.c ncbc_enc.c pcbc_enc.c qud_cksm.c \
	cfb64ede.c cfb64enc.c cfb_enc.c ecb3_enc.c ecb_enc.c  ede_enc.c  \
	enc_read.c enc_writ.c fcrypt.c	ofb64ede.c ofb64enc.c ofb_enc.c  \
	rand_key.c	      set_key.c rpc_enc.c  str2key.c  supp.c

PERL=	des.pl testdes.pl doIP doPC1 doPC2 PC1 PC2 shifts.pl

ALL=	$(GENERAL) $(DES) $(TESTING) $(LIBDES) $(PERL) $(HEADERS)

DLIB=	libdes.a

all: $(DLIB) # destest rpw des speed

test:	all
	./destest

$(DLIB): $(OBJS)
	/bin/rm -f $(DLIB)
	ar cr $(DLIB) $(OBJS)
	-if test -s /bin/ranlib; then /bin/ranlib $(DLIB); \
	else if test -s /usr/bin/ranlib; then /usr/bin/ranlib $(DLIB); \
	else exit 0; fi; fi

destest: destest.o libdes.a
	$(CC) $(CFLAGS) -o destest destest.o libdes.a

rpw: rpw.o libdes.a
	$(CC) $(CFLAGS) -o rpw rpw.o libdes.a

speed: speed.o libdes.a
	$(CC) $(CFLAGS) -o speed speed.o libdes.a

des: des.o libdes.a
	$(CC) $(CFLAGS) -o des des.o libdes.a

tags:
	ctags $(DES) $(TESTING) $(LIBDES)

tar:
	tar chf libdes.tar $(ALL)

shar:
	shar $(ALL) >libdes.shar

depend:
	makedepend $(LIBDES) $(DES) $(TESTING)

clean:
	/bin/rm -f *.o tags core rpw destest des speed $(DLIB) .nfs* *.old \
	*.bak destest rpw des speed

dclean:
	sed -e '/^# DO NOT DELETE THIS LINE/ q' Makefile >Makefile.new
	mv -f Makefile.new Makefile

# Eric is probably going to choke when he next looks at this --tjh
install: $(DLIB) des
	if test $(INSTALLTOP); then \
	    echo SSL style install; \
	    cp $(DLIB) $(INSTALLTOP)/lib; \
	    if test -s /bin/ranlib; then \
		/bin/ranlib $(INSTALLTOP)/lib/$(DLIB); \
	    else \
		if test -s /usr/bin/ranlib; then \
		/usr/bin/ranlib $(INSTALLTOP)/lib/$(DLIB); \
	    fi; fi; \
	    chmod 644 $(INSTALLTOP)/lib/$(DLIB); \
	    cp des.h $(INSTALLTOP)/include; \
	    chmod 644 $(INSTALLTOP)/include/des.h; \
	    cp des $(INSTALLTOP)/bin; \
	    chmod 755 $(INSTALLTOP)/bin/des; \
	else \
	    echo Standalone install; \
	    cp $(DLIB) $(LIBDIR)/$(DLIB); \
	    if test -s /bin/ranlib; then \
	      /bin/ranlib $(LIBDIR)/$(DLIB); \
	    else \
	      if test -s /usr/bin/ranlib; then \
		/usr/bin/ranlib $(LIBDIR)/$(DLIB); \
	      fi; \
	    fi; \
	    chmod 644 $(LIBDIR)/$(DLIB); \
	    cp des $(BINDIR)/des; \
	    chmod 711 $(BINDIR)/des; \
	    cp des_crypt.man $(MANDIR)/man$(MAN3)/des_crypt.$(MAN3); \
	    chmod 644 $(MANDIR)/man$(MAN3)/des_crypt.$(MAN3); \
	    cp des.man $(MANDIR)/man$(MAN1)/des.$(MAN1); \
	    chmod 644 $(MANDIR)/man$(MAN1)/des.$(MAN1); \
	    cp des.h $(INCDIR)/des.h; \
	    chmod 644 $(INCDIR)/des.h; \
	fi
# DO NOT DELETE THIS LINE -- make depend depends on it.


cbc3_enc.o: des.h des_locl.h /usr/include/alloca.h
cbc3_enc.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
cbc3_enc.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
cbc3_enc.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
cbc3_enc.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
cbc3_enc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
cbc3_enc.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
cbc3_enc.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
cbc3_enc.o: /usr/include/bits/wordsize.h /usr/include/endian.h
cbc3_enc.o: /usr/include/features.h /usr/include/_G_config.h
cbc3_enc.o: /usr/include/gconv.h /usr/include/getopt.h
cbc3_enc.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
cbc3_enc.o: /usr/include/stdlib.h /usr/include/string.h
cbc3_enc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
cbc3_enc.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
cbc3_enc.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
cbc3_enc.o: /usr/include/xlocale.h
cbc3_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
cbc3_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
cbc_cksm.o: des.h des_locl.h /usr/include/alloca.h
cbc_cksm.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
cbc_cksm.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
cbc_cksm.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
cbc_cksm.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
cbc_cksm.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
cbc_cksm.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
cbc_cksm.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
cbc_cksm.o: /usr/include/bits/wordsize.h /usr/include/endian.h
cbc_cksm.o: /usr/include/features.h /usr/include/_G_config.h
cbc_cksm.o: /usr/include/gconv.h /usr/include/getopt.h
cbc_cksm.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
cbc_cksm.o: /usr/include/stdlib.h /usr/include/string.h
cbc_cksm.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
cbc_cksm.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
cbc_cksm.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
cbc_cksm.o: /usr/include/xlocale.h
cbc_cksm.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
cbc_cksm.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
cbc_enc.o: des.h des_locl.h /usr/include/alloca.h /usr/include/bits/confname.h
cbc_enc.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
cbc_enc.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
cbc_enc.o: /usr/include/bits/sched.h /usr/include/bits/select.h
cbc_enc.o: /usr/include/bits/sigset.h /usr/include/bits/stdio_lim.h
cbc_enc.o: /usr/include/bits/time.h /usr/include/bits/types.h
cbc_enc.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
cbc_enc.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
cbc_enc.o: /usr/include/endian.h /usr/include/features.h
cbc_enc.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
cbc_enc.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
cbc_enc.o: /usr/include/stdlib.h /usr/include/string.h
cbc_enc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
cbc_enc.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
cbc_enc.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
cbc_enc.o: /usr/include/xlocale.h
cbc_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
cbc_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
cfb64ede.o: des.h des_locl.h /usr/include/alloca.h
cfb64ede.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
cfb64ede.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
cfb64ede.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
cfb64ede.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
cfb64ede.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
cfb64ede.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
cfb64ede.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
cfb64ede.o: /usr/include/bits/wordsize.h /usr/include/endian.h
cfb64ede.o: /usr/include/features.h /usr/include/_G_config.h
cfb64ede.o: /usr/include/gconv.h /usr/include/getopt.h
cfb64ede.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
cfb64ede.o: /usr/include/stdlib.h /usr/include/string.h
cfb64ede.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
cfb64ede.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
cfb64ede.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
cfb64ede.o: /usr/include/xlocale.h
cfb64ede.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
cfb64ede.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
cfb64enc.o: des.h des_locl.h /usr/include/alloca.h
cfb64enc.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
cfb64enc.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
cfb64enc.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
cfb64enc.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
cfb64enc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
cfb64enc.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
cfb64enc.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
cfb64enc.o: /usr/include/bits/wordsize.h /usr/include/endian.h
cfb64enc.o: /usr/include/features.h /usr/include/_G_config.h
cfb64enc.o: /usr/include/gconv.h /usr/include/getopt.h
cfb64enc.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
cfb64enc.o: /usr/include/stdlib.h /usr/include/string.h
cfb64enc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
cfb64enc.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
cfb64enc.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
cfb64enc.o: /usr/include/xlocale.h
cfb64enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
cfb64enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
cfb_enc.o: des.h des_locl.h /usr/include/alloca.h /usr/include/bits/confname.h
cfb_enc.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
cfb_enc.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
cfb_enc.o: /usr/include/bits/sched.h /usr/include/bits/select.h
cfb_enc.o: /usr/include/bits/sigset.h /usr/include/bits/stdio_lim.h
cfb_enc.o: /usr/include/bits/time.h /usr/include/bits/types.h
cfb_enc.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
cfb_enc.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
cfb_enc.o: /usr/include/endian.h /usr/include/features.h
cfb_enc.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
cfb_enc.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
cfb_enc.o: /usr/include/stdlib.h /usr/include/string.h
cfb_enc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
cfb_enc.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
cfb_enc.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
cfb_enc.o: /usr/include/xlocale.h
cfb_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
cfb_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
des.o: des.h /usr/include/alloca.h /usr/include/bits/confname.h
des.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
des.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
des.o: /usr/include/bits/sched.h /usr/include/bits/select.h
des.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
des.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
des.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
des.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
des.o: /usr/include/bits/wordsize.h /usr/include/endian.h
des.o: /usr/include/features.h /usr/include/_G_config.h /usr/include/gconv.h
des.o: /usr/include/getopt.h /usr/include/gnu/stubs.h /usr/include/libio.h
des.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
des.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
des.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
des.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
des.o: /usr/include/wchar.h /usr/include/xlocale.h
des.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
des.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h version.h
destest.o: des.h /usr/include/alloca.h /usr/include/bits/confname.h
destest.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
destest.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
destest.o: /usr/include/bits/sched.h /usr/include/bits/select.h
destest.o: /usr/include/bits/sigset.h /usr/include/bits/stdio_lim.h
destest.o: /usr/include/bits/time.h /usr/include/bits/types.h
destest.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
destest.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
destest.o: /usr/include/endian.h /usr/include/features.h
destest.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
destest.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
destest.o: /usr/include/stdlib.h /usr/include/string.h
destest.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
destest.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
destest.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
destest.o: /usr/include/xlocale.h
destest.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
destest.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
ecb3_enc.o: des.h des_locl.h /usr/include/alloca.h
ecb3_enc.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
ecb3_enc.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
ecb3_enc.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
ecb3_enc.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ecb3_enc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
ecb3_enc.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
ecb3_enc.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
ecb3_enc.o: /usr/include/bits/wordsize.h /usr/include/endian.h
ecb3_enc.o: /usr/include/features.h /usr/include/_G_config.h
ecb3_enc.o: /usr/include/gconv.h /usr/include/getopt.h
ecb3_enc.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
ecb3_enc.o: /usr/include/stdlib.h /usr/include/string.h
ecb3_enc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
ecb3_enc.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
ecb3_enc.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
ecb3_enc.o: /usr/include/xlocale.h
ecb3_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
ecb3_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
ecb_enc.o: des.h des_locl.h spr.h /usr/include/alloca.h
ecb_enc.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
ecb_enc.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
ecb_enc.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
ecb_enc.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ecb_enc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
ecb_enc.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
ecb_enc.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
ecb_enc.o: /usr/include/bits/wordsize.h /usr/include/endian.h
ecb_enc.o: /usr/include/features.h /usr/include/_G_config.h
ecb_enc.o: /usr/include/gconv.h /usr/include/getopt.h /usr/include/gnu/stubs.h
ecb_enc.o: /usr/include/libio.h /usr/include/stdio.h /usr/include/stdlib.h
ecb_enc.o: /usr/include/string.h /usr/include/sys/cdefs.h
ecb_enc.o: /usr/include/sys/select.h /usr/include/sys/sysmacros.h
ecb_enc.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
ecb_enc.o: /usr/include/wchar.h /usr/include/xlocale.h
ecb_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
ecb_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
ede_enc.o: des.h des_locl.h /usr/include/alloca.h /usr/include/bits/confname.h
ede_enc.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
ede_enc.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
ede_enc.o: /usr/include/bits/sched.h /usr/include/bits/select.h
ede_enc.o: /usr/include/bits/sigset.h /usr/include/bits/stdio_lim.h
ede_enc.o: /usr/include/bits/time.h /usr/include/bits/types.h
ede_enc.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
ede_enc.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
ede_enc.o: /usr/include/endian.h /usr/include/features.h
ede_enc.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
ede_enc.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
ede_enc.o: /usr/include/stdlib.h /usr/include/string.h
ede_enc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
ede_enc.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
ede_enc.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
ede_enc.o: /usr/include/xlocale.h
ede_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
ede_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
enc_read.o: des.h des_locl.h /usr/include/alloca.h /usr/include/asm/errno.h
enc_read.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
enc_read.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
enc_read.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
enc_read.o: /usr/include/bits/sched.h /usr/include/bits/select.h
enc_read.o: /usr/include/bits/sigset.h /usr/include/bits/stdio_lim.h
enc_read.o: /usr/include/bits/time.h /usr/include/bits/types.h
enc_read.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
enc_read.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
enc_read.o: /usr/include/endian.h /usr/include/errno.h /usr/include/features.h
enc_read.o: /usr/include/_G_config.h /usr/include/gconv.h
enc_read.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
enc_read.o: /usr/include/libio.h /usr/include/linux/errno.h
enc_read.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
enc_read.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
enc_read.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
enc_read.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
enc_read.o: /usr/include/xlocale.h
enc_read.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
enc_read.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
enc_writ.o: des.h des_locl.h /usr/include/alloca.h /usr/include/asm/errno.h
enc_writ.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
enc_writ.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
enc_writ.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
enc_writ.o: /usr/include/bits/sched.h /usr/include/bits/select.h
enc_writ.o: /usr/include/bits/sigset.h /usr/include/bits/stdio_lim.h
enc_writ.o: /usr/include/bits/time.h /usr/include/bits/types.h
enc_writ.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
enc_writ.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
enc_writ.o: /usr/include/endian.h /usr/include/errno.h /usr/include/features.h
enc_writ.o: /usr/include/_G_config.h /usr/include/gconv.h
enc_writ.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
enc_writ.o: /usr/include/libio.h /usr/include/linux/errno.h
enc_writ.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
enc_writ.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
enc_writ.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
enc_writ.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
enc_writ.o: /usr/include/xlocale.h
enc_writ.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
enc_writ.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
fcrypt.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
fcrypt.o: /usr/include/bits/stdio_lim.h /usr/include/bits/types.h
fcrypt.o: /usr/include/bits/wchar.h /usr/include/features.h
fcrypt.o: /usr/include/_G_config.h /usr/include/gconv.h
fcrypt.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
fcrypt.o: /usr/include/sys/cdefs.h /usr/include/wchar.h
fcrypt.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
fcrypt.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
ncbc_enc.o: des.h des_locl.h /usr/include/alloca.h
ncbc_enc.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
ncbc_enc.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
ncbc_enc.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
ncbc_enc.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ncbc_enc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
ncbc_enc.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
ncbc_enc.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
ncbc_enc.o: /usr/include/bits/wordsize.h /usr/include/endian.h
ncbc_enc.o: /usr/include/features.h /usr/include/_G_config.h
ncbc_enc.o: /usr/include/gconv.h /usr/include/getopt.h
ncbc_enc.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
ncbc_enc.o: /usr/include/stdlib.h /usr/include/string.h
ncbc_enc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
ncbc_enc.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
ncbc_enc.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
ncbc_enc.o: /usr/include/xlocale.h
ncbc_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
ncbc_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
ofb64ede.o: des.h des_locl.h /usr/include/alloca.h
ofb64ede.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
ofb64ede.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
ofb64ede.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
ofb64ede.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ofb64ede.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
ofb64ede.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
ofb64ede.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
ofb64ede.o: /usr/include/bits/wordsize.h /usr/include/endian.h
ofb64ede.o: /usr/include/features.h /usr/include/_G_config.h
ofb64ede.o: /usr/include/gconv.h /usr/include/getopt.h
ofb64ede.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
ofb64ede.o: /usr/include/stdlib.h /usr/include/string.h
ofb64ede.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
ofb64ede.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
ofb64ede.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
ofb64ede.o: /usr/include/xlocale.h
ofb64ede.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
ofb64ede.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
ofb64enc.o: des.h des_locl.h /usr/include/alloca.h
ofb64enc.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
ofb64enc.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
ofb64enc.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
ofb64enc.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ofb64enc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
ofb64enc.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
ofb64enc.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
ofb64enc.o: /usr/include/bits/wordsize.h /usr/include/endian.h
ofb64enc.o: /usr/include/features.h /usr/include/_G_config.h
ofb64enc.o: /usr/include/gconv.h /usr/include/getopt.h
ofb64enc.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
ofb64enc.o: /usr/include/stdlib.h /usr/include/string.h
ofb64enc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
ofb64enc.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
ofb64enc.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
ofb64enc.o: /usr/include/xlocale.h
ofb64enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
ofb64enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
ofb_enc.o: des.h des_locl.h /usr/include/alloca.h /usr/include/bits/confname.h
ofb_enc.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
ofb_enc.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
ofb_enc.o: /usr/include/bits/sched.h /usr/include/bits/select.h
ofb_enc.o: /usr/include/bits/sigset.h /usr/include/bits/stdio_lim.h
ofb_enc.o: /usr/include/bits/time.h /usr/include/bits/types.h
ofb_enc.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
ofb_enc.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
ofb_enc.o: /usr/include/endian.h /usr/include/features.h
ofb_enc.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
ofb_enc.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
ofb_enc.o: /usr/include/stdlib.h /usr/include/string.h
ofb_enc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
ofb_enc.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
ofb_enc.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
ofb_enc.o: /usr/include/xlocale.h
ofb_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
ofb_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
pcbc_enc.o: des.h des_locl.h /usr/include/alloca.h
pcbc_enc.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
pcbc_enc.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
pcbc_enc.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
pcbc_enc.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
pcbc_enc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
pcbc_enc.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
pcbc_enc.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
pcbc_enc.o: /usr/include/bits/wordsize.h /usr/include/endian.h
pcbc_enc.o: /usr/include/features.h /usr/include/_G_config.h
pcbc_enc.o: /usr/include/gconv.h /usr/include/getopt.h
pcbc_enc.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
pcbc_enc.o: /usr/include/stdlib.h /usr/include/string.h
pcbc_enc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
pcbc_enc.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
pcbc_enc.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
pcbc_enc.o: /usr/include/xlocale.h
pcbc_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
pcbc_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
qud_cksm.o: des.h des_locl.h /usr/include/alloca.h
qud_cksm.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
qud_cksm.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
qud_cksm.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
qud_cksm.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
qud_cksm.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
qud_cksm.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
qud_cksm.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
qud_cksm.o: /usr/include/bits/wordsize.h /usr/include/endian.h
qud_cksm.o: /usr/include/features.h /usr/include/_G_config.h
qud_cksm.o: /usr/include/gconv.h /usr/include/getopt.h
qud_cksm.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
qud_cksm.o: /usr/include/stdlib.h /usr/include/string.h
qud_cksm.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
qud_cksm.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
qud_cksm.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
qud_cksm.o: /usr/include/xlocale.h
qud_cksm.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
qud_cksm.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
rand_key.o: des.h des_locl.h /usr/include/alloca.h
rand_key.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
rand_key.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
rand_key.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
rand_key.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
rand_key.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
rand_key.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
rand_key.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
rand_key.o: /usr/include/bits/wordsize.h /usr/include/endian.h
rand_key.o: /usr/include/features.h /usr/include/_G_config.h
rand_key.o: /usr/include/gconv.h /usr/include/getopt.h
rand_key.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
rand_key.o: /usr/include/stdlib.h /usr/include/string.h
rand_key.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
rand_key.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
rand_key.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
rand_key.o: /usr/include/xlocale.h
rand_key.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
rand_key.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
read_pwd.o: des.h des_locl.h /usr/include/alloca.h /usr/include/asm/ioctl.h
read_pwd.o: /usr/include/asm/ioctls.h /usr/include/asm/sigcontext.h
read_pwd.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
read_pwd.o: /usr/include/bits/environments.h /usr/include/bits/ioctls.h
read_pwd.o: /usr/include/bits/ioctl-types.h /usr/include/bits/posix_opt.h
read_pwd.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
read_pwd.o: /usr/include/bits/select.h /usr/include/bits/setjmp.h
read_pwd.o: /usr/include/bits/sigaction.h /usr/include/bits/sigcontext.h
read_pwd.o: /usr/include/bits/siginfo.h /usr/include/bits/signum.h
read_pwd.o: /usr/include/bits/sigset.h /usr/include/bits/sigstack.h
read_pwd.o: /usr/include/bits/sigthread.h /usr/include/bits/stdio_lim.h
read_pwd.o: /usr/include/bits/time.h /usr/include/bits/types.h
read_pwd.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
read_pwd.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
read_pwd.o: /usr/include/endian.h /usr/include/features.h
read_pwd.o: /usr/include/_G_config.h /usr/include/gconv.h
read_pwd.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
read_pwd.o: /usr/include/libio.h /usr/include/setjmp.h /usr/include/sgtty.h
read_pwd.o: /usr/include/signal.h /usr/include/stdio.h /usr/include/stdlib.h
read_pwd.o: /usr/include/string.h /usr/include/sys/cdefs.h
read_pwd.o: /usr/include/sys/ioctl.h /usr/include/sys/select.h
read_pwd.o: /usr/include/sys/sysmacros.h /usr/include/sys/ttydefaults.h
read_pwd.o: /usr/include/sys/types.h /usr/include/sys/ucontext.h
read_pwd.o: /usr/include/time.h /usr/include/ucontext.h /usr/include/unistd.h
read_pwd.o: /usr/include/wchar.h /usr/include/xlocale.h
read_pwd.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
read_pwd.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
rpc_enc.o: des.h des_locl.h rpc_des.h /usr/include/alloca.h
rpc_enc.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
rpc_enc.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
rpc_enc.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
rpc_enc.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
rpc_enc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
rpc_enc.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
rpc_enc.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
rpc_enc.o: /usr/include/bits/wordsize.h /usr/include/endian.h
rpc_enc.o: /usr/include/features.h /usr/include/_G_config.h
rpc_enc.o: /usr/include/gconv.h /usr/include/getopt.h /usr/include/gnu/stubs.h
rpc_enc.o: /usr/include/libio.h /usr/include/stdio.h /usr/include/stdlib.h
rpc_enc.o: /usr/include/string.h /usr/include/sys/cdefs.h
rpc_enc.o: /usr/include/sys/select.h /usr/include/sys/sysmacros.h
rpc_enc.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
rpc_enc.o: /usr/include/wchar.h /usr/include/xlocale.h
rpc_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
rpc_enc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h version.h
rpw.o: des.h /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
rpw.o: /usr/include/bits/stdio_lim.h /usr/include/bits/types.h
rpw.o: /usr/include/bits/wchar.h /usr/include/features.h
rpw.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/gnu/stubs.h
rpw.o: /usr/include/libio.h /usr/include/stdio.h /usr/include/sys/cdefs.h
rpw.o: /usr/include/wchar.h
rpw.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
rpw.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
set_key.o: des.h des_locl.h podd.h sk.h /usr/include/alloca.h
set_key.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
set_key.o: /usr/include/bits/environments.h /usr/include/bits/posix_opt.h
set_key.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
set_key.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
set_key.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
set_key.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
set_key.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
set_key.o: /usr/include/bits/wordsize.h /usr/include/endian.h
set_key.o: /usr/include/features.h /usr/include/_G_config.h
set_key.o: /usr/include/gconv.h /usr/include/getopt.h /usr/include/gnu/stubs.h
set_key.o: /usr/include/libio.h /usr/include/stdio.h /usr/include/stdlib.h
set_key.o: /usr/include/string.h /usr/include/sys/cdefs.h
set_key.o: /usr/include/sys/select.h /usr/include/sys/sysmacros.h
set_key.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
set_key.o: /usr/include/wchar.h /usr/include/xlocale.h
set_key.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
set_key.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
speed.o: des.h /usr/include/asm/sigcontext.h /usr/include/bits/confname.h
speed.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
speed.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
speed.o: /usr/include/bits/sched.h /usr/include/bits/select.h
speed.o: /usr/include/bits/sigaction.h /usr/include/bits/sigcontext.h
speed.o: /usr/include/bits/siginfo.h /usr/include/bits/signum.h
speed.o: /usr/include/bits/sigset.h /usr/include/bits/sigstack.h
speed.o: /usr/include/bits/sigthread.h /usr/include/bits/stdio_lim.h
speed.o: /usr/include/bits/time.h /usr/include/bits/types.h
speed.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
speed.o: /usr/include/endian.h /usr/include/features.h
speed.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
speed.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/signal.h
speed.o: /usr/include/stdio.h /usr/include/sys/cdefs.h
speed.o: /usr/include/sys/select.h /usr/include/sys/sysmacros.h
speed.o: /usr/include/sys/times.h /usr/include/sys/types.h
speed.o: /usr/include/sys/ucontext.h /usr/include/time.h
speed.o: /usr/include/ucontext.h /usr/include/unistd.h /usr/include/wchar.h
speed.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
speed.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
str2key.o: des.h des_locl.h /usr/include/alloca.h /usr/include/bits/confname.h
str2key.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
str2key.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
str2key.o: /usr/include/bits/sched.h /usr/include/bits/select.h
str2key.o: /usr/include/bits/sigset.h /usr/include/bits/stdio_lim.h
str2key.o: /usr/include/bits/time.h /usr/include/bits/types.h
str2key.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
str2key.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
str2key.o: /usr/include/endian.h /usr/include/features.h
str2key.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
str2key.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/stdio.h
str2key.o: /usr/include/stdlib.h /usr/include/string.h
str2key.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
str2key.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
str2key.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
str2key.o: /usr/include/xlocale.h
str2key.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
str2key.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
supp.o: des.h des_locl.h /usr/include/alloca.h /usr/include/bits/confname.h
supp.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
supp.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
supp.o: /usr/include/bits/sched.h /usr/include/bits/select.h
supp.o: /usr/include/bits/sigset.h /usr/include/bits/stdio_lim.h
supp.o: /usr/include/bits/time.h /usr/include/bits/types.h
supp.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
supp.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
supp.o: /usr/include/endian.h /usr/include/features.h /usr/include/_G_config.h
supp.o: /usr/include/gconv.h /usr/include/getopt.h /usr/include/gnu/stubs.h
supp.o: /usr/include/libio.h /usr/include/stdio.h /usr/include/stdlib.h
supp.o: /usr/include/string.h /usr/include/sys/cdefs.h
supp.o: /usr/include/sys/select.h /usr/include/sys/sysmacros.h
supp.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
supp.o: /usr/include/wchar.h /usr/include/xlocale.h
supp.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
supp.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
