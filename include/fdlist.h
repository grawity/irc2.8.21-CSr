#ifndef FDLIST_H
#define FDLIST_H

/*
**  $Id: fdlist.h,v 1.1.1.1 1997/07/23 18:02:02 cbehrens Exp $
*/

typedef struct fdstruct
{
	int entry [MAXCONNECTIONS+2];
	int last_entry;
} fdlist;

void add_to_fdlist( int a, fdlist *b);
void del_from_fdlist( int a, fdlist *b);

#define init_fdlist(__x)  bzero((char *)(__x), sizeof(fdlist));

#endif /* FDLIST_H */
