/************************************************************************
 *   IRC - Internet Relay Chat, include/sys.h
 *   Copyright (C) 1990 University of Oulu, Computing Center
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/*
**  $Id: sys.h,v 1.3 1998/01/09 07:34:49 cbehrens Exp $
*/

#ifndef	__sys_include__
#define __sys_include__

#include "setup.h"

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <pwd.h>

#include "zlib.h"

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#ifdef USE_POLL
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif
#ifdef HAVE_POLL_H
#include <poll.h>
#else
#include <sys/poll.h>
#endif
#define pollfd_t struct pollfd
#endif /* USE_POLL_ */

#ifdef HAVE_ERRNO_H
# include <errno.h>
#else
# ifdef HAVE_SYS_ERRNO_H
#  include <sys/errno.h>
# else
#  ifdef HAVE_NET_ERRNO_H
#   include <net/errno.h>
#  endif
# endif
#endif

#ifdef HAVE_SYS_CDEFS_H
# include <sys/cdefs.h>
#else
# include "cdefs.h"
#endif
#ifdef HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#else
# if (!defined(BSD)) || (BSD < 199306)
# ifdef NEED_BITYPES_
#  include "bitypes.h"
# endif
# endif
#endif

#ifdef	HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef	HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef	HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif


#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#define	strcasecmp	mycmp
#define	strncasecmp	myncmp

#ifdef NEED_INDEX
#define   index   strchr
#define   rindex  strrchr
#else
#if !defined(HAVE_STRINGS_H) && !defined(HAVE_STRING_H)
extern  char    *index __P((char *, char));
extern  char    *rindex __P((char *, char));
#endif
#endif

extern	RETSIGTYPE	dummy();

#ifdef	DYNIXPTX
#define	NO_U_TYPES
#endif

#ifdef	NO_U_TYPES
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned long	u_long;
typedef	unsigned int	u_int;
#endif

#ifdef	USE_VARARGS
#include <varargs.h>
#endif

#ifndef SYS_ERRLIST_DECLARED
extern char *sys_errlist[];
extern  int     sys_nerr;
#endif

#ifdef NEED_BCOPY
void bcopy(const void *, const void *, size_t);
#else
#ifdef HAVE_MEMMOVE
#define bcopy(__x,__y,__z) memmove(__y,__x,__z)
#endif
#endif

#ifdef NEED_BCMP
int bcmp(const void *s1, const void *s2, size_t n);
#endif

#ifdef NEED_BZERO
void bzero(void *s, size_t n);
#endif

#ifdef NEED_DNSKIPNAME
#define dn_skipname __dn_skipname
#endif

#if defined(__hpux)
# include "inet.h"
#endif

#endif /* __sys_include__ */
