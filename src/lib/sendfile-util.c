/* Copyright (c) 2002-2003 Timo Sirainen */

/* kludge a bit to remove _FILE_OFFSET_BITS definition from config.h.
   It's required to be able to include sys/sendfile.h with Linux. */
#include "../../config.h"
#undef HAVE_CONFIG_H

#ifdef HAVE_LINUX_SENDFILE
#  undef _FILE_OFFSET_BITS
#endif

#include "lib.h"
#include "sendfile-util.h"

#ifdef HAVE_LINUX_SENDFILE

#include <sys/sendfile.h>

ssize_t safe_sendfile(int out_fd, int in_fd, uoff_t *offset, size_t count)
{
	/* REMEBER: uoff_t and off_t may not be of same size. */
	off_t safe_offset;
	ssize_t ret;

	/* make sure given offset fits into off_t */
	if (sizeof(off_t) * CHAR_BIT == 32) {
		/* 32bit off_t */
		if (*offset >= 2147483647L) {
			errno = EINVAL;
			return -1;
		}
		if (count > 2147483647L - *offset)
			count = 2147483647L - *offset;
	} else {
		/* they're most likely the same size. if not, fix this
		   code later */
		i_assert(sizeof(off_t) == sizeof(uoff_t));

		if (*offset >= OFF_T_MAX) {
			errno = EINVAL;
			return -1;
		}
		if (count > OFF_T_MAX - *offset)
			count = OFF_T_MAX - *offset;
	}

	safe_offset = (off_t)*offset;
	ret = sendfile(out_fd, in_fd, &safe_offset, count);
	*offset = (uoff_t)safe_offset;

	return ret;
}

#elif defined(HAVE_FREEBSD_SENDFILE)

#include <sys/socket.h>
#include <sys/uio.h>

ssize_t safe_sendfile(int out_fd, int in_fd, uoff_t *offset, size_t count)
{
	struct sf_hdtr hdtr;
	off_t sbytes;
	int ret;

	i_assert(count <= SSIZE_T_MAX);

	memset(&hdtr, 0, sizeof(hdtr));
	ret = sendfile(in_fd, out_fd, *offset, count, &hdtr, &sbytes, 0);

	*offset += sbytes;

	if (ret == 0 || (ret == 0 && errno == EAGAIN && sbytes > 0))
		return (ssize_t)sbytes;
	else {
		if (errno == ENOTSOCK) {
			/* out_fd wasn't a socket. behave as if sendfile()
			   wasn't supported at all. */
			errno = EINVAL;
		}
		return -1;
	}
}

#elif defined (HAVE_SOLARIS_SENDFILE)

#include <sys/sendfile.h>
#include "network.h"

ssize_t safe_sendfile(int out_fd, int in_fd, uoff_t *offset, size_t count)
{
	ssize_t ret;
	off_t s_offset;

	i_assert(count <= SSIZE_T_MAX);

	/* NOTE: if outfd is not a socket, some Solaris versions will
	   kernel panic */

	s_offset = (off_t)*offset;
	ret = sendfile(out_fd, in_fd, &s_offset, count);
	*offset = (uoff_t)s_offset;

	if (ret < 0 && errno == EAFNOSUPPORT) {
		/* not supported, return Linux-like EINVAL so caller
		   sees only consistent errnos. */
		errno = EINVAL;
	}
	return ret;
}

#else
ssize_t safe_sendfile(int out_fd __attr_unused__, int in_fd __attr_unused__,
		      uoff_t *offset __attr_unused__,
		      size_t count __attr_unused__)
{
	errno = EINVAL;
	return -1;
}

#endif
