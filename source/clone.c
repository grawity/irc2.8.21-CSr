
#ifndef lint
static  char rcsid[] = "@(#)$Id: clone.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "numeric.h"
#ifdef  DBMALLOC
# include "malloc.h"
#endif


#include "comstud.h"

#ifdef IDLE_CHECK

anIdle	*make_idle()
{
	Reg1	anIdle *ani = NULL;
	Reg2	size = sizeof(anIdle);

	if (!(ani = (anIdle *)MyMalloc(size)))
		outofmemory();
	bzero((char *)ani, (int)size);
	ani->last = NOW;
	ani->prev = NULL;
	if (Idles)
		Idles->prev = ani;
	ani->next = Idles;
	Idles = ani;
	return ani;
}

anIdle *find_idle(cptr)
aClient *cptr;
{
        Reg1 anIdle *ani;
	char *user = cptr->user->username;

	if (*user == '~')
		user++;
        for(ani=Idles;ani;ani=ani->next)
                if (!mycmp(ani->username, user) &&
			(ani->ip.s_addr == cptr->ip.s_addr))
                        return ani;
        return NULL;
}

void remove_idle(cptr)
anIdle *cptr;
{
        if (cptr->prev)
                cptr->prev->next = cptr->next;
        else
        {
                Idles = cptr->next;
                if (Idles)
                        Idles->prev = NULL;
        }
        if (cptr->next)
                cptr->next->prev = cptr->prev;
        MyFree(cptr);
}

void update_idles()
{
        Reg1 anIdle *ani = Idles;
        Reg2 anIdle *temp;

        while (ani != NULL)
        {
                temp = ani->next;
                if (NOW > ani->last)
                        remove_idle(ani);
                ani = temp;
        }
}

#endif /* IDLE_CHECK */

#ifdef CLONE_CHECK
/*
** Create Clone structure
**
*/
aClone *make_clone()
{
        Reg1    aClone *acl = NULL;
        Reg2    size = sizeof(aClone);
        if (!(acl = (aClone *)MyMalloc(size)))
                outofmemory();
        bzero((char *)acl, (int)size);
        acl->last = NOW;
        acl->prev = NULL;
        if (Clones)
                Clones->prev = acl;
        acl->next = Clones;
        Clones = acl;
        return acl;
}

aClone *find_clone(cptr)
aClient *cptr;
{
	Reg1 aClone *acl;

	for(acl=Clones;acl;acl=acl->next)
		if (acl->ip.s_addr == cptr->ip.s_addr)
			return acl;
	return NULL;
}

void remove_clone(cptr)
aClone *cptr;
{
        if (cptr->prev)
                cptr->prev->next = cptr->next;
        else
        {
                Clones = cptr->next;
                if (Clones)
                        Clones->prev = NULL;
        }
        if (cptr->next)
                cptr->next->prev = cptr->prev;
        MyFree(cptr);
}

void update_clones()
{
        Reg1 aClone *acl = Clones;
        Reg2 aClone *temp;

        while (acl != NULL)
        {
                temp = acl->next;
                if ((NOW - acl->last) > CLONE_RESET)
                        remove_clone(acl);
                acl = temp;
        }
}

#endif /* CLONE_CHECK */

