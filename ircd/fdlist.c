/* fdlist.c   maintain lists of certain important fds */

#include "config.h"
#include "fdlist.h"

#ifdef DOG3

void addto_fdlist(fd,listp)
     int fd;
     fdlist *listp;
{
  register int index;
  if ( (index = ++listp->last_entry) >= MAXCONNECTIONS) {
    /* list too big.. must exit */
    --listp->last_entry;
    exit(-1);
  }
  else
    listp->entry[index] = fd;
  return;
}

void delfrom_fdlist(fd,listp)
     int fd;
     fdlist *listp;
{
  register int i;
  for (i=listp->last_entry; i ; i--) {
    if (listp->entry[i]==fd) break;
  }
  if (!i) return; /* could not find it! */
  /* swap with last_entry */
  if (i==listp->last_entry) {
    listp->entry[i] =0;
    listp->last_entry--;
    return;
  }
  else {
    listp->entry[i] = listp->entry[listp->last_entry];
    listp->entry[listp->last_entry] = 0;
    listp->last_entry--;
    return;
  }
}

void init_fdlist(listp)
     fdlist *listp;
{
  register int i;
  listp->last_entry=0;
  for (i=0; i<MAXCONNECTIONS+1; i++) 
    listp->entry[i] = 0;
  return;
}

#endif  
