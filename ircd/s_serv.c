/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_serv.c (formerly ircd/s_msg.c)
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
static  char sccsid[] = "@(#)s_serv.c	2.55 2/7/94 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif
#include <string.h> 

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#if defined(PCS) || defined(AIX) || defined(DYNIXPTX) || defined(SVR3)
#include <time.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <utmp.h>
#include "h.h"

#include "dich_conf.h"

#ifdef DOG3
extern time_t check_fdlists();
#endif

static	char	buf[BUFSIZE];

#ifdef HIGHEST_CONNECTION
int     max_connection_count = 1, max_client_count = 1;
#endif

#ifdef HIGHEST_CONNECTION
void
check_max_count()
{
      if (m_clients > max_client_count)
              max_client_count = m_clients;
      if ((m_clients + m_servers) > max_connection_count)
      {
              max_connection_count = m_clients + m_servers;
              if (max_connection_count % 10 == 0)
                      sendto_flagops(OPERS,
                              "New highest connections: %d (%d clients)",
                              max_connection_count, max_client_count);
      }
}
#endif /* HIGHEST_CONNECTION */

/* These can be static once we take check out of ircd.c */
int s_ct, c_ct, u_ct, i_ct;
int o_ct, m_cs, m_is, m_ss;

void
compute_lusers(mask)
char *mask;
{
	aClient *acptr;

	c_ct = i_ct = s_ct = o_ct = 0;
	m_cs = m_is = m_ss = u_ct = 0;

	for (acptr = client; acptr; acptr = acptr->next)
	{
		if (mask)
		{
			if (!IsServer(acptr) && acptr->user)
			{
				if (match(mask, acptr->user->server))
					continue;
			}
			else if (match(mask, acptr->name))
				continue;
		}
		switch (acptr->status)
		{
		case STAT_SERVER:
			if (MyConnect(acptr))
				m_ss++;
		case STAT_ME:
			s_ct++;
			break;
		case STAT_CLIENT:
			c_ct++;
			if (MyConnect(acptr))
				m_cs++;
			if (IsOper(acptr))
				o_ct++;
			if (IsInvisible(acptr))
			{
				i_ct++;
				if (MyConnect(acptr))
					m_is++;
			}
			break;
		default:
			u_ct++;
			break;
		}
	}
}

extern 	ts_val	timedelta;

/*
** m_functions execute protocol messages on this server:
**
**	cptr	is always NON-NULL, pointing to a *LOCAL* client
**		structure (with an open socket connected!). This
**		identifies the physical socket where the message
**		originated (or which caused the m_function to be
**		executed--some m_functions may call others...).
**
**	sptr	is the source of the message, defined by the
**		prefix part of the message if present. If not
**		or prefix not found, then sptr==cptr.
**
**		(!IsServer(cptr)) => (cptr == sptr), because
**		prefixes are taken *only* from servers...
**
**		(IsServer(cptr))
**			(sptr == cptr) => the message didn't
**			have the prefix.
**
**			(sptr != cptr && IsServer(sptr) means
**			the prefix specified servername. (?)
**
**			(sptr != cptr && !IsServer(sptr) means
**			that message originated from a remote
**			user (not local).
**
**		combining
**
**		(!IsServer(sptr)) means that, sptr can safely
**		taken as defining the target structure of the
**		message in this server.
**
**	*Always* true (if 'parse' and others are working correct):
**
**	1)	sptr->from == cptr  (note: cptr->from == cptr)
**
**	2)	MyConnect(sptr) <=> sptr == cptr (e.g. sptr
**		*cannot* be a local connection, unless it's
**		actually cptr!). [MyConnect(x) should probably
**		be defined as (x == x->from) --msa ]
**
**	parc	number of variable parameter strings (if zero,
**		parv is allowed to be NULL)
**
**	parv	a NULL terminated list of parameter pointers,
**
**			parv[0], sender (prefix string), if not present
**				this points to an empty string.
**			parv[1]...parv[parc-1]
**				pointers to additional parameters
**			parv[parc] == NULL, *always*
**
**		note:	it is guaranteed that parv[0]..parv[parc-1] are all
**			non-NULL pointers.
*/

#ifdef DOG3

int	m_htm(cptr, sptr, parc, parv)
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
		sendto_one(sptr, ":%s NOTICE %s :Hello???  Try a number > 0.",
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
                sendto_one(sptr, ":%s NOTICE %s :Hello???  Try a number >= 0.",
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

#ifdef IDLE_CHECK

int     m_idle(cptr, sptr, parc, parv)
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
                sendto_one(sptr, ":%s NOTICE %s :The current idle limit is set at %i minutes",
                        me.name, parv[0], idlelimit/60);
                return 0;
        }
      temp = atoi(parv[1]);
      if (temp && (temp < 10))
      {
              sendto_one(sptr, ":%s NOTICE %s :Hello???  Try a number > 10.",
                      me.name, parv[0]);
              return 0;
      }
      idlelimit = temp*60;
      sendto_ops("%s has changed the idle time limit to %i minute(s).",
              parv[0], idlelimit/60);
      sendto_one(sptr, ":%s NOTICE %s :The idle limit is now set to %i minute(s)",
              me.name, parv[0], idlelimit/60);
      return 0;
}

#endif

/*
** m_version
**	parv[0] = sender prefix
**	parv[1] = remote server
*/
int	m_version(cptr, sptr, parc, parv)
aClient *sptr, *cptr;
int	parc;
char	*parv[];
{
	extern	char	serveropts[];

	if (check_registered(sptr))
		return 0;

	if (hunt_server(cptr,sptr,":%s VERSION :%s",1,parc,parv)==HUNTED_ISME)
		sendto_one(sptr, rpl_str(RPL_VERSION), me.name,
			   parv[0], version, debugmode, me.name, serveropts);
	return 0;
}

/*
** m_squit
**	parv[0] = sender prefix
**	parv[1] = server name
**	parv[2] = comment
*/
int	m_squit(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	Reg1	aConfItem *aconf;
	char	*server;
	Reg2	aClient	*acptr;
	char	*comment = (parc > 2 && parv[2]) ? parv[2] : cptr->name;

	if (!IsPrivileged(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }

	if (parc > 1)
	    {
		server = parv[1];
		/*
		** To accomodate host masking, a squit for a masked server
		** name is expanded if the incoming mask is the same as
		** the server name for that link to the name of link.
		*/
		while ((*server == '*') && IsServer(cptr))
		    {
			aconf = cptr->serv->nline;
			if (!aconf)
				break;
			if (!mycmp(server, my_name_for_link(me.name, aconf)))
				server = cptr->name;
			break; /* WARNING is normal here */
		    }
		/*
		** The following allows wild cards in SQUIT. Only usefull
		** when the command is issued by an oper.
		*/
		for (acptr = client; (acptr = next_client(acptr, server));
		     acptr = acptr->next)
			if (IsServer(acptr) || IsMe(acptr))
				break;
		if (acptr && IsMe(acptr))
		    {
			acptr = cptr;
			server = cptr->sockhost;
		    }
	    }
	else
	    {
		/*
		** This is actually protocol error. But, well, closing
		** the link is very proper answer to that...
		*/
		server = cptr->sockhost;
		acptr = cptr;
	    }

	/*
	** SQUIT semantics is tricky, be careful...
	**
	** The old (irc2.2PL1 and earlier) code just cleans away the
	** server client from the links (because it is never true
	** "cptr == acptr".
	**
	** This logic here works the same way until "SQUIT host" hits
	** the server having the target "host" as local link. Then it
	** will do a real cleanup spewing SQUIT's and QUIT's to all
	** directions, also to the link from which the orinal SQUIT
	** came, generating one unnecessary "SQUIT host" back to that
	** link.
	**
	** One may think that this could be implemented like
	** "hunt_server" (e.g. just pass on "SQUIT" without doing
	** nothing until the server having the link as local is
	** reached). Unfortunately this wouldn't work in the real life,
	** because either target may be unreachable or may not comply
	** with the request. In either case it would leave target in
	** links--no command to clear it away. So, it's better just
	** clean out while going forward, just to be sure.
	**
	** ...of course, even better cleanout would be to QUIT/SQUIT
	** dependant users/servers already on the way out, but
	** currently there is not enough information about remote
	** clients to do this...   --msa
	*/
	if (!acptr)
	    {
		sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
			   me.name, parv[0], server);
		return 0;
	    }
	if (IsLocOp(sptr) && !MyConnect(acptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
	/*
	**  Notify all opers, if my local link is remotely squitted
	*/
	if (MyConnect(acptr) && !IsAnOper(cptr))
	  {
	    sendto_ops_butone(NULL, &me,
		":%s WALLOPS :Received SQUIT %s from %s (%s)",
		me.name, server, get_client_name(sptr,FALSE), comment);
#if defined(USE_SYSLOG) && defined(SYSLOG_SQUIT)
	    syslog(LOG_DEBUG,"SQUIT From %s : %s (%s)",
		   parv[0], server, comment);
#endif
	  }
	else if (MyConnect(acptr))
		sendto_ops("Received SQUIT %s from %s (%s)",
			   acptr->name, get_client_name(sptr,FALSE), comment);

	return exit_client(cptr, acptr, sptr, comment);
    }

/*
** ts_servcount
**	returns the number of TS servers that are connected to us
*/
int	ts_servcount()
{
	Reg1	int	i;
	Reg2	aClient	*acptr;
	Reg3	int	r = 0;

	for (i = 0; i <= highest_fd; i++)
	    	if ((acptr = local[i]) && IsServer(acptr) && DoesTS(acptr))
			r++;
	return r;
}

/*
** m_svinfo
**	parv[0] = sender prefix
**	parv[1] = TS_CURRENT for the server
**	parv[2] = TS_MIN for the server
**	parv[3] = server is standalone or connected to non-TS only
**	parv[4] = server's idea of UTC time
*/
int	m_svinfo(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	ts_val	v;

	if (!IsServer(sptr) || !MyConnect(sptr) || !DoesTS(sptr) || parc < 5 ||
		TS_CURRENT < atoi(parv[2]) || atoi(parv[1]) < TS_MIN)
		return 0;

	if (atoi(parv[3]))
	    	v = (atol(parv[4]) - (ts_val)NOW - timedelta) / 2;
	else
	    	v = atol(parv[4]) - (ts_val)NOW - timedelta;

	if (ts_servcount() == 1)
		timedelta += v;
	return 0;
}

/*
** m_server
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = serverinfo/hopcount
**      parv[3] = serverinfo
*/
int	m_server(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	char	*ch;
	Reg2	int	i;
	char	info[REALLEN+1], *inpath, *host;
	aClient *acptr, *bcptr;
	aConfItem *aconf;
	int	hop;

	info[0] = '\0';
	inpath = get_client_name(cptr,FALSE);
	if (parc < 2 || *parv[1] == '\0')
	    {
			sendto_one(cptr,"ERROR :No servername");
			return 0;
	    }
	hop = 0;
	host = parv[1];
	if (parc > 3 && atoi(parv[2]))
	    {
		hop = atoi(parv[2]);
		(void)strncpy(info, parv[3], REALLEN);
	    }
	else if (parc > 2)
	    {
		(void)strncpy(info, parv[2], REALLEN);
		if (parc > 3)
		    {
				i = strlen(info);
				(void)strncat(info, " ", REALLEN - i - 1);
				(void)strncat(info, parv[3], REALLEN - i - 2);
		    }
	    }
	/*
	** Check for "FRENCH " infection ;-) (actually this should
	** be replaced with routine to check the hostname syntax in
	** general). [ This check is still needed, even after the parse
	** is fixed, because someone can send "SERVER :foo bar " ].
	** Also, changed to check other "difficult" characters, now
	** that parse lets all through... --msa
	*/
	if (strlen(host) > HOSTLEN)
		host[HOSTLEN] = '\0';
	for (ch = host; *ch; ch++)
		if (*ch <= ' ' || *ch > '~')
			break;
	if (*ch || !index(host, '.'))
	    {
		sendto_one(sptr,"ERROR :Bogus server name (%s)",
			   sptr->name, host);
		sendto_ops("Bogus server name (%s) from %s", host,
			   get_client_name(cptr, TRUE));
		return 0;
	    }

	if (IsPerson(cptr))
	    {
		/*
		** A local link that has been identified as a USER
		** tries something fishy... ;-)
		*/
		sendto_one(cptr, err_str(ERR_ALREADYREGISTRED),
			   me.name, parv[0]);
		sendto_ops("User %s trying to become a server %s",
			   get_client_name(cptr, TRUE),host);
		return 0;
	    }
	/* *WHEN* can it be that "cptr != sptr" ????? --msa */
	/* When SERVER command (like now) has prefix. -avalon */
	
	if ((acptr = find_name(host, NULL)))
	    {
		/*
		** This link is trying feed me a server that I already have
		** access through another path -- multiple paths not accepted
		** currently, kill this link immeatedly!!
		**
		** Rather than KILL the link which introduced it, KILL the
		** youngest of the two links. -avalon
		*/
		acptr = acptr->from;
		acptr = (cptr->firsttime > acptr->firsttime) ? cptr : acptr;
		sendto_one(acptr,"ERROR :Server %s already exists", host);
		sendto_ops("Link %s cancelled, server %s already exists",
			   get_client_name(acptr, TRUE), host);
		return exit_client(acptr, acptr, acptr, "Server Exists");
	    }
	if ((acptr = find_client(host, NULL)) && acptr != cptr)
	    {
		/*
		** Server trying to use the same name as a person. Would
		** cause a fair bit of confusion. Enough to make it hellish
		** for a while and servers to send stuff to the wrong place.
		*/
		sendto_one(cptr,"ERROR :Nickname %s already exists!", host);
		sendto_ops("Link %s cancelled: Server/nick collision on %s",
			   inpath, host);
		return exit_client(cptr, cptr, cptr, "Nick as Server");
	    }

	if (IsServer(cptr))
	    {
		/*
		** Server is informing about a new server behind
		** this link. Create REMOTE server structure,
		** add it to list and propagate word to my other
		** server links...
		*/
		if (parc == 1 || info[0] == '\0')
		    {
	  		sendto_one(cptr,
				   "ERROR :No server info specified for %s",
				   host);
	  		return 0;
		    }

		/*
		** See if the newly found server is behind a guaranteed
		** leaf (L-line). If so, close the link.
		*/
		if ((aconf = find_conf_host(cptr->confs, host, CONF_LEAF)) &&
		    (!aconf->port || (hop > aconf->port)))
		    {
	      		sendto_ops("Leaf-only link %s->%s - Closing",
				   get_client_name(cptr,  TRUE),
				   aconf->host ? aconf->host : "*");
	      		sendto_one(cptr, "ERROR :Leaf-only link, sorry.");
      			return exit_client(cptr, cptr, cptr, "Leaf Only");
		    }
		/*
		**
		*/
		if (!(aconf = find_conf_host(cptr->confs, host, CONF_HUB)) ||
		    (aconf->port && (hop > aconf->port)) )
		    {
			sendto_ops("Non-Hub link %s introduced %s(%s).",
				   get_client_name(cptr,  TRUE), host,
				   aconf ? (aconf->host ? aconf->host : "*") :
				   "!");
			return exit_client(cptr, cptr, cptr,
					   "Too many servers");
		    }
		/*
		** See if the newly found server has a Q line for it in
		** our conf. If it does, lose the link that brought it
		** into our network. Format:
		**
		** Q:<unused>:<reason>:<servername>
		**
		** Example:  Q:*:for the hell of it:eris.Berkeley.EDU
		*/
		if ((aconf = find_conf_name(host, CONF_QUARANTINED_SERVER)))
		    {
			sendto_ops_butone(NULL, &me,
				":%s WALLOPS * :%s brought in %s, %s %s",
				me.name, me.name, get_client_name(cptr,FALSE),
				host, "closing link because",
				BadPtr(aconf->passwd) ? "reason unspecified" :
				aconf->passwd);

			sendto_one(cptr,
				   "ERROR :%s is not welcome: %s. %s",
				   host, BadPtr(aconf->passwd) ?
				   "reason unspecified" : aconf->passwd,
				   "Go away and get a life");

			return exit_client(cptr, cptr, cptr, "Q-Lined Server");
		    }

		acptr = make_client(cptr);
		(void)make_server(acptr);
		acptr->hopcount = hop;
		strncpyzt(acptr->name, host, sizeof(acptr->name));
		strncpyzt(acptr->info, info, sizeof(acptr->info));
		strncpyzt(acptr->serv->up, parv[0], sizeof(acptr->serv->up));
		SetServer(acptr);
		s_count++;
		add_client_to_list(acptr);
		(void)add_to_client_hash_table(acptr->name, acptr);
		/*
		** Old sendto_serv_but_one() call removed because we now
		** need to send different names to different servers
		** (domain name matching)
		*/
		for (i = 0; i <= highest_fd; i++)
		    {
			if (!(bcptr = local[i]) || !IsServer(bcptr) ||
			    bcptr == cptr || IsMe(bcptr))
				continue;
			if (!(aconf = bcptr->serv->nline))
			    {
				sendto_ops("Lost N-line for %s on %s. Closing",
					   get_client_name(cptr, TRUE), host);
				return exit_client(cptr, cptr, cptr,
						   "Lost N line");
			    }
			if (matches(my_name_for_link(me.name, aconf),
				    acptr->name) == 0)
				continue;
			sendto_one(bcptr, ":%s SERVER %s %d :%s",
				   parv[0], acptr->name, hop+1, acptr->info);
		    }
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_SERVER, sptr,
					":%s SERVER %s %d :%s", parv[0],
					acptr->name, hop+1, acptr->info);
#endif
		return 0;
	    }

	if (!IsUnknown(cptr) && !IsHandshake(cptr))
		return 0;
	/*
	** A local link that is still in undefined state wants
	** to be a SERVER. Check if this is allowed and change
	** status accordingly...
	*/
	strncpyzt(cptr->name, host, sizeof(cptr->name));
	strncpyzt(cptr->info, info[0] ? info:me.name, sizeof(cptr->info));
	cptr->hopcount = hop;

	switch (check_server_init(cptr))
	{
	case 0 :
		return m_server_estab(cptr);
	case 1 :
		sendto_ops("Access check for %s in progress",
			   get_client_name(cptr,TRUE));
		return 1;
	default :
		ircstp->is_ref++;
		sendto_ops("Received unauthorized connection from %s.",
		           get_client_host(cptr));
		return exit_client(cptr, cptr, cptr, "No C/N conf lines");
	}

}

static void	sendnick_TS(cptr, acptr)
aClient	*cptr, *acptr;
{
	static	char ubuf[12];

	if (IsPerson(acptr))
	    {
		send_umode(NULL, acptr, 0, SEND_UMODES,
			   ubuf);
		if (!*ubuf)
			strcpy(ubuf, "+");
		sendto_one(cptr, "NICK %s %d %ld %s %s %s %s :%s", acptr->name, 
			   acptr->hopcount + 1, acptr->tsinfo, ubuf,
			   acptr->user->username, acptr->user->host,
			   acptr->user->server, acptr->info);
	    }
	else if (IsService(acptr))
	    {
		sendto_one(cptr,"NICK %s :%d",
			   acptr->name, 
			   acptr->hopcount + 1);
		sendto_one(cptr,":%s SERVICE * * :%s", acptr->name,
			   acptr->info);
	    }
}

int	m_server_estab(cptr)
Reg1	aClient	*cptr;
{
	Reg1	aChannel *chptr;
	Reg2	aClient	*acptr;
	Reg3	aConfItem	*aconf, *bconf;
	char	*inpath, *host, *s, *encr;
	int	split, i;

	inpath = get_client_name(cptr,TRUE); /* "refresh" inpath with host */
	split = mycmp(cptr->name, cptr->sockhost);
	host = cptr->name;
#ifdef HIGHEST_CONNECTION
	check_max_count();
#endif
	if (!(aconf = find_conf(cptr->confs, host, CONF_NOCONNECT_SERVER)))
	    {
		ircstp->is_ref++;
		sendto_one(cptr,
			   "ERROR :Access denied. No N line for server %s",
			   inpath);
		sendto_ops("Access denied. No N line for server %s", inpath);
		return exit_client(cptr, cptr, cptr, "No N line for server");
	    }
	if (!(bconf = find_conf(cptr->confs, host, CONF_CONNECT_SERVER)))
	    {
		ircstp->is_ref++;
		sendto_one(cptr, "ERROR :Only N (no C) field for server %s",
			   inpath);
		sendto_ops("Only N (no C) field for server %s",inpath);
		return exit_client(cptr, cptr, cptr, "No C line for server");
	    }

#ifdef CRYPT_LINK_PASSWORD
	/* use first two chars of the password they send in as salt */

	/* passwd may be NULL. Head it off at the pass... */
	if(*cptr->passwd)
	    {
		char    salt[3];
		extern  char *crypt();

		salt[0]=aconf->passwd[0];
		salt[1]=aconf->passwd[1];
		salt[2]='\0';
		encr = crypt(cptr->passwd, salt);
	    }
	else
		encr = "";
#else
	encr = cptr->passwd;
#endif  /* CRYPT_LINK_PASSWORD */
	if (*aconf->passwd && !StrEq(aconf->passwd, encr))
	    {
		ircstp->is_ref++;
		sendto_one(cptr, "ERROR :No Access (passwd mismatch) %s",
			   inpath);
		sendto_ops("Access denied (passwd mismatch) %s", inpath);
		return exit_client(cptr, cptr, cptr, "Bad Password");
	    }
	bzero(cptr->passwd, sizeof(cptr->passwd));

#ifndef	HUB
	for (i = 0; i <= highest_fd; i++)
		if (local[i] && IsServer(local[i]))
		    {
			ircstp->is_ref++;
			sendto_one(cptr, "ERROR :I'm a leaf not a hub");
			return exit_client(cptr, cptr, cptr, "I'm a leaf");
		    }
#endif
	if (IsUnknown(cptr))
	    {
		if (bconf->passwd[0])
			sendto_one(cptr,"PASS %s :TS",bconf->passwd);
		/*
		** Pass my info to the new server
		*/
		sendto_one(cptr, "SERVER %s 1 :%s",
			   my_name_for_link(me.name, aconf), 
			   (me.info[0]) ? (me.info) : "IRCers United");
	    }
	else
	    {
		s = (char *)index(aconf->host, '@');
		*s = '\0'; /* should never be NULL */
		Debug((DEBUG_INFO, "Check Usernames [%s]vs[%s]",
			aconf->host, cptr->username));
		if (matches(aconf->host, cptr->username))
		    {
			*s = '@';
			ircstp->is_ref++;
			sendto_ops("Username mismatch [%s]v[%s] : %s",
				   aconf->host, cptr->username,
				   get_client_name(cptr, TRUE));
			sendto_one(cptr, "ERROR :No Username Match");
			return exit_client(cptr, cptr, cptr, "Bad User");
		    }
		*s = '@';
	    }

	if (DoesTS(cptr))
		sendto_one(cptr, "SVINFO %d %d %d :%ld", TS_CURRENT, TS_MIN,
			(ts_servcount() == 0 ? 1 : 0),
			(ts_val)NOW + timedelta);
 
	det_confs_butmask(cptr, CONF_LEAF|CONF_HUB|CONF_NOCONNECT_SERVER);
	/*
	** *WARNING*
	** 	In the following code in place of plain server's
	**	name we send what is returned by get_client_name
	**	which may add the "sockhost" after the name. It's
	**	*very* *important* that there is a SPACE between
	**	the name and sockhost (if present). The receiving
	**	server will start the information field from this
	**	first blank and thus puts the sockhost into info.
	**	...a bit tricky, but you have been warned, besides
	**	code is more neat this way...  --msa
	*/
	SetServer(cptr);
	m_servers++;
	s_count++;
#ifdef DOG3
	/* adds to fdlist */
	addto_fdlist(cptr->fd,&serv_fdlist);
	/* this causes the server to be marked as "busy" */
	check_fdlists();
#endif 
	nextping = NOW;
	sendto_ops("Link with %s (%s) established.", inpath, DoesTS(cptr) ? "TS" : "NoTS");
	(void)add_to_client_hash_table(cptr->name, cptr);
	/* doesnt duplicate cptr->serv if allocted this struct already */
	(void)make_server(cptr);
	(void)strcpy(cptr->serv->up, me.name);
	cptr->serv->nline = aconf;
#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_SERVER, sptr,
				":%s SERVER %s %d :%s", parv[0],
				cptr->name, hop+1, cptr->info);
#endif
	/*
	** Old sendto_serv_but_one() call removed because we now
	** need to send different names to different servers
	** (domain name matching) Send new server to other servers.
	*/
	for (i = 0; i <= highest_fd; i++) 
	    {
		if (!(acptr = local[i]) || !IsServer(acptr) ||
		    acptr == cptr || IsMe(acptr))
			continue;
		if ((aconf = acptr->serv->nline) &&
		    !matches(my_name_for_link(me.name, aconf), cptr->name))
			continue;
		if (split)
			sendto_one(acptr,":%s SERVER %s 2 :[%s] %s",
				   me.name, cptr->name,
				   cptr->sockhost, cptr->info);
		else
			sendto_one(acptr,":%s SERVER %s 2 :%s",
				   me.name, cptr->name, cptr->info);
	    }

	/*
	** Pass on my client information to the new server
	**
	** First, pass only servers (idea is that if the link gets
	** cancelled beacause the server was already there,
	** there are no NICK's to be cancelled...). Of course,
	** if cancellation occurs, all this info is sent anyway,
	** and I guess the link dies when a read is attempted...? --msa
	** 
	** Note: Link cancellation to occur at this point means
	** that at least two servers from my fragment are building
	** up connection this other fragment at the same time, it's
	** a race condition, not the normal way of operation...
	**
	** ALSO NOTE: using the get_client_name for server names--
	**	see previous *WARNING*!!! (Also, original inpath
	**	is destroyed...)
	*/

	aconf = cptr->serv->nline;
	for (acptr = &me; acptr; acptr = acptr->prev)
	    {
		/* acptr->from == acptr for acptr == cptr */
		if (acptr->from == cptr)
			continue;
		if (IsServer(acptr))
		    {
			if (matches(my_name_for_link(me.name, aconf),
				    acptr->name) == 0)
				continue;
			split = (MyConnect(acptr) &&
				 mycmp(acptr->name, acptr->sockhost));
			if (split)
				sendto_one(cptr, ":%s SERVER %s %d :[%s] %s",
		   			   acptr->serv->up, acptr->name,
					   acptr->hopcount+1,
		   			   acptr->sockhost, acptr->info);
			else
				sendto_one(cptr, ":%s SERVER %s %d :%s",
		   			   acptr->serv->up, acptr->name,
					   acptr->hopcount+1, acptr->info);
		    }
	    }

	if (DoesTS(cptr))
	    {
	    	Link	*l;
		static	char nickissent = 1;

		/*
		** Send it in the shortened format with the TS, if
		** it's a TS server; walk the list of channels, sending
		** all the nicks that haven't been sent yet for each
		** channel, then send the channel itself -- it's less
		** obvious than sending all nicks first, but on the
		** receiving side memory will be allocated more nicely
		** saving a few seconds in the handling of a split
		** -orabidoo
		*/

		nickissent = 3 - nickissent;
		/* flag used for each nick to check if we've sent it
		   yet - must be different each time and !=0, so we
		   alternate between 1 and 2 -orabidoo
	        */
		for (chptr = channel; chptr; chptr = chptr->nextch)
		    {
			for (l = chptr->members; l; l = l->next)
			    {
				acptr = l->value.cptr;
				if (acptr->nicksent != nickissent)
				    {
					acptr->nicksent = nickissent;
					if (acptr->from != cptr)
						sendnick_TS(cptr, acptr);
				    }
			    }
			send_channel_modes(cptr, chptr);
		    }
		/*
		** also send out those that are not on any channel
		*/
		for (acptr = &me; acptr; acptr = acptr->prev)
			if (acptr->nicksent != nickissent)
			    {
				acptr->nicksent = nickissent;
				if (acptr->from != cptr)
					sendnick_TS(cptr, acptr);
			    }
			
	    }
	else
	    {
	    	/*
		** do it the usual way, for non-TS servers  -orabidoo
		*/
		for (acptr = &me; acptr; acptr = acptr->prev)
		    {
			/* acptr->from == acptr for acptr == cptr */
			if (acptr->from == cptr)
				continue;
			if (IsPerson(acptr))
			    {
			/*
			** IsPerson(x) is true only when IsClient(x) is true.
			** These are only true when *BOTH* NICK and USER have
			** been received. -avalon
			*/
				sendto_one(cptr,"NICK %s :%d",acptr->name,
					   acptr->hopcount + 1);
				sendto_one(cptr,
					":%s USER %s %s %s :%s", acptr->name,
				       acptr->user->username, acptr->user->host,
				       acptr->user->server, acptr->info);
				send_umode(cptr, acptr, 0, SEND_UMODES, buf);
				send_user_joins(cptr, acptr);
			    }
			else if (IsService(acptr))
			    {
				sendto_one(cptr,"NICK %s :%d",
					   acptr->name, acptr->hopcount + 1);
				sendto_one(cptr,":%s SERVICE * * :%s",
					   acptr->name, acptr->info);
			    }
		    }
		/*
		** Last, pass all channels plus statuses
		*/
		for (chptr = channel; chptr; chptr = chptr->nextch)
			send_channel_modes(cptr, chptr);
	    }
	return 0;
}

/*
** m_info
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_info(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	char **text = infotext;

	if (check_registered(sptr))
		return 0;

	if (hunt_server(cptr,sptr,":%s INFO :%s",1,parc,parv) == HUNTED_ISME)
	    {
		while (*text)
			sendto_one(sptr, rpl_str(RPL_INFO),
				   me.name, parv[0], *text++);

		sendto_one(sptr, rpl_str(RPL_INFO), me.name, parv[0], "");
		sendto_one(sptr,
			   ":%s %d %s :Birth Date: %s, compile # %s",
			   me.name, RPL_INFO, parv[0], creation, generation);
		sendto_one(sptr, ":%s %d %s :On-line since %s",
			   me.name, RPL_INFO, parv[0],
			   myctime(me.firsttime));
		sendto_one(sptr, rpl_str(RPL_ENDOFINFO), me.name, parv[0]);
	    }

    return 0;
}

/*
** m_links
**	parv[0] = sender prefix
**	parv[1] = servername mask
** or
**	parv[0] = sender prefix
**	parv[1] = server to query 
**      parv[2] = servername mask
*/
int	m_links(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	char *mask;
	aClient *acptr;

	if (check_registered_user(sptr))
		return 0;
    
	if (parc > 2)
	    {
		if (hunt_server(cptr, sptr, ":%s LINKS %s :%s", 1, parc, parv)
				!= HUNTED_ISME)
			return 0;
		mask = parv[2];
	    }
	else
		mask = parc < 2 ? NULL : parv[1];

	for (acptr = client, (void)collapse(mask); acptr; acptr = acptr->next) 
	    {
		if (!IsServer(acptr) && !IsMe(acptr))
			continue;
		if (!BadPtr(mask) && match(mask, acptr->name))
			continue;
		sendto_one(sptr, rpl_str(RPL_LINKS),
			   me.name, parv[0], acptr->name, acptr->serv->up,
			   acptr->hopcount, (acptr->info[0] ? acptr->info :
			   "(Unknown Location)"));
	    }

	sendto_one(sptr, rpl_str(RPL_ENDOFLINKS), me.name, parv[0],
		   BadPtr(mask) ? "*" : mask);
	return 0;
}

/*
** m_summon should be redefined to ":prefix SUMMON host user" so
** that "hunt_server"-function could be used for this too!!! --msa
** As of 2.7.1e, this was the case. -avalon
**
**	parv[0] = sender prefix
**	parv[1] = user
**	parv[2] = server
**	parv[3] = channel (optional)
*/
int	m_summon(cptr, sptr, parc, parv)
aClient *sptr, *cptr;
int	parc;
char	*parv[];
{
	char	*host, *user, *chname;
#ifdef	ENABLE_SUMMON
	char	hostbuf[17], namebuf[10], linebuf[10];
#  ifdef LEAST_IDLE
        char	linetmp[10], ttyname[15]; /* Ack */
        struct	stat stb;
        time_t	ltime = (time_t)0;
#  endif
	int	fd, flag = 0;
#endif

	if (check_registered_user(sptr))
		return 0;
	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NORECIPIENT),
			   me.name, parv[0], "SUMMON");
		return 0;
	    }
	user = parv[1];
	host = (parc < 3 || BadPtr(parv[2])) ? me.name : parv[2];
	chname = (parc > 3) ? parv[3] : "*";
	/*
	** Summoning someone on remote server, find out which link to
	** use and pass the message there...
	*/
	parv[1] = user;
	parv[2] = host;
	parv[3] = chname;
	parv[4] = NULL;
	if (hunt_server(cptr, sptr, ":%s SUMMON %s %s %s", 2, parc, parv) ==
	    HUNTED_ISME)
	    {
#ifdef ENABLE_SUMMON
		if ((fd = utmp_open()) == -1)
		    {
			sendto_one(sptr, err_str(ERR_FILEERROR),
				   me.name, parv[0], "open", UTMP);
			return 0;
		    }
#  ifndef LEAST_IDLE
		while ((flag = utmp_read(fd, namebuf, linebuf, hostbuf,
					 sizeof(hostbuf))) == 0) 
			if (StrEq(namebuf,user))
				break;
#  else
                /* use least-idle tty, not the first
                 * one we find in utmp. 10/9/90 Spike@world.std.com
                 * (loosely based on Jim Frost jimf@saber.com code)
                 */
		
                while ((flag = utmp_read(fd, namebuf, linetmp, hostbuf,
					 sizeof(hostbuf))) == 0)
		    {
			if (StrEq(namebuf,user))
			    {
				(void)irc_sprintf(ttyname,"/dev/%s",linetmp);
				if (stat(ttyname,&stb) == -1)
				    {
					sendto_one(sptr,
						   err_str(ERR_FILEERROR),
						   me.name, sptr->name,
						   "stat", ttyname);
					return 0;
				    }
				if (!ltime)
				    {
					ltime= stb.st_mtime;
					(void)strcpy(linebuf,linetmp);
				    }
				else if (stb.st_mtime > ltime) /* less idle */
				    {
					ltime= stb.st_mtime;
					(void)strcpy(linebuf,linetmp);
				    }
			    }
		    }
#  endif
		(void)utmp_close(fd);
#  ifdef LEAST_IDLE
                if (ltime == 0)
#  else
		if (flag == -1)
#  endif
			sendto_one(sptr, err_str(ERR_NOLOGIN),
				   me.name, parv[0], user);
		else
			summon(sptr, user, linebuf, chname);
#else
		sendto_one(sptr, err_str(ERR_SUMMONDISABLED),
			   me.name, parv[0]);
#endif /* ENABLE_SUMMON */
	    }
	return 0;
}


/*
** m_stats
**	parv[0] = sender prefix
**	parv[1] = statistics selector (defaults to Message frequency)
**	parv[2] = server name (current server defaulted, if omitted)
**
**	Currently supported are:
**		M = Message frequency (the old stat behaviour)
**		L = Local Link statistics
**              C = Report C and N configuration lines
*/
/*
** m_stats/stats_conf
**    Report N/C-configuration lines from this server. This could
**    report other configuration lines too, but converting the
**    status back to "char" is a bit akward--not worth the code
**    it needs...
**
**    Note:   The info is reported in the order the server uses
**            it--not reversed as in ircd.conf!
*/

static	int report_array[11][3] = {
		{ CONF_CONNECT_SERVER,    RPL_STATSCLINE, 'C'},
		{ CONF_NOCONNECT_SERVER,  RPL_STATSNLINE, 'N'},
		{ CONF_CLIENT,            RPL_STATSILINE, 'I'},
		{ CONF_KILL,              RPL_STATSKLINE, 'K'},
		{ CONF_QUARANTINED_SERVER,RPL_STATSQLINE, 'Q'},
		{ CONF_LEAF,		  RPL_STATSLLINE, 'L'},
		{ CONF_OPERATOR,	  RPL_STATSOLINE, 'O'},
		{ CONF_HUB,		  RPL_STATSHLINE, 'H'},
		{ CONF_LOCOP,		  RPL_STATSOLINE, 'o'},
		{ CONF_SERVICE,		  RPL_STATSSLINE, 'S'},
		{ 0, 0}
				};

static	void	report_configured_links(sptr, mask)
aClient *sptr;
int	mask;
{
	static	char	null[] = "<NULL>";
	aConfItem *tmp;
	int	*p, port;
	char	c, *host, *pass, *name;
	char	string[10];
	
	for (tmp = conf; tmp; tmp = tmp->next)
		if (tmp->status & mask)
		    {
			for (p = &report_array[0][0]; *p; p += 3)
				if (*p == tmp->status)
					break;
			if (!*p)
				continue;
			c = (char)*(p+2);
			host = BadPtr(tmp->host) ? null : tmp->host;
			pass = BadPtr(tmp->passwd) ? null : tmp->passwd;
			name = BadPtr(tmp->name) ? null : tmp->name;
			port = (int)tmp->port;
			*string = (char) 0;
			if (IsNoTilde(tmp))
				strcat(string, "-");
			if (IsLimitIp(tmp))
				strcat(string, "!");
			if (IsNeedIdentd(tmp))
				strcat(string, "+");
			if (IsPassIdentd(tmp))
				strcat(string, "#");
			/*
			 * On K line the passwd contents can be
			/* displayed on STATS reply. 	-Vesa
			 */
			if (tmp->status == CONF_KILL)
				sendto_one(sptr, rpl_str(p[1]), me.name,
					   sptr->name, c, host, pass,
					   name, port, get_conf_class(tmp));
			else if (mask == CONF_CLIENT)
				sendto_one(sptr, rpl_str(p[1]), me.name,
					sptr->name, c, host, string,
					name, port,
					get_conf_class(tmp));
			else
				sendto_one(sptr, rpl_str(p[1]), me.name,
					sptr->name, c, host, name, port,
					get_conf_class(tmp));
		    }
	return;
}

int	m_stats(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	static	char	Lformat[]  = ":%s %d %s %s %u %u %u %u %u :%u";
	static	char	CSformat[] = ":%s %d %s %s %u %u %u %u %u %u :%s";
	struct	Message	*mptr;
	aClient	*acptr, *tmpptr;
	char	stat = parc > 1 ? parv[1][0] : '\0';
	Reg1	int	i;
	int	doall = 0, wilds = 0, j, num = 0;
	char	*name, *user, *host;
	char	buffer[512];

	if (check_registered(sptr))
		return 0;

	if (hunt_server(cptr,sptr,":%s STATS %s :%s",2,parc,parv)!=HUNTED_ISME)
		return 0;
        if (parc < 2)
        {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                           me.name, parv[0], "TOPIC");
                return 0;
        }
	if (parc > 3)
	{
		name = parv[3];
		if (!mycmp(name, me.name))
			doall = 2;
		else if (matches(name, me.name) == 0)
			doall = 1;
		if (index(name, '*') || index(name, '?'))
			wilds = 1;
	}
	else
	{
		name = me.name;
		if (parc > 2)
			doall = 2;
	}
#ifdef STATS_NOTICE
	if ((stat != (char) 0) && sptr->user)
		sendto_flagops(OPERS,"STATS %c requested by %s (%s@%s)", stat,
			sptr->name, sptr->user->username, sptr->user->host);
#endif 

#define Meg 1024
#define Gig (Meg*1024)
#define Tera (Gig*1024)
#define IsMeg(x) (x>Meg)
#define IsGig(x) (x>Gig)
#define IsTera(x) (x>Tera)
#define ByteString(x) (IsTera(x) ? "terabytes" : \
			IsGig(x) ? "gigabytes" : \
			IsMeg(x) ? "megabytes" : \
			"Kilobytes")
#define ByteConvert(x) (IsTera(x) ? (float)((float)x/(float)Tera) : \
			IsGig(x) ? (float)((float)x/(float)Gig) : \
			IsMeg(x) ? (float)((float)x/(float)Meg) : \
			(float)x)

	switch (stat)
	{
	case '?':
	{
		int tottime = 0;
		long int totsent = 0, totrecv = 0, totsentb = 0, totrecvb = 0;
		int uptime = NOW - me.firsttime;
#ifndef DOG3
		for(i=0;i<=highest_fd;i++)
		{
			if (!(acptr=local[i]) || !IsServer(acptr))
				continue;
#else
                for (i=serv_fdlist.entry[j=1];j<=serv_fdlist.last_entry;
                        i=serv_fdlist.entry[++j])
		{
			if (!(acptr=local[i]))
				continue;
#endif
                        sendto_one(sptr, CSformat, me.name,
                                   RPL_STATSLINKINFO, parv[0],
                                   get_client_name(acptr, TRUE),
                                   (int)DBufLength(&acptr->sendQ),
                                   (int)acptr->sendM, (int)acptr->sendK,
                                   (int)acptr->receiveM, (int)acptr->receiveK,
                                   NOW - acptr->firsttime, DoesTS(acptr) ?
						"TS" : "NoTS");
			tottime += (NOW - acptr->firsttime);
			totsent += acptr->sendK;
			totrecv += acptr->receiveK;
			totsentb += acptr->sendB;
			totrecvb += acptr->receiveB;
			if (totsentb > 1023)
			{
				totsent += (totsentb >> 10);
				totsentb &= 0x3ff;
			}
			if (totrecvb > 1023)
			{
				totrecv += (totrecvb >> 10);
				totrecvb &= 0x3ff;
			}
		} 
		if (uptime)
		{
		sendto_one(sptr, ":%s %d %s :Since startup, I've:",
			me.name, RPL_STATSDEBUG, parv[0]);
		sendto_one(sptr, ":%s %d %s :Sent a total of %0.3f %s at a rate of %0.2f kilobytes per second.",
			me.name, RPL_STATSDEBUG, parv[0],
			ByteConvert(me.sendK), ByteString(me.sendK),
			(float)((float)me.sendK/(float)uptime));
		sendto_one(sptr, ":%s %d %s :Received a total of %0.3f %s at a rate of %0.2f kilobytes per second.",
			me.name, RPL_STATSDEBUG, parv[0],
			ByteConvert(me.receiveK), ByteString(me.receiveK),
			(float)((float)me.receiveK/(float)uptime));
		}
		if (tottime)
		{
			sendto_one(sptr, ":%s %d %s :The current averages for servers:",
			me.name, RPL_STATSDEBUG, parv[0]);
			sendto_one(sptr, ":%s %d %s :Sent a total of %0.3f %s at a rate of %0.2f kilobytes per second.",
				me.name, RPL_STATSDEBUG, parv[0],
				ByteConvert(totsent), ByteString(totsent),
				(float)((float)totsent/(float)tottime));
			sendto_one(sptr, ":%s %d %s :Received a total of %0.3f %s at a rate of %0.2f kilobytes per second.",
				me.name, RPL_STATSDEBUG, parv[0],
				ByteConvert(totrecv), ByteString(totrecv),
				(float)((float)totrecv/(float)tottime));
		}
		break;
	}
	case 'L' : case 'l' :
		/*
		 * send info about connections which match, or all if the
		 * mask matches me.name.  Only restrictions are on those who
		 * are invisible not being visible to 'foreigners' who use
		 * a wild card based search to list it.
		 */
		acptr = NULL;
		if (parc > 2)
			acptr = find_chasing(NULL, parv[2], NULL);
		if (acptr && IsPerson(acptr))
		{
gohere:
			if (MyConnect(acptr))
			sendto_one(sptr, Lformat, me.name, RPL_STATSLINKINFO,
				parv[0], isupper(stat) ?
					get_client_name(acptr, TRUE) :
					get_client_name(acptr, FALSE),
				(int)DBufLength(&cptr->sendQ),
				(int)acptr->sendM, (int)acptr->sendK,
				(int)acptr->receiveM, (int)acptr->receiveK,
				NOW - acptr->firsttime);
			break;
		}
		acptr = find_chasing(NULL, me.name, NULL);
		if (acptr && IsPerson(acptr))
			goto gohere;
		for (i = 0; i <= highest_fd; i++)
		    {
			if (!(acptr = local[i]))
				continue;
			if (IsInvisible(acptr) && (doall || wilds) &&
			    !(MyConnect(sptr) && IsOper(sptr)) &&
			    !IsAnOper(acptr) && (acptr != sptr))
				continue;
			if (!doall && wilds && matches(name, acptr->name))
				continue;
			if (!(doall || wilds) && mycmp(name, acptr->name))
				continue;
			sendto_one(sptr, Lformat, me.name,
				   RPL_STATSLINKINFO, parv[0],
				   (isupper(stat)) ?
				   get_client_name(acptr, TRUE) :
				   get_client_name(acptr, FALSE),
				   (int)DBufLength(&acptr->sendQ),
				   (int)acptr->sendM, (int)acptr->sendK,
				   (int)acptr->receiveM, (int)acptr->receiveK,
				   NOW - acptr->firsttime);
		    }
		break;
#ifdef B_LINES
	case 'B' : case 'b' :
		report_conf_links(sptr, &BList1, RPL_STATSBLINE, 'B',
			NULL, NULL, NULL);
		report_conf_links(sptr, &BList2, RPL_STATSBLINE, 'B',
			NULL, NULL, NULL);
		report_conf_links(sptr, &BList3, RPL_STATSBLINE, 'B',
			NULL, NULL, NULL);
	break;
#endif
	case 'C' : case 'c' :
                report_configured_links(sptr, CONF_CONNECT_SERVER|
					CONF_NOCONNECT_SERVER);
		break;
#ifdef E_LINES
	case 'E' : case 'e' :
		report_conf_links(sptr, &EList1, RPL_STATSELINE, 'E',
                        NULL, NULL, NULL);
		report_conf_links(sptr, &EList2, RPL_STATSELINE, 'E',
                        NULL, NULL, NULL);
		report_conf_links(sptr, &EList3, RPL_STATSELINE, 'E',
                        NULL, NULL, NULL);
	break;
#endif 
	case 'H' : case 'h' :
                report_configured_links(sptr, CONF_HUB|CONF_LEAF);
		break;
	case 'I' : case 'i' :
		report_configured_links(sptr, CONF_CLIENT);
		break;
	case 'K' : case 'k' :
		tmpptr = NULL;
		user = host = NULL;
#ifdef RESTRICT_STATSK
		if (name == me.name)
		{
			if (!IsAnOper(sptr))
				tmpptr = sptr;
				
			else
				tmpptr = NULL;
		}
		else if (!strchr(name, '@') && !strchr(name, '.'))
		{
			if (!(tmpptr = find_chasing(sptr, name, NULL)))
				return 0;
		}
		if (!tmpptr && (me.name != name))
		{
			char *ptr;

			strcpy(buffer, name);
			ptr = strchr(buffer, '@');
			if (!ptr)
			{
				user = "*";
				host = buffer;
			}
			else
			{
				*ptr = (char) 0;
				user = buffer;
				host = ptr+1;
			}
		}			
		if (tmpptr && tmpptr->user)
		{
			user = tmpptr->user->username;
			host = tmpptr->user->host;
		}
		if (user)
		sendto_one(sptr, ":%s NOTICE %s :Finding bans matching %s@%s",
			me.name, parv[0], user, host);
#endif
		report_conf_links(sptr, &KList1, RPL_STATSKLINE, 'K',
			user, host, &num);
                report_conf_links(sptr, &KList2, RPL_STATSKLINE, 'K',
			user, host, &num);
                report_conf_links(sptr, &KList3, RPL_STATSKLINE, 'K',
			user, host, &num);
#ifdef RESTRICT_STATSK
		if (!num && user)
		   sendto_one(sptr,":%s NOTICE %s :No bans for %s@%s found.",
			me.name, parv[0], user, host);
#endif
		break;
	case 'M' : case 'm' :
		for (mptr = msgtab; mptr->cmd; mptr++)
			if (mptr->count)
				sendto_one(sptr, rpl_str(RPL_STATSCOMMANDS),
					   me.name, parv[0], mptr->cmd,
					   mptr->count, mptr->bytes);
		break;
	case 'o' : case 'O' :
		report_configured_links(sptr, CONF_OPS);
		break;
	case 'Q' : case 'q' :
		report_configured_links(sptr, CONF_QUARANTINED_SERVER);
		break;
	case 'R' : case 'r' :
#ifdef DEBUGMODE
		send_usage(sptr,parv[0]);
#endif
		break;
	case 'S' : case 's' :
		report_configured_links(sptr, CONF_SERVICE);
		break;
	case 'T' : case 't' :
		tstats(sptr, parv[0]);
		break;
	case 'U' : case 'u' :
	    {
		register time_t now;

		now = NOW - me.since;
		sendto_one(sptr, rpl_str(RPL_STATSUPTIME), me.name, parv[0],
			   now/86400, (now/3600)%24, (now/60)%60, now%60);
#ifdef HIGHEST_CONNECTION
                sendto_one(sptr, rpl_str(RPL_STATSCONN), me.name, parv[0],
                           max_connection_count, max_client_count);
#endif 
		break;
	    }
	case 'X' : case 'x' :
#ifdef	DEBUGMODE
		send_listinfo(sptr, parv[0]);
#endif
		break;
	case 'Y' : case 'y' :
		report_classes(sptr);
		break;
	case 'Z' : case 'z' :
		count_memory(sptr, parv[0]);
		break;
	default :
		stat = '*';
		break;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFSTATS), me.name, parv[0], stat);
	return 0;
    }

/*
** m_users
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_users(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
#ifdef ENABLE_USERS
	char	namebuf[10],linebuf[10],hostbuf[17];
	int	fd, flag = 0;
#endif

	if (check_registered_user(sptr))
		return 0;

	if (hunt_server(cptr,sptr,":%s USERS :%s",1,parc,parv) == HUNTED_ISME)
	    {
#ifdef ENABLE_USERS
		if ((fd = utmp_open()) == -1)
		    {
			sendto_one(sptr, err_str(ERR_FILEERROR),
				   me.name, parv[0], "open", UTMP);
			return 0;
		    }

		sendto_one(sptr, rpl_str(RPL_USERSSTART), me.name, parv[0]);
		while (utmp_read(fd, namebuf, linebuf,
				 hostbuf, sizeof(hostbuf)) == 0)
		    {
			flag = 1;
			sendto_one(sptr, rpl_str(RPL_USERS), me.name, parv[0],
				   namebuf, linebuf, hostbuf);
		    }
		if (flag == 0) 
			sendto_one(sptr, rpl_str(RPL_NOUSERS),
				   me.name, parv[0]);

		sendto_one(sptr, rpl_str(RPL_ENDOFUSERS), me.name, parv[0]);
		(void)utmp_close(fd);
#else
		sendto_one(sptr, err_str(ERR_USERSDISABLED), me.name, parv[0]);
#endif
	    }
	return 0;
}

/*
** Note: At least at protocol level ERROR has only one parameter,
** although this is called internally from other functions
** --msa
**
**	parv[0] = sender prefix
**	parv[*] = parameters
*/
int	m_error(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	Reg1	char	*para;

	para = (parc > 1 && *parv[1] != '\0') ? parv[1] : "<>";

	Debug((DEBUG_ERROR,"Received ERROR message from %s: %s",
	      sptr->name, para));
	/*
	** Ignore error messages generated by normal user clients
	** (because ill-behaving user clients would flood opers
	** screen otherwise). Pass ERROR's from other sources to
	** the local operator...
	*/
	if (IsPerson(cptr) || IsUnknown(cptr) || IsService(cptr))
		return 0;
	if (cptr == sptr)
		sendto_ops("ERROR :from %s -- %s",
			   get_client_name(cptr, FALSE), para);
	else
		sendto_ops("ERROR :from %s via %s -- %s", sptr->name,
			   get_client_name(cptr,FALSE), para);
	return 0;
    }

/*
** m_help
**	parv[0] = sender prefix
*/
int	m_help(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	int i;

	for (i = 0; msgtab[i].cmd; i++)
		sendto_one(sptr,":%s NOTICE %s :%s",
			   me.name, parv[0], msgtab[i].cmd);
	return 0;
    }

/*
 * parv[0] = sender
 * parv[1] = host/server mask.
 * parv[2] = server to query
 */
int	 m_lusers(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aClient *acptr;

	if (check_registered_user(sptr))
		return 0;

	if (parc > 2)
		if(hunt_server(cptr, sptr, ":%s LUSERS %s :%s", 2, parc, parv)
				!= HUNTED_ISME)
			return 0;
	if (parc > 3)
	{
		(void)collapse(parv[1]);
		compute_lusers(parv[1]);
	}
	else
	{
		s_ct = s_count;
		c_ct = c_count;
		i_ct = i_count;
		o_ct = o_count;
		m_cs = m_clients;
		m_ss = m_servers;
		u_ct = 0;
	}
		
#ifndef	SHOW_INVISIBLE_LUSERS
	if (IsAnOper(sptr) && i_ct)
#endif
	sendto_one(sptr, rpl_str(RPL_LUSERCLIENT), me.name, parv[0],
		   c_ct-i_ct, i_ct, s_ct);
#ifndef	SHOW_INVISIBLE_LUSERS
	else
		sendto_one(sptr,
			":%s %d %s :There are %d users on %d servers", me.name,
			    RPL_LUSERCLIENT, parv[0], c_ct-i_ct, s_ct);
#endif
	if (o_ct)
		sendto_one(sptr, rpl_str(RPL_LUSEROP),
			   me.name, parv[0], o_ct);
	if (u_ct > 0)
		sendto_one(sptr, rpl_str(RPL_LUSERUNKNOWN),
			   me.name, parv[0], u_ct);
	if (ch_count > 0)
		sendto_one(sptr, rpl_str(RPL_LUSERCHANNELS),
			   me.name, parv[0], ch_count);
	sendto_one(sptr, rpl_str(RPL_LUSERME),
		   me.name, parv[0], m_cs, m_ss);
#ifdef HIGHEST_CONNECTION
	sendto_one(sptr, rpl_str(RPL_STATSCONN), me.name, parv[0],
                          max_connection_count, max_client_count);
#endif
	return 0;
}
 
/***********************************************************************
 * m_connect() - Added by Jto 11 Feb 1989
 ***********************************************************************/

/*
** m_connect
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = port number
**	parv[3] = remote server
*/
int	m_connect(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	int	port, tmpport, retval;
	aConfItem *aconf;
	aClient *acptr;

	if (!IsPrivileged(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return -1;
	    }

	if (IsLocOp(sptr) && parc > 3)	/* Only allow LocOps to make */
		return 0;		/* local CONNECTS --SRB      */

	if (hunt_server(cptr,sptr,":%s CONNECT %s %s :%s",
		       3,parc,parv) != HUNTED_ISME)
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "CONNECT");
		return -1;
	    }

	if ((acptr = find_server(parv[1], NULL)))
	    {
		sendto_one(sptr, ":%s NOTICE %s :Connect: Server %s %s %s.",
			   me.name, parv[0], parv[1], "already exists from",
			   acptr->from->name);
		return 0;
	    }

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status == CONF_CONNECT_SERVER &&
		    matches(parv[1], aconf->name) == 0)
		  break;
	/* Checked first servernames, then try hostnames. */
	if (!aconf)
        	for (aconf = conf; aconf; aconf = aconf->next)
                	if (aconf->status == CONF_CONNECT_SERVER &&
                            (matches(parv[1], aconf->host) == 0 ||
                             matches(parv[1], index(aconf->host, '@')+1) == 0))
                  		break;

	if (!aconf)
	    {
	      sendto_one(sptr,
			 "NOTICE %s :Connect: Host %s not listed in irc.conf",
			 parv[0], parv[1]);
	      return 0;
	    }
	/*
	** Get port number from user, if given. If not specified,
	** use the default form configuration structure. If missing
	** from there, then use the precompiled default.
	*/
	tmpport = port = aconf->port;
	if (parc > 2 && !BadPtr(parv[2]))
	    {
		if ((port = atoi(parv[2])) <= 0)
		    {
			sendto_one(sptr,
				   "NOTICE %s :Connect: Illegal port number",
				   parv[0]);
			return 0;
		    }
	    }
	else if (port <= 0 && (port = PORTNUM) <= 0)
	    {
		sendto_one(sptr, ":%s NOTICE %s :Connect: missing port number",
			   me.name, parv[0]);
		return 0;
	    }
	/*
	** Notify all operators about remote connect requests
	*/
	if (!IsAnOper(cptr))
	    {
		sendto_ops_butone(NULL, &me,
				  ":%s WALLOPS :Remote CONNECT %s %s from %s",
				   me.name, parv[1], parv[2] ? parv[2] : "",
				   get_client_name(sptr,FALSE));
#if defined(USE_SYSLOG) && defined(SYSLOG_CONNECT)
		syslog(LOG_DEBUG, "CONNECT From %s : %s %d", parv[0], parv[1], parv[2] ? parv[2] : "");
#endif
	    }
	aconf->port = port;
	switch (retval = connect_server(aconf, sptr, NULL))
	{
	case 0:
		sendto_one(sptr,
			   ":%s NOTICE %s :*** Connecting to %s[%s].",
			   me.name, parv[0], aconf->host, aconf->name);
		break;
	case -1:
		sendto_one(sptr, ":%s NOTICE %s :*** Couldn't connect to %s.",
			   me.name, parv[0], aconf->host);
		break;
	case -2:
		sendto_one(sptr, ":%s NOTICE %s :*** Host %s is unknown.",
			   me.name, parv[0], aconf->host);
		break;
	default:
		sendto_one(sptr,
			   ":%s NOTICE %s :*** Connection to %s failed: %s",
			   me.name, parv[0], aconf->host, strerror(retval));
	}
	aconf->port = tmpport;
	return 0;
    }

/*
** m_wallops (write to *all* opers currently online)
**	parv[0] = sender prefix
**	parv[1] = message text
*/
int	m_wallops(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	char	*message, *pv[4];

	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "WALLOPS");
		return 0;
	    }

	if (!IsServer(sptr))
	    {
		pv[0] = parv[0];
		pv[1] = "#wallops";
		pv[2] = message;
		pv[3] = NULL;
		return m_private(cptr, sptr, 3, pv);
	    }
	sendto_ops_butone(IsServer(cptr) ? cptr : NULL, sptr,
			":%s WALLOPS :%s", parv[0], message);
#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_WALLOP, sptr, ":%s WALLOP :%s",
				parv[0], message);
#endif
	return 0;
    }

/*
** m_time
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_time(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	if (check_registered_user(sptr))
		return 0;
	if (hunt_server(cptr,sptr,":%s TIME :%s",1,parc,parv) == HUNTED_ISME)
		sendto_one(sptr, rpl_str(RPL_TIME), me.name,
			   parv[0], me.name, date((long)0));
	return 0;
    }


/*
** m_admin
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_admin(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aConfItem *aconf;

	if (check_registered(sptr))
		return 0;

	if (hunt_server(cptr,sptr,":%s ADMIN :%s",1,parc,parv) != HUNTED_ISME)
		return 0;
	if ((aconf = find_admin()))
	    {
		sendto_one(sptr, rpl_str(RPL_ADMINME),
			   me.name, parv[0], me.name);
		sendto_one(sptr, rpl_str(RPL_ADMINLOC1),
			   me.name, parv[0], aconf->host);
		sendto_one(sptr, rpl_str(RPL_ADMINLOC2),
			   me.name, parv[0], aconf->passwd);
		sendto_one(sptr, rpl_str(RPL_ADMINEMAIL),
			   me.name, parv[0], aconf->name);
	    }
	else
		sendto_one(sptr, err_str(ERR_NOADMININFO),
			   me.name, parv[0], me.name);
	return 0;
    }

#ifdef QUOTE_KLINE

int numchar(string, c)
char *string;
char c;
{
        int num = 0;
        while (*string)
        {
                if (tolower(*string) == tolower(c))
                        num++;
                string++;
        }
        return num;
}

char *right(string, num)
char *string;
int num;
{
        if (strlen(string) < num)
                return string;
        return (string+strlen(string)-num);
}

char *my_stristr(s1, s2)
     char *s1;
     char *s2;
{
  char n1[1024], n2[1024];
  char *temp, *ptr1, *ptr2;

  ptr1 = n1;
  ptr2 = n2;
  if (!s1 || !s2)
    return NULL;
  while (*s1)
    *(ptr1++) = toupper(*(s1++));
  while (*s2)
    *(ptr2++) = toupper(*(s2++));
  *ptr1 = '\0';
  *ptr2 = '\0';
  temp = strstr(n1, n2);
  if (temp)
    return (s1 + (temp-n1));
  return NULL;
}

char *cluster(hostname)
char *hostname;
{
        static char result[1024];
        char        temphost[255];
        char        *host;

        if (!hostname)
                return (char *) 0;
        host = temphost;
        strcpy(result, "");
        if (strchr(hostname, '@'))
        {
            strcpy(result, hostname);
            *strchr(result, '@') = '\0';
            strcat(result, "@");
            if (!(hostname = strchr(hostname, '@')))
                return (char *) 0;
            hostname++;
        }
        strcpy(host, hostname);

        if (isdigit(*(host+strlen(host)-1)))
        {
                int i;
                char *tmp;
                tmp = host;
                for (i=0;i<2;i++)
                        tmp = strchr(tmp, '.')+1;
                *tmp = '\0';
                strcat(result, host);
                strcat(result, "*.*");
        }
        else
        {
                char    *tmp;
                int     num;

                num = 1;
                tmp = right(host, 3);
                if (mycmp(tmp, "com") &&
                        mycmp(tmp, "edu") &&
                        (my_stristr(host, "com") ||
                                my_stristr(host, "edu")))
                        num = 2;
                while (host && *host && (numchar(host, '.') > num))
                {
                        if ((host = strchr(host, '.')) != NULL)
                                host++;
                        else
                                return (char *) NULL;
                }
                strcat(result, "*");
                if (mycmp(host, temphost))
                        strcat(result, ".");
                strcat(result, host);
        }
        return result;
}

#ifdef ALLOW_KLINES_FROM_SERVERS
int rmatches(string, specs)
char *string;
char *specs;
{
	char buffer[1024];
	char *ptr, *ptr2;
	int ok=1;

	if (!specs || !*specs)
		return 1;
	strcpy(buffer, specs);
	ptr = buffer;
	do
	{
		ptr2 = strchr(ptr, ' ');
		if (ptr2)
		{
			*ptr2 = (char) 0;
			ptr2++;
		}
		ok = matches(ptr, string);
		ptr = ptr2;
	} while (ok && ptr);
	return ok;
}

#endif /* ALLOW_KLINES_FROM_SERVERS */

int     m_kline(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{
        int out;
	char buffer[1024];
        char *user, *host, *reason;
        aClient *acptr;
        char tempuser[30];
        char temphost[512];
	aConfItem *aconf;
	char timebuffer[20];
	char filenamebuf[1024];
	char *filename;
	struct tm *tmptr;

#ifdef NO_LOCAL_KLINE
	if (!IsOper(sptr))
#else
	if (!IsAnOper(sptr))
#endif
	{
                sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
                return 0;
	}
        if (!parv[1] || (!MyClient(sptr) && (!parv[2] || !parv[3])))
        {
		if (MyClient(sptr))
			sendto_one(sptr, ":%s NOTICE %s :Not enough parameters",
				me.name, parv[0]);
                return 0;
        }
	if (MyClient(sptr))
	{
        if (strchr(parv[1], '@') || *parv[1] == '*')
        {
		if (strchr(parv[1], '@'))
		{
	                user = parv[1];
			host = strchr(parv[1], '@');
			*(host++) = '\0';
		}
		else
		{
			user = "*";
			host = parv[1];
		}
                if (!*host)
                        host = "*";
		strcpy(tempuser, user);
                strcpy(temphost, host);
                user = tempuser;
                host = temphost;
        }
        else /* strchr(...) || ... */
        {
                if (!(acptr = find_chasing(sptr, parv[1], NULL)))
                        return 0;
		if (IsServer(acptr))
		{
			sendto_one(sptr, ":%s NOTICE %s :Can't KLINE a server, use @'s where appropriate",
				me.name, parv[0]);
			return 0;
		}
                strcpy(tempuser, "*");
		if (*acptr->user->username == '~')
			strcat(tempuser, (char *)acptr->user->username+1);
		else
	                strcat(tempuser, acptr->user->username);
                user = tempuser;
                host = cluster(acptr->user->host);
        }
	reason = parv[2];
	} /* if (MyClient(sptr)) */
	else
	{
		user = parv[1];
		host = parv[2];
		reason = parv[3];
	}
	reason = (reason && *reason) ? reason : "No reason";
        if (!matches(user, "akjhfkahfasfjd") &&
                !matches(host, "ldksjfl.kss...kdjfd.jfklsjf"))
        {
                sendto_one(sptr, ":%s NOTICE %s :Can't K-Line *@*", me.name,
                        parv[0]);
                return 0;
        }
#ifdef PASS_KLINES
	sendto_serv_butone(MyClient(sptr) ? NULL : cptr,
		":%s KLINE %s %s :%s", parv[0], user, host, reason);
#endif
#ifdef ALLOW_KLINES_FROM_SERVERS
        if (!MyClient(sptr))
        {
                if (IsPerson(sptr) && IsOper(sptr) && parv[1] &&
                        *parv[1] && parv[2] && *parv[2] &&
                        !rmatches(sptr->user->server, ALLOWED_KLINES))
			;
                else
                        return 0;
        }
#endif
#ifdef NO_REDUNDANT_KLINES
	if (test_kline_userhost(sptr, &KList1, user, host) ||
		test_kline_userhost(sptr, &KList2, user, host) ||
		test_kline_userhost(sptr, &KList3, user, host))
		return 0;
#endif
	aconf = make_conf();
	aconf->status = CONF_KILL;
	DupString(aconf->host, host);
	if (reason)
		sprintf(buffer, "%s", reason);
	DupString(aconf->passwd, reason ? buffer : "");
	DupString(aconf->name, user);
	aconf->port = 0;
	Class(aconf) = find_class(0);
	if ((aconf->status & CONF_KILL) && aconf->host)
	{
		char    *host = host_field(aconf);

		switch (sortable(host))
		{
			case 0 :
				l_addto_conf_list(&KList3, aconf, host_field);
				break;
			case 1 :
				addto_conf_list(&KList1, aconf, host_field);
				break;
			case -1 :
				addto_conf_list(&KList2, aconf, rev_host_field);
				break;
		}
		MyFree(host);
	}
	rehashed = 1; /* Forces looping thru clients to check k-lines */
	if (MyClient(sptr))
        sendto_flagops(OPERS,"%s added K-Line for [%s@%s]: %s", parv[0], user,
		host, reason && *reason ? reason : "No reason");
	else
	sendto_flagops(OPERS,"%s from %s added K-Line for [%s@%s]: %s",
		parv[0], sptr->user->server, user, host,
		reason && *reason ? reason:"No reason");
#ifdef PUT_KLINES_IN_IRCD_CONF
	filename = configfile;
#elif defined(SEPARATE_QUOTE_KLINES_BY_DATE)
	tmptr = localtime(&NOW);	
	strftime(timebuffer, 20, "%y%m%d", tmptr);
	sprintf(filenamebuf, "%s.%s", klinefile, timebuffer);
	filename = filenamebuf;
#else
	filename = klinefile;
#endif
        if ((out = open(filename, O_RDWR|O_APPEND|O_CREAT))==-1)
        {
                sendto_one(sptr, ":%s NOTICE %s :Problem opening server configfile", me.name, parv[0]);
                return 0;
        }
#ifdef SEPARATE_QUOTE_KLINES_BY_DATE
	fchmod(out, 432);
#endif
	irc_sprintf(buffer, "#%s!%s@%s K'd: %s@%s: %s\n",
		sptr->name, sptr->user->username,
		sptr->user->host, user, host,
		(reason && *reason) ? reason : "No reason");
	if (write(out, buffer, strlen(buffer)) <= 0)
	{
		sendto_one(sptr, ":%s NOTICE %s :Problem writing to the configfile", me.name, parv[0]);
		close(out);
		return 0;
	}
        irc_sprintf(buffer, "K:%s:%s%s:%s\n", host,
		(reason && *reason) ? "" : "",
		(reason && *reason) ? reason : "", user);
	if (write(out, buffer, strlen(buffer)) <= 0)
		sendto_one(sptr, ":%s NOTICE %s :Problem writing to the configfile", me.name, parv[0]);
        close(out);
        sendto_one(sptr, ":%s NOTICE %s :Added K-Line [%s@%s] to configfile", me.name, parv[0], user, host);

	return 0;
}

#endif /* QUOTE_KLINE */

#ifdef SEPARATE_QUOTE_KLINES_BY_DATE

/*
** m_rehashadd
**
*/
int     m_rehashadd(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{
	struct tm *tmptr;
	char timebuffer[25];
	char filename[1024];

#ifndef LOCOP_REHASH
        if (!MyClient(sptr) || !IsOper(sptr))
#else
# ifdef OPER_REHASH
        if (!MyClient(sptr) || !IsAnOper(sptr))
# else
        if (!MyClient(sptr) || !IsLocOp(sptr))
# endif
#endif
            {
                sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
                return 0;
            }
        sendto_one(sptr, rpl_str(RPL_REHASHING), me.name, parv[0], configfile);
        sendto_ops("%s is rehashing Server config file", parv[0]);
#ifdef USE_SYSLOG
        syslog(LOG_INFO, "REHASHADD From %s\n", get_client_name(sptr, FALSE));
#endif
        (void)rehash(cptr, sptr, (parc > 1) ? ((*parv[1] == 'q')?2:0) : 0);
        tmptr = localtime(&NOW);
        strftime(timebuffer, 20, "%y%m%d", tmptr);
        sprintf(filename, "%s.%s", klinefile, timebuffer);
        if (initconf(0,filename))
                sendto_one(sptr,":%s NOTICE %s :There was a problem opening %s",
			me.name, parv[0], filename);
	else
	{
		sendto_one(sptr, ":%s NOTICE %s :K-lines were loaded from %s",
			me.name, parv[0], filename); 
		sendto_flagops(OPERS, "%s loaded K-lines from %s",
			parv[0], filename);
	}
	rehashed = 1;
	return 0;
}

/*
** m_addklines
**
*/
int	m_addklines(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{
	char buffer[1024];

	if (!MyClient(sptr) || !IsAnOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (parc < 2 || (!parv[1] || !*parv[1]))
	{
		sendto_one(sptr, ":%s NOTICE %s :No date given to load",
			me.name, parv[0]);
		return 0;
	}
	sprintf(buffer, "%s.%s", KLINEFILE, parv[1]);
        sendto_one(sptr, ":%s NOTICE %s :Loading K-lines from %s", me.name, parv[0], buffer);
	if (initconf(0,buffer))
		sendto_one(sptr, ":%s NOTICE %s :There was a problem opening the file", me.name, parv[0]);
	else
	{
		sendto_flagops(OPERS, "%s loaded K-lines from %s",
			parv[0], buffer);
		sendto_one(sptr, ":%s NOTICE %s :K-lines have been loaded",
			me.name, parv[0]);
	}
	rehashed = 1;
	return 0;
}

#endif /* SEPARATE_QUOTE_KLINES_BY_DATE */

#if defined(OPER_REHASH) || defined(LOCOP_REHASH)
/*
** m_rehash
**
*/
int	m_rehash(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
#ifndef	LOCOP_REHASH
	if (!MyClient(sptr) || !IsOper(sptr))
#else
# ifdef	OPER_REHASH
	if (!MyClient(sptr) || !IsAnOper(sptr))
# else
	if (!MyClient(sptr) || !IsLocOp(sptr))
# endif
#endif
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
#ifdef SEPARATE_QUOTE_KLINES_BY_DATE
	return m_rehashadd(cptr, sptr, parc, parv);
#else
	sendto_one(sptr, rpl_str(RPL_REHASHING), me.name, parv[0], configfile);
	sendto_ops("%s is rehashing Server config file", parv[0]);
#ifdef USE_SYSLOG
	syslog(LOG_INFO, "REHASH From %s\n", get_client_name(sptr, FALSE));
#endif
	return rehash(cptr, sptr, (parc > 1) ? ((*parv[1] == 'q')?2:0) : 0);
#endif
}
#endif

#if defined(OPER_RESTART) || defined(LOCOP_RESTART)
/*
** m_restart
**
*/
int	m_restart(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
#ifndef	LOCOP_RESTART
	if (!MyClient(sptr) || !IsOper(sptr))
#else
# ifdef	OPER_RESTART
	if (!MyClient(sptr) || !IsAnOper(sptr))
# else
	if (!MyClient(sptr) || !IsLocOp(sptr))
# endif
#endif
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
#ifdef USE_SYSLOG
	syslog(LOG_WARNING, "Server RESTART by %s\n",
		get_client_name(sptr,FALSE));
#endif
#ifdef BUFFERED_LOGS
        cs_buf_logs(1,"",0);
        cs_buf_logs(2,"",0);
#endif
	server_reboot();
	return 0;
}
#endif

/*
** m_trace
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_trace(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	int	i;
	Reg2	aClient	*acptr;
	aClass	*cltmp;
	char	*tname;
	int	doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
	int	cnt = 0, wilds, dow;

	if (check_registered(sptr))
		return 0;

	if (parc > 2)
		if (hunt_server(cptr, sptr, ":%s TRACE %s :%s",
				2, parc, parv))
			return 0;

	if (parc > 1)
		tname = parv[1];
	else
		tname = me.name;
	switch (hunt_server(cptr, sptr, ":%s TRACE :%s", 1, parc, parv))
	{
	case HUNTED_PASS: /* note: gets here only if parv[1] exists */
	    {
		aClient	*ac2ptr;

#ifdef DOG3
		ac2ptr = next_client_double(client, tname);
#else
		ac2ptr = next_client(client, tname);
#endif
		sendto_one(sptr, rpl_str(RPL_TRACELINK), me.name, parv[0],
			   version, debugmode, tname, ac2ptr->from->name);
		return 0;
	    }
	case HUNTED_ISME:
		break;
	default:
		return 0;
	}

	doall = (parv[1] && (parc > 1)) ? !matches(tname, me.name): TRUE;
	wilds = !parv[1] || index(tname, '*') || index(tname, '?');
	dow = wilds || doall;

	for (i = 0; i < MAXCONNECTIONS; i++)
		link_s[i] = 0, link_u[i] = 0;

	if (doall)
		for (acptr = client; acptr; acptr = acptr->next)
#ifdef	SHOW_INVISIBLE_LUSERS
			if (IsPerson(acptr))
				link_u[acptr->from->fd]++;
#else
			if (IsPerson(acptr) &&
			    (!IsInvisible(acptr) || IsOper(sptr)))
				link_u[acptr->from->fd]++;
#endif
			else if (IsServer(acptr))
				link_s[acptr->from->fd]++;

	/* report all direct connections */
	
	for (i = 0; i <= highest_fd; i++)
	    {
		char	*name;
		int	class;

		if (!(acptr = local[i])) /* Local Connection? */
			continue;
		if (IsInvisible(acptr) && dow &&
		    !(MyConnect(sptr) && IsAnOper(sptr)) &&
		    !IsAnOper(acptr) && (acptr != sptr))
			continue;
		if (!doall && wilds && matches(tname, acptr->name))
			continue;
		if (!dow && mycmp(tname, acptr->name))
			continue;
		name = get_client_name(acptr,FALSE);
		class = get_client_class(acptr);

		switch(acptr->status)
		{
		case STAT_CONNECTING:
			sendto_one(sptr, rpl_str(RPL_TRACECONNECTING), me.name,
				   parv[0], class, name);
			cnt++;
			break;
		case STAT_HANDSHAKE:
			sendto_one(sptr, rpl_str(RPL_TRACEHANDSHAKE), me.name,
				   parv[0], class, name);
			cnt++;
			break;
		case STAT_ME:
			break;
		case STAT_UNKNOWN:
			sendto_one(sptr, rpl_str(RPL_TRACEUNKNOWN),
				   me.name, parv[0], class, name);
			cnt++;
			break;
		case STAT_CLIENT:
			/* Only opers see users if there is a wildcard
			 * but anyone can see all the opers.
			 */
			if (IsAnOper(sptr)  &&
			    (MyClient(sptr) || !(dow && IsInvisible(acptr)))
			    || !dow || IsAnOper(acptr))
			    {
				if (IsAnOper(acptr))
					sendto_one(sptr,
						   rpl_str(RPL_TRACEOPERATOR),
						   me.name,
						   parv[0], class, name);
				else
					sendto_one(sptr,rpl_str(RPL_TRACEUSER),
						   me.name, parv[0],
						   class, name,
#ifdef DOG3
						acptr->priority,
#else
						0,
#endif
						NOW-acptr->lasttime);
				cnt++;
			    }
			break;
		case STAT_SERVER:
			if (acptr->serv->user)
				sendto_one(sptr, rpl_str(RPL_TRACESERVER),
					   me.name, parv[0], class, link_s[i],
					   link_u[i], name, acptr->serv->by,
					   acptr->serv->user->username,
					   acptr->serv->user->host);
			else
				sendto_one(sptr, rpl_str(RPL_TRACESERVER),
					   me.name, parv[0], class, link_s[i],
					   link_u[i], name, *(acptr->serv->by) ?
					   acptr->serv->by : "*", "*", me.name);
			cnt++;
			break;
		case STAT_SERVICE:
			sendto_one(sptr, rpl_str(RPL_TRACESERVICE),
				   me.name, parv[0], class, name);
			cnt++;
			break;
		case STAT_LOG:
			sendto_one(sptr, rpl_str(RPL_TRACELOG), me.name,
				   parv[0], LOGFILE, acptr->port);
			cnt++;
			break;
		default: /* ...we actually shouldn't come here... --msa */
			sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
				   parv[0], name);
			cnt++;
			break;
		}
	    }
	/*
	 * Add these lines to summarize the above which can get rather long
         * and messy when done remotely - Avalon
         */
       	if (!IsAnOper(sptr) || !cnt)
	    {
		if (cnt)
		{
			sendto_one(sptr,rpl_str(RPL_ENDOFTRACE),me.name,parv[0],
				tname);
			return 0;
		}
		/* let the user have some idea that its at the end of the
		 * trace
		 */
		sendto_one(sptr, rpl_str(RPL_TRACESERVER),
			   me.name, parv[0], 0, link_s[me.fd],
			   link_u[me.fd], me.name, "*", "*", me.name);
		sendto_one(sptr, rpl_str(RPL_ENDOFTRACE), me.name, parv[0],
			tname);
		return 0;
	    }
	for (cltmp = FirstClass(); doall && cltmp; cltmp = NextClass(cltmp))
		if (Links(cltmp) > 0)
			sendto_one(sptr, rpl_str(RPL_TRACECLASS), me.name,
				   parv[0], Class(cltmp), Links(cltmp));
	sendto_one(sptr, rpl_str(RPL_ENDOFTRACE), me.name, parv[0], tname);
	return 0;
    }

#ifdef BETTER_MOTD

void read_motd(filename)
char *filename;
{
	int fd;
        register aMotd *temp, *last;
        struct stat sb;
        char    line[80];
	register char *tmp;

	while(motd)
	{
		temp = motd->next;
		MyFree(motd);
		motd = temp;
	}
 

        if ((fd = open(MOTD, O_RDONLY)) == -1)
		return;
        (void)fstat(fd, &sb);
        motd_tm = localtime(&sb.st_mtime);
        (void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	last = NULL;
        while (dgets(fd, line, sizeof(line)-1) > 0)
	{
		if ((tmp = (char *)index(line,'\n')))
			*tmp = '\0';
		if ((tmp = (char *)index(line,'\r')))
			*tmp = '\0';
		temp = (aMotd *) MyMalloc(sizeof(aMotd));
		if (!temp)
			outofmemory();
		strcpy(temp->line, line);
		temp->next = NULL;
		if (!motd)
			motd = temp;
		else
			last->next = temp;
		last = temp;
	}
        (void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
        (void)close(fd);
}

#endif
				
/*
** m_motd
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_motd(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
#ifdef BETTER_MOTD
	register aMotd *temp;
	struct tm *tm;
#else
	int	fd;
	char	line[80];
	Reg1	char	 *tmp;
	struct	stat	sb;
	struct	tm	*tm;
#endif

	if (check_registered(sptr))
		return 0;

	if (hunt_server(cptr, sptr, ":%s MOTD :%s", 1,parc,parv)!=HUNTED_ISME)
		return 0;

#ifdef BETTER_MOTD
	tm = motd_tm;
	if (motd == NULL)
	{
		sendto_one(sptr, err_str(ERR_NOMOTD), me.name, parv[0]);
		return 0;
	}
	sendto_one(sptr, rpl_str(RPL_MOTDSTART), me.name, parv[0], me.name);
	if (tm)
	sendto_one(sptr, ":%s %d %s :- %d/%d/%d %d:%02d", me.name, RPL_MOTD,
		parv[0], tm->tm_mday, tm->tm_mon+1, 1900+tm->tm_year,
		tm->tm_hour, tm->tm_min);
	temp=motd;
	while(temp)
	{
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0], temp->line);
		temp=temp->next;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFMOTD), me.name, parv[0]);
	return 0;
#else

/* Here's old MOTD way of opening and reading on every client connect
   and /MOTD.  Pretty dumb if you ask me... */

	fd = open(MOTD, O_RDONLY);
	if (fd == -1)
	    {
		sendto_one(sptr, err_str(ERR_NOMOTD), me.name, parv[0]);
		return 0;
	    }
	(void)fstat(fd, &sb);
	sendto_one(sptr, rpl_str(RPL_MOTDSTART), me.name, parv[0], me.name);
	tm = localtime(&sb.st_mtime);
	sendto_one(sptr, ":%s %d %s :- %d/%d/%d %d:%02d", me.name, RPL_MOTD,
		   parv[0], tm->tm_mday, tm->tm_mon + 1, 1900 + tm->tm_year,
		   tm->tm_hour, tm->tm_min);
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	while (dgets(fd, line, sizeof(line)-1) > 0)
	    {
		if ((tmp = (char *)index(line,'\n')))
			*tmp = '\0';
		if ((tmp = (char *)index(line,'\r')))
			*tmp = '\0';
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0], line);
	    }
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	sendto_one(sptr, rpl_str(RPL_ENDOFMOTD), me.name, parv[0]);
	(void)close(fd);
	return 0;
#endif
    }

/*
** m_close - added by Darren Reed Jul 13 1992.
*/
int	m_close(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	aClient	*acptr;
	Reg2	int	i;
	int	closed = 0;

	if (!MyOper(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }

	if (parv[1] && *parv[1] && !matches(parv[1], "sdkjfkdsjf.aosidasdk.as02ks"))
		return 0; 
	for (i = highest_fd; i; i--)
	    {
		if (!(acptr = local[i]))
			continue;
		if (!IsUnknown(acptr) && !IsConnecting(acptr) &&
		    !IsHandshake(acptr))
		{
			if (parv[1] && *parv[1] && acptr->user)
			{
				if (IsAnOper(acptr) ||
					matches(parv[1], acptr->user->host))
					continue;
			}
			else
				continue;
		}
		else if (parv[1] && *parv[1])
			continue;
		sendto_one(sptr, rpl_str(RPL_CLOSING), me.name, parv[0],
			   get_client_name(acptr, TRUE), acptr->status);
		(void)exit_client(acptr, acptr, acptr, "Closing connection");
		closed++;
	    }
	sendto_one(sptr, rpl_str(RPL_CLOSEEND), me.name, parv[0], closed);
	return 0;
}

#if defined(OPER_DIE) || defined(LOCOP_DIE)
int	m_die(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	aClient	*acptr;
	Reg2	int	i;

#ifndef	LOCOP_DIE
	if (!MyClient(sptr) || !IsOper(sptr))
#else
# ifdef	OPER_DIE
	if (!MyClient(sptr) || !IsAnOper(sptr))
# else
	if (!MyClient(sptr) || !IsLocOp(sptr))
# endif
#endif
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }

	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(acptr = local[i]))
			continue;
		if (IsClient(acptr))
			sendto_one(acptr,
				   ":%s NOTICE %s :Server Terminating. %s",
				   me.name, acptr->name,
				   get_client_name(sptr, TRUE));
		else if (IsServer(acptr))
			sendto_one(acptr, ":%s ERROR :Terminated by %s",
				   me.name, get_client_name(sptr, TRUE));
	    }
#ifdef BUFFERED_LOGS
	cs_buf_logs(1,"",0);
	cs_buf_logs(2,"",0);
#endif
	(void)s_die();
	return 0;
}
#endif
