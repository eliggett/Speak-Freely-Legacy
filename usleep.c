/*

    Emulation of BSD usleep() for Solaris 2.x

    Contributed by Hans Werner Strube

*/

#include "speakfree.h"
#include <sys/signal.h>

static int
#ifndef OLDCC
    	   volatile
#endif
    	    	    waiting;

static void getalrm(int i)
{
    waiting = 0;
}

void sf_usleep(unsigned t)
{
    static struct itimerval it, ot;
    void (*oldsig)();
    long nt;

    it.it_value.tv_sec = t / 1000000;
    it.it_value.tv_usec = t % 1000000;
    oldsig = signalFUNCreturn signal(SIGALRM, getalrm);
    waiting = 1;
    if (setitimer(ITIMER_REAL, &it, &ot))
	return /*error*/;
    while (waiting) {
	pause();
    }
    signal(SIGALRM, oldsig);
    nt = ((ot.it_value.tv_sec * 1000000L) + ot.it_value.tv_usec) - t;
/*printf("NT = %d\n", nt);*/
    if (nt <= 0) {
	kill(getpid(), SIGALRM);
    } else {
	ot.it_value.tv_sec = nt / 1000000;
	ot.it_value.tv_usec = nt % 1000000;
	setitimer(ITIMER_REAL, &ot, 0);
    }
}
