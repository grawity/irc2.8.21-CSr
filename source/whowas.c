/************************************************************************
*   IRC - Internet Relay Chat, ircd/whowas.c
*   Copyright (C) 1990 Markku Savela
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
* --- avalon --- 6th April 1992
* rewritten to scrap linked lists and use a table of structures which
* is referenced like a circular loop. Should be faster and more efficient.
*/

/*
* --- comstud --- 25th March 1997
* Everything rewritten from scratch.  Avalon's code was bad.  My version
* is faster and more efficient.  No more hangs on /squits and you can
* safely raise NICKNAMEHISTORYLENGTH to a higher value without hurting
* performance
*/

#ifndef lint
static  char rcsid[] = "@(#)$Id: whowas.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "h.h"
 
aWhowas WHOWAS[NICKNAMEHISTORYLENGTH];
aWhowas *WHOWASHASH[WW_MAX];

int whowas_next = 0;

void add_history(cptr, online)
aClient *cptr;
int online;
{
	aWhowas *new;

	new = &WHOWAS[whowas_next];
	if (new->hashv != -1)
	{
		if (new->online)
			del_whowas_from_clist(&(new->online->whowas),
				new);
		del_whowas_from_list(&WHOWASHASH[new->hashv], new);
		if (new->name)
			MyFree(new->name);
		if (new->username)
			MyFree(new->username);
		if (new->servername)
			MyFree(new->servername);
		if (new->hostname)
			MyFree(new->hostname);
		if (new->realname)
			MyFree(new->realname);
		if (new->away)
			MyFree(new->away);
	}
	new->hashv = hash_whowas_name(cptr->name);
	new->logoff = NOW;
	mstrcpy(&new->name, cptr->name);
	mstrcpy(&new->username, cptr->user->username);
	mstrcpy(&new->hostname, cptr->user->host);
	mstrcpy(&new->realname, cptr->info);
	mstrcpy(&new->servername, cptr->user->server);
#if 0
	if (cptr->user->away)
		mstrcpy(&new->away, cptr->user->away);
	else
#endif
		new->away = NULL;
	if (online)
	{
		new->online = cptr;
		add_whowas_to_clist(&(cptr->whowas), new);
	}
	else
		new->online = NULL;
	add_whowas_to_list(&WHOWASHASH[new->hashv], new);
	whowas_next++;
	if (whowas_next == NICKNAMEHISTORYLENGTH)
		whowas_next = 0;
}

void off_history(cptr)
aClient *cptr;
{
	aWhowas *temp, *next;

	for(temp=cptr->whowas;temp;temp=next)
	{
		next = temp->cnext;
		temp->online = NULL;
		del_whowas_from_clist(&(cptr->whowas), temp);
	}
}

aClient *get_history(nick, timelimit)
char    *nick;
time_t  timelimit;
{
	aWhowas *temp;
	int blah;

	timelimit = NOW - timelimit;
	blah = hash_whowas_name(nick);
	temp = WHOWASHASH[blah];
	for(;temp;temp=temp->next)
	{
		if (mycmp(nick, temp->name))
			continue;
		if (temp->logoff < timelimit)
			continue;
		return temp->online;
	}
	return NULL;
}

void    count_whowas_memory(wwu, wwum, wwa, wwam)
int     *wwu, *wwa;
u_long  *wwum, *wwam;
{
	register aWhowas *tmp;
	register int i;
        int     u = 0, a = 0;
        u_long  um = 0, am = 0;

        for (i = 0, tmp = &WHOWAS[0]; i < NICKNAMEHISTORYLENGTH; i++, tmp++)
		if (tmp->hashv != -1)
		{
			u++;
			um += (strlen(tmp->name) + 1);
			um += (strlen(tmp->username) + 1);
			um += (strlen(tmp->hostname) + 1);
			um += (strlen(tmp->servername) + 1);
			if (tmp->away)
			{
				a++;
				am += (strlen(tmp->away) + 1);
			}
		}
        *wwu = u;
	*wwum = um;
        *wwa = a;
        *wwam = am;

        return;
}
/*
** m_whowas
**      parv[0] = sender prefix
**      parv[1] = nickname queried
*/
int     m_whowas(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{
	register aWhowas *temp;
	register int cur = 0;
        int     max = -1, found = 0;
        char    *p, *nick, *s;

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
			me.name, parv[0]);
		return 0;
	}
	if (parc > 2)
		max = atoi(parv[2]);
	if (parc > 3)
		if (hunt_server(cptr,sptr,":%s WHOWAS %s %s :%s", 3,parc,parv))
			return 0;

	parv[1] = canonize(parv[1], NULL);
	if (!MyConnect(sptr) && (max > 20))
		max = 20;
	for (s = parv[1]; (nick = strtoken(&p, s, ",")); s = NULL)
	{
		temp = WHOWASHASH[hash_whowas_name(nick)];
		found = 0;
		for(;temp;temp=temp->next)
		{
			if (!mycmp(nick, temp->name))
			{
                                sendto_one(sptr, rpl_str(RPL_WHOWASUSER),
                                           me.name, parv[0], temp->name,
					temp->username,
					temp->hostname,
					temp->realname);
                                sendto_one(sptr, rpl_str(RPL_WHOISSERVER),
                                           me.name, parv[0], temp->name,
                                           temp->servername, myctime(temp->logoff));
                                if (temp->away)
                                        sendto_one(sptr, rpl_str(RPL_AWAY),
                                                   me.name, parv[0],
						temp->name, temp->away);
				cur++;
				found++;
			}
                        if (max > 0 && cur >= max)
                                break;
		}
		if (!found)
                        sendto_one(sptr, err_str(ERR_WASNOSUCHNICK),
                                   me.name, parv[0], nick);
                if (p)
                        p[-1] = ',';
	}
        sendto_one(sptr, rpl_str(RPL_ENDOFWHOWAS), me.name, parv[0], parv[1]);
        return 0;
}

void    initwhowas()
{
	register int i;

	for (i=0;i<NICKNAMEHISTORYLENGTH;i++)
	{
		bzero((char *)&WHOWAS[i], sizeof(aWhowas));
		WHOWAS[i].hashv = -1;
	}
	for (i=0;i<WW_MAX;i++)
		WHOWASHASH[i] = NULL;        
}

