/*

			HTML utility routines

*/

#include "speakfree.h"

/*  OUTHTML  --  Transcribe a string to an HTML output file, quoting
		 HTML special characters as necessary.	*/

void outHTML(FILE *fp, char *s)
{
    int c;

    while ((c = *s++) != 0) {
	switch (c) {

            case '&':
                fputs("&amp;", fp);
		break;

            case '<':
                fputs("&lt;", fp);
		break;

            case '>':
                fputs("&gt;", fp);
		break;

	    default:
		fputc(c, fp);
		break;
	}
    }
}
