/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_misc.c (formerly ircd/date.c)
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
static  char rcsid[] = "@(#)$Id: s_misc.c,v 1.2 1997/07/29 19:49:04 cbehrens Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "h.h"

static	void	exit_one_client PROTO((aClient *,aClient *,aClient *,char *));

static	char	*months[] = {
	"January",	"February",	"March",	"April",
	"May",	        "June",	        "July",	        "August",
	"September",	"October",	"November",	"December"
};

static	char	*weekdays[] = {
	"Sunday",	"Monday",	"Tuesday",	"Wednesday",
	"Thursday",	"Friday",	"Saturday"
};

#define ULOGBUFFERLEN 4000
#define CLOGBUFFERLEN 2000

static char userbuf[ULOGBUFFERLEN] = "\0";
static char clonebuf[CLOGBUFFERLEN] = "\0";
static int userbuflen = 0, clonebuflen = 0;

/*
 * stats stuff
 */
struct	stats	ircst, *ircstp = &ircst;


/*
** only allow FLOOD_MAX requests in FLOOD_WAIT seconds.
** and when you hit that max, wait FLOOD_WAIT seconds before
** another request is allowed.
*/
int flood_check(aClient *sptr, time_t thetime)
{
#if defined(FLOOD_MAX) && defined(FLOOD_WAIT)
	static int		num = 0;
	static time_t	starttime = 0;

	if (!starttime || ((starttime + FLOOD_WAIT) < thetime))
	{
		starttime = thetime;
		num = 1;
		return 0;
	}
	if (++num <= FLOOD_MAX)
		return 0;
	if (num == (FLOOD_MAX + 1))
		starttime = thetime; /* reset the time to wait.. */
	sendto_one(sptr, rpl_str(RPL_LOAD2HI), me.name, sptr->name);
	return 1;
#else
	return 0;
#endif
}

char	*date(clock) 
time_t	clock;
{
	static	char	buf[80], plus;
	Reg1	struct	tm *lt, *gm;
	struct	tm	gmbuf;
	int	minswest;

	if (!clock) 
		time(&clock);
	gm = gmtime(&clock);
	bcopy((char *)gm, (char *)&gmbuf, sizeof(gmbuf));
	gm = &gmbuf;
	lt = localtime(&clock);

	if (lt->tm_yday == gm->tm_yday)
		minswest = (gm->tm_hour - lt->tm_hour) * 60 +
			   (gm->tm_min - lt->tm_min);
	else if (lt->tm_yday > gm->tm_yday)
		minswest = (gm->tm_hour - (lt->tm_hour + 24)) * 60;
	else
		minswest = ((gm->tm_hour + 24) - lt->tm_hour) * 60;
/*
		minswest = ((lt->tm_hour + 24) - gm->tm_hour) * 60;
*/
	plus = (minswest > 0) ? '-' : '+';
	if (minswest < 0)
		minswest = -minswest;

	(void)irc_sprintf(buf, "%s %s %d %04d -- %02d:%02d %c%02d:%02d",
		weekdays[lt->tm_wday], months[lt->tm_mon],lt->tm_mday,
		lt->tm_year+1900, lt->tm_hour, lt->tm_min,
		plus, minswest/60, minswest%60);

	return buf;
}

/**
 ** myctime()
 **   This is like standard ctime()-function, but it zaps away
 **   the newline from the end of that string. Also, it takes
 **   the time value as parameter, instead of pointer to it.
 **   Note that it is necessary to copy the string to alternate
 **   buffer (who knows how ctime() implements it, maybe it statically
 **   has newline there and never 'refreshes' it -- zapping that
 **   might break things in other places...)
 **
 **/

char	*myctime(value)
time_t	value;
{
	static	char	buf[28];
	Reg1	char	*p;

	(void)strcpy(buf, ctime(&value));
	if ((p = (char *)index(buf, '\n')) != NULL)
		*p = '\0';

	return buf;
}

/*
** check_registered_user is used to cancel message, if the
** originator is a server or not registered yet. In other
** words, passing this test, *MUST* guarantee that the
** sptr->user exists (not checked after this--let there
** be coredumps to catch bugs... this is intentional --msa ;)
**
** There is this nagging feeling... should this NOT_REGISTERED
** error really be sent to remote users? This happening means
** that remote servers have this user registered, althout this
** one has it not... Not really users fault... Perhaps this
** error message should be restricted to local clients and some
** other thing generated for remotes...
*/
int	check_registered_user(sptr)
aClient	*sptr;
{
	if (!IsRegisteredUser(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "*");
		return -1;
	    }
	return 0;
}

/*
** check_registered user cancels message, if 'x' is not
** registered (e.g. we don't know yet whether a server
** or user)
*/
int	check_registered(sptr)
aClient	*sptr;
{
	if (!IsRegistered(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "*");
		return -1;
	    }
	return 0;
}

/*
** get_client_name
**      Return the name of the client for various tracking and
**      admin purposes. The main purpose of this function is to
**      return the "socket host" name of the client, if that
**	differs from the advertised name (other than case).
**	But, this can be used to any client structure.
**
**	Returns:
**	  "name[user@ip#.port]" if 'showip' is true;
**	  "name[sockethost]", if name and sockhost are different and
**	  showip is false; else
**	  "name".
**
** NOTE 1:
**	Watch out the allocation of "nbuf", if either sptr->name
**	or sptr->sockhost gets changed into pointers instead of
**	directly allocated within the structure...
**
** NOTE 2:
**	Function return either a pointer to the structure (sptr) or
**	to internal buffer (nbuf). *NEVER* use the returned pointer
**	to modify what it points!!!
*/
char	*get_client_name(sptr, showip)
aClient *sptr;
int	showip;
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if (MyConnect(sptr))
	{
		if (IsUnixSocket(sptr))
		{
			if (showip)
				(void) irc_sprintf(nbuf, "%s[%s]",
					sptr->name, sptr->sockhost);
			else
				(void) irc_sprintf(nbuf, "%s[%s]",
					sptr->name, me.sockhost);
		}
		else
		{
			if (showip)
				(void)irc_sprintf(nbuf, "%s[%s@%s]",
					sptr->name,
					(!(sptr->flags & FLAGS_GOTID)) ? "" :
					sptr->username,
					sptr->ipstr);
			else
			{
				if (mycmp(sptr->name, sptr->sockhost))
#ifdef USERNAMES_IN_TRACE
					(void)irc_sprintf(nbuf, "%s[%s%s%s]",
						sptr->name,
					sptr->user && *(sptr->user->username) ?
					sptr->user->username : "",
					sptr->user && *(sptr->user->username) ?
						"@" : "",
							sptr->sockhost);
#else
					(void)irc_sprintf(nbuf, "%s[%s]",
						sptr->name,
						sptr->sockhost);
#endif
				else
					return sptr->name;
			}
		}
		return nbuf;
	}
	return sptr->name;
}

char	*get_client_host(cptr)
aClient	*cptr;
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if (!MyConnect(cptr))
		return cptr->name;
	if (!cptr->hostp)
		return get_client_name(cptr, FALSE);
	if (IsUnixSocket(cptr))
		(void) irc_sprintf(nbuf, "%s[%s]", cptr->name, me.name);
	else
		(void)irc_sprintf(nbuf, "%s[%-.*s@%-.*s]",
			cptr->name, USERLEN,
			(!(cptr->flags & FLAGS_GOTID)) ? "" : cptr->username,
			HOSTLEN, cptr->hostp->h_name);
	return nbuf;
}

/*
 * Form sockhost such that if the host is of form user@host, only the host
 * portion is copied.
 */
void	get_sockhost(cptr, host)
Reg1	aClient	*cptr;
Reg2	char	*host;
{
	Reg3	char	*s;
	if ((s = (char *)index(host, '@')))
		s++;
	else
		s = host;
	strncpyzt(cptr->sockhost, s, sizeof(cptr->sockhost));
}

/*
 * Return wildcard name of my server name according to given config entry
 * --Jto
 */
char	*my_name_for_link(name, aconf)
char	*name;
aConfItem *aconf;
{
	static	char	namebuf[HOSTLEN];
	register int	count = aconf->port;
	register char	*start = name;

	if (count <= 0 || count > 5)
		return start;

	while (count-- && name)
	    {
		name++;
		name = (char *)index(name, '.');
	    }
	if (!name)
		return start;

	namebuf[0] = '*';
	(void)strncpy(&namebuf[1], name, HOSTLEN - 1);
	namebuf[HOSTLEN - 1] = '\0';

	return namebuf;
}

#ifdef BUFFERED_LOGS

void cs_buf_logs(which, buf, len)
int which;
char *buf;
int len;
{
	int doit=0;
	char *filename, *buffer;
	int fd, *len2;

	switch(which)
	{
	case 1:
#ifdef FNAME_USERLOG
		if (userbuflen+len >= ULOGBUFFERLEN-1) 
			doit++;
		filename = FNAME_USERLOG;
		buffer = userbuf;
		len2 = &userbuflen;
#else
		return;
#endif
		break;
	case 2:
#ifdef FNAME_CLONELOG
		if (clonebuflen+len >= CLOGBUFFERLEN-1)
			doit++;
		filename = FNAME_CLONELOG;
		buffer = clonebuf;
		len2 = &clonebuflen;
#else
		return;
#endif
		break;
	}
	if (!len || doit)
	{
		if ((fd = open(filename, O_WRONLY|O_APPEND)) != -1)
		{
			(void)write(fd, buffer, *len2);
			close(fd);
		}
		*buffer = (char) 0;
		*len2 = 0;
	}
	if (buf)
		strcat(buffer, buf);
	*len2 = *len2 + len;
}

#endif

/*
** exit_client
**	This is old "m_bye". Name  changed, because this is not a
**	protocol function, but a general server utility function.
**
**	This function exits a client of *any* type (user, server, etc)
**	from this server. Also, this generates all necessary prototol
**	messages that this exit may cause.
**
**   1) If the client is a local client, then this implicitly
**	exits all other clients depending on this connection (e.g.
**	remote clients having 'from'-field that points to this.
**
**   2) If the client is a remote client, then only this is exited.
**
** For convenience, this function returns a suitable value for
** m_funtion return value:
**
**	FLUSH_BUFFER	if (cptr == sptr)
**	0		if (cptr != sptr)
*/
int	exit_client(cptr, sptr, from, comment)
aClient *cptr;	/*
		** The local client originating the exit or NULL, if this
		** exit is generated by this server for internal reasons.
		** This will not get any of the generated messages.
		*/
aClient *sptr;	/* Client exiting */
aClient *from;	/* Client firing off this Exit, never NULL! */
char	*comment;	/* Reason for the exit */
    {
	Reg1	aClient	*acptr;
	Reg2	aClient	*next;
#if defined(FNAME_USERLOG) || (defined(SYSLOG) && defined(SYSLOG_USERS))
	time_t	on_for;
#endif
	char	comment1[HOSTLEN + HOSTLEN + 2];

	if (MyConnect(sptr))
	{
		if (IsClient(sptr))
			m_clients--;
		if (IsInvisible(sptr))
			m_invis--;
		if (IsServer(sptr))
			m_servers--;
		if (IsAnOper(sptr))
			del_from_fdlist(sptr->fd, &new_operfdlist);
		if (IsServer(sptr))
			del_from_fdlist(sptr->fd, &new_servfdlist);
		sptr->flags |= FLAGS_CLOSING;
#ifdef CLIENT_NOTICES
                if (IsPerson(sptr))
                	sendto_flagops(UFLAGS_CMODE, "Client exiting: %s (%s@%s) [%s]",
				sptr->name, sptr->user->username,
				sptr->user->host,
				sptr->flags & FLAGS_NORMALEX ? "Client Quit":
					(comment?comment:""));
#endif
#if defined(FNAME_USERLOG) || (defined(SYSLOG) && defined(SYSLOG_USERS))
		on_for = NOW - sptr->firsttime;
#endif
#if defined(USE_SYSLOG) && defined(SYSLOG_USERS)
		if (IsPerson(sptr))
			syslog(LOG_NOTICE, "%s (%3d:%02d:%02d): %s!%s@%s\n",
				myctime(sptr->firsttime),
				on_for / 3600, (on_for % 3600)/60,
				on_for % 60,
				sptr->name,
				sptr->user->username, sptr->user->host);
#endif
#ifdef FNAME_USERLOG
	    {
		char linebuf[160];
		int logfile, len;

		if (IsPerson(sptr))
		{
			len = irc_sprintf(linebuf,
			"%s (%3d:%02d:%02d): %s!%s@%s [%s]\n",
				myctime(sptr->firsttime),
				on_for / 3600, (on_for % 3600)/60,
				on_for % 60,
				sptr->name, sptr->user->username,
				sptr->user->host, sptr->username);
#ifdef BUFFERED_LOGS
			cs_buf_logs(1, linebuf, len);
#else 
		    if ((logfile = open(FNAME_USERLOG,
				O_WRONLY|O_APPEND)) != -1)
		    {
			(void)write(logfile, linebuf, strlen(linebuf));
			(void)close(logfile);
		    }
#endif /* BUFFERED_LOGS */
		}
	}
#endif /* FNAME_OPERLOG */
		if (sptr->fd >= 0)
		{
		      if (cptr != NULL && sptr != cptr)
			sendto_one(sptr, "ERROR :Closing Link: %s %s (%s)",
				   get_client_name(sptr,FALSE),
				   cptr->name, comment);
		      else
			sendto_one(sptr, "ERROR :Closing Link: %s (%s)",
				   get_client_name(sptr,FALSE), comment);
		}
		/*
		** Currently only server connections can have
		** depending remote clients here, but it does no
		** harm to check for all local clients. In
		** future some other clients than servers might
		** have remotes too...
		**
		** Close the Client connection first and mark it
		** so that no messages are attempted to send to it.
		** (The following *must* make MyConnect(sptr) == FALSE!).
		** It also makes sptr->from == NULL, thus it's unnecessary
		** to test whether "sptr != acptr" in the following loops.
		*/
		close_connection(sptr);
		/*
		** First QUIT all NON-servers which are behind this link
		**
		** Note	There is no danger of 'cptr' being exited in
		**	the following loops. 'cptr' is a *local* client,
		**	all dependants are *remote* clients.
		*/

		/* This next bit is a a bit ugly but all it does is take the
		** name of us.. me.name and tack it together with the name of
		** the server sptr->name that just broke off and puts this
		** together into exit_one_client() to provide some useful
		** information about where the net is broken.      Ian 
		*/

/* The above comment says that only server connections can have depending
   remote clients.  This is true, and I don't see this changing...so I'm
   adding in this if(), because looking thru all the clients is really
   really lame -- Comstud */
	if (IsServer(sptr))
	{
		(void)strcpy(comment1, me.name);
		(void)strcat(comment1," ");
		(void)strcat(comment1, sptr->name);
		for (acptr = client; acptr; acptr = next)
		{
			next = acptr->next;
			if (!IsServer(acptr) && acptr->from == sptr)
				exit_one_client(NULL, acptr, &me, comment1);
		}
		/*
		** Second SQUIT all servers behind this link
		*/
		for (acptr = client; acptr; acptr = next)
		{
			next = acptr->next;
			if (IsServer(acptr) && acptr->from == sptr)
				exit_one_client(NULL, acptr, &me, me.name);
		}
	} /* IsServer */
	} /* MyConnect */
	exit_one_client(cptr, sptr, from, comment);
	return cptr == sptr ? FLUSH_BUFFER : 0;
}

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
static	void	exit_one_client(cptr, sptr, from, comment)
aClient *sptr;
aClient *cptr;
aClient *from;
char	*comment;
{
	Reg1	aClient *acptr;
	Reg2	int	i;
	Reg3	Link	*lp;
	static time_t last = 0;

#ifdef G_MODE
	if (IsServer(sptr))
		sendto_flagops(UFLAGS_GMODE, "Server left: %s",
			sptr->name);
#endif
#ifdef DOUGH_MALLOC
	if (IsServer(sptr) || (last+10 < NOW))
	{
		last = NOW;
		if (DoughCheckMallocs(0))
			printf("");
	}
#endif DOUGH_MALLOC

	if (sptr->serv)
	{
		del_stuff(sptr);
		if (sptr->servptr && sptr->servptr->serv)
		del_client_from_llist(&(sptr->servptr->serv->servers), sptr);
	}
	else if (sptr->servptr && sptr->servptr->serv)
		del_client_from_llist(&(sptr->servptr->serv->users), sptr);
#ifdef DOUGH_MALLOC
	if (IsServer(sptr) || (last+10 < NOW))
	{
		last = NOW;
		if (DoughCheckMallocs(0))
			printf("");
	}
#endif DOUGH_MALLOC

	/*
	**  For a server or user quitting, propagage the information to
	**  other servers (except to the one where is came from (cptr))
	*/
	if (IsMe(sptr))
	    {
		sendto_ops("ERROR: tried to exit me! : %s", comment);
		return;	/* ...must *never* exit self!! */
	    }
	else if (IsServer(sptr)) {
	 /*
	 ** Old sendto_serv_but_one() call removed because we now
	 ** need to send different names to different servers
	 ** (domain name matching)
	 */
	 	for (i = 0; i <= highest_fd; i++)
		    {
			Reg4	aConfItem *aconf;

			if (!(acptr = local[i]) || !IsServer(acptr) ||
			    acptr == cptr || IsMe(acptr))
				continue;
			if ((aconf = acptr->serv->nline) &&
			    (match(my_name_for_link(me.name, aconf),
				     sptr->name) == 0))
				continue;
			/*
			** SQUIT going "upstream". This is the remote
			** squit still hunting for the target. Use prefixed
			** form. "from" will be either the oper that issued
			** the squit or some server along the path that
			** didn't have this fix installed. --msa
			*/
			if (sptr->from == acptr)
			    {
				sendto_one(acptr, ":%s SQUIT %s :%s",
					   from->name, sptr->name, comment);
#ifdef	USE_SERVICES
				check_services_butone(SERVICE_WANT_SQUIT, sptr,
							":%s SQUIT %s :%s",
							from->name,
							sptr->name, comment);
#endif
			    }
			else
			    {
				sendto_one(acptr, "SQUIT %s :%s",
					   sptr->name, comment);
#ifdef	USE_SERVICES
				check_services_butone(SERVICE_WANT_SQUIT, sptr,
							"SQUIT %s :%s",
							sptr->name, comment);
#endif
			    }
	    }
	} else if (!(IsPerson(sptr) || IsService(sptr)))
				    /* ...this test is *dubious*, would need
				    ** some thougth.. but for now it plugs a
				    ** nasty hole in the server... --msa
				    */
		; /* Nothing */
	else if (sptr->name[0]) /* ...just clean all others with QUIT... */
	    {
		/*
		** If this exit is generated from "m_kill", then there
		** is no sense in sending the QUIT--KILL's have been
		** sent instead.
		*/
		if ((sptr->flags & FLAGS_KILLED) == 0)
		    {
			sendto_serv_butone(cptr,":%s QUIT :%s",
					   sptr->name, comment);
#ifdef	USE_SERVICES
			check_services_butone(SERVICE_WANT_QUIT,
						":%s QUIT :%s", sptr->name,
						comment);
#endif
		    }
		/*
		** If a person is on a channel, send a QUIT notice
		** to every client (person) on the same channel (so
		** that the client can show the "**signoff" message).
		** (Note: The notice is to the local clients *only*)
		*/
		if (sptr->user)
		    {
			sendto_common_channels(sptr, ":%s QUIT :%s",
						sptr->name, comment);

			while ((lp = sptr->user->channel))
				remove_user_from_channel(sptr,lp->value.chptr);

			/* Clean up invitefield */
			while ((lp = sptr->user->invited))
				del_invite(sptr, lp->value.chptr);
				/* again, this is all that is needed */
		    }
	    }

	/* Remove sptr from the client list */
	if (del_from_client_hash_table(sptr->name, sptr) != 1)
		Debug((DEBUG_ERROR, "%#x !in tab %s[%s] %#x %#x %#x %d %d %#x",
			sptr, sptr->name,
			sptr->from ? sptr->from->sockhost : "??host",
			sptr->from, sptr->next, sptr->prev, sptr->fd,
			sptr->status, sptr->user));
	remove_client_from_list(sptr);
	return;
}

void	checklist()
{
	Reg1	aClient	*acptr;
	Reg2	int	i,j;

	if (!(bootopt & BOOT_AUTODIE))
		return;
	for (j = i = 0; i <= highest_fd; i++)
		if (!(acptr = local[i]))
			continue;
		else if (IsClient(acptr))
			j++;
	if (!j)
	    {
#ifdef	USE_SYSLOG
		syslog(LOG_WARNING,"ircd exiting: autodie");
#endif
		exit(0);
	    }
	return;
}

void	initstats()
{
	bzero((char *)&ircst, sizeof(ircst));
}

void	tstats(cptr, name)
aClient	*cptr;
char	*name;
{
	Reg1	aClient	*acptr;
	Reg2	int	i;
	Reg3	struct stats	*sp;
	struct	stats	tmp;

	sp = &tmp;
	bcopy((char *)ircstp, (char *)sp, sizeof(*sp));
	for (i = 0; i < MAXCONNECTIONS; i++)
	    {
		if (!(acptr = local[i]))
			continue;
		if (IsServer(acptr))
		    {
			sp->is_sbs += acptr->sendB;
			sp->is_sbr += acptr->receiveB;
			sp->is_sks += acptr->sendK;
			sp->is_skr += acptr->receiveK;
			sp->is_sti += NOW - acptr->firsttime;
			sp->is_sv++;
			if (sp->is_sbs > 1023)
			    {
				sp->is_sks += (sp->is_sbs >> 10);
				sp->is_sbs &= 0x3ff;
			    }
			if (sp->is_sbr > 1023)
			    {
				sp->is_skr += (sp->is_sbr >> 10);
				sp->is_sbr &= 0x3ff;
			    }
		    }
		else if (IsClient(acptr))
		    {
			sp->is_cbs += acptr->sendB;
			sp->is_cbr += acptr->receiveB;
			sp->is_cks += acptr->sendK;
			sp->is_ckr += acptr->receiveK;
			sp->is_cti += NOW - acptr->firsttime;
			sp->is_cl++;
			if (sp->is_cbs > 1023)
			    {
				sp->is_cks += (sp->is_cbs >> 10);
				sp->is_cbs &= 0x3ff;
			    }
			if (sp->is_cbr > 1023)
			    {
				sp->is_ckr += (sp->is_cbr >> 10);
				sp->is_cbr &= 0x3ff;
			    }
		    }
		else if (IsUnknown(acptr))
			sp->is_ni++;
	    }

	sendto_one(cptr, ":%s %d %s :accepts %u refused %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_ac, sp->is_ref);
	sendto_one(cptr, ":%s %d %s :unknown commands %u prefixes %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_unco, sp->is_unpf);
	sendto_one(cptr, ":%s %d %s :nick collisions %u unknown closes %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_kill, sp->is_ni);
	sendto_one(cptr, ":%s %d %s :wrong direction %u empty %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_wrdi, sp->is_empt);
	sendto_one(cptr, ":%s %d %s :numerics seen %u mode fakes %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_num, sp->is_fake);
	sendto_one(cptr, ":%s %d %s :auth successes %u fails %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_asuc, sp->is_abad);
	sendto_one(cptr, ":%s %d %s :local connections %u udp packets %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_loc, sp->is_udp);
	sendto_one(cptr, ":%s %d %s :Client Server",
		   me.name, RPL_STATSDEBUG, name);
	sendto_one(cptr, ":%s %d %s :connected %u %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_cl, sp->is_sv);
	sendto_one(cptr, ":%s %d %s :bytes sent %u.%uK %u.%uK",
		   me.name, RPL_STATSDEBUG, name,
		   sp->is_cks, sp->is_cbs, sp->is_sks, sp->is_sbs);
	sendto_one(cptr, ":%s %d %s :bytes recv %u.%uK %u.%uK",
		   me.name, RPL_STATSDEBUG, name,
		   sp->is_ckr, sp->is_cbr, sp->is_skr, sp->is_sbr);
	sendto_one(cptr, ":%s %d %s :time connected %u %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_cti, sp->is_sti);
}

int rule_allow(rule, str, both)
aRule *rule;
char *str;
int both;
{
	int ok = 0;
	anExcept *except;

	while(rule != NULL)
	{
		if (IsAllow(rule) && (!match(rule->string, str) ||
					(both && !match(str, rule->string))))
		{
			ok++;
			except = rule->excepts;
			while(except != NULL)
			{
				if (!match(except->string, str) ||
					(both && !match(str, except->string)))
				{
					ok = 0;
					break;
				}
				except = except->next;
			}
			if (ok)
				break;
		}
		else if (IsAllow(rule))
			; /* will continue... */
		else if (IsDeny(rule) && (!match(rule->string, str) ||
					(both && !match(str, rule->string))))
		{
			ok = 0;
			except = rule->excepts;
			while(except)
			{
				if (!match(except->string, str) ||
					(both && !match(str, except->string)))
				{
					ok = 1;
					break;
				}
				except = except->next;
			}
			if (!ok)
				break;
		}
		/* else if (IsDeny(rule) && match(rule->string, str)) */
		rule = rule->next;
	} /* while(rule) */
	return ok;
} 

void remove_rules(bucket)
aRule **bucket;
{
	register aRule *item, *itemnext;
	register anExcept *item2, *item2next;

	for(item=*bucket;item;item=itemnext)
	{
		itemnext = item->next;
		for(item2 = item->excepts;item2;item2=item2next)
		{
			item2next = item2->next;
			MyFree(item2->string);
			MyFree(item2);
		}
		MyFree(item->string);
		MyFree(item);
	}
	*bucket = NULL;
}

