/************************************************************************
 *   IRC - Internet Relay Chat, ircd/list.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Finland
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

/* -- Jto -- 20 Jun 1990
 * extern void free() fixed as suggested by
 * gruner@informatik.tu-muenchen.de
 */

/* -- Jto -- 03 Jun 1990
 * Added chname initialization...
 */

/* -- Jto -- 24 May 1990
 * Moved is_full() to channel.c
 */

/* -- Jto -- 10 May 1990
 * Added #include <sys.h>
 * Changed memset(xx,0,yy) into bzero(xx,yy)
 */


#ifndef lint
static  char rcsid[] = "@(#)$Id: list.c,v 1.1.1.1 1997/07/23 18:02:04 cbehrens Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "numeric.h"
#ifdef	DBMALLOC
#include "malloc.h"
#endif
void	free_link PROTO((Link *));
Link	*make_link PROTO(());

#ifdef	DEBUGMODE
static	struct	liststats {
	int	inuse;
} cloc, crem, users, servs, links, classs, aconfs;

#endif

void	outofmemory();

int	numclients = 0;

void	initlists()
{
#ifdef	DEBUGMODE
	bzero((char *)&cloc, sizeof(cloc));
	bzero((char *)&crem, sizeof(crem));
	bzero((char *)&users, sizeof(users));
	bzero((char *)&servs, sizeof(servs));
	bzero((char *)&links, sizeof(links));
	bzero((char *)&classs, sizeof(classs));
	bzero((char *)&aconfs, sizeof(aconfs));
#endif
}

void	outofmemory()
{
	Debug((DEBUG_FATAL, "Out of memory: restarting server..."));
	restart("Out of Memory");
}

	
/*
** Create a new aClient structure and set it to initial state.
**
**	from == NULL,	create local client (a client connected
**			to a socket).
**
**	from,	create remote client (behind a socket
**			associated with the client defined by
**			'from'). ('from' is a local client!!).
*/
aClient	*make_client(from)
aClient	*from;
{
	Reg1	aClient *cptr = NULL;
	Reg2	unsigned size = CLIENT_REMOTE_SIZE;

	/*
	 * Check freelists first to see if we can grab a client without
	 * having to call malloc.
	 */
	if (!from)
		size = CLIENT_LOCAL_SIZE;

	if (!(cptr = (aClient *)MyMalloc(size)))
		outofmemory();
	bzero((char *)cptr, (int)size);

#ifdef	DEBUGMODE
	if (size == CLIENT_LOCAL_SIZE)
		cloc.inuse++;
	else
		crem.inuse++;
#endif

	/* Note:  structure is zero (calloc) */
	cptr->from = from ? from : cptr; /* 'from' of local client is self! */
	cptr->next = NULL; /* For machines with NON-ZERO NULL pointers >;) */
	cptr->prev = NULL;
	cptr->hnext = NULL;
	cptr->user = NULL;
	cptr->serv = NULL;
	cptr->lnext = NULL;
	cptr->lprev = NULL;
	cptr->servptr = NULL;
	cptr->whowas = NULL;
	cptr->status = STAT_UNKNOWN;
	cptr->fd = -1;
	cptr->ping = 0;
	(void)strcpy(cptr->username, "unknown");
	if (size == CLIENT_LOCAL_SIZE)
	    {
		cptr->since = cptr->lasttime = cptr->firsttime = NOW;
#ifdef NO_NICK_FLOODS
		cptr->lastnick = 0;
		cptr->numnicks = 0;
#endif
		cptr->confs = NULL;
		cptr->sockhost[0] = '\0';
		cptr->ipstr[0] = '\0';
		cptr->buffer[0] = '\0';
		cptr->authfd = -1;
	    }
	return (cptr);
}

void	free_client(cptr)
aClient	*cptr;
{
	MyFree((char *)cptr);
}

/*
** 'make_user' add's an User information block to a client
** if it was not previously allocated.
*/
anUser	*make_user(cptr)
aClient *cptr;
{
	Reg1	anUser	*user;

	user = cptr->user;
	if (!user)
	    {
		user = (anUser *)MyMalloc(sizeof(anUser));
#ifdef	DEBUGMODE
		users.inuse++;
#endif
		user->away = NULL;
		user->refcnt = 1;
		user->joined = 0;
		user->channel = NULL;
		user->invited = NULL;
		user->last = NOW;
		user->last2 = NOW;
		cptr->user = user;
	    }
	return user;
}

aServer	*make_server(cptr)
aClient	*cptr;
{
	Reg1	aServer	*serv = cptr->serv;

	if (!serv)
	    {
		serv = (aServer *)MyMalloc(sizeof(aServer));
#ifdef	DEBUGMODE
		servs.inuse++;
#endif
		serv->user = NULL;
		serv->nexts = NULL;
		serv->users = NULL;
		serv->servers = NULL;
		*serv->by = '\0';
		*serv->up = '\0';
		cptr->serv = serv;
	    }
	return cptr->serv;
}

/*
** free_user
**	Decrease user reference count by one and realease block,
**	if count reaches 0
*/
void	free_user(user, cptr)
Reg1	anUser	*user;
aClient	*cptr;
{
	if (--user->refcnt <= 0)
	    {
		if (user->away)
			MyFree((char *)user->away);
		/*
		 * sanity check
		 */
		if (user->joined || user->refcnt < 0 ||
		    user->invited || user->channel)
#ifdef DEBUGMODE
			dumpcore("%#x user (%s!%s@%s) %#x %#x %#x %d %d",
				cptr, cptr ? cptr->name : "<noname>",
				user->username, user->host, user,
				user->invited, user->channel, user->joined,
				user->refcnt);
#else
			sendto_ops("* %#x user (%s!%s@%s) %#x %#x %#x %d %d *",
				cptr, cptr ? cptr->name : "<noname>",
				user->username, user->host, user,
				user->invited, user->channel, user->joined,
				user->refcnt);
#endif
		MyFree((char *)user);
#ifdef	DEBUGMODE
		users.inuse--;
#endif
	    }
}

/*
 * taken the code from ExitOneClient() for this and placed it here.
 * - avalon
 */
void	remove_client_from_list(cptr)
Reg1	aClient	*cptr;
{
	if (IsServer(cptr))
		s_count--;
	if (IsClient(cptr))
	{
		if (IsInvisible(cptr))
			i_count--;
		c_count--;
		if (IsOper(cptr))
			o_count--;
	}
	checklist();
	if (cptr->prev)
		cptr->prev->next = cptr->next;
	else
	    {
		client = cptr->next;
		client->prev = NULL;
	    }
	if (cptr->next)
		cptr->next->prev = cptr->prev;
	if (IsPerson(cptr) && cptr->user)
	    {
		add_history(cptr, 0);
		off_history(cptr);
	    }
	if (cptr->user)
		(void)free_user(cptr->user, cptr);
	if (cptr->serv)
	    {
		if (cptr->serv->user)
			free_user(cptr->serv->user, cptr);
		MyFree((char *)cptr->serv);
#ifdef	DEBUGMODE
		servs.inuse--;
#endif
	    }
#ifdef	DEBUGMODE
	if (cptr->fd == -2)
		cloc.inuse--;
	else
		crem.inuse--;
#endif
	(void)free_client(cptr);
	numclients--;
	return;
}

/*
 * although only a small routine, it appears in a number of places
 * as a collection of a few lines...functions like this *should* be
 * in this file, shouldnt they ?  after all, this is list.c, isnt it ?
 * -avalon
 */
void	add_client_to_list(cptr)
aClient	*cptr;
{
	/*
	 * since we always insert new clients to the top of the list,
	 * this should mean the "me" is the bottom most item in the list.
	 */
	cptr->next = client;
	client = cptr;
	if (cptr->next)
		cptr->next->prev = cptr;
	return;
}

/*
 * Look for ptr in the linked listed pointed to by link.
 */
Link	*find_user_link(lp, ptr)
Reg1	Link	*lp;
Reg2	aClient *ptr;
{
	if (ptr)
		for(;lp;lp=lp->next)
			if (lp->value.cptr == ptr)
				return (lp);
	return NULL;
}


Link *find_channel_link(lp, chptr)
Reg1	Link	*lp;
Reg2	aChannel *chptr;
{
	if (chptr)
		for(;lp;lp=lp->next)
			if (lp->value.chptr == chptr)
				return lp;
	return NULL;
}

Link	*make_link()
{
	Reg1	Link	*lp;

	lp = (Link *)MyMalloc(sizeof(Link));
#ifdef	DEBUGMODE
	links.inuse++;
#endif
	return lp;
}

void	free_link(lp)
Reg1	Link	*lp;
{
	MyFree((char *)lp);
#ifdef	DEBUGMODE
	links.inuse--;
#endif
}


aClass	*make_class()
{
	Reg1	aClass	*tmp;

	tmp = (aClass *)MyMalloc(sizeof(aClass));
#ifdef	DEBUGMODE
	classs.inuse++;
#endif
	return tmp;
}

void	free_class(tmp)
Reg1	aClass	*tmp;
{
	MyFree((char *)tmp);
#ifdef	DEBUGMODE
	classs.inuse--;
#endif
}

aConfItem	*make_conf()
{
	Reg1	aConfItem *aconf;

	aconf = (struct ConfItem *)MyMalloc(sizeof(aConfItem));
#ifdef	DEBUGMODE
	aconfs.inuse++;
#endif
	bzero((char *)&aconf->ipnum, sizeof(struct in_addr));
	aconf->next = NULL;
	aconf->host = aconf->passwd = aconf->name = NULL;
	aconf->status = CONF_ILLEGAL;
	aconf->clients = 0;
	aconf->port = 0;
	aconf->hold = 0;
	aconf->flags = 0;
	aconf->expires = 0;
	Class(aconf) = 0;
	return (aconf);
}

void	delist_conf(aconf)
aConfItem	*aconf;
{
	if (aconf == conf)
		conf = conf->next;
	else
	    {
		aConfItem	*bconf;

		for (bconf = conf; aconf != bconf->next; bconf = bconf->next)
			;
		bconf->next = aconf->next;
	    }
	aconf->next = NULL;
}

void	free_conf(aconf)
aConfItem *aconf;
{
	del_queries((char *)aconf);
	MyFree(aconf->host);
	if (aconf->passwd)
		bzero(aconf->passwd, strlen(aconf->passwd));
	MyFree(aconf->passwd);
	MyFree(aconf->name);
	MyFree((char *)aconf);
#ifdef	DEBUGMODE
	aconfs.inuse--;
#endif
	return;
}

#ifdef	DEBUGMODE
void	send_listinfo(cptr, name)
aClient	*cptr;
char	*name;
{
	int	inuse = 0, mem = 0, tmp = 0;

	sendto_one(cptr, ":%s %d %s :Local: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, inuse += cloc.inuse,
		   tmp = cloc.inuse * CLIENT_LOCAL_SIZE);
	mem += tmp;
	sendto_one(cptr, ":%s %d %s :Remote: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name,
		   crem.inuse, tmp = crem.inuse * CLIENT_REMOTE_SIZE);
	mem += tmp;
	inuse += crem.inuse;
	sendto_one(cptr, ":%s %d %s :Users: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, users.inuse,
		   tmp = users.inuse * sizeof(anUser));
	mem += tmp;
	inuse += users.inuse,
	sendto_one(cptr, ":%s %d %s :Servs: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, servs.inuse,
		   tmp = servs.inuse * sizeof(aServer));
	mem += tmp;
	inuse += servs.inuse,
	sendto_one(cptr, ":%s %d %s :Links: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, links.inuse,
		   tmp = links.inuse * sizeof(Link));
	mem += tmp;
	inuse += links.inuse,
	sendto_one(cptr, ":%s %d %s :Classes: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, classs.inuse,
		   tmp = classs.inuse * sizeof(aClass));
	mem += tmp;
	inuse += classs.inuse,
	sendto_one(cptr, ":%s %d %s :Confs: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, aconfs.inuse,
		   tmp = aconfs.inuse * sizeof(aConfItem));
	mem += tmp;
	inuse += aconfs.inuse,
	sendto_one(cptr, ":%s %d %s :Totals: inuse %d %d",
		   me.name, RPL_STATSDEBUG, name, inuse, mem);
}
#endif

void add_whowas_to_clist(bucket, whowas)
aWhowas **bucket;
aWhowas *whowas;
{
	whowas->cprev = NULL;
	if ((whowas->cnext = *bucket) != NULL)
		whowas->cnext->cprev = whowas;
	*bucket = whowas;
}

void    del_whowas_from_clist(bucket, whowas)
aWhowas **bucket;
aWhowas *whowas;
{
	if (whowas->cprev)
		whowas->cprev->cnext = whowas->cnext;
	else
		*bucket = whowas->cnext;
	if (whowas->cnext)
		whowas->cnext->cprev = whowas->cprev;
}

void add_whowas_to_list(bucket, whowas)
aWhowas **bucket;
aWhowas *whowas;
{
	whowas->prev = NULL;
	if ((whowas->next = *bucket) != NULL)
		whowas->next->prev = whowas;
	*bucket = whowas;
}

void    del_whowas_from_list(bucket, whowas)
aWhowas **bucket;
aWhowas *whowas;
{
	if (whowas->prev)
		whowas->prev->next = whowas->next;
	else
		*bucket = whowas->next;
	if (whowas->next)
		whowas->next->prev = whowas->prev;
}

void add_client_to_llist(bucket, client)
aClient **bucket;
aClient *client;
{
	client->lprev = NULL;
	if ((client->lnext = *bucket) != NULL)
		client->lnext->lprev = client;
	*bucket = client;
}

void    del_client_from_llist(bucket, client)
aClient **bucket;
aClient *client;
{
	if (client->lprev)
		client->lprev->lnext = client->lnext;
	else
		*bucket = client->lnext;
	if (client->lnext)
		client->lnext->lprev = client->lprev;
}

void del_stuff(cptr)
aClient *cptr;
{
	aClient **userbucket, **servbucket;
	aClient *tempuser, *tempserv;
	aClient *nextuser, *nextserv;

	servbucket = &(cptr->serv->servers);
	userbucket = &(cptr->serv->users);
	tempserv = *servbucket;
	while(tempserv)
	{
		nextserv = tempserv->lnext;
		del_stuff(tempserv);
		del_client_from_llist(servbucket, tempserv);
		tempserv->servptr = NULL;
		tempserv = nextserv;
	}
	tempuser = *userbucket;
	while(tempuser)
	{
		nextuser = tempuser->lnext;
		del_client_from_llist(userbucket, tempuser);
		tempuser->servptr = NULL;
		tempuser = nextuser;
	}
	*servbucket = NULL;
	*userbucket = NULL;
}

