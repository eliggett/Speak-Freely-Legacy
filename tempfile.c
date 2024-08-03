/*

    	    	Create secure temporary files
	
    	        Adapted from the sample code at:
    
  http://www.dwheeler.com/secure-programs/Secure-Programs-HOWTO/avoid-race.html

    	    	    by John Walker
    
    I added the ability to specify the umask() used to create
    temporary files, control whether the temporary file name is
    immediately unlinked or left for subsequent use (for example,
    from the command line of programs launched or attached
    to pipes), and return the complete path name for temporary
    files generated based on environment variables.

*/


#include "speakfree.h"

/*
    Given a "pattern" for a temporary filename
    (starting with the directory location and ending in XXXXXX),
    create the file and return it.
 
    If the unLink argument is nonzero, the temporary file is
    unlinked so it does not appear in a directory listing.
    
    The pattern will be changed to show the actual filename
    opened.  If a temporary file cannot be created, NULL is
    returned.
    
*/

FILE *create_tempfile(char *temp_filename_pattern, const int uMask, const int unLink)
{
    int temp_fd;
    mode_t old_mode;
    FILE *temp_file;

    old_mode = umask(uMask);  /* Create file with specified permissions */
    temp_fd = mkstemp(temp_filename_pattern);
    (void) umask(old_mode);
    if (temp_fd == -1) {
    	return NULL;
    }
    if (!(temp_file = fdopen(temp_fd, "w+b"))) {
    	return NULL;
    }
    if (unLink && (unlink(temp_filename_pattern) == -1)) {
    	return NULL;
    }
    return temp_file;
}


/*
    Given a "tag" (a relative filename ending in XXXXXX),
    create a temporary file using the tag.  The file will be created
    in the directory specified in the environment variables
    TMPDIR or TMP, if defined and we aren't setuid/setgid, otherwise
    it will be created in /tmp.  Note that root (and su'd to root)
    _will_ use TMPDIR or TMP, if defined.
*/
 
FILE *create_tempfile_in_tempdir(const char *tag, char **genName, const int uMask, const int unLink)
{
    char *tmpdir = NULL;
    char *pattern;
    FILE *result;

    if ((getuid() == geteuid()) && (getgid() == getegid())) {
	if (!((tmpdir = getenv("TMPDIR")))) {
	    tmpdir = getenv("TMP");
	}
    }
    if (!tmpdir) {
    	tmpdir = "/tmp";
    }

    pattern = malloc(strlen(tmpdir) + strlen(tag) + 2);
    if (pattern == NULL) {
      	return NULL;
    }
    strcpy(pattern, tmpdir);
    strcat(pattern, "/");
    strcat(pattern, tag);
    result = create_tempfile(pattern, uMask, unLink);
    if (result == NULL) {
    	free(pattern);
    } else {
    	*genName = pattern;
    }
    return result;
}
