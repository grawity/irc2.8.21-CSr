#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "numeric.h"
#ifdef  DBMALLOC
# include "malloc.h"
#endif

#include "comstud.h"

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
	*acl->hostname = (char) 0;
        acl->num = 0;
        acl->last = NOW;
        acl->prev = NULL;
        if (Clones)
                Clones->prev = acl;
        acl->next = Clones;
        Clones = acl;
        return acl;
}

aClone *find_clone(host)
char *host;
{
        Reg1 aClone *acl;

        for(acl=Clones;acl;acl=acl->next)
                if (!mycmp(acl->hostname, host))
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
        time_t old;

        old = NOW;
        while (acl != NULL)
        {
                temp = acl->next;
                if ((old - acl->last) > CLONE_RESET)
                        remove_clone(acl);
                acl = temp;
        }
}

#endif /* CLONE_CHECK */

