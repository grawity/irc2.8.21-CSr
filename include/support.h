#ifndef SUPPORT_H
#define SUPPORT_H

/*
**  $Id: support.h,v 1.1.1.1 1997/07/23 18:02:02 cbehrens Exp $
*/

#include "setup.h"

#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifndef SYS_ERRLIST_DECLARED
extern char *sys_errlist[];
extern  int     sys_nerr;
#endif

#ifdef NEED_BCOPY
void bcopy(const void *, const void *, size_t);
#else
#if HAVE_MEMCPY
#define bcopy(__x,__y,__z) memmove(__y,__x,__z)
#endif
#endif


