/************************************************************************
 *   IRC - Internet Relay Chat, source/gline.c
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

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "h.h"

#ifndef lint
static  char rcsid[] = "@(#)$Id: gline.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $";
#endif

#ifdef G_LINES

static void handle_rules(aRule **, char **);

static int do_fromoper(char *, char **);
static int do_fromserver(char *, char **);
static int do_foruser(char *, char **);
static int do_lifetime(char *, char **);

typedef struct _glinecfgcom
{
	char    *name;
	int     (*function)(char *, char **);
} aGlineCfgCom;

aGlineCfgCom ConfCommands[] =
{
	{ "FROMOPER",	do_fromoper },
	{ "FROMSERVER",	do_fromserver },
	{ "FORUSER",	do_foruser },
	{ "LIFETIME",	do_lifetime },
	{ NULL }
};

void clear_glineconf()
{
	remove_rules(&(GlineConf.fromserver));
	remove_rules(&(GlineConf.fromoper));
	remove_rules(&(GlineConf.foruser));
	GlineConf.seconds = 60*10;
}

void report_gline_rules(cptr)
aClient *cptr;
{
	int i;
	aRule *rules;
	anExcept *except;

	for(i=0;i<3;i++)
	{
		switch(i)
		{
			case 0:
				sendto_one(cptr,
					":%s %d %s :Gline fromoper rules",
					me.name, RPL_STATSDEBUG, cptr->name);
				rules = GlineConf.fromoper;
				break;
			case 1:
				sendto_one(cptr,
					":%s %d %s :Gline fromserver rules",
					me.name, RPL_STATSDEBUG, cptr->name);
				rules = GlineConf.fromserver;
				break;
			case 2:
				sendto_one(cptr,
					":%s %d %s :Gline foruser rules",
					me.name, RPL_STATSDEBUG, cptr->name);
				rules = GlineConf.foruser;
				break;
		}
		for(;rules;rules=rules->next)
		{
			if (!rules->string) /* what?? */
				continue;
			if (IsAllow(rules))
				sendto_one(cptr,
					":%s %d %s :  ALLOW %s",
					me.name, RPL_STATSDEBUG, cptr->name,
					rules->string);
			else
				sendto_one(cptr,
					":%s %d %s :  DENY  %s",
					me.name, RPL_STATSDEBUG, cptr->name,
					rules->string);
			for(except = rules->excepts;except;except=except->next)
				sendto_one(cptr,
					":%s %d %s :    EXCEPT %s",
					me.name, RPL_STATSDEBUG, cptr->name,
					except->string);
		}
	}
}

int init_glineconf(filename)
char *filename;
{
	char *stuff, *ptr, *line, *command;
	aGlineCfgCom *cp;

	if ((stuff = loadfile(filename)) == NULL)
		return -1;
	clear_glineconf();
	ptr = stuff;
	do
	{
		line = ptr;
		if ((ptr = strchr(ptr, '\n')) != NULL)
		{
			*ptr = (char) 0;
			ptr++;
		}
		if (*line == '#')
			continue;
		if ((command = strchr(line, '\r')) != NULL)
			*command = (char) 0;
		command = get_token(&line, ":= \t");
		if (!command)
			continue;
		for(cp=ConfCommands;cp->name;cp++)
			if (!mycmp(cp->name, command))
				break;
		if (!cp->name)
			continue;
		(void)cp->function(line && *line ? line : NULL, &ptr);
	} while(ptr);
	MyFree(ptr);
	return 0;
}

static int do_fromserver(args, stuff)
char *args;
char **stuff;
{
	handle_rules(&(GlineConf.fromserver), stuff);
	return 0;
}

static int do_fromoper(args, stuff)
char *args;
char **stuff;
{
	handle_rules(&(GlineConf.fromoper), stuff);
	return 0;
}

static int do_foruser(args, stuff)
char *args;
char **stuff;
{
	handle_rules(&(GlineConf.foruser), stuff);
	return 0;
}

static int do_lifetime(args, stuff)
char *args;
char **stuff;
{
	int blah;

	if (!args)
		return -1;
	blah = atoi(args);
	if (blah < 0)
		return -1;
	GlineConf.seconds = (time_t)blah;
	return 0;
}

static void handle_rules(gconf, stuff)
aRule **gconf;
char **stuff;
{
	char *ptr = *stuff;
	char *command;
	char *line;
	int flags;
	aRule *rule = NULL;
	anExcept *except = NULL;

	if (!ptr)
		return;
	do
	{
		line = ptr;
		if ((ptr = strchr(ptr, '\n')) != NULL)
		{
			*ptr = (char) 0;
			ptr++;
		}
		if (*line == '#')
			continue;
		if ((command = strchr(line, '\r')) != NULL)
			*command = (char) 0;
		command = get_token(&line, ":= \t");
		if (!command)
			continue;
		flags = 0;
		if (!mycmp(command, "EXCEPT") && rule && line && *line)
		{
			except = (anExcept *)MyMalloc(sizeof(anExcept));
			except->next = rule->excepts;
			rule->excepts = except;
			mstrcpy(&except->string, line);
		}
		else if (!mycmp(command, "ALLOW") && line && *line)
		{
			rule = (aRule *)MyMalloc(sizeof(aRule));
			mstrcpy(&(rule->string), line);
			rule->next = (*gconf);
			rule->flags = FLAGS_ALLOW;
			rule->excepts = NULL;
			(*gconf) = rule;
		}
		else if (!mycmp(command, "DENY") && line && *line)
		{
			rule = (aRule *)MyMalloc(sizeof(aRule));
			mstrcpy(&(rule->string), line);
			rule->next = (*gconf);
			rule->flags = FLAGS_DENY;
			rule->excepts = NULL;
			(*gconf) = rule;
		}
		else if (!mycmp(command, "END"))
			break;
	} while(ptr);
	*stuff = ptr;
	return;
}

int     m_clearglines(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{
	if (check_registered_user(sptr))
		return 0;
	if (!MyConnect(sptr))
		return 0;
	if (!IsOper(sptr) || IsServer(sptr)) /* local opers can't gline */
	{
		if (!IsServer(sptr))
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES),
				me.name, parv[0]);
		return 0;
	}
	clear_dichconf(Glines, 1);
	sendto_flagops(UFLAGS_OPERS, "%s cleared G-lines", parv[0]);
	sendto_one(sptr, ":%s NOTICE %s :G-lines have been cleared",
			me.name, parv[0]);
	return 0;
}

int	m_glinelifetime(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	char *asec = parc > 1 ? parv[1] : NULL;
	time_t seconds;

	if (check_registered_user(sptr))
		return 0;
	if (!MyConnect(sptr))
		return 0;
	if (!IsOper(sptr) || IsServer(sptr)) /* local opers can't gline */
	{
		if (!IsServer(sptr))
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES),
				me.name, parv[0]);
		return 0;
	}
	if (!asec || !*asec)
	{
		seconds = GlineConf.seconds;
		sendto_one(sptr,
		":%s NOTICE %s :Current G-line lifetime is %li second%s",
			me.name, parv[0], seconds, seconds == 1 ? "" : "s");
		return 0;
	}
	seconds = atol(asec);
	if (seconds < 0)
	{
		sendto_one(sptr,
			":%s NOTICE %s :Lifetime must be greater than 0.",
			me.name, parv[0]);
		return 0;
	}
	if (seconds == GlineConf.seconds)
	{
		sendto_one(sptr,
			":%s NOTICE %s :Lifetime was not changed.",
			me.name, parv[0]);
		return 0;
	}
	GlineConf.seconds = seconds;
	sendto_flagops(UFLAGS_OPERS,
			"%s changed G-line lifetime to %i second%s",
			parv[0], seconds, seconds == 1 ? "" : "s");
	sendto_one(sptr, ":%s NOTICE %s :Lifetime was changed to %i second%s",
			me.name, parv[0], seconds, seconds == 1 ? "" : "s");
	return 0;
}

#endif /* G_LINES */
