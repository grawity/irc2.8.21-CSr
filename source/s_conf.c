/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_conf.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

/* -- avalon -- 20 Feb 1992
 * Reversed the order of the params for attach_conf().
 * detach_conf() and attach_conf() are now the same:
 * function_conf(aClient *, aConfItem *)
 */

/* -- Jto -- 20 Jun 1990
 * Added gruner's overnight fix..
 */

/* -- Jto -- 16 Jun 1990
 * Moved matches to ../common/match.c
 */

/* -- Jto -- 03 Jun 1990
 * Added Kill fixes from gruner@lan.informatik.tu-muenchen.de
 * Added jarlek's msgbase fix (I still don't understand it... -- Jto)
 */

/* -- Jto -- 13 May 1990
 * Added fixes from msa:
 * Comments and return value to init_conf()
 */

/*
 * -- Jto -- 12 May 1990
 *  Added close() into configuration file (was forgotten...)
 */

#ifndef lint
static  char rcsid[] = "@(#)$Id: s_conf.c,v 1.1.1.1 1997/07/23 18:02:04 cbehrens Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "h.h"
#include "comstud.h"
#include "dich_conf.h"

int	check_time_interval PROTO((char *, char *));
static	int	lookup_confhost PROTO((aConfItem *));
static	int	same_conf PROTO((aConfItem *, aConfItem *, int));

aConfItem	*conf = NULL;

/*
 * remove all conf entries from the client except those which match
 * the status field mask.
 */
void	det_confs_butmask(cptr, mask)
aClient	*cptr;
int	mask;
{
	Reg1 Link *tmp, *tmp2;

	for (tmp = cptr->confs; tmp; tmp = tmp2)
	{
		tmp2 = tmp->next;
		if ((tmp->value.aconf->status & mask) == 0)
			(void)detach_conf(cptr, tmp->value.aconf);
	}
}

#ifdef I_LINES

/*
 * find the best I line to attach.
 */
int	attach_Iline(cptr, hp, sockhost, username)
aClient *cptr;
Reg2	struct	hostent	*hp;
char	*sockhost;
char	*username;
{
	aConfItem *aconf = NULL;
	static	char	fullname[HOSTLEN+1];
	static	int	fullsize = 0;
	register int i;
	register char *hname;

	if (!fullsize)
		fullsize = sizeof(fullname);
	if (hp)
		for (i = 0, hname = hp->h_name; hname;
			hname = hp->h_aliases[i++])
		{
			(void)strncpy(fullname, hname, fullsize - 1);
			fullname[fullsize-1] = (char) 0;
			add_local_domain(fullname, HOSTLEN - strlen(fullname));
			Debug((DEBUG_DNS, "a_il: %s->%s", sockhost, fullname));
			aconf = find_dichconf(Ilines, username,
				fullname, NULL, 1, NULL);
			if (aconf)
				break;
		}
	if (!aconf)
	{
		(void)strncpy(fullname, sockhost, fullsize-1);
		fullname[fullsize-1] = (char) 0;
		aconf = find_dichconf(Ilines, username, NULL, cptr->ipstr,
				1, NULL);
	}
	if (!aconf)
		return -1;
	if (hp && hp->h_name)
		get_sockhost(cptr, hp->h_name);
	else
		get_sockhost(cptr, fullname);
	return attach_conf(cptr, aconf);
}


#else /* I_LINES */

/*
 * find the first (best) I line to attach.
 */
int	attach_Iline(cptr, hp, sockhost, username)
aClient *cptr;
Reg2	struct	hostent	*hp;
char	*sockhost;
char	*username;
{
	Reg1	aConfItem	*aconf;
	Reg3	char	*hname;
	Reg4	int	i;
	static	char	uhost[HOSTLEN+USERLEN+3];
	static	char	fullname[HOSTLEN+1];
	static	int	fullsize = 0, uhostsize = 0;

	if (!fullsize)
	{
		fullsize = sizeof(fullname);
		uhostsize = sizeof(uhost);
	}
	bzero((char *)fullname, fullsize);
	bzero((char *)uhost, uhostsize);
	for (aconf = conf; aconf; aconf = aconf->next)
	{
		if (aconf->status != CONF_CLIENT)
			continue;
		if (aconf->port && aconf->port != cptr->acpt->port)
			continue;
		if (!aconf->host || !aconf->name)
			goto attach_iline;
		if (hp)
			for (i = 0, hname = hp->h_name; hname;
			     hname = hp->h_aliases[i++])
			{
				(void)strncpy(fullname, hname, fullsize - 1);
				add_local_domain(fullname,
						 HOSTLEN - strlen(fullname));
				Debug((DEBUG_DNS, "a_il: %s->%s",
				      sockhost, fullname));
				if (index(aconf->name, '@') && *username)
				{
					strncpyzt(uhost, username, USERLEN+1);
					(void)strcat(uhost, "@");
				}
				else
					*uhost = '\0';
				(void)strncat(uhost, fullname,
					uhostsize - strlen(uhost));
				if (!match(aconf->name, uhost))
					goto attach_iline;
			}

		if (index(aconf->host, '@') && *username)
		{	
			strncpyzt(uhost, username, USERLEN+1);
			(void)strcat(uhost, "@");
		}
		else
			*uhost = '\0';
		(void)strncat(uhost, sockhost, uhostsize - strlen(uhost));
		if (!match(aconf->host, uhost))
			goto attach_iline;
		continue;
attach_iline:
		if (hp && hp->h_name)
			get_sockhost(cptr, hp->h_name);
		else
			get_sockhost(cptr, uhost);
		return attach_conf(cptr, aconf);
	    }
	return -1;
}

#endif /* I_LINES */

/*
 * Find the single N line and return pointer to it (from list).
 * If more than one then return NULL pointer.
 */
aConfItem	*count_cnlines(lp)
Reg1	Link	*lp;
{
	Reg1	aConfItem	*aconf, *cline = NULL, *nline = NULL;

	for (; lp; lp = lp->next)
	    {
		aconf = lp->value.aconf;
		if (!(aconf->status & CONF_SERVER_MASK))
			continue;
		if ((aconf->status == CONF_CONNECT_SERVER ||
		     aconf->status == CONF_NZCONNECT_SERVER) && !cline)
			cline = aconf;
		else if (aconf->status == CONF_NOCONNECT_SERVER && !nline)
			nline = aconf;
	    }
	return nline;
}

/*
** detach_conf
**	Disassociate configuration from the client.
**      Also removes a class from the list if marked for deleting.
*/
int	detach_conf(cptr, aconf)
aClient *cptr;
aConfItem *aconf;
{
	Reg1	Link	**lp, *tmp;

	lp = &(cptr->confs);

	while (*lp)
	{
		if ((*lp)->value.aconf == aconf)
		{
			if ((aconf) && (Class(aconf)))
			{
				if (aconf->status & CONF_CLIENT_MASK)
					if (ConfLinks(aconf) > 0)
						--ConfLinks(aconf);
       				if (ConfMaxLinks(aconf) == -1 &&
				    ConfLinks(aconf) == 0)
				{
					free_class(Class(aconf));
					Class(aconf) = NULL;
				}
			}
			if (aconf && !--aconf->clients && IsIllegal(aconf))
				free_conf(aconf);
			tmp = *lp;
			*lp = tmp->next;
			free_link(tmp);
			return 0;
		}
		else
			lp = &((*lp)->next);
	}
	return -1;
}

static	int	is_attached(aconf, cptr)
aConfItem *aconf;
aClient *cptr;
{
	Reg1	Link	*lp;

	for (lp = cptr->confs; lp; lp = lp->next)
		if (lp->value.aconf == aconf)
			break;

	return (lp) ? 1 : 0;
}

/*
** attach_conf
**	Associate a specific configuration entry to a *local*
**	client (this is the one which used in accepting the
**	connection). Note, that this automaticly changes the
**	attachment if there was an old one...
*/
int	attach_conf(cptr, aconf)
aClient *cptr;
aConfItem *aconf;
{
	Reg1 Link *lp;

	if (is_attached(aconf, cptr))
		return 1;
	if (IsIllegal(aconf))
		return -1;
#ifdef OLD_Y_LIMIT
	if ((aconf->status & (CONF_LOCOP | CONF_OPERATOR | CONF_CLIENT)) &&
	    aconf->clients >= ConfMaxLinks(aconf) && ConfMaxLinks(aconf) > 0)
		return -3;	/* Use this for printing error message */
#else
	if ((aconf->status & (CONF_LOCOP | CONF_OPERATOR | CONF_CLIENT)) &&
		(ConfLinks(aconf) >= ConfMaxLinks(aconf)) &&
			(ConfMaxLinks(aconf) > 0))
		return -3;
#endif
	lp = make_link();
	lp->next = cptr->confs;
	lp->value.aconf = aconf;
	cptr->confs = lp;
	aconf->clients++;
	if (aconf->status & CONF_CLIENT_MASK)
		ConfLinks(aconf)++;
	return 0;
}


aConfItem *find_admin()
    {
	Reg1 aConfItem *aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ADMIN)
			break;
	
	return (aconf);
    }

aConfItem *find_me()
    {
	Reg1 aConfItem *aconf;
	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ME)
			break;
	
	return (aconf);
    }

/*
 * attach_confs
 *  Attach a CONF line to a client if the name passed matches that for
 * the conf file (for non-C/N lines) or is an exact match (C/N lines
 * only).  The difference in behaviour is to stop C:*::* and N:*::*.
 */
aConfItem *attach_confs(cptr, name, statmask)
aClient	*cptr;
char	*name;
int	statmask;
{
	Reg1 aConfItem *tmp;
	aConfItem *first = NULL;
	int len = strlen(name);
  
	if (!name || len > HOSTLEN)
		return NULL;
	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if ((tmp->status & statmask) && !IsIllegal(tmp) &&
		    ((tmp->status & (CONF_SERVER_MASK|CONF_HUB)) == 0) &&
		    tmp->name && !match(tmp->name, name))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
		else if ((tmp->status & statmask) && !IsIllegal(tmp) &&
			 (tmp->status & (CONF_SERVER_MASK|CONF_HUB)) &&
			 tmp->name && !mycmp(tmp->name, name))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
	    }
	return (first);
}

/*
 * Added for new access check    meLazy
 */
aConfItem *attach_confs_host(cptr, host, statmask)
aClient *cptr;
char	*host;
int	statmask;
{
	Reg1	aConfItem *tmp;
	aConfItem *first = NULL;
	int	len = strlen(host);
  
	if (!host || len > HOSTLEN)
		return NULL;

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if ((tmp->status & statmask) && !IsIllegal(tmp) &&
		    (tmp->status & CONF_SERVER_MASK) == 0 &&
		    (!tmp->host || match(tmp->host, host) == 0))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
		else if ((tmp->status & statmask) && !IsIllegal(tmp) &&
	       	    (tmp->status & CONF_SERVER_MASK) &&
	       	    (tmp->host && mycmp(tmp->host, host) == 0))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
	    }
	return (first);
}

/*
 * find a conf entry which matches the hostname and has the same name.
 */
aConfItem *find_conf_exact(name, user, host, statmask)
char	*name, *host, *user;
int	statmask;
{
	Reg1	aConfItem *tmp;
	char	userhost[USERLEN+HOSTLEN+3];

	(void)irc_sprintf(userhost, "%s@%s", user, host);

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if (!(tmp->status & statmask) || !tmp->name || !tmp->host ||
		    mycmp(tmp->name, name))
			continue;
		/*
		** Accept if the *real* hostname (usually sockecthost)
		** socket host) matches *either* host or name field
		** of the configuration.
		*/
		if (match(tmp->host, userhost))
			continue;
		if (tmp->status & (CONF_OPERATOR|CONF_LOCOP))
		    {
			if (tmp->clients < MaxLinks(Class(tmp)))
				return tmp;
			else
				continue;
		    }
		else
			return tmp;
	    }
	return NULL;
}

aConfItem *find_conf_name(name, statmask)
char	*name;
int	statmask;
{
	Reg1	aConfItem *tmp;
 
	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		/*
		** Accept if the *real* hostname (usually sockecthost)
		** matches *either* host or name field of the configuration.
		*/
		if ((tmp->status & statmask) &&
		    (!tmp->name || match(tmp->name, name) == 0))
			return tmp;
	    }
	return NULL;
}

aConfItem *find_conf(lp, name, statmask)
char	*name;
Link	*lp;
int	statmask;
{
	Reg1	aConfItem *tmp;
	int	namelen = name ? strlen(name) : 0;
  
	if (namelen > HOSTLEN)
		return (aConfItem *) 0;

	for (; lp; lp = lp->next)
	    {
		tmp = lp->value.aconf;
		if ((tmp->status & statmask) &&
		    (((tmp->status & (CONF_SERVER_MASK|CONF_HUB)) &&
	 	     tmp->name && !mycmp(tmp->name, name)) ||
		     ((tmp->status & (CONF_SERVER_MASK|CONF_HUB)) == 0 &&
		     tmp->name && !match(tmp->name, name))))
			return tmp;
	    }
	return NULL;
}

/*
 * Added for new access check    meLazy
 */
aConfItem *find_conf_host(lp, host, statmask)
Reg2	Link	*lp;
char	*host;
Reg3	int	statmask;
{
	Reg1	aConfItem *tmp;
	int	hostlen = host ? strlen(host) : 0;
  
	if (hostlen > HOSTLEN || BadPtr(host))
		return (aConfItem *)NULL;
	for (; lp; lp = lp->next)
	    {
		tmp = lp->value.aconf;
		if (tmp->status & statmask &&
		    (!(tmp->status & CONF_SERVER_MASK || tmp->host) ||
	 	     (tmp->host && !match(tmp->host, host))))
			return tmp;
	    }
	return NULL;
}

/*
 * find_conf_ip
 *
 * Find a conf line using the IP# stored in it to search upon.
 * Added 1/8/92 by Avalon.
 */
aConfItem *find_conf_ip(lp, ip, user, statmask)
char	*ip, *user;
Link	*lp;
int	statmask;
{
	Reg1	aConfItem *tmp;
	Reg2	char	*s;
  
	for (; lp; lp = lp->next)
	    {
		tmp = lp->value.aconf;
		if (!(tmp->status & statmask))
			continue;
		s = index(tmp->host, '@');
		*s = '\0';
		if (match(tmp->host, user))
		    {
			*s = '@';
			continue;
		    }
		*s = '@';
		if (!bcmp((char *)&tmp->ipnum, ip, sizeof(struct in_addr)))
			return tmp;
	    }
	return NULL;
}

static int same_conf(aconf, bconf, mask)
aConfItem *aconf, *bconf;
int mask;
{
	if (!aconf && !bconf)
		return 1;
	if (!aconf || !bconf)
		return 0;
	if (mask == -1)
	{
		mask = aconf->status;
		mask &= ~CONF_ILLEGAL;
	}
	if (!(bconf->status & mask) ||
		(bconf->port != aconf->port))
		return 0;
	if (bconf->flags != aconf->flags)
		return 0;
	if ((bconf->host && !aconf->host) || (aconf->host && !bconf->host))
		return 0;
	if (bconf->host && mycmp(bconf->host, aconf->host))
		return 0;
	if ((bconf->name && !aconf->name) || (aconf->name && !bconf->name))
		return 0;
	if (bconf->name && mycmp(bconf->name, aconf->name))
		return 0;
	return 1;
}

/*
 * find_conf_entry
 *
 * - looks for a match on all given fields.
 */
aConfItem *find_conf_entry(aconf, mask)
aConfItem *aconf;
u_int	mask;
{
	register aConfItem *bconf;

	mask &= ~CONF_ILLEGAL;
#ifdef I_LINES
	if (mask & CONF_CLIENT)
	{
		if (BadPtr(aconf->host))
			return NULL;
		bconf = find_dichconf(Ilines, aconf->name,
			aconf->host, NULL, 1, NULL);
		if (bconf && !same_conf(aconf, bconf, mask))
			return NULL;
	}
	else
#endif
	for (bconf = conf; bconf; bconf = bconf->next)
		if (same_conf(aconf, bconf, mask))
			break;
	return bconf;
}

/*
 * rehash
 *
 * Actual REHASH service routine. Called with sig == 0 if it has been called
 * as a result of an operator issuing this command, else assume it has been
 * called as a result of the server receiving a HUP signal.
 */
int	rehash(cptr, sptr, sig)
aClient	*cptr, *sptr;
int	sig;
{
	Reg1	aConfItem **tmp = &conf, *tmp2;
	Reg2	aClass	*cltmp;
	Reg1	aClient	*acptr;
	Reg2	int	i;
	int	ret = 0;

	if (sig == 1)
	{
		sendto_ops("Got signal SIGHUP, reloading ircd conf. file");
#ifdef	ULTRIX
		if (fork() > 0)
			exit(0);
		write_pidfile();
#endif
	}

	for (i = 0; i <= highest_fd; i++)
		if ((acptr = local[i]) && !IsMe(acptr))
		{
			/*
			 * Nullify any references from client structures to
			 * this host structure which is about to be freed.
			 * Could always keep reference counts instead of
			 * this....-avalon
			 */
			acptr->hostp = NULL;
#if defined(R_LINES_REHASH) && !defined(R_LINES_OFTEN)
			if (find_restrict(acptr))
			{
				sendto_ops("Restricting %s, closing lp",
					   get_client_name(acptr,FALSE));
				if (exit_client(cptr,acptr,sptr,"R-lined") ==
				    FLUSH_BUFFER)
					ret = FLUSH_BUFFER;
			}
#endif
		}

	while ((tmp2 = *tmp) != NULL)
		if (tmp2->clients || (tmp2->status & CONF_LISTEN_PORT))
		{
			/*
			** Configuration entry is still in use by some
			** local clients, cannot delete it--mark it so
			** that it will be deleted when the last client
			** exits...
			*/
			if (!(tmp2->status & (CONF_LISTEN_PORT|CONF_CLIENT)))
			{
				*tmp = tmp2->next;
				tmp2->next = NULL;
			}
			else
				tmp = &tmp2->next;
			tmp2->status |= CONF_ILLEGAL;
		}
		else
		{
			*tmp = tmp2->next;
			free_conf(tmp2);
		}
#ifdef I_LINES
	mark_dichconf(Ilines, CONF_ILLEGAL);
#endif
	/*
	 * We don't delete the class table, rather mark all entries
	 * for deletion. The table is cleaned up by check_class. - avalon
	 */
	for (cltmp = NextClass(FirstClass()); cltmp; cltmp = NextClass(cltmp))
		MaxLinks(cltmp) = -1;
#ifndef DONT_FLUSH_CACHE
/*  Let's don't do this...somewhere, the cache doesn't get cleared correctly.
    At least on some OS's.  - Comstud
    Well...let's try it again now since it's been many versions since I
     commented this out -- Comstud
*/
	if (sig != 2)
		flush_cache();
#endif
	clear_dichconf(Klines, 1);
#ifdef B_LINES
	clear_dichconf(Blines, 1);
#endif /* B_LINES */
#ifdef E_LINES
	clear_dichconf(Elines, 1);
#endif /* E_LINES */
#ifdef D_LINES
	clear_dichconf(Dlines, 1);
#endif /* D_LINES */
#ifdef J_LINES
	clear_dichconf(Jlines, 1);
#endif /* J_LINES */
#ifdef G_LINES
	clear_glineconf();
	init_glineconf(GLINE_CONFFILE);
#endif
	(void) initconf(0, configfile);
#ifndef PUT_KLINES_IN_IRCD_CONF
	(void) initconf(0, klinefile);
#endif
	close_listeners();

	/*
	 * flush out deleted I and P lines although still in use.
	 */
	for (tmp = &conf; (tmp2 = *tmp); )
		if (!(tmp2->status & CONF_ILLEGAL))
			tmp = &tmp2->next;
		else
		{
			*tmp = tmp2->next;
			tmp2->next = NULL;
			if (!tmp2->clients)
				free_conf(tmp2);
		}
#ifdef I_LINES
	flush_dichconf(Ilines);
	flush_dichconf(Glines); /* removes expired ones */
#endif
#ifdef BETTER_MOTD
	read_motd(MOTD);
#endif
#ifdef BUFFERED_LOGS
        cs_buf_logs(1,NULL,0);
        cs_buf_logs(2,NULL,0);
#endif
	rehashed = 1;
	return ret;
}

/*
 * openconf
 *
 * returns -1 on any error or else the fd opened from which to read the
 * configuration file from.  This may either be th4 file direct or one end
 * of a pipe from m4.
 */
int	openconf(filename)
char *filename;
{
#ifdef	M4_PREPROC
	int	pi[2], i;

	if (pipe(pi) == -1)
		return -1;
	switch(fork())
	{
	case -1 :
		return -1;
	case 0 :
		(void)close(pi[0]);
		if (pi[1] != 1)
		    {
			(void)dup2(pi[1], 1);
			(void)close(pi[1]);
		    }
		(void)dup2(1,2);
		for (i = 3; i < MAXCONNECTIONS; i++)
			if (local[i])
				(void) close(i);
		/*
		 * m4 maybe anywhere, use execvp to find it.  Any error
		 * goes out with report_error.  Could be dangerous,
		 * two servers running with the same fd's >:-) -avalon
		 */
		(void)execlp("m4", "m4", "ircd.m4", filename, 0);
		report_error("Error executing m4 %s:%s", &me);
		exit(-1);
	default :
		(void)close(pi[1]);
		return pi[0];
	}
#else
	return open(filename, O_RDONLY);
#endif
}

extern char *getfield();

char *set_conf_flags(aconf, tmp)
aConfItem *aconf;
char *tmp;
{
	while(1==1)
	{
		switch(*tmp)
		{
			case '!':
				aconf->flags |= FLAGS_LIMIT_IP;
				break;
			case '-':
				aconf->flags |= FLAGS_NO_TILDE;
				break;
			case '+':
				aconf->flags |= FLAGS_NEED_IDENTD;
				break;
			case '$':
				aconf->flags |= FLAGS_PASS_IDENTD;
				break;
			case '%':
				aconf->flags |= FLAGS_NOMATCH_IP;
				break;
			default:
				return tmp;
		}
		tmp++;
	}
	/* won't get here, but... */
	return tmp;
}

/*
** initconf() 
**    Read configuration file.
**
**    returns -1, if file cannot be opened
**             0, if file opened
*/

#define MAXCONFLINKS 150

int 	initconf(opt, filename)
int	opt;
char *filename;
{
	static	char	quotes[9][2] = {{'b', '\b'}, {'f', '\f'}, {'n', '\n'},
					{'r', '\r'}, {'t', '\t'}, {'v', '\v'},
					{'\\', '\\'}, { 0, 0}};
	Reg1	char	*tmp, *s;
	int	fd, i, dontadd, dontadd2, dontadd3;
	char	line[512], c[80];
	int	ccount = 0, ncount = 0;
	aConfItem *aconf = NULL;
	aConfItem *bconf = NULL;
#ifdef I_LINES
	aConfItem *aconf2 = NULL;
#endif

	Debug((DEBUG_DEBUG, "initconf(): ircd.conf = %s", filename));
	if ((fd = openconf(filename)) == -1)
	    {
#ifdef	M4_PREPROC
		(void)wait(0);
#endif
		return -1;
	    }
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	while ((i = dgets(fd, line, sizeof(line) - 1)) > 0)
	{
		line[i] = '\0';
		if ((tmp = (char *)index(line, '\n')))
			*tmp = 0;
		else while(dgets(fd, c, sizeof(c) - 1) > 0)
			if ((tmp = (char *)index(c, '\n')))
			{
				*tmp = 0;
				break;
			}
		/*
		 * Do quoting of characters and # detection.
		 */
		for (tmp = line; *tmp; tmp++)
		{
			if (*tmp == '\\')
			{
				for (i = 0; quotes[i][0]; i++)
					if (quotes[i][0] == *(tmp+1))
					{
						*tmp = quotes[i][1];
						break;
					}
				if (!quotes[i][0])
					*tmp = *(tmp+1);
				if (!*(tmp+1))
					break;
				else
					for (s = tmp; (*s = *(s+1)); s++)
						;
			}
			else if (*tmp == '#')
				*tmp = '\0';
		}
		if (!*line || line[0] == '#' || line[0] == '\n' ||
		    line[0] == ' ' || line[0] == '\t')
			continue;
		/* Could we test if it's conf line at all?	-Vesa */
		if (line[1] != ':')
		{
                        Debug((DEBUG_ERROR, "Bad config line: %s", line));
                        continue;
		}
		if (aconf)
			free_conf(aconf);
		aconf = make_conf();

		tmp = getfield(line);
		if (!tmp)
			continue;
		dontadd = 0;
		dontadd2 = 0;
		dontadd3 = 0;
		switch (*tmp)
		{
			case 'A': /* Name, e-mail address of administrator */
			case 'a': /* of this server. */
				aconf->status = CONF_ADMIN;
				break;
#ifdef B_LINES
			case 'B': /* Addresses that we don't want to check */
			case 'b': /* for bots. */
				aconf->status = CONF_BOT_IGNORE;
				dontadd++;
				break;
#endif /* B_LINES */
			case 'C': /* Server where I should try to connect */
			          /* in case of lp failures             */
				ccount++;
				aconf->status = CONF_CONNECT_SERVER;
				break;
			case 'c':
				ccount++;
				aconf->status = CONF_NZCONNECT_SERVER;
				break;
#ifdef D_LINES
			case 'D':
			case 'd':
				aconf->status = CONF_DLINE;
				dontadd++;
				break;
#endif
#ifdef E_LINES
                        case 'E': /* Addresses that we don't want to check */
                        case 'e': /* for bots. */
                                aconf->status = CONF_ELINE;
				dontadd++;
                                break;
#endif /* E_LINES */
			case 'H': /* Hub server line */
			case 'h':
				aconf->status = CONF_HUB;
				break;
			case 'I': /* Just plain normal irc client trying  */
			case 'i': /* to connect me */
				aconf->status = CONF_CLIENT;
#ifdef I_LINES
				aconf2 = make_conf();
				aconf2->status = aconf->status;
				dontadd++;
#endif
				break;
#ifdef J_LINES
                        case 'J': /* Unused as of yet..*/
                        case 'j':
                                aconf->status = CONF_JLINE;
				dontadd++;
                                break;
#endif /* J_LINES */
			case 'K': /* Kill user line on irc.conf           */
			case 'k':
				aconf->status = CONF_KILL;
				dontadd++;
				break;
			/* Operator. Line should contain at least */
			/* password and host where connection is  */
			case 'L': /* guaranteed leaf server */
			case 'l':
				aconf->status = CONF_LEAF;
				break;
			/* Me. Host field is name used for this host */
			/* and port number is the number of the port */
			case 'M':
			case 'm':
				aconf->status = CONF_ME;
				break;
			case 'N': /* Server where I should NOT try to     */
			case 'n': /* connect in case of lp failures     */
				  /* but which tries to connect ME        */
				++ncount;
				aconf->status = CONF_NOCONNECT_SERVER;
				break;
			case 'O':
				aconf->status = CONF_OPERATOR;
				break;
			/* Local Operator, (limited privs --SRB) */
			case 'o':
				aconf->status = CONF_LOCOP;
				break;
			case 'P': /* listen port line */
			case 'p':
				aconf->status = CONF_LISTEN_PORT;
				break;
			case 'Q': /* a server that you don't want in your */
			case 'q': /* network. USE WITH CAUTION! */
				aconf->status = CONF_QUARANTINED_SERVER;
				break;
#ifdef R_LINES
			case 'R': /* extended K line */
			case 'r': /* Offers more options of how to restrict */
				aconf->status = CONF_RESTRICT;
				break;
#endif
			case 'S': /* Service. Same semantics as   */
			case 's': /* CONF_OPERATOR                */
				aconf->status = CONF_SERVICE;
				break;
			case 'U': /* Uphost, ie. host where client reading */
			case 'u': /* this should connect.                  */
			/* This is for client only, I must ignore this */
			/* ...U-line should be removed... --msa */
				break;
			case 'Y':
			case 'y':
			        aconf->status = CONF_CLASS;
		        	break;
		    default:
			Debug((DEBUG_ERROR, "Error in config file: %s", line));
			break;
		    }
		if (IsIllegal(aconf))
			continue;

		for (;;) /* Fake loop, that I can use break here --msa */
		{
			if ((tmp = getfield(NULL)) == NULL)
				break;
			if (aconf->status & CONF_CLIENT)
				tmp = set_conf_flags(aconf, tmp);
			DupString(aconf->host, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->passwd, tmp);
#ifdef I_LINES
			if (aconf->status & CONF_CLIENT)
			{
				aconf->name = NULL;
				aconf2->name = NULL;
				DupString(aconf2->passwd, tmp);
			}
#endif
			if ((tmp = getfield(NULL)) == NULL)
				break;
			if (aconf->status & CONF_CLIENT)
#ifdef I_LINES
			{
#endif
				tmp = set_conf_flags(aconf, tmp);
#ifdef I_LINES
				DupString(aconf2->host, tmp);
			}
			else
#endif
				DupString(aconf->name, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			aconf->port = atoi(tmp);
#ifdef I_LINES
			if (aconf->status & CONF_CLIENT)
				aconf2->port = atoi(tmp);
#endif
			if ((tmp = getfield(NULL)) == NULL)
				break;
			Class(aconf) = find_class(atoi(tmp));
#ifdef I_LINES
			if (aconf->status & CONF_CLIENT)
				Class(aconf2) = find_class(atoi(tmp));
#endif
			break;
		}
		/*
                ** If conf line is a class definition, create a class entry
                ** for it and make the conf_line illegal and delete it.
                */
		if (aconf->status & CONF_CLASS)
		{
			add_class(atoi(aconf->host), atoi(aconf->passwd),
				  atoi(aconf->name), aconf->port,
				  tmp ? atoi(tmp) : 0);
			continue;
		}
		/*
                ** associate each conf line with a class by using a pointer
                ** to the correct class record. -avalon
                */
		if (aconf->status & (CONF_CLIENT_MASK|CONF_LISTEN_PORT))
		{
			if (Class(aconf) == 0)
				Class(aconf) = find_class(0);
			if (MaxLinks(Class(aconf)) < 0)
				Class(aconf) = find_class(0);
#ifdef I_LINES
			if (aconf->status & CONF_CLIENT)
				Class(aconf2) = Class(aconf);
#endif
		}
#ifdef I_LINES
		if (aconf->status & CONF_CLIENT)
		{
			char *tmp;
			char *tmp2;

			if ((tmp = strchr(aconf->host, '@')) != NULL)
			{
				*tmp = (char) 0;
				DupString(aconf->name, aconf->host);
				DupString(tmp2, tmp+1);
				MyFree(aconf->host);
				aconf->host = tmp2;
			}
			if ((tmp = strchr(aconf2->host, '@')) != NULL)
			{
				*tmp = (char) 0;
				DupString(aconf2->name, aconf2->host);
				DupString(tmp2, tmp+1);
				MyFree(aconf2->host);
				aconf2->host = tmp2;
			}
			if (BadPtr(aconf2->name))
				DupString(aconf2->name, "*");
			if (BadPtr(aconf->name))
				DupString(aconf->name, "*");
			if (!strcmp(aconf2->host, "*"))
				aconf2->flags |= FLAGS_NOMATCH_IP;
		}
#endif
		if (aconf->status & (CONF_LISTEN_PORT|CONF_CLIENT))
		{
#ifdef I_LINES
			if (aconf->status & CONF_CLIENT)
			{
				bconf = find_conf_entry(aconf, aconf->status);
				if (bconf)
				{
					bconf->class->links -= bconf->clients;
					bconf->class = aconf->class;
					bconf->class->links += bconf->clients;
					bconf->flags = aconf->flags;
					bconf->status &= ~CONF_ILLEGAL;
					free_conf(aconf);
					aconf = bconf;
					dontadd2++;
				}
				if (!same_conf(aconf, aconf2, CONF_CLIENT))
				{
					bconf = find_conf_entry(aconf2,
						aconf2->status);
					if (bconf)
					{
						bconf->class->links -=
							bconf->clients;
						bconf->class = aconf2->class;
						bconf->class->links +=
							bconf->clients;
						bconf->flags = aconf2->flags;
						bconf->status &= ~CONF_ILLEGAL;
						free_conf(aconf2);
						aconf2 = bconf;
						dontadd3++;
					}
				}
				else
				{
					free_conf(aconf2);
					dontadd3++;
				}
			}
			else
#endif /* I_LINES */
			if ((bconf = find_conf_entry(aconf, aconf->status)))
			{
				delist_conf(bconf);
				bconf->status &= ~CONF_ILLEGAL;
				if (aconf->status == CONF_CLIENT)
				{
					bconf->class->links -= bconf->clients;
					bconf->class = aconf->class;
					bconf->class->links += bconf->clients;
					bconf->flags = aconf->flags;
				}
				free_conf(aconf);
				aconf = bconf;
			}
			else if (aconf->host &&
				 aconf->status == CONF_LISTEN_PORT)
				(void)add_listener(aconf);
		}
		if (aconf->status & CONF_SERVER_MASK)
			if (ncount > MAXCONFLINKS || ccount > MAXCONFLINKS ||
			    !aconf->host || index(aconf->host, '*') ||
			     index(aconf->host,'?') || !aconf->name)
				continue;

		if (aconf->status &
		    (CONF_SERVER_MASK|CONF_LOCOP|CONF_OPERATOR))
			if (!index(aconf->host, '@') && *aconf->host != '/')
			{
				char	*newhost;
				int	len = 3;	/* *@\0 = 3 */

				len += strlen(aconf->host);
				newhost = (char *)MyMalloc(len);
				(void)irc_sprintf(newhost, "*@%s", aconf->host);
				MyFree(aconf->host);
				aconf->host = newhost;
			}
		if (aconf->status & CONF_SERVER_MASK)
		{
			if (BadPtr(aconf->passwd))
				continue;
			else if (!(opt & BOOT_QUICK))
				(void)lookup_confhost(aconf);
		}
		/*
		** Own port and name cannot be changed after the startup.
		** (or could be allowed, but only if all links are closed
		** first).
		** Configuration info does not override the name and port
		** if previously defined. Note, that "info"-field can be
		** changed by "/rehash".
		*/
		if (aconf->status == CONF_ME)
		{
			strncpyzt(me.info, aconf->name, sizeof(me.info));
			if (me.name[0] == '\0' && aconf->host[0])
				strncpyzt(me.name, aconf->host,
					  sizeof(me.name));
			if (aconf->passwd[0] && (aconf->passwd[0] != '*'))
				me.bindip.s_addr = inet_addr(aconf->passwd);
			else
				me.bindip.s_addr = INADDR_ANY;
			if (portnum < 0 && aconf->port >= 0)
				portnum = aconf->port;
		}

		(void)collapse(aconf->host);
		(void)collapse(aconf->name);
#ifdef I_LINES
		if ((aconf->status & CONF_CLIENT) && !dontadd3)
		{
			(void)collapse(aconf2->host);
			(void)collapse(aconf2->name);
		}
#endif
		Debug((DEBUG_NOTICE,
		      "Read Init: (%d) (%s) (%s) (%s) (%d) (%d)",
		      aconf->status, aconf->host, aconf->passwd,
		      aconf->name, aconf->port, Class(aconf)));

		if (aconf->host)
		{
			if (aconf->status & CONF_KILL)
				addto_dichconf(Klines, aconf);
#ifdef I_LINES
			if ((aconf->status & CONF_CLIENT) && !dontadd2)
			{
				if (mycmp(aconf->host, "X") &&
					mycmp(aconf->host, "NOMATCH") &&
					mycmp(aconf->host, "NOMATCHME"))
				{
					addto_dichconf(Ilines, aconf);
				}
				else
					free_conf(aconf);
			}
#endif
#ifdef B_LINES
			if (aconf->status & CONF_BOT_IGNORE)
				addto_dichconf(Blines, aconf);
#endif /* B_LINES */
#ifdef E_LINES
			if (aconf->status & CONF_ELINE)
				addto_dichconf(Elines, aconf);
#endif /* E_LINES */
#ifdef D_LINES
			if (aconf->host && (aconf->status & CONF_DLINE))
				addto_dichconf(Dlines, aconf);
#endif /* D_LINES */
#ifdef J_LINES
			if (aconf->host && (aconf->status & CONF_JLINE))
				addto_dichconf(Jlines, aconf);
#endif /* J_LINES */
		} /* aconf->host */
		else
			free_conf(aconf);
#ifdef I_LINES
		if ((aconf->status & CONF_CLIENT) && !dontadd3)
		{
			if (aconf2->host && mycmp(aconf2->host, "X") &&
				mycmp(aconf2->host, "NOMATCH") &&
				mycmp(aconf2->host, "NOMATCHME"))
			{
				addto_dichconf(Ilines, aconf2);
			}
			else
				free_conf(aconf2);
		}
#endif

		if (!dontadd)
		{
			aconf->next = conf;
			conf = aconf;
		}
		aconf = NULL;
#ifdef I_LINES
		aconf2 = NULL;
#endif
	}
	if (aconf)
		free_conf(aconf);
#ifdef I_LINES
	if (aconf2)
		free_conf(aconf2);
#endif
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	(void)close(fd);
#ifdef	M4_PREPROC
	(void)wait(0);
#endif
	check_class();
	nextping = nextconnect = NOW;
	return 0;
}

/*
 * lookup_confhost
 *   Do (start) DNS lookups of all hostnames in the conf line and convert
 * an IP addresses in a.b.c.d number for to IP#s.
 */
static	int	lookup_confhost(aconf)
Reg1	aConfItem	*aconf;
{
	Reg2	char	*s;
	Reg3	struct	hostent *hp;
	Link	ln;

	if (BadPtr(aconf->host) || BadPtr(aconf->name))
		goto badlookup;
	if ((s = index(aconf->host, '@')))
		s++;
	else
		s = aconf->host;
	/*
	** Do name lookup now on hostnames given and store the
	** ip numbers in conf structure.
	*/
	if (!isalpha(*s) && !isdigit(*s))
		goto badlookup;

	/*
	** Prepare structure in case we have to wait for a
	** reply which we get later and store away.
	*/
	ln.value.aconf = aconf;
	ln.flags = ASYNC_CONF;

	if (isdigit(*s))
		aconf->ipnum.s_addr = inet_addr(s);
	else if ((hp = gethost_byname(s, &ln)))
		bcopy(hp->h_addr, (char *)&(aconf->ipnum),
			sizeof(struct in_addr));

	if (aconf->ipnum.s_addr == -1)
		goto badlookup;
	return 0;
badlookup:
	if (aconf->ipnum.s_addr == -1)
		bzero((char *)&aconf->ipnum, sizeof(struct in_addr));
	Debug((DEBUG_ERROR,"Host/server name error: (%s) (%s)",
		aconf->host, aconf->name));
	return -1;
}

int is_comment(comment)
char  *comment;
{
	return (*comment == '');
}

#ifdef NO_REDUNDANT_KLINES

int _dich_test_kline_userhost(sptr, List, kuser, khost)
aClient *sptr;
aConfList       *List;
char            *kuser, *khost;
{
	register aConfItem      *tmp;
	register int            current;
	char                    *host, *pass, *name;
	static char             null[] = "<NULL>";
	int                     port;
	aConfEntry		*ptr;

	if (!List || !List->conf_list)
		return 0;

	for (current = 0; current < List->length; current++)
	{
		ptr = &List->conf_list[current];

		for(; ptr; ptr = ptr->next)
		{
			if (ptr->sub)
			if (_dich_test_kline_userhost(sptr, ptr->sub, kuser, khost))
				return 1;
			if (!(tmp = ptr->conf))
				continue;
			host = BadPtr(tmp->host) ? null : tmp->host;
			pass = BadPtr(tmp->passwd) ? null : tmp->passwd;
			name = BadPtr(tmp->name) ? null : tmp->name;
                        port = (int)tmp->port;
                        if (tmp->status == CONF_KILL)
                        if (!match(name,kuser) && !match(host,khost))
			{
				if (sptr && MyClient(sptr))
				sendto_one(sptr, ":%s NOTICE %s :K: line not added. %s@%s already matched by %s@%s", me.name, sptr->name, kuser, khost, name, host );
				return 1;
                        }
                }
        }
        return 0;
}

int test_kline_userhost(sptr, List, kuser, khost)
aClient *sptr;
aDichConf       *List;
char            *kuser, *khost;
{
	return _dich_test_kline_userhost(sptr, &(List->Backward),
			kuser, khost) ||
		_dich_test_kline_userhost(sptr, &(List->Forward),
			kuser, khost) ||
		_dich_test_kline_userhost(sptr, &(List->Unsorted),
			kuser, khost);
}
#endif

int	find_kill(cptr, checkuh, checkeline, reason)
aClient *cptr;
int	checkuh;
int	checkeline;
char	**reason;
{
	char *username;

	if (!cptr)
		return 0;
	username = cptr->user ? cptr->user->username : NULL;
	return find_kline(cptr, cptr->sockhost,
		cptr->ipstr, username, checkuh, checkeline, reason);
}

int find_kline(cptr, host, ip, username, checkuh, checkeline, reason)
aClient *cptr;
char *host;
char *ip;
char *username;
int checkuh;
int checkeline;
char **reason;
{
	char	reply[256], *name;
	aConfItem *tmp;
	static char toomany[100] = "Over the limit of number of clients allowed";
	static char klined[20] = "K-lined";
	static char nothing[] = "";

	if (reason)
		*reason = klined;

#ifdef E_LINES
	if (checkeline && find_eline(host, ip, username))
		return 0;
#endif

	reply[0] = '\0';

	if ((tmp = find_dichconf(Klines, username, host, ip, 0, reply)) != NULL)
	{
		if (cptr != NULL)
		{
		if (reply[0])
			sendto_one(cptr, reply,
				me.name, ERR_YOUREBANNEDCREEP, cptr->name);
		else
			sendto_one(cptr, err_str(ERR_YOUREBANNEDCREEP),
				me.name, cptr->name,
				(BadPtr(tmp->passwd) ||
					!is_comment(tmp->passwd)) ?
					"<No reason>" : tmp->passwd);
		}
	}

/* Oh, what the fuck...let's put the user@host limit check right here.
   I'm not sure that I actually like this patch since you have to loop
   through every client on every client-connect...but oh well... - CS
*/

#ifdef LIMIT_UH
	if (!tmp && checkuh && (cptr != NULL))
	{
		register aClient *sptr;
		register int i;
		int num = 0, tot;
		int byip = 0;
		int cc = get_client_class(cptr);
		aConfItem *aconf;
		aClass *cs;
		
		if (!username)
			username = nothing;
		aconf = cptr->confs->value.aconf;
		cs = aconf->class;
		tot = get_con_freq(cs);
		if (tot)
		{
			if (tot < 0)
			{
				byip++;
				tot = -tot;
			}
			if (IsLimitIp(aconf))
				byip++;
			for (i = highest_fd; i >= 0; i--)
			{
			if (!(sptr=local[i]) || !IsPerson(sptr))
				continue;
			if (cs != sptr->confs->value.aconf->class)
				continue;
			if ((sptr->ip.s_addr == cptr->ip.s_addr) &&
				(byip || !strcmp(sptr->user->username,
						username)) &&
					(++num >= tot))
				{
					sendto_one(cptr, ":%s NOTICE %s :This server is currently limited to %i client%s per user in your class",
						me.name, cptr->name, tot, tot==1?"":"s"); 
					sendto_flagops(UFLAGS_LMODE, "Rejecting for too many clients: %s [%s@%s]",
						cptr->name, cptr->user->username, cptr->user->host);	
					if (reason)
						*reason = toomany;
					return 1;
				}
			} /* for() */
		} /* if (tot) */
        }
#endif /* LIMIT_UH */
	if (tmp)
	{
		if (reason)
			*reason = tmp->passwd ? tmp->passwd : klined;
		return -1;
	}
	return 0;
}

#ifdef G_LINES

int	find_gline(cptr, checkeline, reason)
aClient	*cptr;
int checkeline;
char **reason;
{
	char *name;
	aConfItem *tmp;
	static char klined[20] = "K-lined";

	if (reason)
		*reason = klined;
	if (!cptr->user)
		return 0;

	name = cptr->user->username;
#ifdef E_LINES
	if (checkeline && find_eline(cptr->sockhost, cptr->ipstr, name));
		return 0;
#endif

	if ((tmp = find_dichconf(Glines, name, cptr->sockhost, cptr->ipstr,
			NULL, 0, NULL)) != NULL)
	{
		if (tmp->expires)
		{
			char blah[255];

			sendto_one(cptr, ":%s NOTICE %s :You've been banned for: %s",
				me.name, cptr->name,
				tmp->passwd ? tmp->passwd : "<No reason>");
			sprintf(blah, "Ban expires at %s",
				myctime(tmp->expires));
			sendto_one(cptr, err_str(ERR_YOUREBANNEDCREEP),
			me.name, cptr->name, blah);
		}
		else
			sendto_one(cptr, err_str(ERR_YOUREBANNEDCREEP),
				me.name, cptr->name,
				tmp->passwd ? tmp->passwd : "<No reason>");
	}
	if (tmp)
	{
		if (reason)
			*reason = tmp->passwd ? tmp->passwd : klined;
		return -1;
	}
	return 0;
}
#endif /* G_LINES */


#ifdef B_LINES
int	find_bline(cptr)
aClient *cptr;
{
	if (!cptr->user)
		return 0;

	return (find_dichconf(Blines, cptr->user->username,
		cptr->sockhost, cptr->ipstr, NULL, 0, NULL) != NULL);
}
#endif /* B_LINES */


#ifdef R_LINES
/* find_restrict works against host/name and calls an outside program 
 * to determine whether a client is allowed to connect.  This allows 
 * more freedom to determine who is legal and who isn't, for example
 * machine load considerations.  The outside program is expected to 
 * return a reply line where the first word is either 'Y' or 'N' meaning 
 * "Yes Let them in" or "No don't let them in."  If the first word 
 * begins with neither 'Y' or 'N' the default is to let the person on.
 * It returns a value of 0 if the user is to be let through -Hoppie
 */
int	find_restrict(cptr)
aClient	*cptr;
{
	aConfItem *tmp;
	char	reply[80], temprpl[80];
	char	*rplhold = reply, *host, *name, *s;
	char	rplchar = 'Y';
	int	pi[2], rc = 0, n;

	if (!cptr->user)
		return 0;
	name = cptr->user->username;
	host = cptr->sockhost;
	Debug((DEBUG_INFO, "R-line check for %s[%s]", name, host));

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if (tmp->status != CONF_RESTRICT ||
		    (tmp->host && host && match(tmp->host, host)) ||
		    (tmp->name && name && match(tmp->name, name)))
			continue;

		if (BadPtr(tmp->passwd))
		    {
			sendto_ops("Program missing on R-line %s/%s, ignoring",
				   name, host);
			continue;
		    }

		if (pipe(pi) == -1)
		    {
			report_error("Error creating pipe for R-line %s:%s",
				     &me);
			return 0;
		    }
		switch (rc = fork())
		{
		case -1 :
			report_error("Error forking for R-line %s:%s", &me);
			return 0;
		case 0 :
		    {
			Reg1	int	i;

			(void)close(pi[0]);
			for (i = 2; i < MAXCONNECTIONS; i++)
				if (i != pi[1])
					(void)close(i);
			if (pi[1] != 2)
				(void)dup2(pi[1], 2);
			(void)dup2(2, 1);
			if (pi[1] != 2 && pi[1] != 1)
				(void)close(pi[1]);
			(void)execlp(tmp->passwd, tmp->passwd, name, host, 0);
			exit(-1);
		    }
		default :
			(void)close(pi[1]);
			break;
		}
		*reply = '\0';
		(void)dgets(-1, NULL, 0); /* make sure buffer marked empty */
		while ((n = dgets(pi[0], temprpl, sizeof(temprpl)-1)) > 0)
		    {
			temprpl[n] = '\0';
			if ((s = (char *)index(temprpl, '\n')))
			      *s = '\0';
			if (strlen(temprpl) + strlen(reply) < sizeof(reply)-2)
				(void)irc_sprintf(rplhold, "%s %s", rplhold,
					temprpl);
			else
			    {
				sendto_ops("R-line %s/%s: reply too long!",
					   name, host);
				break;
			    }
		    }
		(void)dgets(-1, NULL, 0); /* make sure buffer marked empty */
		(void)close(pi[0]);
		(void)kill(rc, SIGKILL); /* cleanup time */
		(void)wait(0);

		rc = 0;
		while (*rplhold == ' ')
			rplhold++;
		rplchar = *rplhold; /* Pull out the yes or no */
		while (*rplhold != ' ')
			rplhold++;
		while (*rplhold == ' ')
			rplhold++;
		(void)strcpy(reply,rplhold);
		rplhold = reply;

		if ((rc = (rplchar == 'n' || rplchar == 'N')))
			break;
	    }
	if (rc)
	    {
		sendto_one(cptr, ":%s %d %s :Restriction: %s",
			   me.name, ERR_YOUREBANNEDCREEP, cptr->name,
			   reply);
		return -1;
	    }
	return 0;
}
#endif


/*
** check against a set of time intervals
*/

int	check_time_interval(interval, reply)
char	*interval, *reply;
{
	struct tm *tptr;
 	time_t	tick;
 	char	*p;
 	int	perm_min_hours, perm_min_minutes,
 		perm_max_hours, perm_max_minutes;
 	int	now, perm_min, perm_max;

 	tick = NOW;
	tptr = localtime(&tick);
 	now = tptr->tm_hour * 60 + tptr->tm_min;

	while (interval)
	    {
		p = (char *)index(interval, ',');
		if (p)
			*p = '\0';
		if (sscanf(interval, "%2d%2d-%2d%2d",
			   &perm_min_hours, &perm_min_minutes,
			   &perm_max_hours, &perm_max_minutes) != 4)
		    {
			if (p)
				*p = ',';
			return(0);
		    }
		if (p)
			*(p++) = ',';
		perm_min = 60 * perm_min_hours + perm_min_minutes;
		perm_max = 60 * perm_max_hours + perm_max_minutes;
           	/*
           	** The following check allows intervals over midnight ...
           	*/
		if ((perm_min < perm_max)
		    ? (perm_min <= now && now <= perm_max)
		    : (perm_min <= now || now <= perm_max))
		    {
			if (reply != NULL)
			(void)irc_sprintf(reply,
				":%%s %%d %%s :%s %d:%02d to %d:%02d.",
				"You are not allowed to connect from",
				perm_min_hours, perm_min_minutes,
				perm_max_hours, perm_max_minutes);
			return(ERR_YOUREBANNEDCREEP);
		    }
		if ((perm_min < perm_max)
		    ? (perm_min <= now + 5 && now + 5 <= perm_max)
		    : (perm_min <= now + 5 || now + 5 <= perm_max))
		    {
			if (reply != NULL)
			(void)irc_sprintf(reply, ":%%s %%d %%s :%d minute%s%s",
				perm_min-now,(perm_min-now)>1?"s ":" ",
				"and you will be denied for further access");
			return(ERR_YOUWILLBEBANNED);
		    }
		interval = p;
	    }
	return(0);
}
