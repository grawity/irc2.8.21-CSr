/************************************************************************
 *   IRC - Internet Relay Chat, source/dog3.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *		      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
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

#ifndef lint
static  char rcsid[] = "@(#)$Id: dog3.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $";
#endif


#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "h.h"

#ifdef DOG3

extern float	currentrate;
extern int	dog3loadrecv;
extern int	dog3loadcfreq;


int     m_htm(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
	sendto_one(sptr, ":%s NOTICE %s :The incoming rate is %0.2f kb/s",
		me.name, parv[0], currentrate);
	return 0;
}

int     m_dog3freq(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{
	int temp;

	if (!MyClient(sptr) || !IsAnOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (!parv[1] || !*parv[1])
	{
		sendto_one(sptr, ":%s NOTICE %s :The current load check frequency is set at %i seconds",
			me.name, parv[0], dog3loadcfreq);
		return 0;
	}
	temp = atoi(parv[1]);
	if (temp <= 0)
	{
		sendto_one(sptr, ":%s NOTICE %s :Invalid number.",
			me.name, parv[0]);
		return 0;
	}
	dog3loadcfreq = temp;
	sendto_ops("%s has changed the load check frequency to %i second(s).",
		parv[0], dog3loadcfreq);
	sendto_one(sptr, ":%s NOTICE %s :The load check frequency is now set to %i second(s)",
		me.name, parv[0], dog3loadcfreq);
	return 0;
}

int     m_dog3load(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{
	int temp;

	if (!MyClient(sptr) || !IsAnOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (!parv[1] || !*parv[1])
	{
		sendto_one(sptr, ":%s NOTICE %s :The current load limit is set at %i kb",
			me.name, parv[0], dog3loadrecv);
		return 0;
	}
	temp = atoi(parv[1]);
	if (temp < 0)
	{
		sendto_one(sptr, ":%s NOTICE %s :Invalid number.",
			me.name, parv[0]);
		return 0;
	}
	dog3loadrecv = temp;
	sendto_ops("%s has changed the load limit to %i kb.",
		parv[0], dog3loadrecv);
	sendto_one(sptr, ":%s NOTICE %s :The load limit is now set to %i kb",
		me.name, parv[0], dog3loadrecv);
	return 0;
}

#endif /* DOG3 */

