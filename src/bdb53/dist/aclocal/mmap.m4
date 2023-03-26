# Copyright (c) 2011, 2013 Oracle and/or its affiliates.  All rights reserved.

# Detect mmap capability: If the file underlying an mmap is extended,
# does the addressable memory grow too?
AC_DEFUN(AM_MMAP_EXTEND, [

    AH_TEMPLATE(HAVE_MMAP_EXTEND, [Define to 1 where mmap() incrementally extends the accessible mapping as the underlying file grows.])


if test "$mmap_ok" = "yes" ; then 
    AC_MSG_CHECKING([for growing a file under an mmap region])

    db_cv_mmap_extend="no"

    AC_TRY_RUN([
    /*
     *	Most mmap() implemenations allow you to map in a region which is much
     *	larger than the underlying file. Only the second less than the actual
     *	file size is accessible -- a SIGSEV typically results when attemping
     *	a memory reference between EOF and the end of the mapped region.
     *	One can extend the file to allow references into higher-addressed
     *	sections of the region. However this automatic extension of the
     *	addressible memory is beyond what POSIX requires. This function detects
     *	whether mmap supports this automatic extension. If not (e.g. cygwin)
     *	then the entire (hopefully sparse) file will need to be written before
     * 	the first mmap.
     */
    /* Not all these includes are needed, but the minimal set varies from
     * system to system.
     */
    #include <stdio.h>
    #include <string.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <signal.h>

    #define TEST_MMAP_BUFSIZE	(16 * 1024)
    #define TEST_MMAP_EXTENDSIZE	(16 * 1024 * 1024)
    #ifndef MAP_FAILED
    #define MAP_FAILED (-1)
    #endif

    int catch_sig(sig)
	    int sig;
    {
	    exit(1);
    }

    main() {
	    const char *underlying;
	    unsigned gapsize;
	    char *base;
	    int count, fd, i, mode, open_flags, ret, total_size;
	    char buf[TEST_MMAP_BUFSIZE];

	    gapsize = 1024;
	    underlying = ".mmap_config";
	    (void) unlink(underlying);

	    open_flags = O_CREAT | O_TRUNC | O_RDWR;
	    mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;

	    if ((fd = open(underlying, open_flags, mode)) < 0) {
		    perror("open");
		    return (1);
	    }

	    total_size = TEST_MMAP_EXTENDSIZE;

	    memset(buf, 0, sizeof(buf));
	    if ((count = write(fd, buf, sizeof(buf))) != sizeof(buf)) {
		    perror("initial write");
		    return (2);
	    }

	    if ((base = mmap(NULL, total_size,
		PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		    perror("mmap");
		    return (3);
	    }

	    /* Extend the file with just 1 byte */
	    if (lseek(fd, total_size - 1, SEEK_SET) < 0 ||
		(count = write(fd, buf, 1)) != 1) {
		    perror("extending write");
		    return (4);
	    }

	    (void) signal(SIGSEGV, catch_sig);
	    (void) signal(SIGBUS, catch_sig);

	    for (i = sizeof(buf); i < total_size; i += gapsize)
		    base[i] = 'A';

	    close(fd);
	    (void) unlink(underlying);
	    return (0);
    }], [db_cv_mmap_extend="yes"],[db_cv_mmap_extend="no"],[db_cv_mmap_extend="no"])


    if test "$db_cv_mmap_extend" = yes; then
	    AC_DEFINE(HAVE_MMAP_EXTEND)
    fi
    AC_MSG_RESULT($db_cv_mmap_extend)
fi
])

