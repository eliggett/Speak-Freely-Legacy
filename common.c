
#include "speakfree.h"

/*  StrReplace	-- Replace sequence OldStr with NewStr in Str.
		   Returns a newly allocated string with the sequence
		   replaced or NULL is the new string cannot be allocated.
		   If OldStr does not occur in Str, the input string Str
		   is returned.  */

char *StrReplace(char *Str, char *OldStr, char *NewStr)
{
    int OldLen, NewLen;
    char *p, *q, *qq, *pp;
    char *result;
    int oc = 0;

    q = Str;
    while ((q = strstr(q, OldStr)) != NULL) {
	oc++;
	q++;
    }

    if (oc == 0) {
	return Str;
    }

    OldLen = strlen(OldStr);
    NewLen = strlen(NewStr);

    result = (char *) malloc(strlen(Str) + (oc * (NewLen - OldLen)) + 1);

    if (result != NULL) {
	qq = result;
	q = Str;

	while ((p = strstr(q, OldStr)) != NULL) {
	    pp = qq + (p - q);
	    memcpy(qq, q, p - q);
	    memcpy(pp, NewStr, NewLen);
	    q = p + OldLen;
	    qq = pp + NewLen;
	}

	memcpy(qq, q, strlen(Str) - (q - Str));

        *((qq + strlen(Str)) - (q - Str)) = '\0';
    }
    return result;
}
