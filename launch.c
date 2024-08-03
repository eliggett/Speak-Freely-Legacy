/*

			Speak Freely for Unix
		    Launcher for Mike and Speaker

     Designed and implemented in September of 1999 by John Walker

*/

#include "speakfree.h"
#include "version.h"

#ifdef AUDIO_DEVICE_FILE
extern char *devAudioOutput,	      /* Audio file names */
	    *devAudioControl;
#endif

/*  Command names to run for mike and speaker.	The #ifdefs permit
    you to override these at compile time.  */

#ifndef CMD_sfmike
#define CMD_sfmike "sfmike"
#endif

#ifndef CMD_sfspeaker
#define CMD_sfspeaker "sfspeaker"
#endif

static int debug = FALSE;	      /* Debug flag */
static char *progname;		      /* Program name */
static int cpmike;		      /* Sfmike child process ID */
static int nmike = 0;		      /* Number of mike arguments */
static char *cmdmike;		      /* Command to launch sfmike */
static char **argmike;		      /* Sfmike command line arguments */
static char file_descriptors[40];     /* -y option to pass file descriptors to sfmike and sfspeaker */
static char newdest[256];	      /* Destination for new connection */

/*  STRCMPCI  --  Case-insensitive string compare.  Roll our own
		  because the name of this function is notorious
		  for differing from one system to another.  */

static int strcmpci(char *a, char *b)
{
    char la[256], lb[256];
    int i;

    for (i = 0; a[i] != 0; i++) {
	la[i] = isupper(a[i]) ? tolower(a[i]) : a[i];
    }
    la[i] = 0;

    for (i = 0; b[i] != 0; i++) {
	lb[i] = isupper(b[i]) ? tolower(b[i]) : b[i];
    }
    lb[i] = 0;

#ifdef DEBUG_STRCMPCI
    fprintf(stderr, "strcmpci(<%s>, <%s>)\n", la, lb);
#endif
    return strcmp(la, lb);
}

/*  PROG_NAME  --  Extract program name from argv[0].  */

static char *prog_name(char *arg)
{
    char *cp = strrchr(arg, '/');

    return (cp != NULL) ? cp + 1 : arg;
}

/*  SHOWCMD  --  Show effective command line for program.  */

static void showcmd(char *progname, char *program, char **args)
{
    int i;

    fprintf(stderr, "%s: invoking %s as \"", progname, program);
    for (i = 0; args[i] != NULL; i++) {
	if (i > 0) {
            fprintf(stderr, " ");
	}
        fprintf(stderr, "%s", args[i]);
    }
    fprintf(stderr, "\"\n");
}

/*  STARTMIKE  --  Start sfmike in child process.  */

static void startMike(void)
{
    if (debug) {
        showcmd(progname, "sfmike", argmike);
    }
    cpmike = fork();
    if (cpmike == 0) {
	if (debug) {
            fprintf(stderr, "%s: launching \"sfmike%s%s\" in child process.\n",
                progname, file_descriptors[0] == 0 ? "" : " ", file_descriptors);
	}
	execvp(cmdmike, argmike);
        perror("launching sfmike in child process");
	exit(0);
    } else if (cpmike == (pid_t) -1) {
        perror("creating child process to run sfmike");
	exit(1);
    }
}

/*  NEWCONNECTION  --  Prompt user for new connection.	If a
		       non-blank destination is entered,
		       startMike() is called to connect to it
		       and TRUE is returned.  A blank entry
		       indicates the user wants to quit and
		       returns FALSE.  */

static int newConnection(void)
{
    int i;

    printf("%s: New connection (return to exit)? ", progname);
    fflush(stdout);
    if (fgets(newdest, (sizeof newdest) - 2, stdin) != NULL) {
	while ((strlen(newdest) > 0) &&
	       (isspace(newdest[strlen(newdest) - 1]))) {
	    newdest[strlen(newdest) - 1] = 0;
	}
	if (strlen(newdest) > 0) {
	    for (i = 1; i < nmike; i++) {
                if (*argmike[i] != '-') {
		    argmike[i] = newdest;
		    startMike();
		    return TRUE;
		}
	    }
	}
    }
    return FALSE;
}

/*  USAGE  --  Print how-to-call information.  */

static void usage(void)
{
    V fprintf(stderr, "%s  --  Speak Freely launcher.\n", progname);
    V fprintf(stderr, "              %s.\n", Relno);
    V fprintf(stderr, "\n");
    V fprintf(stderr, "Usage: %s [options] hostname[:port] [ file1 / . ]...\n", progname);
    V fprintf(stderr, "Options: (* indicates defaults)\n");
    V fprintf(stderr, "           -BOTH      Options for sfmike and sfspeaker follow\n");
    V fprintf(stderr, "           -D         Enable debug output\n");
    V fprintf(stderr, "         * -LAUNCH    Options for sflaunch follow\n");
    V fprintf(stderr, "           -MIKE      Options for sfmike follow\n");
    V fprintf(stderr, "           -Q         Quit when first connection ends\n");
    V fprintf(stderr, "         * -SFLAUNCH  Options for sflaunch follow\n");
    V fprintf(stderr, "           -SFMIKE    Options for sfmike follow\n");
    V fprintf(stderr, "           -SFSPEAKER Options for sfspeaker follow\n");
    V fprintf(stderr, "           -U         Print this message\n");
#ifdef AUDIO_DEVICE_FILE
    V fprintf(stderr, "           -Yaudiodev[:ctldev] Override default audio device file name(s)\n");
#endif
    V fprintf(stderr, "\n");
    V fprintf(stderr, "by John Walker\n");
    V fprintf(stderr, "   http://www.fourmilab.ch/\n");
}

/*  Main program.  */

int main(int argc, char *argv[])
{
    int cpspeaker, fdio, fdctl, i,
	ndest = 0, nspeaker = 0, oneconnect = FALSE,
	optmike = FALSE, optspeaker = FALSE, wpid, wstat;
    char *cmdspeaker, *cp;
    char **argspeaker;

    progname = prog_name(argv[0]);    /* Save program name */
    file_descriptors[0] = 0;

    /*	Allocate argument arrays for sfmike and sfspeaker and
	plug in always-supplied arguments.  */

    argmike = malloc((argc + 3) * sizeof (char *));
    argspeaker = malloc((argc + 3) * sizeof(char *));
    cmdmike = malloc(strlen(argv[0]) + 12);
    cmdspeaker = malloc(strlen(argv[0]) + 12);

    if ((argmike == NULL) || (argspeaker == NULL) ||
	(cmdmike == NULL) || (cmdspeaker == NULL)) {
        fprintf(stderr, "%s: unable to allocate command and argument arrays for sfmike and sfspeaker.\n",
	    progname);
	return 1;
    }

    /*	Construct commands to invoke sfmike and sfspeaker.  As
	long as argv[0] supplied a fully qualified path, we
	execute them from the same directory from which sflaunch
	was run.  */

    strcpy(cmdmike, argv[0]);
    if ((cp = strrchr(cmdmike, '/')) == NULL) {
	cp = cmdmike;
    } else {
	cp++;
    }
    strcpy(cp, CMD_sfmike);
    argmike[nmike++] = cmdmike;

    strcpy(cmdspeaker, argv[0]);
    if ((cp = strrchr(cmdspeaker, '/')) == NULL) {
	cp = cmdspeaker;
    } else {
	cp++;
    }
    strcpy(cp, CMD_sfspeaker);
    argspeaker[nspeaker++] = cmdspeaker;

    /*	Process command line options.  */

    for (i = 1; i < argc; i++) {
	char *op, opt;

	op = argv[i];
        if (*op == '-') {
	    opt = *(++op);

	    /*	Check for options which direct those which follow
		to sfmike, sfspeaker, both, or us.  Recognised options are
		as follows with those on the same line equivalent:

		    -both
		    -sfmike -mike
		    -sfspeaker -speaker
		    -sflaunch -launch
	    */

            if ((strcmpci(op, "both") == 0)) {
		optmike = TRUE;
		optspeaker = TRUE;
		continue;
	    }

            if ((strcmpci(op, "mike") == 0) ||
                (strcmpci(op, "sfmike") == 0)) {
		optmike = TRUE;
		optspeaker = FALSE;
		continue;
	    }

            if ((strcmpci(op, "speaker") == 0) ||
                (strcmpci(op, "sfspeaker") == 0)) {
		optmike = FALSE;
		optspeaker = TRUE;
		continue;
	    }

            if ((strcmpci(op, "launch") == 0) ||
                (strcmpci(op, "sflaunch") == 0)) {
		optmike = FALSE;
		optspeaker = FALSE;
		continue;
	    }

	    /*	If this option is directed at sfmike or sfspeaker, append
                it to that program's argument vector.  */

	    if (optmike) {
		argmike[nmike++] = argv[i];
	    }
	    if (optspeaker) {
		argspeaker[nspeaker++] = argv[i];
	    }
	    if (optmike || optspeaker) {
		continue;
	    }

	    if (islower(opt)) {
		opt = toupper(opt);
	    }

	    switch (opt) {
                case 'D':             /* -D  --  Debug output */
		    debug = TRUE;
		    break;

                case 'Q':             /* -Q  --  Quit after first connection */
		    oneconnect = TRUE;
		    break;

                case 'U':             /* -U  --  Print usage information */
                case '?':             /* -?  --  Print usage information */
		    usage();
		    return 0;

#ifdef AUDIO_DEVICE_FILE
                case 'Y':             /* -Yaudiodev:[ctldev] -- Specify audio
						 I/O and control device file
						 names. */
		    devAudioOutput = op + 1;
                    if (strchr(op + 1, ':') != NULL) {
                        devAudioControl = strchr(op + 1, ':') + 1;
		    }
		    break;
#endif
	    }
	} else {
            argmike[nmike++] = op;    /* Plug destination into sfmike's argument list */
	    ndest++;
	}
    }

    if (ndest == 0) {
        argmike[nmike++] = "X";       /* Placeholder for new connection */
    }

#ifdef AUDIO_DEVICE_FILE
    if (!soundinit(O_RDWR)) {
        perror("opening sound device");
        fprintf(stderr, "%s cannot open one or both of the audio and\n", progname);
        fprintf(stderr, "audio control device files.  Be sure you specified\n");
        fprintf(stderr, "the correct names for your system and audio drivers.\n");
	exit(1);
    }

    sound_open_file_descriptors(&fdio, &fdctl);

    if (debug) {
        fprintf(stderr, "%s: Open file descriptors: audio = %d, control = %d\n",
	    progname, fdio, fdctl);
    }
    sprintf(file_descriptors, "-y#%d", fdio);
    if (fdctl >= 0) {
        sprintf(file_descriptors + strlen(file_descriptors), ":#%d", fdctl);
    }

    /* Add file descriptor argument to mike and speaker argument lists, then
       NULL terminate said lists. */

    argmike[nmike++] = file_descriptors;
    argspeaker[nspeaker++] = file_descriptors;
#endif

    argmike[nmike] = argspeaker[nspeaker] = NULL;

    if (debug) {
        showcmd(progname, "sfspeaker", argspeaker);
    }

    /* Fork child process and start sfspeaker, passing the file
       descriptors for the audio and control files in via the
       -y option. */

    cpspeaker = fork();
    if (cpspeaker == 0) {
	if (debug) {
            fprintf(stderr, "%s: launching \"sfspeaker%s%s\" in child process.\n",
                progname, file_descriptors[0] == 0 ? "" : " ", file_descriptors);
	}
	execvp(cmdspeaker, argspeaker);
        perror("launching sfspeaker in child process");
	return 0;
    } else if (cpspeaker == (pid_t) -1) {
        perror("creating child process to run sfspeaker");
	return 1;
    }
    free(argspeaker);

    /* Now that sfspeaker is running, we spawn another child process
       to run sfmike, again invoked with the -y option to hand over
       the file descriptors we've opened.  Why not launch sfmike in
       the main process?  Because we need to wait on both processes
       in order to avoid zombie processes and to kill sfspeaker
       when the user quits sfmike. */

    if (ndest == 0) {
	cpmike = -1;
	if (!newConnection()) {
	    kill(cpspeaker, SIGHUP);
	}
    } else {
	startMike();
    }

    if (debug) {
        fprintf(stderr, "%s: waiting on child processes for sfspeaker (%d) and sfmike (%d).\n",
	    progname, cpspeaker, cpmike);
    }

    while (((cpmike != -1) || (cpspeaker != -1)) &&
	   ((wpid = wait(&wstat)) != -1)) {
	if (debug) {
            fprintf(stderr, "Child process %d (%s) terminated.\n", wpid,
                wpid == cpmike ? "sfmike" : (wpid == cpspeaker ? "sfspeaker" : "??unknown??"));
	}

        /* The normal state of affairs is that we'll receive
	   notice of termination of sfmike first, when the user
	   ends the conversation.  But just in case sfspeaker
	   dies, we also shut down sfmike, since that makes it
	   easier for the user to restart the whole stack. */

	if (wpid == cpmike) {
	    cpmike = -1;	      /* Mark mike exited */
	    if (!oneconnect && newConnection()) {
		goto waiton;
	    }
	    if (cpspeaker != -1) {
		kill(cpspeaker, SIGHUP);
	    }
	} else {
	    cpspeaker = -1;	      /* Mark speaker exited */
	    if (cpmike != -1) {
                fprintf(stderr, "%s: abnormal termination in sfspeaker.  Shutting down sfmike.\n",
		    progname);
		kill(cpmike, SIGHUP);
	    } else {
		if (debug) {
                    fprintf(stderr, "%s: normal termination notification from sfspeaker.\n",
			progname);
		}
	    }
	}
waiton:;
    }

    /* And now, with both child processes done, we can close the
       audio device. */

#ifdef AUDIO_DEVICE_FILE
    if (debug) {
        fprintf(stderr, "%s: closing audio device.\n", progname);
    }
    soundterm();
#endif

    return 0;
}
