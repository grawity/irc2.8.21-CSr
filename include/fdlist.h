#ifndef _IRCD_DOG3_FDLIST
#define _IRCD_DOG3_FDLIST

typedef struct fdstruct {
  int entry [MAXCONNECTIONS+2];
  int last_entry;
} fdlist;
/*
void addto_fdlist( int a, fdlisttype *b);
void delfrom_fdlist( int a, fdlisttype *b);
void init_fdlist(fdlisttype *b);
*/
#endif /* _IRCD_DOG3_FDLIST */
