/* fdlist.c   maintain lists of certain important fds */

#ifndef lint
static  char rcsid[] = "@(#)$Id: fdlist.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $";
#endif

#include "config.h"
#include "fdlist.h"

void add_to_fdlist(fd, listp)
int fd;
fdlist *listp;
{
	register int index;
	if ( (index = ++listp->last_entry) >= MAXCONNECTIONS)
	{
		/* list too big.. must exit */
		--listp->last_entry;
		exit(-1);
	}
	else
		listp->entry[index] = fd;
	return;
}

void del_from_fdlist(fd,listp)
int fd;
fdlist *listp;
{
	register int i;

	for (i=listp->last_entry; i ; i--)
	{
		if (listp->entry[i]==fd)
			break;
	}
	if (!i)
		return; /* could not find it! */
	/* swap with last_entry */
	if (i==listp->last_entry)
	{
		listp->entry[i] =0;
		listp->last_entry--;
		return;
	}
	listp->entry[i] = listp->entry[listp->last_entry];
	listp->entry[listp->last_entry] = 0;
	listp->last_entry--;
}

