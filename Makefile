
#   Make file for Speak Freely for Unix

# Debugging options

#DEBUG = -O
#DEBUG = -g -DHEXDUMP
#DEBUG = -g -DHEXDUMP -DNOCRYPTO
DEBUG = -O3 -DHEXDUMP
  
# Installation 

# Install program
INSTALL = /usr/bin/install
# Installation root directory
INSTDIR = /usr/local
# Binaries
INSTDIR_BIN = $(INSTDIR)/bin
# Manual pages
INSTDIR_MAN = $(INSTDIR)/man

#   Speak Freely can use a public key encryption utility to
#   exchange a session key with a remote site.  By default,
#   Speak Freely uses PGP.  If you prefer to use GPG, uncomment
#   the following definition.

#PKOPTS = -DGPG_KEY_EXCHANGE

#   Uncomment the appropriate CC, CCFLAGS, and LFLAGS statements below
#   according to your machine type.  The CELPFLAGS variable allows you
#   to pass additional options for the compilation of the CELP library.
#   CELP encoding is fantastically computationally intense, and tweaking
#   optimisation options has a big payoff here.

#				Linux

#    Linux users please note: many Linux audio drivers are
#    half-duplex, even through your sound card may actually
#    have full-duplex hardware.
#
#    The following settings are for the most "vanilla" Linux
#    sound configuration.  This should work with the OSS/Free
#    sound drivers and audio hardware which emulates a Sound
#    Blaster Pro.  If you get long delays, try also adding
#    -DLINUX_DSP_SMALL_BUFFER.	If you have fancier hardware
#    and/or drivers it's wise to start out with simple settings
#    like those below and experiment with fancier modes (for
#    example, full duplex) only after you're sure the basic
#    functionality is working.	Please see the detailed description
#    of the available flags which follows these declarations.
#
#CCFLAGS =  -DAUDIO_BLOCKING -DLINUX -DHALF_DUPLEX -DM_LITTLE_ENDIAN
CCFLAGS =  -DAUDIO_BLOCKING -DLINUX -DHALF_DUPLEX -DM_LITTLE_ENDIAN -DNEEDED_LINEAR -DLINUX_DSP_SMALL_BUFFER -DHAVE_DEV_RANDOM
CC = gcc -Wall # for GNU's gcc compiler
#   CELPFLAGS below are tweaked for GCC 2.96 on Intel Pentium.
#   Comment out if you are using a compiler which doesn't
#   understand these options.
CELPFLAGS = -fomit-frame-pointer -ffast-math -funroll-loops
LFLAGS = -lncurses -lm


# If you want to use ALSA instead of OSS, uncomment the following
# two definitions.
#CCFLAGS = -DLINUX_ALSA -DM_LITTLE_ENDIAN
#SOUNDLIB = -lasound

#
# If the above LFLAGS doesn't work, try the one below.
#LFLAGS = -lcurses -lm
#
#   The following is a detailed description of these and other flags
#   you may want to specify or not depending upon the details of your
#   Linux sound configuration.
#
#	-DAUDIO_BLOCKING
#	    You almost always want to specify this.  If it's
#	    not present, Speak Freely may read short blocks of
#	    audio from the input source and send compressed
#	    packets filled with mostly silence.  The option is
#	    provided since it's required with some workstation
#	    audio hardware to avoid buffering problems.  However,
#	    some Linux configurations will encounter long delays
#	    between audio input and transmission which cannot be
#	    fixed entirely by setting the LINUX_DSP_SMALL_BUFFER and
#	    associated flags described below.  On such systems, you
#	    may need to also to build without AUDIO_BLOCKING defined.
#	    Such is the state of sound drivers for Linux that you may
#	    have to experiment with various settings of these
#	    variables to find a combination which works acceptably for
#	    your audio hardware and drivers.
#
#	-DHALF_DUPLEX
#	    Required if your audio hardware and/or driver does not
#	    permit simultaneous opening of the audio device for
#	    input and output by two separate programs.	As noted
#	    above, many Linux audio drivers are half duplex and
#	    require this flag even though the underlying audio hardware
#	    is full duplex.  Start by specifying this and then, if
#	    you believe your system capable of full duplex,
#	    experiment with turning it off.
#
#	-DIN_AUDIO_DEV=\"/dev/audio\"
#	-DOUT_AUDIO_DEV=\"/dev/audio1\"
#
#	    Some Linux audio drivers, for example the commercial
#	    OSS/Linux full-duplex driver, require full duplex programs
#	    to open separate /dev/audio and /dev/audio1 devices for
#	    input and output (or vice versa, presumably).  To
#	    configure the audio drivers in soundbyte.c to do this add
#	    the above to the CCFLAGS declaration.
#
#	    If your make or shell has different opinions about how to
#	    get quotes all the way from a make macro to the C compiler
#	    command line, you may have to experiment with the quoting
#	    on these declarations.  As a last resort, just edit the
#	    top of soundbyte.c and hammer in hard-coded definitions of
#	    symbols IN_AUDIO_DEV and OUT_AUDIO_DEV.  You can set the
#	    input and output audio device file names to anything you
#	    wish, not just the values given above.
#
#	-DLINUX
#	    Required for all Linux configurations.
#
#	-DLINUX_DSP_SMALL_BUFFER
#	    Some Linux sound drivers default to a very large buffer
#	    for audio input, which results in long delays between
#	    the time audio is received by the microphone and when
#	    Speak Freely receives it to be transmitted.  Defining this
#	    symbol attempts to set the audio input buffer size to
#	    2048 bytes to minimise this delay.
#
#	-DFRAGMENT_BUFSIZE=32
#	-DFRAGMENT_BUFPOWER=8
#    
#	    Control the audio input buffer size.  Only works if
#	    LINUX_DSP_SMALL_BUFFER is also defined. The buffer size is 
#	    calculated as FRAGMENT_BUFSIZE * 2 ^ FRAGMENT_BUFPOWER.
#	    Big input buffers will delay when transmitting (input ->
#	    recording) while too small input buffers cause clipping.
#	    If you have still trouble reducing the delay, try compiling
#	    _without_ -DAUDIO_BLOCKING.
#	    For more details please refer to README.Linux_OSS_bufsize
#	    and soundinit() of soundbyte.c.  This code was developed
#	    and contributed by Walter Haidinger (walter.haidinger@gmx.at)
#	    who reports that the above values work for him, with no
#	    clipping with just about one second delay.	These values
#	    are applicable only to the OSS sound driver; defining them
#	    requires -DLINUX_DSP_SMALL_BUFFER also be defined,
#	    otherwise they will have no effect.
#
#	-DLINUX_FPU_FIX
#	    Some older C libraries on Intel-based Linux systems did
#	    not place the processor's floating point unit into default
#	    IEEE exception handling mode, which could result in
#	    program crashes due to harmless floating point underflows
#	    to zero which occur in the LPC compression library.
#	    Defining this symbol compiles in code which explicitly
#	    sets IEEE exception handling.
#
#	-DM_LITTLE_ENDIAN
#	    This symbol should be defined when compiling on "little-
#	    endian" platforms such as the Intel x86 series and
#	    clones.  Little-endian machines store multi-byte values
#	    with the least significant byte first in memory; big-endian
#	    machines store bytes in the opposite order.  This should
#	    be defined unless you're running on a big-endian processor
#	    (a SPARC, for example). 
#
#	-DNEEDED_LINEAR
#	    Some Linux audio drivers (for example, the OSS/Free [but
#	    *not* the commercial OSS/Linux drivers for the very same
#	    card] drivers for the Ensoniq AudioPCI card) do not
#	    support 8 bit mu-law I/O (which has been the default
#	    /dev/audio format since the first Sun workstations were
#	    shipped with audio in the late 1980's).  Defining this
#	    symbol compiles in code, developed and contributed by
#	    Jean-Marc Orliaguet, which translates between the 16 bit
#	    PCM audio used by such drivers and the 8 bit mu-law
#	    representation expected by Speak Freely.  If you're able
#	    to send and receive audio but the sound is horribly
#	    distorted, you may need to enable this.  If you're able to
#	    play the mu-law sound files included with Speak Freely,
#	    for example:
#		    cat ring.au >/dev/audio
#	    without distortion, then it's unlikely this option will help.
#
#   Another problem frequently encountered by Linux users is the
#   permissions on the audio device.  To prevent eavesdropping, some
#   Linux distributions require root privilege to open audio input.
#   Unless you want to become root in order to run Speak Freely,
#   you'll need to change the permissions on /dev/audio (or whatever)
#   to allow regular users to open it for input.
#
#   Finally, if you wish to operate a Look Who's Listening server (sflwld) and
#   your system supports POSIX threads, your server will be much more
#   responsive if you compile with the -DTHREADS option.  You'll
#   need to add -lpthreads to LFLAGS as well.  If this doesn't work
#   on your system, the Look Who's Listening server will still function
#   correctly, but it will take longer to respond to requests at times
#   of heavy load.

#			   Silicon Graphics

# In order to build Speak Freely, you need to have the dmedia_dev
# packages installed.  In IRIX 5.3 and later, they are included with
# The IRIX Development Option (IDO), but may not be installed by a
# default installation of IDO. The command "versions dmedia_dev.sw"
# will tell you whether these components are present on your system.
# If they aren't, you need to install them before building Speak
# Freely.
#
# The following options are optimal for IRIX 6.5 with C 7.2.1.
# If you're compiling on an earlier version, adding the -float
# option may speed up certain compression modes.  If you get a
# warning about -float being ignored in non -cckr compiles, it
# has no effect on your system.
#
# The -diag_suppress and -LD_MSG options turn off warnings
# from the compiler and loader for completely harmless code.
# The MIPSPro compiler attempts to do "compile-time subscript
# checking" and emits warnings when it detects an error.  In
# all of the cases in Speak Freely, these are perfectly legitimate
# C code in the CODECs, mostly in code converted from FORTRAN,
# which compiles without warnings at the highest warning levels
# of other compilers.  The -LD_MSG option suppresses warnings
# from the loader when libraries are specified which don't
# have any code pulled in from them.  Again, this is just
# silly--we want to specify all the libraries we need for the
# suite of programs and let the individual links pull in what
# they need.
#DIAGFLAGS = -diag_suppress 1167,1172 -LD_MSG:off=84
#CC = cc -signed $(DIAGFLAGS)
#LFLAGS = -laudio -lcurses -lm

#			      Solaris 2.x
#		    (courtesy of Hans Werner Strube)

# (-fsingle is needed for pre-4.0 compilers and is ignored by 4.0 in
# ANSI mode.)  Defining THREADS enables multi-threaded operation in
# sflwld (and has no effect on any other component of Speak Freely).
# THREADS has been tested on Solaris 2.5 through 2.7 (a.k.a. 7) SPARC
# and requires POSIX thread support.  If you cannot build with THREADS
# defined, simply remove it from the CCFLAGS line below.  If you
# disable THREADS, you can also remove the "-lpthread" library
# specification from the LFLAGS line, which may cause an error if the
# system does not include the POSIX threads library.  If you're
# building for Solaris 9, you may also specify -DHAVE_DEV_RANDOM
# to include the system's entropy generator in the session key
# generation process.

#CC = cc -fsingle # for Sun Compiler
#CCFLAGS = -DSolaris -DTHREADS
#LFLAGS = -lcurses -lsocket -lnsl -lm -lpthread

#			     SunOS 4.1.x

#CC = cc -fsingle -DOLDCC
#LFLAGS = -lcurses -ltermcap -lm

#			     FreeBSD 2.2
#		    (courtesy of Andrey A. Chernov)

#
# ** FreeBSD users please note: many FreeBSD audio drivers are
#    half-duplex, even through your sound card may actually
#    have full-duplex hardware.  If you have trouble running
#    sfmike and sfspeaker at the same time, try uncommenting
#    the definition DUPLEX = -DHALF_DUPLEX later in this file.
#    Depending on how your driver handles non-blocking I/O,
#    you may also have to add -DAUDIO_BLOCKING to the
#    CCFLAGS line.
#CCFLAGS = -DM_LITTLE_ENDIAN
#LFLAGS = -lcurses -ltermcap -lcompat -lm

#			   Hewlett-Packard
#		      (courtesy of Marc Kilian)
#
#	   PRELIMINARY--NOT FULLY TESTED
#CC = cc
#CCFLAGS = -DHEWLETT_PACKARD -DOLDCC
#LFLAGS = -lAlib -lcurses -ltermcap -lm

# Where Perl is located on your system.  This is used to make
# a directly-executable version of sfvod.
PERL = /usr/bin/perl

# If your audio hardware is half duplex, uncomment the next line.
# You can also, if you wish, define this on the CCFLAGS definition
# for your hardware platform.
#DUPLEX = -DHALF_DUPLEX

# If your getdomainname() does not return the DNS domainname, define:
#DOMAIN=-DMYDOMAIN=\"somedomain.net\"

#   ################################################################
#   ################################################################

#   Everything will probably work OK without any changes below
#   this line.

#   Default Internet socket port used by sfmike and sfspeaker.	If you
#   change this, you will not be able to exchange sound with users
#   who've built Speak Freely with different values.  This default can
#   be overridden by the "-Pport" option on sfspeaker and the ":port"
#   hostname suffix in sfmike.	The ports used by Speak Freely are as
#   follows:
#
#	    INTERNET_PORT     UDP   Sound packets
#	    INTERNET_PORT+1   UDP   Control messages (RTCP)
#	    INTERNET_PORT+2   TCP   Communications with LWL server
#
#   If you don't publish your information or query an LWL server,
#   INTERNET_PORT+2 is never used.

INTERNET_PORT = 2074

CARGS = -DInternet_Port=$(INTERNET_PORT)

#   Compiler flags

CFLAGS = $(DEBUG) $(PKOPTS) -Iadpcm -Iaes -Icelp -Ilpc -Igsm/inc -Ilpc10 -Imd5 -Ides -Iidea -Ilibdes -Iblowfish $(CARGS) $(DUPLEX) $(CCFLAGS) $(DOMAIN)

BINARIES = sfspeaker sfmike sflaunch sflwld sflwl sfecho sfreflect

SCRIPTS = sfvod

PROGRAMS = $(BINARIES) $(SCRIPTS)

DIRS = adpcm aes blowfish celp des gsm idea libdes lpc lpc10 md5

all:	$(PROGRAMS)

SPKROBJS = speaker.o codecs.o deskey.o g711.o rate.o rtpacket.o soundbyte.o tempfile.o ulaw.o usleep.o vatpkt.o vox.o audio_hp.o audio_sgi.o audio_alsa.o common.o

sfspeaker: $(SPKROBJS) adpcmlib.o aeslib.o celplib.o libblowfish.o lpclib.o lpc10lib.o gsmlib.o deslib.o md5lib.o idealib.o libdes.o xdsub.o 
	$(CC) $(SPKROBJS) adpcm/adpcm-u.o aes/aes.a blowfish/libblowfish.a celp/celp.o des/des.a md5/md5.o idea/idea.a lpc10/liblpc10.a gsm/lib/libgsm.a lpc/lpc.o xdsub.o libdes/libdes.a $(LFLAGS) $(SOUNDLIB) -o sfspeaker

MIKEOBJS = mike.o codecs.o deskey.o g711.o rate.o rtpacket.o soundbyte.o tempfile.o ulaw.o usleep.o vatpkt.o xdsub.o audio_hp.o audio_sgi.o audio_alsa.o

sfmike: $(MIKEOBJS) adpcmlib.o aeslib.o celplib.o libblowfish.o lpclib.o lpc10lib.o gsmlib.o deslib.o md5lib.o idealib.o libdes.o
	$(CC) $(MIKEOBJS) adpcm/adpcm-u.o aes/aes.a celp/celp.o des/des.a md5/md5.o idea/idea.a -lm blowfish/libblowfish.a lpc10/liblpc10.a gsm/lib/libgsm.a lpc/lpc.o libdes/libdes.a $(LFLAGS) $(SOUNDLIB) -o sfmike

LAUNCHOBJS = launch.o soundbyte.o usleep.o g711.o

sflaunch: $(LAUNCHOBJS)
	$(CC) $(LAUNCHOBJS) $(LFLAGS) -o sflaunch

LWLDOBJS = lwld.o html.o xdsub.o

sflwld: $(LWLDOBJS)
	$(CC) $(LWLDOBJS) $(LFLAGS) -o sflwld

LWLOBJS = lwl.o

sflwl:	$(LWLOBJS)
	$(CC) $(LWLOBJS) $(LFLAGS) -o sflwl

ECHOOBJS = echo.o codecs.o g711.o rtpacket.o ulaw.o xdsub.o

sfecho: $(ECHOOBJS) md5lib.o
	$(CC) $(ECHOOBJS) md5/md5.o adpcm/adpcm-u.o lpc/lpc.o $(LFLAGS) -o sfecho

REFLECTOBJS = reflect.o codecs.o html.o g711.o rtpacket.o ulaw.o xdsub.o

sfreflect: $(REFLECTOBJS) md5lib.o
	$(CC) $(REFLECTOBJS) md5/md5.o adpcm/adpcm-u.o lpc/lpc.o $(LFLAGS) -o sfreflect

#	Configure the voice on demand server for the platform's
#	location of Perl and network constant definitions.

sfvod:	sfvod.pl version.h
	echo \#\! $(PERL) >sfvod
	echo \$$version = `tail -1 version.h`\; >>sfvod
	echo '#include <stdio.h>' >sfvod-t.c
	echo '#include <sys/types.h>' >>sfvod-t.c
	echo '#include <sys/socket.h>' >>sfvod-t.c
	echo 'int main(){printf("$$AF_INET = %d; $$SOCK_DGRAM = %d;%c", AF_INET, SOCK_DGRAM, 10);return 0;}' >>sfvod-t.c
	$(CC) sfvod-t.c -o sfvod-t
	./sfvod-t >>sfvod
	rm sfvod-t.c sfvod-t
	cat sfvod.pl >>sfvod
	chmod 755 sfvod

#	Compression and encryption libraries.  Each of these creates
#	a place-holder .o file in the main directory (which is not
#	an actual object file, simply a place to hang a time and
#	date stamp) to mark whether the library has been built.
#	Note that if you actually modify a library you'll need to
#	delete the place-holder or manually make within the library
#	directory.  This is tacky but it avoids visiting all the
#	library directories on every build and/or relying on features
#	in make not necessarily available on all platforms.

adpcmlib.o:
	( echo "Building ADPCM library."; cd adpcm ; make CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "ADPCM" >adpcmlib.o

aeslib.o:
	( echo "Building AES library."; cd aes ; make CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "AES" >aeslib.o

celplib.o:
	( echo "Building CELP library."; cd celp ; make CC="$(CC) $(CCFLAGS) $(DEBUG) $(CELPFLAGS)" )
	echo "CELP" >celplib.o

deslib.o:
	( echo "Building DES library."; cd des ; make CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "DES" >deslib.o

libblowfish.o:
	( echo "Building BLOWFISH library."; cd blowfish ; make CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "BLOWFISH" >libblowfish.o

libdes.o:
	( echo "Building LIBDES library."; cd libdes ; make -f Makefile.sf CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "LIBDES" >libdes.o

lpclib.o:
	( echo "Building LPC library."; cd lpc ; make CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "LPC" >lpclib.o

lpc10lib.o:
	( echo "Building LPC10 library."; cd lpc10 ; make CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "LPC" >lpc10lib.o

gsmlib.o:
	( echo "Building GSM library."; cd gsm ; make CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "GSM" >gsmlib.o

md5lib.o:
	( echo "Building MD5 library."; cd md5 ; make CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "MD5" >md5lib.o

idealib.o:
	( echo "Building IDEA library."; cd idea ; make CC="$(CC) $(CCFLAGS) $(DEBUG)" )
	echo "IDEA" >idealib.o

#   Object file dependencies

codecs.o:   codecs.c speakfree.h

common.o:   common.c  speakfree.h

html.o: html.c

mike.o: mike.c speakfree.h version.h

launch.o: launch.c speakfree.h version.h

lwl.o:	lwl.c speakfree.h version.h

lwld.o: lwld.c speakfree.h version.h

echo.o: echo.c speakfree.h vat.h version.h

reflect.o: reflect.c speakfree.h vat.h version.h

rtpacket.o: rtpacket.c speakfree.h rtp.h

soundbyte.o: Makefile soundbyte.c speakfree.h

tempfile.o: Makefile tempfile.c speakfree.h

audio_alsa.o: Makefile audio_alsa.c speakfree.h

g711.o: Makefile g711.c

speaker.o: speaker.c speakfree.h version.h

vatpkt.o:   vatpkt.c speakfree.h vat.h

speakfree.h:	audio_descr.h rtp.h rtpacket.h ulaw2linear.h types.h

testgsm:    testgsm.o gsmlib.o
	$(CC) testgsm.o -lm gsm/lib/libgsm.a $(LFLAGS) -o testgsm

manpage:
	nroff -man sfmike.1 | $(PAGER)
	nroff -man sfspeaker.1 | $(PAGER)
	nroff -man sflaunch.1 | $(PAGER)
	nroff -man sflwl.1 | $(PAGER)
	nroff -man sflwld.1 | $(PAGER)
	nroff -man sfecho.1 | $(PAGER)
	nroff -man sfreflect.1 | $(PAGER)
	nroff -man sfvod.1 | $(PAGER)

#	Process NROFF manual pages into cat-able .man pages

MANTWEAK = tr '\255' -
mantext:
	nroff -man sfmike.1 | col -b | $(MANTWEAK) >/tmp/sfmike.man
	nroff -man sfspeaker.1 | $(MANTWEAK) | col -b >/tmp/sfspeaker.man
	nroff -man sflaunch.1 | $(MANTWEAK) | col -b >/tmp/sflaunch.man
	nroff -man sflwl.1 | $(MANTWEAK) | col -b >/tmp/sflwl.man
	nroff -man sflwld.1 | $(MANTWEAK) | col -b >/tmp/sflwld.man
	nroff -man sfecho.1 | $(MANTWEAK) | col -b >/tmp/sfecho.man
	nroff -man sfreflect.1 | $(MANTWEAK) | col -b >/tmp/sfreflect.man
	nroff -man sfvod.1 | $(MANTWEAK) | col -b >/tmp/sfvod.man

#	Print manual pages for all programs.  Assumes you have "ptroff"

printman:
	ptroff -man sfmike.1
	ptroff -man sfspeaker.1
	ptroff -man sflaunch.1
	ptroff -man sflwl.1
	ptroff -man sflwld.1
	ptroff -man sfecho.1
	ptroff -man sfreflect.1
	ptroff -man sfvod.1

#	Clean everything

clean:
	find . -name Makefile.bak -exec rm {} \;
	rm -f core *.out *.o *.bak $(PROGRAMS) *.shar sfvod-t*
	@for I in $(DIRS); \
	  do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) $@ || exit 1); done
	
#	Clean only the main directory, not the libraries

dusty:
	rm -f core *.out *.o *.bak $(PROGRAMS) *.shar sfvod-t*

#	Install binaries, scripts, and manual pages.  You'll need to
#	be root to install in system directories.

install: $(PROGRAMS)
	$(INSTALL) -o root -g root -m 755 -s $(BINARIES) $(INSTDIR_BIN)
	$(INSTALL) -o root -g root -m 755 $(SCRIPTS) $(INSTDIR_BIN)
	$(INSTALL) -o root -g root -m 644 *.1 $(INSTDIR_MAN)/man1

# DO NOT DELETE
