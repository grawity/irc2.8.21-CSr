/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_user.c (formerly ircd/s_msg.c)
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
static  char rcsid[] = "@(#)$Id: s_user.c,v 1.3 1997/07/29 19:49:06 cbehrens Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "h.h"

void    send_umode_out PROTO((aClient*, aClient *, int));
void	send_umode PROTO((aClient *, aClient *, int, int, char *));
static	int do_user PROTO((char *, aClient *, aClient*, char *, char *, char *, 
			char *, char *));

static char buf[BUFSIZE], buf2[BUFSIZE];
static int sizebuf = sizeof(buf);

#ifdef CLONE_CHECK
	aClone *clone;
	extern char clonekillhost[100];
#endif

#ifdef IDLE_CHECK
	anIdle *blahidle;
#endif

static int user_modes[]	=
{
	FLAGS_OPER,		'o',
	FLAGS_LOCOP,		'O',
	FLAGS_INVISIBLE,	'i',
	FLAGS_WALLOP,		'w',
	FLAGS_ZMODE,		'z',
	FLAGS_SERVNOTICE,	's',
#ifdef FK_USERMODES
	FLAGS_FMODE,		'f',
	FLAGS_UMODE,		'u',
#endif
#ifdef CLIENT_NOTICES
	FLAGS_CMODE,		'c',
#endif
#ifdef FK_USERMODES
	FLAGS_KMODE,		'k',
#endif
	FLAGS_DMODE,		'd',
	FLAGS_BMODE,		'b',
	FLAGS_LMODE,		'l',
#ifdef SHOW_NICKCHANGES
	FLAGS_NMODE,		'n',
#endif
#ifdef G_MODE
	FLAGS_GMODE,		'g',
#endif
	0, 0
};

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
**				this points to an empty string.  If this
**                              was an ID, it has been translated to a
**                              nick.
**			parv[1]...parv[parc-1]
**				pointers to additional parameters
**                              (possibly ID's)
**			parv[parc] == NULL, *always*
**
**		note:	it is guaranteed that parv[0]..parv[parc-1] are all
**			non-NULL pointers.
*/

/*
** next_client
**	Local function to find the next matching client. The search
**	can be continued from the specified client entry. Normal
**	usage loop is:
**
**	for (x = client; x = next_client(x,mask); x = x->next)
**		HandleMatchingClient;
**	      
*/
aClient *next_client(next, ch)
Reg1	aClient *next;	/* First client to check */
Reg2	char	*ch;	/* search string (may include wilds) */
{
	Reg3	aClient	*tmp = next;

	next = find_client(ch, tmp);
	if (tmp && tmp->prev == next)
		return NULL;
	if (next != tmp)
		return next;
	for ( ; next; next = next->next)
	    {
		if (IsService(next))
			continue;
#ifdef DOG3
		if (!match(ch, next->name))
#else
		if (!match(ch, next->name) || !match(next->name, ch))
#endif
			break;
	    }
	return next;
}

#ifdef DOG3

aClient *next_client_double(next, ch)
/* this slow version needs to be used for hostmasks *sigh * */
Reg1  aClient *next;  /* First client to check */
Reg2  char    *ch;    /* search string (may include wilds) */
{
        Reg3    aClient *tmp = next;

        next = find_client(ch, tmp);
        if (tmp && tmp->prev == next)
                return NULL;
        if (next != tmp)
                return next;
        for ( ; next; next = next->next)
        {
                if (IsService(next))
                        continue;
                if (!match(ch,next->name) || !match(next->name,ch))
                        break;
        }
        return next;
}

#endif

/*
** hunt_server
**
**	Do the basic thing in delivering the message (command)
**	across the relays to the specific server (server) for
**	actions.
**
**	Note:	The command is a format string and *MUST* be
**		of prefixed style (e.g. ":%s COMMAND %s ...").
**		Command can have only max 8 parameters.
**
**	server	parv[server] is the parameter identifying the
**		target server.
**
**	*WARNING*
**		parv[server] is replaced with the pointer to the
**		real servername from the matched client (I'm lazy
**		now --msa).
**
**	returns: (see #defines)
*/
int	hunt_server(cptr, sptr, command, server, parc, parv)
aClient	*cptr, *sptr;
char	*command, *parv[];
int	server, parc;
    {
	aClient *acptr;

	/*
	** Assume it's me, if no server
	*/
	if (parc <= server || BadPtr(parv[server]) ||
	    match(me.name, parv[server]) == 0 ||
	    match(parv[server], me.name) == 0)
		return (HUNTED_ISME);
	/*
	** These are to pickup matches that would cause the following
	** message to go in the wrong direction while doing quick fast
	** non-matching lookups.
	*/
	if ((acptr = find_client(parv[server], NULL)))
		if (acptr->from == sptr->from && !MyConnect(acptr))
			acptr = NULL;
	if (!acptr && (acptr = find_server(parv[server], NULL)))
		if (acptr->from == sptr->from && !MyConnect(acptr))
			acptr = NULL;
	if (!acptr)
		for (acptr = client, (void)collapse(parv[server]);
		     (acptr = next_client(acptr, parv[server]));
		     acptr = acptr->next)
		    {
			if (acptr->from == sptr->from && !MyConnect(acptr))
				continue;
			/*
			 * Fix to prevent looping in case the parameter for
			 * some reason happens to match someone from the from
			 * link --jto
			 */
			if (IsRegistered(acptr) && (acptr != cptr))
				break;
		    }
	 if (acptr)
	    {
		if (IsMe(acptr) || MyClient(acptr))
			return HUNTED_ISME;

                /* this seemingly obscure test replaces a partial name
                 * like *domain* with the full server name    -orabidoo
                 */
                if (parv[server][0] != '.' &&
                    match(acptr->name, parv[server]))
                        parv[server] = ID(acptr);

		sendto_one(acptr, command, parv[0],
			   parv[1], parv[2], parv[3], parv[4],
			   parv[5], parv[6], parv[7], parv[8]);
		return(HUNTED_PASS);
	    } 
	sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name,
		   parv[0], parv[server]);
	return(HUNTED_NOSUCH);
    }

/*
** 'do_nick_name' ensures that the given parameter (nick) is
** really a proper string for a nickname (note, the 'nick'
** may be modified in the process...)
**
**	RETURNS the length of the final NICKNAME (0, if
**	nickname is illegal)
**
**  Nickname characters are in range
**	'A'..'}', '_', '-', '0'..'9'
**  anything outside the above set will terminate nickname.
**  In addition, the first character cannot be '-'
**  or a Digit.
**
**  Note:
**	'~'-character should be allowed, but
**	a change should be global, some confusion would
**	result if only few servers allowed it...
*/

static	int do_nick_name(nick)
char	*nick;
{
	Reg1 char *ch;

	if (*nick == '-' || isdigit(*nick)) /* first character in [0..9-] */
		return 0;

	for (ch = nick; *ch && (ch - nick) < NICKLEN; ch++)
		if (!isvalid(*ch) || isspace(*ch))
			break;

	*ch = '\0';

	return (ch - nick);
}


/*
** canonize
**
** reduce a string of duplicate list entries to contain only the unique
** items.  Unavoidably O(n^2).
*/
char	*canonize(buffer, num)
char	*buffer;
int	*num;
{
	static	char	cbuf[BUFSIZ];
	register char	*s, *t, *cp = cbuf;
	register int	l = 0;
	char	*p = NULL, *p2;
	
	*cp = '\0';
	if (num)
		*num = 0;
	for (s = strtoken(&p, buffer, ","); s; s = strtoken(&p, NULL, ","))
	    {
		if (l)
		    {
			for (p2 = NULL, t = strtoken(&p2, cbuf, ","); t;
			     t = strtoken(&p2, NULL, ","))
				if (!mycmp(s, t))
					break;
				else if (p2)
					p2[-1] = ',';
		    }
		else
			t = NULL;
		if (!t)
		    {
			if (l)
				*(cp-1) = ',';
			else
				l = 1;
			(void)strcpy(cp, s);
			if (num)
				(*num)++;
			if (p)
				cp += (p - s);
		    }
		else if (p2)
			p2[-1] = ',';
	    }
	return cbuf;
}


/*
** register_user
**	This function is called when both NICK and USER messages
**	have been accepted for the client, in whatever order. Only
**	after this the USER message is propagated.
**
**	NICK's must be propagated at once when received, although
**	it would be better to delay them too until full info is
**	available. Doing it is not so simple though, would have
**	to implement the following:
**
**	(actually it has been implemented already for a while) -orabidoo
**
**	1) user telnets in and gives only "NICK foobar" and waits
**	2) another user far away logs in normally with the nick
**	   "foobar" (quite legal, as this server didn't propagate
**	   it).
**	3) now this server gets nick "foobar" from outside, but
**	   has already the same defined locally. Current server
**	   would just issue "KILL foobar" to clean out dups. But,
**	   this is not fair. It should actually request another
**	   nick from local user or kill him/her...
*/

static	int	register_user(cptr, sptr, nick, username)
aClient	*cptr;
aClient	*sptr;
char	*nick, *username;
{
	Reg1	aConfItem *aconf;
        char	*parv[3], *id, *cookie;
	char	*temp, *reason, *bottype = "";
	static	char ubuf[12];
	char	origuser[USERLEN+1];
	short	oldstatus = sptr->status;
	anUser	*user = sptr->user;
	int	i, reject = 0;

	parv[0] = sptr->name;
	parv[1] = parv[2] = NULL;

        if (MyConnect(sptr) && (sptr->flags & FLAGS_NKILLED))
            {
                do {
                        id = id_get();
                } while (hash_find_id(id, NULL));
                strncpyzt(user->id, id, IDLEN+1);
                add_to_id_hash_table(user->id, sptr);
                sptr->servptr = &me;
                add_client_to_llist(&(me.serv->users), sptr);
                SetClient(sptr);
                sendto_one(sptr, ":%s NICK %s", sptr->oldnick, sptr->name);
                sptr->flags &= ~(FLAGS_NKILLED|FLAGS_NKILL);
                goto send_user_out;
            }

	if (MyConnect(sptr))
	{
		strncpy(origuser, username, USERLEN);
		origuser[USERLEN] = (char) 0;
		if (strstr(sptr->info, "^GuArDiAn^"))
			reject = 4; /* Reject Guardian Bots */
		else if (!strcmp(user->host, "null"))
			reject = 1; /* Vlad/Com/Joh */
		else if (!strcmp(user->host, "1"))
			reject = 2; /* EggDrop */
		else if (!strcmp(user->host, "."))
			reject = 3; /* Annoy/OJNK */
		if (sptr->flags & FLAGS_GOTID)
			temp = sptr->username;
		else
			temp = username;
		/* Should we have to call exit_client(), set a "correct"
			username -Sol */
		strncpy(user->username, temp, USERLEN);
		user->username[USERLEN] = '\0';
		if ((i = check_client(sptr, temp)))
		{
			sendto_flagops(UFLAGS_UMODE,"%s from %s.", i == -3 ?
						  "Too many connections" :
			 			  "Unauthorized connection",
				   get_client_host(sptr));
			ircstp->is_ref++;
			return exit_client(cptr, sptr, &me, i == -3 ?
					     "No more connections" :
					     "No Authorization");
		} 
		if (IsUnixSocket(sptr))
			strncpy(user->host, me.sockhost, HOSTLEN);
		else
			strncpy(user->host, sptr->sockhost, HOSTLEN);
		user->host[HOSTLEN] = (char) 0;
		if (strchr(user->host, '\n') || strchr(user->host, ':') ||
			strchr(user->host, ' '))
		{
			sendto_flagops(UFLAGS_OPERS,
				"%s tried an invalid hostname", sptr->name);
			ircstp->is_ref++;
			return exit_client(cptr, sptr, &me, "Invalid hostname");
		}
		aconf = sptr->confs->value.aconf;
		if (sptr->flags & FLAGS_GOTID)
		{
			strncpy(user->username, sptr->username, USERLEN);
			user->username[USERLEN] = (char) 0;
		}
		else
		{
			if (IsNoTilde(aconf))
			{
				strncpy(user->username, origuser, USERLEN);
				user->username[USERLEN] = (char) 0;
			}
			else
			{
				*user->username = '~';
				strncpy(&user->username[1], origuser, USERLEN-1);
				user->username[USERLEN] = (char) 0;
			}
#ifndef IDENTD_ONLY
			if (IsNeedIdentd(aconf))
#else
			if (!IsPassIdentd(aconf))
#endif
			{
                        ircstp->is_ref++;
			sendto_one(sptr, ":%s NOTICE %s :This server doesn't allow connections from your site, unless it runs identd. (RFC1413)", me.name, parv[0]);
                        sendto_one(sptr, ":%s NOTICE %s :Have your system administrator install it if you wish to connect here.", me.name, parv[0]);
			return exit_client(cptr, sptr, &me, "Install identd");
			}
		}

		if (!BadPtr(aconf->passwd) &&
		    !StrEq(sptr->passwd, aconf->passwd))
		{
			ircstp->is_ref++;
			sendto_one(sptr, err_str(ERR_PASSWDMISMATCH),
				   me.name, parv[0]);
			return exit_client(cptr, sptr, &me, "Bad Password");
		}
		bzero(sptr->passwd, sizeof(sptr->passwd));

#ifdef E_LINES
		if (!find_eline(sptr->sockhost, sptr->ipstr,
				sptr->user->username))
		{
#endif
#ifdef G_LINES
			if (find_gline(sptr, 0, &reason))
			{
				ircstp->is_ref++;
				return exit_client(cptr, sptr, &me, reason);
			}
#endif
			if (find_kill(sptr, 1, 0, &reason))
			{
				ircstp->is_ref++;
				return exit_client(cptr, sptr, &me, reason);
			}
#ifdef R_LINES
			if (find_restrict(sptr))
			{
				ircstp->is_ref++;
				return exit_client(cptr, sptr, &me , "R-lined");
			}
#endif
#ifdef E_LINES
		}
#endif

                do {
                        id = id_get();
                } while (hash_find_id(id, NULL));
                strncpyzt(user->id, id, IDLEN+1);

#ifdef KEEP_OPS
                do {
                        cookie = cookie_get();
                } while (find_cookie(cookie) != NULL);
                strncpyzt(sptr->cookie, cookie, COOKIELEN+1);
#endif

		if (oldstatus == STAT_MASTER && MyConnect(sptr))
			(void)m_oper(&me, sptr, 1, parv);
#ifdef STRICT_USERNAMES
{
		register char *tmpstr;
		register int special = 0;
		static char validchars[]=
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-_";

		tmpstr = (user->username[0] == '~') ?
				user->username+1 :
				user->username;
		while(*tmpstr)
		{
			if (!strchr(validchars, *tmpstr))
				special++;
			tmpstr++;
		}
		if (special)
		{
			sendto_flagops(UFLAGS_BMODE,"Invalid username: %s [%s@%s]",
				nick, user->username, user->host);
			ircstp->is_ref++;
			sendto_one(sptr, ":%s NOTICE %s :Sorry, your userid contains invalid characters.", me.name, parv[0]);
			sendto_one(sptr, ":%s NOTICE %s :Only letters and numbers are allowed.", me.name, parv[0]);
			return exit_client(cptr, sptr, &me, "Invalid username");
		}
}
#else /* STRICT_USERNAMES */
#if defined(NO_MIXED_CASE) || defined(NO_SPECIAL)
{
                register char *tmpstr, c;
                register int lower, upper, special;
                char *Myptr, *Myptr2;
		char *username = user->username;

                lower = upper = special = 0;
#ifdef IGNORE_FIRST_CHAR
                tmpstr = (username[0] == '~' ? &username[2] : &username[1]);
#else
                tmpstr = (username[0] == '~' ? &username[1] : username);
#endif
                while(*tmpstr)
		{
			c = *(tmpstr++);
			if (islower(c))
				lower++;
			if (isupper(c))
				upper++;
			if (strchr("[]().;",c) ||
				(!isalnum(c) && !strchr("-_.", c)))
			special++;
		}
#endif /* NO_MIXED_CASE || NO_SPECIAL */
#ifdef NO_MIXED_CASE
                if (lower && upper)
		{
                    sendto_flagops(UFLAGS_BMODE, "Invalid username: %s [%s@%s]",
                               nick, username, user->host);
                    ircstp->is_ref++;
		    sendto_one(sptr, ":%s NOTICE %s :Sorry, your userid contains a mix of lower and upper case characters.", me.name, parv[0]);
		    sendto_one(sptr, ":%s NOTICE %s :Only all lower or all upper case is allowed.", me.name, parv[0]);
                    return exit_client(cptr, sptr, &me, "Invalid username");
		}
#endif /* NO_MIXED_CASE */
#ifdef NO_SPECIAL
                if (special)
		{
                    sendto_flagops(UFLAGS_BMODE,"Invalid username: %s [%s@%s]",
                               nick, user->username, user->host);
                    ircstp->is_ref++;
                    sendto_one(sptr, ":%s NOTICE %s :Sorry, your userid contains invalid characters.", me.name, parv[0]);
                    sendto_one(sptr, ":%s NOTICE %s :Only alphanumeric characters are allowed.", me.name, parv[0]);
                    return exit_client(cptr, sptr, &me, "Invalid username");
		}
#endif /* NO_SPECIAL */
#if defined(NO_MIXED_CASE) || defined(NO_SPECIAL)
}
#endif
#endif /* STRICT_USERNAMES */


#ifdef REJECT_BOTS
	bottype = "Rejecting";
#elif defined(BOTS_NOTICE)
	bottype = "Possible";
#endif
#ifdef B_LINES
		if (!find_bline(cptr))
		{
#endif

#define bot_msg sendto_one(sptr, ":%s NOTICE %s :Sorry, certain bots currently aren't allowed.", me.name, parv[0]); \
                sendto_one(sptr, ":%s NOTICE %s :If you are not a bot, your client is probably outdated, and you should contact the author.", \
		me.name, parv[0]);

#if defined(BOTS_NOTICE) || defined(REJECT_BOTS)
			if (reject == 1)
			{
                                sendto_flagops(UFLAGS_BMODE,"%s vlad/joh/com bot: %s [%s@%s]",
					bottype, nick, user->username,
                                        user->host);
#ifdef REJECT_BOTS
				ircstp->is_ref++;
				bot_msg;
                                return exit_client(cptr, sptr, &me, "No bots allowed");
#endif
			} 
			if ((reject == 2) || strstr(nick, "LameHelp"))
			{
                                sendto_flagops(UFLAGS_BMODE,"%s eggdrop bot: %s [%s@%s]",
					bottype,
                                        nick, user->username, user->host);
#ifdef REJECT_BOTS
				ircstp->is_ref++;
				bot_msg;
                                return exit_client(cptr, sptr, &me, "No bots allowed");
#endif
			}
			if (reject == 3)
			{
				sendto_flagops(UFLAGS_BMODE,"%s ojnk/annoy bot: %s [%s@%s]",
					bottype,
					nick, user->username, user->host);
#ifdef REJECT_BOTS
				ircstp->is_ref++;
				bot_msg;
				return exit_client(cptr, sptr, &me, "No bots allowed");
#endif
			}
			if (reject == 4)
			{
				sendto_flagops(UFLAGS_BMODE,"%s Guardian bot: %s [%s@%s]",
				bottype,
				nick, user->username, user->host);
#ifdef REJECT_BOTS
				ircstp->is_ref++;
				bot_msg;
				return exit_client(cptr, sptr, &me, "No bots allowed");
#endif
			}
                        if (my_stristr(nick, "bot")||my_stristr(nick, "Serv")||
				my_stristr(nick, "help"))
                        {
                                sendto_flagops(UFLAGS_BMODE,"%s bot: %s [%s@%s]",
					bottype,
                                        nick, user->username, user->host);
#ifdef REJECT_BOTS
				ircstp->is_ref++;
				return exit_client(cptr, sptr, &me, "No bots allowed");
#endif
                        }
#endif /* REJECT_BOTS || BOTS_NOTICE */

#ifdef IDLE_CHECK
		if ((blahidle=find_idle(cptr)) != NULL)
		{
			if (NOW <= (blahidle->last+60))
			{
				blahidle->last += 60; /* Add 60 seconds */
				sendto_flagops(UFLAGS_OPERS,"Rejecting idle exceeder %s [%s@%s]",
					nick, user->username, user->host);
				ircstp->is_ref++;
				return exit_client(cptr, sptr, &me, "No bots allowed");
			}
			else
				remove_idle(blahidle);
		}
#endif

#ifdef CLONE_CHECK
                update_clones();
                if ((clone = find_clone(sptr)) == NULL)
                {
                        clone = make_clone();
                        if (clone)
				clone->ip = sptr->ip;
                }
                if (clone)
                {
                        clone->num++;
                        clone->last = NOW;
                        if (clone->num == NUM_CLONES)
                                sendto_flagops(UFLAGS_OPERS, "CloneBot protection activated against %s", user->host);
                        if (clone->num >= NUM_CLONES)
                        {
#ifdef FNAME_CLONELOG
			int logfile;
			int len;

			len = irc_sprintf(buf, "%s: Clonebot rejected: %s!%s@%s\n",
				myctime(NOW), parv[0], sptr->user->username,
				sptr->user->host);
#ifdef BUFFERED_LOGS
			cs_buf_logs(2, buf, len);
#else
			if ((logfile = open(FNAME_CLONELOG,
					O_WRONLY|O_APPEND)) != -1)
			{
				(void)write(logfile, buf, strlen(buf));
				(void)close(logfile);
			}
#endif /* BUFFERED_LOGS */
#endif /* FNAME_CLONELOG */
                                sendto_flagops(UFLAGS_OPERS, "Rejecting clonebot: %s [%s@%s]",
                                        nick, username, user->host);
#ifdef KILL_CLONES
                                strcpy(clonekillhost, user->host);
#endif /* KILL_CLONES */
                                ircstp->is_ref++;
                                return exit_client(cptr, sptr, &me, "CloneBot"
);
                        }
                }
#endif /* CLONE_CHECK */
#ifdef B_LINES
		} /* end of check for ip# */
#endif

#ifdef CLIENT_NOTICES
                sendto_flagops(UFLAGS_CMODE,"Client connecting: %s (%s@%s)",
                                nick, user->username, user->host);
#endif /* CLIENT_NOTICES */
		if (sptr->flags & FLAGS_GOTID)
			if (mycmp(origuser, sptr->username))
				sendto_flagops(UFLAGS_DMODE,"Identd response differs: %s [%s]", nick, origuser);
	    }
	else
	{
		strncpy(user->username, username, USERLEN);
		user->username[USERLEN] = (char) 0;
                if (!UserHasID(sptr))
                        strncpyzt(user->id, sptr->name, IDLEN+1);
	}
	SetClient(sptr);
	c_count++;
	sptr->servptr = find_server(sptr->user->server, NULL);
	if (!sptr->servptr)
	{
		sendto_flagops(UFLAGS_OPERS,
			"Ghost killed: %s on invalid server %s",
			sptr->name, sptr->user->server);
		sendto_serv_butone(NULL, ":%s KILL %s :%s (Ghosted, %s doesn't exist)",
			me.name, sptr->name, me.name, sptr->user->server);
		sptr->flags |= FLAGS_KILLED;
		return exit_client(NULL, sptr, &me, "Ghost");
	}
	add_client_to_llist(&(sptr->servptr->serv->users), sptr);

        if (UserHasID(sptr))
                add_to_id_hash_table(user->id, sptr);

	if (MyConnect(sptr))
	{
		m_clients++;
#ifdef HIGHEST_CONNECTION
		check_max_count();
#endif
		sendto_one(sptr, rpl_str(RPL_WELCOME), me.name, nick, nick);
		/* This is a duplicate of the NOTICE but see below...*/
		sendto_one(sptr, rpl_str(RPL_YOURHOST), me.name, nick,
			   get_client_name(&me, FALSE), version);
#ifdef	IRCII_KLUDGE
		/*
		** Don't mess with this one - IRCII needs it! -Avalon
		*/
		sendto_one(sptr,
			"NOTICE %s :*** Your host is %s, running version %s",
			nick, get_client_name(&me, FALSE), version);
#endif
		sendto_one(sptr, rpl_str(RPL_CREATED),me.name,nick,creation);
		sendto_one(sptr, rpl_str(RPL_MYINFO), me.name, parv[0],
			   me.name, version);
#ifdef KEEP_OPS
                sendto_one(sptr, rpl_str(RPL_YOURCOOKIE), me.name, nick,
                           sptr->cookie);
#endif
		(void)m_lusers(sptr, sptr, 1, parv);
		(void)m_motd(sptr, sptr, 1, parv);
		nextping = NOW;
	    }
	else if (IsServer(cptr))
	    {
		aClient	*acptr;

		if ((acptr = sptr->servptr) && acptr->from != sptr->from)
		   {
			sendto_ops("Bad User [%s] :%s USER %s %s, != %s[%s]",
				cptr->name, nick, user->username, user->server,
				acptr->name, acptr->from->name);
			sendto_one(cptr, ":%s KILL %s :%s (%s != %s[%s])",
				   me.name, ID(sptr), me.name, user->server,
				   acptr->from->name, acptr->from->sockhost);
			sptr->flags |= FLAGS_KILLED;
			return exit_client(sptr, sptr, &me,
					   "USER server wrong direction");
		   }
	       /*
		* Super GhostDetect:
		*   If we can't find the server the user is supposed to be on,
		* then simply blow the user away.   -Taner
		*/
	       if (!acptr)
		 {
		   sendto_one(cptr,
			      ":%s KILL %s :%s GHOST (no server %s on the net)",
			      me.name,
			      sptr->name, me.name, user->server);
		   sendto_ops("No server %s for user %s[%s@%s] from %s",
				  user->server,
				  sptr->name, user->username,
				  user->host, sptr->from->name);
		   sptr->flags |= FLAGS_KILLED;
		   return exit_client(sptr, sptr, &me, "Ghosted Client");
		 }
	    }

send_user_out:

	send_umode(NULL, sptr, 0, SEND_UMODES, ubuf);
	if (!*ubuf)
		strcpy(ubuf, "+");

#ifdef TS4_ONLY
	if (UserHasID(sptr))
		sendto_serv_butone(cptr, ":%s CLIENT %s %ld %s %d %s %s %s :%s",
				user->server, user->id, sptr->tsinfo,
				nick, sptr->hopcount+1, ubuf, user->username,
				user->host, sptr->info);
	else
		sendto_serv_butone(cptr, "NICK %s %d %ld %s %s %s %s :%s",
			   	nick, sptr->hopcount+1, sptr->tsinfo, ubuf,
			   	user->username, user->host, user->server,
			   	sptr->info);
#else
	if (UserHasID(sptr))
	    {
		sendto_TS4_serv_butone(1, cptr, 
				":%s CLIENT %s %ld %s %d %s %s %s :%s",
				user->server, user->id, sptr->tsinfo,
				nick, sptr->hopcount+1, ubuf, user->username,
				user->host, sptr->info);
		sendto_TS4_serv_butone(0, cptr, 
				"NICK %s %d %ld %s %s %s %s :%s",
			   	nick, sptr->hopcount+1, sptr->tsinfo, ubuf,
			   	user->username, user->host, user->server,
			   	sptr->info);
	    }
	else
		sendto_serv_butone(cptr, "NICK %s %d %ld %s %s %s %s :%s",
			   	nick, sptr->hopcount+1, sptr->tsinfo, ubuf,
			   	user->username, user->host, user->server,
			   	sptr->info);
#endif

#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_NICK, sptr, "NICK %s :%d",
				nick, sptr->hopcount);
	check_services_butone(SERVICE_WANT_USER, sptr, ":%s USER %s %s %s :%s",
				nick, user->username, user->host,
				user->server, sptr->info);
#endif

	return 0;
    }

/*
** checks a proposed new nickname for acceptability, possibly generating
** all kinds of kills and warnings  [this can be either a brand new client,
** or a known one changing nicks]
**
** return values:
**     GO_ON       --  accepted (nick may have been modified)
**     other       --  caller must return the same value (typically, 0 or
**                     FLUSH_BUFFER)
** 
** Note: the 'id' param is the nick if the client is introduced via m_nick
** rather than m_client, and only if it's a new client.
**/
static	int	validate_nickname(from, nick, id, newts, sptr, cptr, user, host,				  dontcheck)
char	*from, *nick, *user, *host, *id;
ts_val	newts;
aClient	*sptr, *cptr;
int	*dontcheck;
{
	char	mnick[NICKLEN+2], *s;
	aClient	*acptr;
	int	sameuser = 0;

	if (MyConnect(sptr) && (s = (char *)index(nick, '~')))
		*s = '\0';
	strncpyzt(mnick, nick, NICKLEN+1);
	/*
	 * if do_nick_name() returns a null name OR if the server sent a nick
	 * name and do_nick_name() changed it in some way (due to rules of nick
	 * creation) then reject it. If from a server and we reject it,
	 * and KILL it. -avalon 4/4/92
	 */
	if (do_nick_name(nick) == 0 ||
	    (IsServer(cptr) && strcmp(nick, mnick)))
	    {
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME),
			   me.name, from, nick);

		if (IsServer(cptr))
		    {
			ircstp->is_kill++;
			sendto_ops("Bad Nick: %s From: %s %s",
				   nick, from, get_client_name(cptr, FALSE));
			sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
				   me.name, nick, me.name, nick, nick, 
				   cptr->name);
			if (sptr != cptr) /* bad nick change */
			    {
				sendto_serv_butone(cptr,
					":%s KILL %s :%s (%s <- %s!%s@%s)",
					me.name, from, me.name,
					get_client_name(cptr, FALSE),
					from,
					sptr->user ? sptr->username : "",
					sptr->user ? sptr->user->server :
						     cptr->name);
				sptr->flags |= FLAGS_KILLED;
				return exit_client(cptr,sptr,&me,"BadNick");
			    }
		    }
		return 0;
	    }

	/*
	** Check against nick name collisions.
	**
	** Put this 'if' here so that the nesting goes nicely on the screen :)
	** We check against server name list before determining if the nickname
	** is present in the nicklist (due to the way the below for loop is
	** constructed). -avalon
	*/
	if ((acptr = find_server(nick, NULL)))
		if (MyConnect(sptr))
		    {
			sendto_one(sptr, err_str(ERR_NICKNAMEINUSE), me.name,
				   BadPtr(from) ? "*" : from, nick);
			return 0; /* NICK message ignored */
		    }
	/*
	** acptr already has result from previous find_server()
	*/
	if (acptr)
	    {
		/*
		** We have a nickname trying to use the same name as
		** a server. Send out a nick collision KILL to remove
		** the nickname. As long as only a KILL is sent out,
		** there is no danger of the server being disconnected.
		** Ultimate way to jupiter a nick ? >;-). -avalon
		*/
		sendto_flagops(UFLAGS_FMODE,"Nick collision on %s(%s <- %s)",
			   sptr->name, acptr->from->name,
			   get_client_name(cptr, FALSE));
		ircstp->is_kill++;
		sendto_one(cptr, ":%s KILL %s :%s (%s <- %s)",
			   me.name, ID(sptr), me.name, acptr->from->name,
			   /* NOTE: Cannot use get_client_name
			   ** twice here, it returns static
			   ** string pointer--the other info
			   ** would be lost
			   */
			   get_client_name(cptr, FALSE));
		sptr->flags |= FLAGS_KILLED;
		return exit_client(cptr, sptr, &me, "Nick/Server collision");
	    }

	if (!(acptr = find_client(nick, NULL)))
		return GO_ON;  /* No collisions, all clear... */
	/*
	** If acptr == sptr, then we have a client doing a nick
	** change between *equivalent* nicknames as far as server
	** is concerned (user is changing the case of his/her
	** nickname or somesuch)
	*/
	if (acptr == sptr)
	    {
		if (strcmp(acptr->name, nick) != 0)
		    {
			/*
			** Allows change of case in his/her nick
			*/
#ifdef NO_NICK_FLOODS
			if (dontcheck)
	                        *dontcheck = 1;
#endif
			return GO_ON; /* -- go and process change */
		    }
		else
			/*
			** This is just ':old NICK old' type thing.
			** Just forget the whole thing here. There is
			** no point forwarding it to anywhere,
			** especially since servers prior to this
			** version would treat it as nick collision.
			*/
			return 0; /* NICK Message ignored */
	    }

	/*
	** Note: From this point forward it can be assumed that
	** acptr != sptr (point to different client structures).
	*/
	/*
	** If the older one is "non-person", the new entry is just
	** allowed to overwrite it. Just silently drop non-person,
	** and proceed with the nick. This should take care of the
	** "dormant nick" way of generating collisions...
	*/
	if (IsUnknown(acptr))
	    {
		/* can't be other than mine on a TS>=3 net */
		exit_client(NULL, acptr, &me, "Overridden");
		return GO_ON;
	    }
	/*
	** Decide, we really have a nick collision and deal with it
	*/
	if (!IsServer(cptr))
	    {
		/*
		** NICK is coming from local client connection. Just
		** send error reply and ignore the command.
		*/
		sendto_one(sptr, err_str(ERR_NICKNAMEINUSE),
			   /* parv[0] (from) is empty when connecting */
			   me.name, BadPtr(from) ? "*" : from, nick);
		return 0; /* NICK message ignored */
	    }
	/*
	** NICK was coming from a server connection. Means that the same
	** nick is registerd for different users by different server.
	** This is either a race condition (two users coming online about
	** same time, or net reconnecting) or just two net fragmens becoming
	** joined and having same nicks in use. We cannot have TWO users with
	** same nick--purge this NICK from the system with a KILL... >;)
	*/
	/*
	** This seemingly obscure test (sptr == cptr) differentiates
	** between "NICK new" (TRUE) and ":old NICK new" (FALSE) forms.
	*/
	/* 
	** Changed to something reasonable like IsServer(sptr)
	** (true if "NICK new", false if ":old NICK new") -orabidoo
	*/

	if (IsServer(sptr))
	    {
	/*
	** A new NICK being introduced by a neighbouring
	** server (e.g. message type "NICK new" received)
	*/
		if (acptr->user)
			sameuser = (user && host && 
			         mycmp(acptr->user->username, user) == 0 &&
			         mycmp(acptr->user->host, host) == 0);
		else
			newts = 0;  /* force both kills */

		if (!newts || !acptr->tsinfo || (newts == acptr->tsinfo))
		    {
		       sendto_flagops(UFLAGS_FMODE,"Nick collision on %s(%s <- %s)(both killed)",
				   acptr->name, acptr->from->name,
				   get_client_name(cptr, FALSE));
			ircstp->is_kill++;
#ifdef RECOVER_NICK_KILLS
			if (!MyClient(acptr))
#endif
			sendto_one(acptr, err_str(ERR_NICKCOLLISION),
				   me.name, acptr->name, acptr->name);
			sendto_one(cptr, /* use received ID backwards */
					   ":%s KILL %s :%s (%s <- %s)",
					   me.name, id, me.name,
					   acptr->from->name,
					   get_client_name(cptr, FALSE));
			sendto_serv_butone(cptr, /* and existing ID forwards */
					   ":%s KILL %s :%s (%s <- %s)",
					   me.name, ID(acptr), me.name, 
					   acptr->from->name,
					   get_client_name(cptr, FALSE));
				   /* NOTE: Cannot use get_client_name twice
				   ** here, it returns static string pointer:
				   ** the other info would be lost
				   */
			/* Just for the sake of paranoia, if acptr has an ID,
			** kill it backwards too  -orabidoo
			*/
			if (DoesTS4(cptr) && UserHasID(acptr))
				sendto_one(cptr,
					":%s KILL %s :%s (%s <- %s)",
					me.name, acptr->user->id, me.name, 
					acptr->from->name,
					get_client_name(cptr, FALSE));

			acptr->flags |= (FLAGS_KILLED|FLAGS_NKILL);
			return exit_client(cptr, acptr, &me, "Nick collision");
		    }
		else if ((sameuser && newts < acptr->tsinfo) ||
			(!sameuser && newts > acptr->tsinfo))
		    {
			/* for the sake of paranoia again, if we got an ID,
			** kill it backwards  -orabidoo
			*/
			if (DoesTS4(cptr) && id && *id == '.')
				sendto_one(cptr, ":%s KILL %s :%s (%s <- %s)",
					me.name, id, me.name, acptr->from->name,
					get_client_name(cptr, FALSE));
			return 0;	/* ignore NICK/CLIENT command */
		    }
		else
		    {
			if (sameuser)
		      sendto_flagops(UFLAGS_FMODE,"Nick collision on %s(%s <- %s)(older killed)",
				   acptr->name, acptr->from->name,
				   get_client_name(cptr, FALSE));
			else
		      sendto_flagops(UFLAGS_FMODE,"Nick collision on %s(%s <- %s)(newer killed)",
				   acptr->name, acptr->from->name,
				   get_client_name(cptr, FALSE));

			ircstp->is_kill++;
#ifdef RECOVER_NICK_KILLS
			if (!MyClient(acptr))
#endif
			sendto_one(acptr, err_str(ERR_NICKCOLLISION),
				   me.name, acptr->name, acptr->name);
			sendto_serv_butone(cptr, /* all servers but sptr */
					   ":%s KILL %s :%s (%s <- %s)",
					   me.name, ID(acptr), me.name, 
					   acptr->from->name,
					   get_client_name(cptr, FALSE));
			/* more paranoia... -orabidoo */
			if (DoesTS4(cptr) && UserHasID(acptr))
				sendto_one(cptr,
					":%s KILL %s :%s (%s <- %s)",
					me.name, acptr->user->id, 
					me.name, acptr->from->name,
					get_client_name(cptr, FALSE));
			acptr->flags |= (FLAGS_KILLED|FLAGS_NKILL);
			(void)exit_client(cptr, acptr, &me, "Nick collision");
			return GO_ON;
		    }
	    }
	/*
	** A NICK change has collided (e.g. message type
	** ":old NICK new". This requires more complex cleanout.
	** Both clients must be purged from this server, with kills
	** sent forward for both, and backwards for the new, and
	** for the old too if we have an ID for it.
	*/
	if (acptr->user && sptr->user)
		sameuser = mycmp(acptr->user->username, sptr->user->username)
				 == 0 &&
			   mycmp(acptr->user->host, sptr->user->host) == 0;
	else
		newts = 0;   /* force both kills */

	if (!newts || !acptr->tsinfo || (newts == acptr->tsinfo))
	    {
	sendto_flagops(UFLAGS_FMODE,"Nick change collision from %s to %s(%s <- %s)(both killed)",
			   sptr->name, acptr->name, acptr->from->name,
			   get_client_name(cptr, FALSE));
		ircstp->is_kill++;
#ifdef RECOVER_NICK_KILLS
		if (!MyClient(acptr))
#endif
		sendto_one(acptr, err_str(ERR_NICKCOLLISION),
			   me.name, acptr->name, acptr->name);
		sendto_serv_butone(cptr, /* KILL old from outgoing servers */
				   ":%s KILL %s :%s (%s(%s) <- %s)",
				   me.name, ID(sptr), me.name, 
				   acptr->from->name, acptr->name, 
				   get_client_name(cptr, FALSE));
		sendto_serv_butone(cptr, /* ... and the collided one too */
			   	   ":%s KILL %s :%s (%s <- %s(%s))",
				   me.name, ID(acptr), me.name, 
				   acptr->from->name,
			           get_client_name(cptr, FALSE), sptr->name);
		ircstp->is_kill++;
		sendto_one(cptr, /* KILL sptr from incoming link */
				   ":%s KILL %s :%s (%s(%s) <- %s)",
			   me.name,
			   ((UserHasID(sptr) && DoesTS4(cptr)) ? sptr->user->id
			   				       : nick), 
			   /* We're killing sptr on this link, but it's
			   ** known as 'nick' by its server; so either
			   ** we're sure to send sptr's id (and that means
			   ** not using the ID macro, which can return a nick),
			   ** or we send the new nick.   -orabidoo
			   */
			   me.name, acptr->from->name, acptr->name, 
			   get_client_name(cptr, FALSE));

		/* are we feeling paranoid today?  -orabidoo */
		if (DoesTS4(cptr) && UserHasID(acptr))
			sendto_one(cptr, ":%s KILL %s :%s (%s <- %s(%s))",
				me.name, acptr->user->id, me.name,
				acptr->from->name,
				get_client_name(cptr, FALSE), sptr->name);

		acptr->flags |= (FLAGS_KILLED|FLAGS_NKILL);
		(void)exit_client(NULL, acptr, &me, "Nick collision(new)");
		sptr->flags |= FLAGS_KILLED; /* not my client */
		return exit_client(cptr, sptr, &me, "Nick collision(old)");
	    }
	else if ((sameuser && newts < acptr->tsinfo) ||
		 (!sameuser && newts > acptr->tsinfo))
	    {
		if (sameuser)
    sendto_flagops(UFLAGS_FMODE,"Nick change collision from %s to %s(%s <- %s)(older killed)",
			   sptr->name, acptr->name, acptr->from->name,
			   get_client_name(cptr, FALSE));
		else
    sendto_flagops(UFLAGS_FMODE,"Nick change collision from %s to %s(%s <- %s)(newer killed)",
			   sptr->name, acptr->name, acptr->from->name,
			   get_client_name(cptr, FALSE));
		ircstp->is_kill++;
		sendto_serv_butone(cptr, /* KILL old from outgoing servers */
				":%s KILL %s :%s (%s(%s) <- %s)",
				me.name, ID(sptr), me.name, acptr->from->name, 
				acptr->name, get_client_name(cptr, FALSE));
		/* how about a shrink?  -orabidoo */
		if (DoesTS4(cptr) && UserHasID(sptr))
			sendto_one(cptr, ":%s KILL %s :%s (%s(%s) <- %s)",
				me.name, sptr->user->id, me.name,
				acptr->from->name, acptr->name,
				get_client_name(cptr, FALSE));
		sptr->flags |= (FLAGS_KILLED|FLAGS_NKILL);
		if (sameuser)
		    return exit_client(cptr, sptr, &me, "Nick collision(old)");
		else
		    return exit_client(cptr, sptr, &me, "Nick collision(new)");
	    }
	else
	    {
		if (sameuser)
		    sendto_flagops(UFLAGS_FMODE,"Nick collision on %s(%s <- %s)(older killed)",
				   acptr->name, acptr->from->name,
				   get_client_name(cptr, FALSE));
		else
		    sendto_flagops(UFLAGS_FMODE,"Nick collision on %s(%s <- %s)(newer killed)",
				   acptr->name, acptr->from->name,
				   get_client_name(cptr, FALSE));

		ircstp->is_kill++;
#ifdef RECOVER_NICK_KILLS
		if (!MyClient(acptr))
#endif
		sendto_one(acptr, err_str(ERR_NICKCOLLISION),
			       me.name, acptr->name, acptr->name);
		sendto_serv_butone(cptr, ":%s KILL %s :%s (%s <- %s)",
				   me.name, ID(acptr), me.name,
				   acptr->from->name,
				   get_client_name(cptr, FALSE));
		/* they're all out to get me  -orabidoo */
		if (DoesTS4(cptr) && UserHasID(acptr))
			sendto_one(cptr, ":%s KILL %s :%s (%s <- %s)",
				   me.name, acptr->user->id, me.name,
				   acptr->from->name,
				   get_client_name(cptr, FALSE));
		acptr->flags |= (FLAGS_KILLED|FLAGS_NKILL);
		(void)exit_client(cptr, acptr, &me, "Nick collision");
		return GO_ON;
	    }
	return GO_ON;
}

static	aClient	*add_remote_user(cptr, hops, ts, nick, umode)
aClient	*cptr;
int	hops;
ts_val	ts;
char	*nick, *umode;
{
	aClient	*sptr;
    	Reg1	int *s, flag;
	Reg2	char *m;

	sptr = make_client(cptr);
	add_client_to_list(sptr);
	sptr->hopcount = hops;
	if (ts)
		sptr->tsinfo = ts;
	else
	    {
		/* this should never happen by now */
		ts = sptr->tsinfo = make_ts();
		ts_warn("Remote nick %s introduced without a TS", nick);
	    }
	/* copy the nick in place */
	(void)strcpy(sptr->name, nick);
	(void)add_to_client_hash_table(nick, sptr);

	/*
	** parse the usermodes -orabidoo
	*/
	m = umode;
	while (*m)
	    {
		for (s = user_modes; (flag = *s); s += 2)
			if (*m == *(s+1))
			    {
				if (flag == FLAGS_INVISIBLE)
					i_count++;
				if (flag == FLAGS_OPER)
					o_count++;
				sptr->extraflags |= flag&SEND_UMODES;
				break;
			    }
		m++;
	    }

	return sptr;
}

/*
** m_client
**      parv[0] = sender prefix (server)
**      parv[1] = ID
**      parv[2] = TS
**      parv[3] = nick
**      parv[4] = hopcount
**      parv[5] = umode
**      parv[6] = username
**      parv[7] = hostname
**      parv[8] = ircname
*/
int	m_client(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	int	r, ts;
	char	nick[NICKLEN+2];
	aClient	*acptr;

	if (!IsServer(sptr) || parc < 9)
		return 0;

	if ((acptr = hash_find_id(parv[1], NULL)))
	    {
	       sendto_flagops(UFLAGS_FMODE,"ID collision on %s(%s <- %s)(both killed)",
			   acptr->name, acptr->from->name,
			   get_client_name(cptr, FALSE));
		ircstp->is_kill++;
#ifdef RECOVER_NICK_KILLS
		if (!MyClient(acptr))
#endif
		sendto_one(acptr, err_str(ERR_IDCOLLISION),
			   me.name, acptr->name);
		sendto_serv_butone(NULL, /* all servers */
				   ":%s KILL %s :%s (%s <- %s)",
				   me.name, parv[1], me.name, acptr->from->name,
				   get_client_name(cptr, FALSE));
		acptr->flags |= (FLAGS_KILLED|FLAGS_NKILL);
		(void)exit_client(cptr, acptr, &me, "ID collision");
	    	return 0;
	    }

	ts = atoi(parv[2]);
	strncpyzt(nick, parv[3], NICKLEN+1);

	if ((r = validate_nickname(parv[0], nick, parv[1], ts, sptr, cptr, 
	     parv[6], parv[7], NULL)) != GO_ON)
		return r;

	acptr = add_remote_user(cptr, atoi(parv[4]), ts, nick, parv[5]);
	return do_user(nick, cptr, acptr, parv[6], parv[7], parv[0],
			parv[8], parv[1]);
}

/*
** m_nick
**	parv[0] = sender prefix
**	parv[1] = nickname
**	parv[2]	= optional hopcount when new user; TS when nick change
**	parv[3] = optional TS
**	parv[4] = optional umode
**	parv[5] = optional username
**	parv[6] = optional hostname
**	parv[7]	= optional server
**	parv[8]	= optional ircname
*/
int	m_nick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	char	nick[NICKLEN+2], *user = NULL, *host = NULL;
	ts_val	newts = 0;
	int	r;
	int	dontcheck = 0;

	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
			   me.name, parv[0]);
		return 0;
	    }
	if (parc < 3 && IsServer(cptr))
	    {
		ts_warn("Remote nick `%s' without a TS from %s -- IGNORED", 
				parv[1], sptr->name);
		return 0;
	    }
	if (IsServer(sptr) && parc < 9)
	    {
	    	ts_warn("Remote nick `%s' with bad params from %s -- IGNORED", 
			parv[1], sptr->name);
		return 0;
	    }

	if (IsServer(sptr))
	    {
		newts = atol(parv[3]);
		user = parv[5];
		host = parv[6];
	    }
	else if (IsServer(cptr))
		newts = atol(parv[2]);
	strncpyzt(nick, parv[1], NICKLEN+1);

	if ((r = validate_nickname(parv[0], nick, nick, newts, sptr, cptr, 
             user, host, &dontcheck)) != GO_ON)
		return r;
#ifdef NO_NICK_FLOODS
        if (!dontcheck && MyClient(sptr) && IsPerson(sptr) && IsRegistered(sptr))
        {
/* "lastnick" will actually be the first time a person did a /nick
   if "lastnick" is 0 (has never /nick'd) or if "lastnick" is more
   than 15 seconds ago, then "lastnick" will be reset to NOW
   Basically, when someone hits 4 nick changes in 15 seconds, boom.
*/

                if (!sptr->lastnick || (NOW-sptr->lastnick > 15))
                {
                        sptr->numnicks = 0;
                        sptr->lastnick = NOW;
                }
                sptr->numnicks++;
                if (sptr->numnicks > 3)
                {
                        sptr->lastnick = NOW+15; /* Hurt the person */
                        sendto_flagops(UFLAGS_OPERS,"Nick flooding detected by: %s (%s@%s)",
                                sptr->name, sptr->user->username, sptr->user->host);
                        sendto_one(sptr, err_str(ERR_TOOMANYNICKS),
                                   me.name, sptr->name);
                        return 0;
                }
        }
#endif /* NO_NICK_FLOODS */

	if (IsServer(sptr))
	    {
#ifdef TS4_GLOBAL_ONLY
		ts_warn("remote user without ID %s(%s<-%s)", nick, parv[7],
			cptr->name);
#endif
		sptr = add_remote_user(cptr, atoi(parv[2]), newts, 
					nick, parv[4]);
		return do_user(nick, cptr, sptr, parv[5], parv[6], parv[7],
			parv[8], NULL);
	    }
	else if (sptr->name[0])
	    {
		/*
		** Client just changing his/her nick. If he/she is
		** on a channel, send note of change to all clients
		** on that channel. Propagate notice to other servers.
		*/
		if (mycmp(parv[0], nick))
			sptr->tsinfo = (newts ? newts : make_ts());
#ifdef SHOW_NICKCHANGES
                if (MyConnect(sptr) && IsRegisteredUser(sptr) &&
                        IsPerson(sptr))
                        sendto_flagops(UFLAGS_NMODE,
                                "Nick change: From %s to %s [%s@%s]",
                                parv[0], nick, sptr->user->username,
                                        sptr->user->host);
#endif
		if (sptr->user)
		    {
			add_history(sptr, 1);
			sendto_common_channels(sptr, ":%s NICK :%s", 
						parv[0], nick);
			if (MyConnect(sptr))
				sendto_prefix_one(sptr, sptr, ":%s NICK :%s",
						parv[0], nick);
			sendto_serv_butone(cptr, ":%s NICK %s :%ld",
				      ID(sptr), nick, sptr->tsinfo);
		    }
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_NICK, sptr, ":%s NICK :%s",
					parv[0], nick);
#endif
	    }
	else
	    {
		/* Client setting NICK the first time */

		/* This had to be copied here to avoid problems.. */
		(void)strcpy(sptr->name, nick);
		sptr->tsinfo = make_ts();
		if (sptr->user)
			/*
			** USER already received, now we have NICK.
			** *NOTE* For servers "NICK" *must* precede the
			** user message (giving USER before NICK is possible
			** only for local client connection!). register_user
			** may reject the client and call exit_client for it
			** --must test this and exit m_nick too!!!
			*/
			if (register_user(cptr, sptr, nick,
					  sptr->user->username)
			    == FLUSH_BUFFER)
				return FLUSH_BUFFER;
	    }
	/*
	**  Finally set new nick name.
	*/
	if (sptr->name[0])
		(void)del_from_client_hash_table(sptr->name, sptr);
	(void)strcpy(sptr->name, nick);
	if (sptr->user && !UserHasID(sptr))
		(void)strcpy(sptr->user->id, nick);
	(void)add_to_client_hash_table(nick, sptr);
	return 0;
}

/*
** m_message (used in m_private() and m_notice())
** the general function to deliver MSG's between users/channels
**
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
**
** massive cleanup
** rev argv 6/91
**
*/

static	int	m_message(cptr, sptr, parc, parv, notice)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
int	notice;
{
	Reg1	aClient	*acptr;
	Reg2	char	*s;
	aChannel *chptr;
	char	*nick, *server, *p, *cmd, *host;
	int num = 0;
	int resetidle = 0;

	if (notice)
	{
		/* no error messages on notice, EVER!   otherwise
		 * hybrid's AUTH crap breaks compression  -orabidoo
		 */
		if (!IsRegistered(sptr))
			return 0;
	}
	else if (check_registered_user(sptr))
		return 0;

	cmd = notice ? MSG_NOTICE : MSG_PRIVATE;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NORECIPIENT),
			   me.name, parv[0], cmd);
		return -1;
	}

	if (parc < 3 || *parv[2] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	}

	if (MyConnect(sptr))
	{
		parv[1] = canonize(parv[1], &num);
		if (IsPerson(sptr) && (num>10))
		{
			sendto_one(sptr, ":%s NOTICE %s :Too many recipients",
				me.name, parv[0]);
			if (num>=20)
				sendto_flagops(UFLAGS_OPERS,
					"%s (%s@%s) tried %i recipients",
					sptr->name, sptr->user->username,
					sptr->user->host, num);
			goto here;
		}
	}
	for (p = NULL, nick = strtoken(&p, parv[1], ","); nick;
	     nick = strtoken(&p, NULL, ","))
	{
		/*
		** nickname addressed?
		*/
		if ((acptr = find_person(nick, NULL)))
		{
			if (!notice && MyConnect(sptr) &&
			    acptr->user && acptr->user->away)
				sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
					   parv[0], acptr->name,
					   acptr->user->away);
			sendto_prefix_one(acptr, sptr, ":%s %s %s :%s",
					  parv[0], cmd, IDORNICK(acptr),
					  parv[2]);
#ifndef RESETIDLEONNOTICE
			if (!notice)
#endif
			if (sptr != acptr) /* don't reset if msging self */
				resetidle = 1;
			continue;
		}

		/*
		** ops/hops/voices msg?
		*/
		/*
		** syntax borrowed and slightly extended from DALnet.  
		** wouldn't have minded borrowing the code too, except that
		** it stinks so bad I wish I'd never seen it... -orabidoo
		*/ 
		if (*nick == '@')
		    {
		    	int flag = CHFL_CHANOP;
			char	*nick0 = nick;

			if ((*++nick == '%' && (flag |= CHFL_HALFOP)) ||
			    (*nick == '+' &&  IsChannelName(nick+1) &&
			     (flag |= CHFL_VOICE|CHFL_HALFOP)))
				nick++;

			if (!(chptr = find_channel(nick, NullChn)))
			    {
			    	if (!notice)
				       sendto_one(sptr, err_str(ERR_NOSUCHNICK),
						  me.name, parv[0], nick);
				continue;
			    }

			if (can_send(sptr, chptr, flag) == 0)
			    {
#ifndef RESETIDLEONNOTICE
				if (!notice)
#endif
					resetidle = 1;
				sendto_channel_butone(cptr, sptr, chptr, flag,
						      ":%s %s %s :%s",
						      parv[0], cmd, nick0,
						      parv[2]);
			    }
			else if (!notice)
				sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
					   me.name, parv[0], nick0);
			continue;
		    }

		/*
		** channel msg?
		*/
		if ((chptr = find_channel(nick, NullChn)))
		{
			if (can_send(sptr, chptr, 0) == 0)
			{
#ifndef RESETIDLEONNOTICE
				if (!notice)
#endif
					resetidle = 1;
				sendto_channel_butone(cptr, sptr, chptr, 0,
						      ":%s %s %s :%s",
						      parv[0], cmd, nick,
						      parv[2]);
			}
			else if (!notice)
				sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
					   me.name, parv[0], nick);
			continue;
		}
	
		/*
		** the following two cases allow masks in NOTICEs
		** (for OPERs only)
		**
		** Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
		*/
		if ((*nick == '$' || *nick == '#') && IsAnOper(sptr))
		{
			if (!(s = (char *)rindex(nick, '.')))
			{
				sendto_one(sptr, err_str(ERR_NOTOPLEVEL),
					   me.name, parv[0], nick);
				continue;
			}
			while (*++s)
				if (*s == '.' || *s == '*' || *s == '?')
					break;
			if (*s == '*' || *s == '?')
			{
				sendto_one(sptr, err_str(ERR_WILDTOPLEVEL),
					   me.name, parv[0], nick);
				continue;
			}
			sendto_match_butone(IsServer(cptr) ? cptr : NULL, 
					    sptr, nick + 1,
					    (*nick == '#') ? MATCH_HOST :
							     MATCH_SERVER,
					    ":%s %s %s :%s", parv[0],
					    cmd, nick, parv[2]);
#ifndef RESETIDLEONNOTICE
			if (!notice)
#endif
				resetidle = 1;
			continue;
		}
	
		/*
		** user[%host]@server addressed?
		*/
		if ((server = (char *)index(nick, '@')) &&
		    (acptr = find_server(server + 1, NULL)))
		{
			int count = 0;

			/*
			** Not destined for a user on me :-(
			*/
			if (!IsMe(acptr))
			{
				sendto_one(acptr,":%s %s %s :%s", parv[0],
					   cmd, nick, parv[2]);
#ifndef RESETIDLEONNOTICE
				if (!notice)
#endif
					resetidle = 1;
				continue;
			}
			*server = '\0';

			if ((host = (char *)index(nick, '%')))
				*host++ = '\0';

			/*
			** Look for users which match the destination host
			** (no host == wildcard) and if one and one only is
			** found connected to me, deliver message!
			*/
			acptr = find_userhost(nick, host, NULL, &count);
			if (server)
				*server = '@';
			if (host)
				*--host = '%';
			if (acptr)
			{
				if (count == 1)
				{
					sendto_prefix_one(acptr, sptr,
							  ":%s %s %s :%s",
					 		  parv[0], cmd,
							  nick, parv[2]);
#ifndef RESETIDLEONNOTICE
					if (!notice)
#endif
						resetidle = 1;
				}
				else if (!notice)
					sendto_one(sptr,
						   err_str(ERR_TOOMANYTARGETS),
						   me.name, parv[0], nick);
				continue;
			}
		}
		if (!notice && nick[0] != '.')
			sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name,
				   parv[0], nick);
            }
here:
	if (resetidle && MyConnect(sptr) && sptr->user)
	{
		sptr->user->last = NOW;	
		sptr->user->last2 = NOW;
#ifndef NO_PRIORITY
		if (sptr->fdlist != &new_fdlists[0])
		{
			if (sptr->fdlist)
				del_from_fdlist(sptr->fd, sptr->fdlist);
			sptr->fdlist = &new_fdlists[0];
			add_to_fdlist(sptr->fd, sptr->fdlist);
		}
#endif
	}
	return 0;
}

/*
** m_private
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
*/

int	m_private(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	return m_message(cptr, sptr, parc, parv, 0);
}

/*
** m_notice
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = notice text
*/

int	m_notice(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	return m_message(cptr, sptr, parc, parv, 1);
}

static	void	do_who(sptr, acptr, repchan, lp)
aClient *sptr;
aClient *acptr;
aChannel *repchan;
Link *lp;
{
	char	status[8];
	int	i = 0, op;

	if (acptr->user->away)
		status[i++] = 'G';
	else
		status[i++] = 'H';
	if (IsAnOper(acptr))
		status[i++] = '*';
	if ((repchan != NULL) && (lp == NULL))
		lp = find_user_link(repchan->members, acptr);
	if (lp != NULL)
	{
		if (lp->flags & CHFL_CHANOP)
			status[i++] = '@';
		if (lp->flags & CHFL_HALFOP)
			status[i++] = '%';
		if (lp->flags & CHFL_VOICE)
			status[i++] = '+';
	}
	status[i] = '\0';
	sendto_one(sptr, rpl_str(RPL_WHOREPLY), me.name, sptr->name,
		   (repchan) ? (repchan->chname) : "*", acptr->user->username,
		   acptr->user->host, acptr->user->server, acptr->name,
		   status, acptr->hopcount, acptr->info);
}

/*
** m_who
**	parv[0] = sender prefix
**	parv[1] = nickname mask list
**	parv[2] = additional selection flag, only 'o' for now.
*/
int	m_who(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	aClient *acptr;
	Reg2	char	*mask = parc > 1 ? parv[1] : NULL;
	Reg3	Link	*lp;
	aChannel *chptr;
	aChannel *mychannel = NULL;
	char	*channame = NULL, *s;
	int	oper = parc > 2 ? (*parv[2] == 'o' ): 0; /* Show OPERS only */
	int	member;
	int	maxmatches = 500;

	if (check_registered_user(sptr))
		return 0;

	mychannel = NullChn;
	if (sptr->user)
		if ((lp = sptr->user->channel))
			mychannel = lp->value.chptr;

	if (MyConnect(sptr))
	{
		sptr->user->last2 = NOW;
#ifndef NO_PRIORITY
		if (sptr->fdlist != &new_fdlists[0])
		{
			if (sptr->fdlist)
				del_from_fdlist(sptr->fd, sptr->fdlist);
			sptr->fdlist = &new_fdlists[0];
			add_to_fdlist(sptr->fd, sptr->fdlist);
		}
#endif
	}

	/*
	**  Following code is some ugly hacking to preserve the
	**  functions of the old implementation. (Also, people
	**  will complain when they try to use masks like "12tes*"
	**  and get people on channel 12 ;) --msa
	*/
	if (mask)
		(void)collapse(mask);
	if (!mask || (*mask == (char) 0))
		goto endofwho;
	else if ((*(mask+1) == (char) 0) && (*mask == '*'))
	{
		if (!mychannel)
			goto endofwho;
		channame = mychannel->chname;
	}
	else
		channame = mask;

	if (IsChannelName(channame))
	{
		/*
		 * List all users on a given channel
		 */
		chptr = find_channel(channame, NULL);
		if (chptr)
		{
			member = IsMember(sptr, chptr);
			if (member || !SecretChannel(chptr))
			for (lp = chptr->members; lp; lp = lp->next)
			{
				if (oper && !IsAnOper(lp->value.cptr))
					continue;
				if (IsInvisible(lp->value.cptr) && !member)
					continue;
				do_who(sptr, lp->value.cptr, chptr, lp);
			}
		}
	}
	else if (mask && 
		((acptr = find_client(mask, NULL)) != NULL) &&
		IsPerson(acptr) && (!oper || IsAnOper(acptr)))
	{
		int isinvis = 0;
		aChannel *ch2ptr = NULL;

		isinvis = IsInvisible(acptr);
		for (lp = acptr->user->channel; lp; lp = lp->next)
		{
			chptr = lp->value.chptr;
			member = IsMember(sptr, chptr);
			if (isinvis && !member)
				continue;
			if (member || (!isinvis && PubChannel(chptr)))
			{
				ch2ptr = chptr;
				break;
			}
		}
		do_who(sptr, acptr, ch2ptr, NULL);
	}
	else for (acptr = client; acptr; acptr = acptr->next)
	{
		aChannel *ch2ptr = NULL;
		int	showperson, isinvis;

		if (!IsPerson(acptr))
			continue;
		if (oper && !IsAnOper(acptr))
			continue;

		showperson = 0;
		/*
		 * Show user if they are on the same channel, or not
		 * invisible and on a non secret channel (if any).
		 * Do this before brute force match on all relevant fields
		 * since these are less cpu intensive (I hope :-) and should
		 * provide better/more shortcuts - avalon
		 */
		isinvis = IsInvisible(acptr);
		for (lp = acptr->user->channel; lp; lp = lp->next)
		{
			chptr = lp->value.chptr;
			member = IsMember(sptr, chptr);
			if (isinvis && !member)
				continue;
			if (member || (!isinvis && PubChannel(chptr)))
			{
				ch2ptr = chptr;
				showperson = 1;
				break;
			}
			if (HiddenChannel(chptr) && !SecretChannel(chptr) &&
			    !isinvis)
				showperson = 1;
		}
		if (!acptr->user->channel && !isinvis)
			showperson = 1;
		if (showperson &&
			(!mask ||
			!match(mask, acptr->name) ||
			!match(mask, acptr->user->username) ||
			!match(mask, acptr->user->host) ||
			!match(mask, acptr->user->server) ||
			!match(mask, acptr->info)))
		{
			do_who(sptr, acptr, ch2ptr, NULL);
			if (!--maxmatches)
				goto endofwho;
		}
	}
endofwho:
	sendto_one(sptr, rpl_str(RPL_ENDOFWHO), me.name, parv[0],
		   BadPtr(mask) ?  "*" : mask);
	return 0;
}

/*
** m_whois
**	parv[0] = sender prefix
**	parv[1] = nickname masklist
*/
int	m_whois(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	static anUser UnknownUser;
	static int times = 0;

	Reg2	Link	*lp;
	Reg3	anUser	*user;
	aClient *acptr, *a2cptr;
	aChannel *chptr;
	char	*nick, *tmp, *name;
	char	*p = NULL;
	int	len, mlen, op;

	if (check_registered_user(sptr))
		return 0;
	if (!times)
	{
		UnknownUser.nextu = NULL;
		UnknownUser.channel = NULL;
		UnknownUser.invited = NULL;
		UnknownUser.away = NULL;
		UnknownUser.last = 0;
		UnknownUser.refcnt = 1;
		UnknownUser.joined = 0;
		strcpy(UnknownUser.username, "<Unknown>");
		strcpy(UnknownUser.host, UnknownUser.username);
		strcpy(UnknownUser.server, UnknownUser.username);
		times++;
	}
	if (MyConnect(sptr))
	{
		sptr->user->last2 = NOW;
#ifndef NO_PRIORITY
		if (sptr->fdlist != &new_fdlists[0])
		{
			if (sptr->fdlist)
				del_from_fdlist(sptr->fd, sptr->fdlist);
			sptr->fdlist = &new_fdlists[0];
			add_to_fdlist(sptr->fd, sptr->fdlist);
		}
#endif
	}

    	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
			   me.name, parv[0]);
		return 0;
	    }

	if (parc > 2)
	    {
		if (hunt_server(cptr,sptr,":%s WHOIS %s :%s", 1,parc,parv) !=
		    HUNTED_ISME)
			return 0;
		parv[1] = parv[2];
	    }

	parv[1] = canonize(parv[1], NULL);

	for (tmp = parv[1]; (nick = strtoken(&p, tmp, ",")); tmp = NULL)
	{
		int	invis, showperson, member, wilds;

		(void)collapse(nick);
		wilds = (index(nick, '?') || index(nick, '*'));
		/* No longer sending replies to remote clients
                   when wildcards are given */
		/* Or local ones...something is broked! */
		if (wilds)
			continue;
/* Since there is no more wilds...no need to do this fucking
   shit. - Comstud
		for (acptr = client; (acptr = next_client(acptr, nick));
		     acptr = acptr->next)
/*/
		acptr = find_client(nick, NULL);
		if (acptr == NULL)
		{
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
				   me.name, parv[0], nick);
			continue;
		}
		if (IsServer(acptr) || IsMe(acptr))
			continue;
		/*
		 * 'Rules' established for sending a WHOIS reply:
		 *
		 * - if wildcards are being used dont send a reply if
		 *   the querier isnt any common channels and the
		 *   client in question is invisible and wildcards are
		 *   in use (allow exact matches only);
		 *
		 * - only send replies about common or public channels
		 *   the target user(s) are on;
		 */
		user = acptr->user ? acptr->user : &UnknownUser;
		name = (!*acptr->name) ? "?" : acptr->name;

		invis = IsInvisible(acptr);
		member = (user->channel) ? 1 : 0;
		showperson = (wilds && !invis && !member) || !wilds;
		for (lp = user->channel; lp; lp = lp->next)
		{
			chptr = lp->value.chptr;
			member = IsMember(sptr, chptr);
			if (invis && !member)
				continue;
			if (member || (!invis && PubChannel(chptr)))
			{
				showperson = 1;
				break;
			}
			if (!invis && HiddenChannel(chptr) &&
				    !SecretChannel(chptr))
				showperson = 1;
		}
		if (!showperson)
			continue;

		a2cptr = find_server(user->server, NULL);

		sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
			   parv[0], name,
			   user->username, user->host, acptr->info);
		mlen = strlen(me.name) + strlen(parv[0]) + 6 + strlen(name);
		for (len = 0, *buf = '\0', lp = user->channel;lp;lp = lp->next)
		{
			chptr = lp->value.chptr;
			if (ShowChannel(sptr, chptr))
			{
				if (len + strlen(chptr->chname)
                                            > (size_t) BUFSIZE - 4 - mlen)
				{
					sendto_one(sptr,
						   ":%s %d %s %s :%s",
						   me.name,
						   RPL_WHOISCHANNELS,
						   parv[0], name, buf);
					*buf = '\0';
					len = 0;
				}
#ifdef	TSDEBUG
					if (is_deopped(acptr, chptr))
						*(buf + len++) = '-';
#endif
				if ((op = is_chan_op(acptr, chptr)) &
				     MODE_CHANOP)
					*(buf + len++) = '@';
				if (op & MODE_HALFOP)
					*(buf + len++) = '%';
				if (has_voice(acptr, chptr))
					*(buf + len++) = '+';
				if (len)
					*(buf + len) = '\0';
				(void)strcpy(buf + len, chptr->chname);
				len += strlen(chptr->chname);
				(void)strcat(buf + len, " ");
				len++;
			}
		}
		if (buf[0] != '\0')
			sendto_one(sptr, rpl_str(RPL_WHOISCHANNELS),
				me.name, parv[0], name, buf);

		sendto_one(sptr, rpl_str(RPL_WHOISSERVER),
			   me.name, parv[0], name, user->server,
			   a2cptr?a2cptr->info:"*Not On This Net*");

		if (user->away)
			sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
				   parv[0], name, user->away);

		if (IsAnOper(acptr))
			sendto_one(sptr, rpl_str(RPL_WHOISOPERATOR),
				   me.name, parv[0], name);

		if (acptr->user && MyConnect(acptr))
			sendto_one(sptr, rpl_str(RPL_WHOISIDLE),
				me.name, parv[0], name,
#ifdef SIGNON_TIME
				NOW - user->last,
				acptr->firsttime);
#else
				NOW - user->last);
#endif
		if (p)
			p[-1] = ',';
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFWHOIS), me.name, parv[0], parv[1]);

	return 0;
}

/*
** m_user
**	parv[0] = sender prefix
**	parv[1] = username (login name, account)
**	parv[2] = client host name (used only from other servers)
**	parv[3] = server host name (used only from other servers)
**	parv[4] = users real name info
*/
int	m_user(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
#define	UFLAGS	(FLAGS_INVISIBLE|FLAGS_WALLOP|FLAGS_SERVNOTICE)
	char	*username, *host, *server, *realname;
 
 	/* just ignore a USER from a user that's already registered but
	** NKILLED (important! otherwise ppl can spoof user@host)  -orabidoo
	*/
	if (sptr->flags & FLAGS_NKILLED)
		return 0;
	if (parc > 2 && (username = (char *)index(parv[1],'@')))
		*username = '\0'; 
	if (parc < 5 || *parv[1] == '\0' || *parv[2] == '\0' ||
	    *parv[3] == '\0' || *parv[4] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
	    		   me.name, parv[0], "USER");
		if (IsServer(cptr))
			sendto_ops("bad USER param count for %s from %s",
				   parv[0], get_client_name(cptr, FALSE));
		else
			return 0;
	}

	/* Copy parameters into better documenting variables */

	username = (parc < 2 || BadPtr(parv[1])) ? "<bad-boy>" : parv[1];
	host     = (parc < 3 || BadPtr(parv[2])) ? "<nohost>" : parv[2];
	server   = (parc < 4 || BadPtr(parv[3])) ? "<noserver>" : parv[3];
	realname = (parc < 5 || BadPtr(parv[4])) ? "<bad-realname>" : parv[4];

	return do_user(parv[0], cptr, sptr, username, host, server, realname, NULL);
}


/*
** do_user
*/
static	int	do_user(nick, cptr, sptr, username, host, server, realname, id)
aClient	*cptr, *sptr;
char	*nick, *username, *host, *server, *realname, *id;
{
	anUser	*user;

 	user = make_user(sptr);

	if (!MyConnect(sptr))
	{
		strncpyzt(user->server, server, sizeof(user->server));
		strncpyzt(user->host, host, sizeof(user->host));
		goto user_finish;
	}

	if (!IsUnknown(sptr))
	{
		sendto_one(sptr, err_str(ERR_ALREADYREGISTRED),
			   me.name, nick);
		return 0;
	}
#ifndef	NO_DEFAULT_INVISIBLE
	sptr->extraflags |= FLAGS_INVISIBLE;
	i_count++;
	m_invis++;
#else
	if (atoi(host) & FLAGS_INVISIBLE)
	{
		m_invis++;
		i_count++;
	}
#endif
	sptr->extraflags |= (UFLAGS & atoi(host));
	strncpyzt(user->host, host, sizeof(user->host));
	strncpyzt(user->server, me.name, sizeof(user->server));
user_finish:
	strncpyzt(sptr->info, realname, sizeof(sptr->info));
        if (id && id[0] == '.')
                strncpyzt(user->id, id, IDLEN+1);
	if (sptr->name[0]) /* NICK already received, now we have USER... */
		return register_user(cptr, sptr, sptr->name, username);
	else
	{
		strncpy(sptr->user->username, username, USERLEN);
		sptr->user->username[USERLEN] = (char) 0;
	}
	return 0;
}

/*
** m_quit
**	parv[0] = sender prefix
**	parv[1] = comment
*/
int	m_quit(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	register char *comment = (parc > 1 && parv[1]) ? parv[1] : cptr->name;

	if (MyClient(sptr))
		if (!strncmp("Local Kill", comment, 10) ||
		    !strncmp(comment, "Killed", 6))
			comment = parv[0];
#ifdef CLIENT_NOTICES
	sptr->flags |= FLAGS_NORMALEX;
#endif
	if (strlen(comment) > (size_t) TOPICLEN)
		comment[TOPICLEN] = '\0';
	return IsServer(sptr) ? 0 : exit_client(cptr, sptr, sptr, comment);
    }

/*
** m_kill
**	parv[0] = sender prefix
**	parv[1] = kill victim
**	parv[2] = kill path
*/
int	m_kill(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	char	*inpath = get_client_name(cptr,FALSE);
	char	*user, *path, *killer;
	int	blah = 0, chasing = 0;

	blah = (!(IsServer(cptr) || IsService(cptr))) ?
		cptr->since + (2 + 50/120) : 0;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "KILL");
		if (blah)
			cptr->since = blah;
		return 0;
	}

	user = parv[1];
	path = parv[2]; /* Either defined or NULL (parc >= 2!!) */

#ifdef	OPER_KILL
	if (!IsPrivileged(cptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
#else
	if (!IsServer(cptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
#endif
	if (IsAnOper(cptr))
	{
/*
		if (BadPtr(path))
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
				   me.name, parv[0], "KILL");
			return 0;
		}
*/
		if (BadPtr(path))
			path = parv[0];
		if (strlen(path) > (size_t) TOPICLEN)
			path[TOPICLEN] = '\0';
	}

	if (!(acptr = find_chasing(sptr, user, &chasing)))
		return 0;
	if (chasing && IsPerson(sptr))
		sendto_one(sptr,":%s NOTICE %s :KILL changed from %s to %s",
			   me.name, parv[0], user, acptr->name);
	if (!MyConnect(acptr) && IsLocOp(cptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		cptr->since = blah;
		return 0;
	}
	if (IsServer(acptr) || IsMe(acptr))
	{
		sendto_one(sptr, err_str(ERR_CANTKILLSERVER),
			   me.name, parv[0]);
		return 0;
	}

#ifdef	LOCAL_KILL_ONLY
	if (MyConnect(sptr) && !MyConnect(acptr))
	{
		if (IsPerson(sptr))
		sendto_one(sptr, ":%s NOTICE %s :Nick %s isnt on your server",
			   me.name, parv[0], acptr->name);
		cptr->since = blah;
		return 0;
	}
#endif
	if (!IsServer(cptr))
	{
		/*
		** The kill originates from this server, initialize path.
		** (In which case the 'path' may contain user suplied
		** explanation ...or some nasty comment, sigh... >;-)
		**
		**	...!operhost!oper
		**	...!operhost!oper (comment)
		*/
		if (IsUnixSocket(cptr)) /* Don't use get_client_name syntax */
			inpath = me.sockhost;
		else
			inpath = cptr->sockhost;
		if (!BadPtr(path))
		{
			(void)irc_sprintf(buf, "%s%s (%s)",
				cptr->name, IsOper(sptr) ? "" : "(L)", path);
			path = buf;
		}
		else
			path = cptr->name;
	}
	else if (BadPtr(path))
		 path = "*no-path*"; /* Bogus server sending??? */
	/*
	** Notify all *local* opers about the KILL (this includes the one
	** originating the kill, if from this server--the special numeric
	** reply message is not generated anymore).
	**
	** Note: "acptr->name" is used instead of "user" because we may
	**	 have changed the target because of the nickname change.
	*/
/* This is already done above. - Comstud
	if (IsLocOp(sptr) && !MyConnect(acptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
*/
	if (blah && IsAnOper(cptr) && !MyConnect(acptr))
		cptr->since = blah;

	if (strchr(parv[0], '.'))
		sendto_flagops(UFLAGS_FMODE,"Received KILL message for %s. From %s Path: %s!%s",
		   acptr->name, parv[0], inpath, path);
	else
	        sendto_flagops(UFLAGS_KMODE,"Received KILL message for %s. From %s Path: %s!%s",
                        acptr->name, parv[0], inpath, path);

#if defined(USE_SYSLOG) && defined(SYSLOG_KILL)
	if (IsOper(sptr))
		syslog(LOG_DEBUG,"KILL From %s For %s Path %s!%s",
			parv[0], acptr->name, inpath, path);
#endif
	/*
	** And pass on the message to other servers. Note, that if KILL
	** was changed, the message has to be sent to all links, also
	** back.
	** Suicide kills are NOT passed on --SRB
	*/
        /*
        ** Actually, if the nick has changed, it does NOT to be sent
        ** back again; the server from which we got the message has
        ** already killed the client using the old nick -orabidoo
        */
	if (!MyConnect(acptr) || !MyConnect(sptr) || !IsAnOper(sptr))
	{
		sendto_serv_butone(cptr, ":%s KILL %s :%s!%s",
				   parv[0], ID(acptr), inpath, path);
		acptr->flags |= FLAGS_KILLED;
	}
#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_KILL, sptr, ":%s KILL %s :%s!%s",
				parv[0], acptr->name, inpath, path);
#endif

	/*
	** Tell the victim she/he has been zapped, but *only* if
	** the victim is on current server--no sense in sending the
	** notification chasing the above kill, it won't get far
	** anyway (as this user don't exist there any more either)
	*/
        /* nkilled users don't get anything here, it's done in
        ** exit_client  -orabidoo
        */
#ifdef RECOVER_NICK_KILLS
        if (MyConnect(acptr) && !IsServer(sptr))
#else
        if (MyConnect(acptr))
#endif
		sendto_prefix_one(acptr, sptr,":%s KILL %s :%s!%s",
				  parv[0], acptr->name, inpath, path);
	/*
	** Set FLAGS_KILLED. This prevents exit_one_client from sending
	** the unnecessary QUIT for this. (This flag should never be
	** set in any other place).
        **
        ** It IS set all over the place in m_client, m_nick, and
        ** validate_nickname   -orabidoo
        */
	if (MyConnect(acptr) && MyConnect(sptr) && IsAnOper(sptr))
		(void)irc_sprintf(buf2, "Local kill by %s (%s)", sptr->name,
			BadPtr(parv[2]) ? sptr->name : parv[2]);
	else
	{
		if ((killer = index(path, ' ')))
		{
			while (*killer && *killer != '!')
				killer--;
			if (!*killer)
				killer = path;
			else
				killer++;
		}
		else
			killer = path;
		(void)irc_sprintf(buf2, "Killed (%s)", killer);
	}
	return exit_client(cptr, acptr, sptr, buf2);
}

/***********************************************************************
 * m_away() - Added 14 Dec 1988 by jto. 
 *            Not currently really working, I don't like this
 *            call at all...
 *
 *            ...trying to make it work. I don't like it either,
 *	      but perhaps it's worth the load it causes to net.
 *	      This requires flooding of the whole net like NICK,
 *	      USER, MODE, etc messages...  --msa
 ***********************************************************************/

/*
** m_away
**	parv[0] = sender prefix
**	parv[1] = away message
*/
int	m_away(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	char	*away, *awy2 = parv[1];

	if (check_registered_user(sptr))
		return 0;

	away = sptr->user->away;

	if (parc < 2 || !*awy2)
	    {
		/* Marking as not away */

		if (away)
		    {
			MyFree(away);
			sptr->user->away = NULL;
		    }
		/* if we're not sending away's might as well not send
		 * unaways either -orabidoo
		 */
		sendto_serv_butone(cptr, ":%s AWAY", ID(sptr));
		if (MyConnect(sptr))
			sendto_one(sptr, rpl_str(RPL_UNAWAY),
				   me.name, parv[0]);
#ifdef	USE_SERVICES
		check_services_butonee(SERVICE_WANT_AWAY, ":%s AWAY", parv[0]);
#endif
		return 0;
	    }

	/* Marking as away */

	if (strlen(awy2) > (size_t) TOPICLEN)
		awy2[TOPICLEN] = '\0';
/*
	sendto_serv_butone(cptr, ":%s AWAY :%s", ID(sptr), awy2);
*/
#ifdef	USE_SERVICES
	check_services_butonee(SERVICE_WANT_AWAY, ":%s AWAY :%s",
				parv[0], parv[1]);
#endif

	if (away)
		away = (char *)MyRealloc(away, strlen(awy2)+1);
	else
		away = (char *)MyMalloc(strlen(awy2)+1);

	sptr->user->away = away;
	(void)strcpy(away, awy2);
	if (MyConnect(sptr))
		sendto_one(sptr, rpl_str(RPL_NOWAWAY), me.name, parv[0]);
	return 0;
}

/*
** m_ping
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
int	m_ping(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	char	*origin, *destination;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NOORIGIN), me.name, parv[0]);
		return 0;
	    }
	origin = parv[1];
	destination = parv[2]; /* Will get NULL or pointer (parc >= 2!!) */

	acptr = find_client(origin, NULL);
	if (!acptr)
		acptr = find_server(origin, NULL);
	if (acptr && acptr != sptr)
		origin = cptr->name;
	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	    {
		if ((acptr = find_server(destination, NULL)))
			sendto_one(acptr,":%s PING %s :%s", ID(sptr),
				   origin, destination);
	    	else
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
				   me.name, parv[0], destination);
			return 0;
		    }
	    }
	else
		sendto_one(sptr,":%s PONG %s :%s", me.name,
			   (destination) ? destination : me.name, origin);
	return 0;
    }

/*
** m_pong
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
int	m_pong(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	char	*origin, *destination;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NOORIGIN), me.name, parv[0]);
		return 0;
	    }

	origin = parv[1];
	destination = parv[2];
	cptr->flags &= ~FLAGS_PINGSENT;
	sptr->flags &= ~FLAGS_PINGSENT;

	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	    {
		if ((acptr = find_client(destination, NULL)) ||
		    (acptr = find_server(destination, NULL)))
			sendto_prefix_one(acptr, sptr, ":%s PONG %s %s",
				   parv[0], origin, destination);
		else
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
				   me.name, parv[0], destination);
			return 0;
		    }
	    }
#ifdef	DEBUGMODE
	else
		Debug((DEBUG_NOTICE, "PONG: %s %s", origin,
		      destination ? destination : "*"));
#endif
	return 0;
    }


/*
** m_oper
**	parv[0] = sender prefix
**	parv[1] = oper name
**	parv[2] = oper password
*/
int	m_oper(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aConfItem *aconf;
	char	*name, *password, *encr;
#ifdef CRYPT_OPER_PASSWORD
	char	salt[3];
	extern	char *crypt();
#endif /* CRYPT_OPER_PASSWORD */

	if (check_registered_user(sptr))
		return 0;

	name = parc > 1 ? parv[1] : NULL;
	password = parc > 2 ? parv[2] : NULL;

	if (!IsServer(cptr) && (BadPtr(name) || BadPtr(password)))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "OPER");
		return 0;
	}
	
	/* if message arrived from server, trust it, and set to oper */
	    
	if ((IsServer(cptr) || IsMe(cptr)) && !IsOper(sptr))
	{
		SetOper(sptr);
		sendto_serv_butone(cptr, ":%s MODE %s :+o", ID(sptr), ID(sptr));
		if (IsMe(cptr))
			sendto_one(sptr, rpl_str(RPL_YOUREOPER),
				   me.name, parv[0]);
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr,
				      ":%s MODE %s :+o", parv[0], parv[0]);
#endif
		return 0;
	}
	else if (IsOper(sptr))
	{
		if (MyConnect(sptr))
			sendto_one(sptr, rpl_str(RPL_YOUREOPER),
				   me.name, parv[0]);
		return 0;
	}
	if (!(aconf = find_conf_exact(name, sptr->username, sptr->sockhost,
				      CONF_OPS)) &&
	    !(aconf = find_conf_exact(name, sptr->username,
				      sptr->ipstr, CONF_OPS)))
	{
		sendto_one(sptr, err_str(ERR_NOOPERHOST), me.name, parv[0]);
		return 0;
	}
#ifdef CRYPT_OPER_PASSWORD
        /* use first two chars of the password they send in as salt */

        /* passwd may be NULL. Head it off at the pass... */
        salt[0] = '\0';
        if (password && aconf->passwd)
	{
        	salt[0] = aconf->passwd[0];
		salt[1] = aconf->passwd[1];
		salt[2] = '\0';
		encr = crypt(password, salt);
	}
	else
		encr = "";
#else
	encr = password;
#endif  /* CRYPT_OPER_PASSWORD */

	if ((aconf->status & CONF_OPS) &&
	    StrEq(encr, aconf->passwd) && !attach_conf(sptr, aconf))
	{
		int old = (sptr->extraflags & ALL_UMODES);
		char *s;

		s = index(aconf->host, '@');
		*s++ = '\0';
#ifdef	OPER_REMOTE
		if (aconf->status == CONF_LOCOP)
#else
		if ((match(s,me.sockhost) && !IsLocal(sptr)) ||
		    aconf->status == CONF_LOCOP)
#endif
			SetLocOp(sptr);
		else
		{
			SetOper(sptr);
			o_count++;
		}
		/* These 2 lines update sptr->ping */
		sptr->ping = 0;
		(void)get_client_ping(sptr);
		*--s =  '@';
		add_to_fdlist(sptr->fd, &new_operfdlist);
		sendto_ops("%s (%s@%s) is now operator (%c)", parv[0],
			   sptr->user->username, sptr->user->host,
			   IsOper(sptr) ? 'O' : 'o');
		sptr->extraflags |= (FLAGS_SERVNOTICE|FLAGS_WALLOP|FLAGS_KMODE);
		send_umode_out(cptr, sptr, old);
 		sendto_one(sptr, rpl_str(RPL_YOUREOPER), me.name, parv[0]);
#if !defined(CRYPT_OPER_PASSWORD) && (defined(FNAME_OPERLOG) ||\
    (defined(USE_SYSLOG) && defined(SYSLOG_OPER)))
		encr = "";
#endif
#if defined(USE_SYSLOG) && defined(SYSLOG_OPER)
		syslog(LOG_INFO, "OPER (%s) (%s) by (%s!%s@%s)",
			name, encr,
			parv[0], sptr->user->username, sptr->sockhost);
#endif
#ifdef FNAME_OPERLOG
	      {
                int     logfile;
 
                /*
                 * This conditional makes the logfile active only after
                 * it's been created - thus logging can be turned off by
                 * removing the file.
                 */
                if (IsPerson(sptr) &&
                    (logfile = open(FNAME_OPERLOG, O_WRONLY|O_APPEND)) != -1)
		{
                        (void)irc_sprintf(buf, "%s OPER (%s) (%s) by (%s!%s@%s)\n",
				      myctime(NOW), name, encr,
				      parv[0], sptr->user->username,
				      sptr->sockhost);
		  (void)write(logfile, buf, strlen(buf));
		  (void)close(logfile);
		}
	      }
#endif /* FNAME_OPERLOG */
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr,
				      ":%s MODE %s :+o", parv[0], parv[0]);
#endif
	}
	else
	{
		int logfile;

		(void)detach_conf(sptr, aconf);
		sendto_one(sptr,err_str(ERR_PASSWDMISMATCH),me.name, parv[0]);
#ifdef FNAME_FAILED_OPER
                if ((logfile=open(FNAME_FAILED_OPER, O_WRONLY|O_APPEND)) != -1)
                {
                      (void)irc_sprintf(buf,
                               "%s: %s!%s@%s tried (%s) %s\n",
                               myctime(NOW), sptr->name,
			       sptr->user->username,
                               sptr->user->host, name, password);
                      (void)write(logfile, buf, strlen(buf));
                      (void)close(logfile);
                }
#endif
#ifdef FAILED_OPER_NOTICE
                sendto_flagops(UFLAGS_OPERS,"Failed OPER attempt: %s (%s@%s) [%s]",
                     parv[0], sptr->user->username, sptr->sockhost, name);
#endif
	}
	return 0;
}

/***************************************************************************
 * m_pass() - Added Sat, 4 March 1989
 ***************************************************************************/

/*
** m_pass
**	parv[0] = sender prefix
**	parv[1] = password
**	parv[2] = optional extra version information
*/
int	m_pass(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	char *password = parc > 1 ? parv[1] : NULL;

	if (BadPtr(password))
	    {
		sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "PASS");
		return 0;
	    }
	if (!MyConnect(sptr) || (!IsUnknown(cptr) && !IsHandshake(cptr)))
	    {
		sendto_one(cptr, err_str(ERR_ALREADYREGISTRED),
			   me.name, parv[0]);
		return 0;
	    }
	strncpyzt(cptr->passwd, password, sizeof(cptr->passwd));
	if (parc > 2)
	{
		int l = strlen(parv[2]);

		if (l < 2)
			return 0;
		if (strcmp(parv[2]+l-2, "TS") == 0)
			SetCapable(cptr, CAP_TS);
	}
	return 0;
    }

/*
 * m_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 */
int	m_userhost(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	char	*p = NULL;
	aClient	*acptr;
	Reg1	char	*s;
	Reg2	int	i, len;

	if (check_registered(sptr))
		return 0;

	if (parc > 2)
		(void)m_userhost(cptr, sptr, parc-1, parv+1);

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "USERHOST");
		return 0;
	}

	(void)irc_sprintf(buf, rpl_str(RPL_USERHOST), me.name, parv[0]);
	len = strlen(buf);
	buf[sizebuf-1] = (char) 0;
	*buf2 = '\0';

	for (i = 5, s = strtoken(&p, parv[1], " "); i && s;
	     s = strtoken(&p, (char *)NULL, " "), i--)
		if ((acptr = find_person(s, NULL)))
		{
			if (*buf2)
				(void)strcat(buf, " ");
			(void)irc_sprintf(buf2, "%s%s=%c%s@%s",
				acptr->name,
				IsAnOper(acptr) ? "*" : "",
				(acptr->user->away) ? '-' : '+',
				acptr->user->username,
				acptr->user->host);
			(void)strncat(buf, buf2, sizebuf - len);
			len += strlen(buf2);
		}
	sendto_one(sptr, "%s", buf);
	return 0;
}

/*
 * m_ison added by Darren Reed 13/8/91 to act as an efficent user indicator
 * with respect to cpu/bandwidth used. Implemented for NOTIFY feature in
 * clients. Designed to reduce number of whois requests. Can process
 * nicknames in batches as long as the maximum buffer length.
 *
 * format:
 * ISON :nicklist
 */

int	m_ison(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	aClient *acptr;
	Reg2	char	*s, **pav = parv;
	Reg3	int	len;
	char	*p = NULL;

	if (check_registered(sptr))
		return 0;

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "ISON");
		return 0;
	}

	(void)irc_sprintf(buf, rpl_str(RPL_ISON), me.name, *parv);
	len = strlen(buf);
	buf[sizebuf-1] = (char) 0;
	for (s = strtoken(&p, *++pav, " "); s; s = strtoken(&p, NULL, " "))
		if ((acptr = find_person(s, NULL)))
		{
			if (len + strlen(acptr->name+1) < sizebuf)
			{
				(void)strcat(buf, acptr->name);
				(void)strcat(buf, " ");
				len += strlen(acptr->name) + 1;
			}
		}
	sendto_one(sptr, "%s", buf);
	return 0;
}

/*
 * m_umode() added 15/10/91 By Darren Reed.
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
int	m_umode(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	int	flag;
	Reg2	int	*s;
	Reg3	char	**p, *m;
	aClient	*acptr;
	int	what, setflags;

	if (check_registered_user(sptr))
		return 0;

	what = MODE_ADD;

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "MODE");
		return 0;
	}

	if (!(acptr = find_person(parv[1], NULL)))
	{
		if (MyConnect(sptr))
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
				   me.name, parv[0], parv[1]);
		return 0;
	}

	if (IsServer(sptr) || sptr != acptr || acptr->from != sptr->from)
	{
		if (IsServer(cptr))
			sendto_ops_butone(NULL, &me,
				  ":%s WALLOPS :MODE for User %s From %s!%s",
				  me.name, parv[1],
				  get_client_name(cptr, FALSE), sptr->name);
		else
			sendto_one(sptr, err_str(ERR_USERSDONTMATCH),
				   me.name, parv[0]);
			return 0;
	}
 
	if (parc < 3)
	{
		m = buf;
		*m++ = '+';
		for (s = user_modes; (flag = *s) && (m - buf < BUFSIZE - 4);
		     s += 2)
			if (sptr->extraflags & flag)
				*m++ = (char)(*(s+1));
		*m = '\0';
		sendto_one(sptr, rpl_str(RPL_UMODEIS),
			   me.name, parv[0], buf);
		return 0;
	}

	/* find flags already set for user */
	setflags = 0;
	for (s = user_modes; (flag = *s); s += 2)
		if (sptr->extraflags & flag)
			setflags |= flag;

	/*
	 * parse mode change string(s)
	 */
	for (p = &parv[2]; p && *p; p++ )
		for (m = *p; *m; m++)
			switch(*m)
			{
			case '+' :
				what = MODE_ADD;
				break;
			case '-' :
				what = MODE_DEL;
				break;	
			/* we may not get these,
			 * but they shouldnt be in default
			 */
			case ' ' :
			case '\n' :
			case '\r' :
			case '\t' :
				break;
			default :
				for (s = user_modes; (flag = *s); s += 2)
					if (*m == (char)(*(s+1)))
				    {
					if (what == MODE_ADD)
						sptr->extraflags |= flag;
					else
						sptr->extraflags &= ~flag;	
					break;
				    }
				if (flag == 0 && MyConnect(sptr))
					sendto_one(sptr,
						err_str(ERR_UMODEUNKNOWNFLAG),
						me.name, parv[0]);
				break;
			}
	/*
	 * stop users making themselves operators too easily
	 */
	if (!(setflags & FLAGS_OPER) && IsOper(sptr) && !IsServer(cptr))
		ClearOper(sptr);
	if (!(setflags & FLAGS_LOCOP) && IsLocOp(sptr) && !IsServer(cptr))
		ClearLocOp(sptr);
	if ((setflags & (FLAGS_OPER|FLAGS_LOCOP)) && !IsAnOper(sptr) &&
	    MyConnect(sptr))
		det_confs_butmask(sptr, CONF_CLIENT & ~CONF_OPS);
	if ((setflags & FLAGS_OPER) && !IsOper(sptr))
	{
		o_count--;
		if (MyConnect(sptr))
			del_from_fdlist(sptr->fd, &new_operfdlist);
	}
	if (!(setflags & FLAGS_INVISIBLE) && IsInvisible(sptr))
	{
		if (MyConnect(sptr))
			m_invis++;
		i_count++;
	}
	if ((setflags & FLAGS_INVISIBLE) && !IsInvisible(sptr))
	{
		if (MyConnect(sptr))
			m_invis--;
		i_count--;
	}
#ifdef	USE_SERVICES
	if (IsOper(sptr) && !(setflags & FLAGS_OPER))
		check_services_butone(SERVICE_WANT_OPER, sptr,
				      ":%s MODE %s :+o", parv[0], parv[0]);
	else if (!IsOper(sptr) && (setflags & FLAGS_OPER))
		check_services_butone(SERVICE_WANT_OPER, sptr,
				      ":%s MODE %s :-o", parv[0], parv[0]);
#endif
	/*
	 * compare new flags with old flags and send string which
	 * will cause servers to update correctly.
	 */
	send_umode_out(cptr, sptr, setflags);

	return 0;
}
	
/*
 * send the MODE string for user (user) to connection cptr
 * -avalon
 */
void	send_umode(cptr, sptr, old, sendmask, umode_buf)
aClient *cptr, *sptr;
int	old, sendmask;
char	*umode_buf;
{
	Reg1	int	*s, flag;
	Reg2	char	*m;
	int	what = MODE_NULL;

	/*
	 * build a string in umode_buf to represent the change in the user's
	 * mode between the new (sptr->flag) and 'old'.
	 */
	m = umode_buf;
	*m = '\0';
	for (s = user_modes; (flag = *s); s += 2)
	    {
		if (MyClient(sptr) && !(flag & sendmask))
			continue;
		if ((flag & old) && !(sptr->extraflags & flag))
		    {
			if (what == MODE_DEL)
				*m++ = *(s+1);
			else
			    {
				what = MODE_DEL;
				*m++ = '-';
				*m++ = *(s+1);
			    }
		    }
		else if (!(flag & old) && (sptr->extraflags & flag))
		    {
			if (what == MODE_ADD)
				*m++ = *(s+1);
			else
			    {
				what = MODE_ADD;
				*m++ = '+';
				*m++ = *(s+1);
			    }
		    }
	    }
	*m = '\0';
	if (*umode_buf && cptr)
		sendto_one(cptr, ":%s MODE %s :%s",
			   sptr->name, sptr->name, umode_buf);
}

/*
 * added Sat Jul 25 07:30:42 EST 1992
 */
void	send_umode_out(cptr, sptr, old)
aClient *cptr, *sptr;
int	old;
{
	Reg1    int     i;
	Reg2    aClient *acptr;

	send_umode(NULL, sptr, old, SEND_UMODES, buf);
	if (*buf)
		for (i = highest_fd; i >= 0; i--)
			if ((acptr = local[i]) && IsServer(acptr) &&
			    (acptr != cptr) && (acptr != sptr))
				sendto_one(acptr, ":%s MODE %s :%s",
					   ID(sptr), ID(sptr), buf);

	if (cptr && MyClient(cptr))
		send_umode(cptr, sptr, old, ALL_UMODES, buf);
}

int	m_gline(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	char *who = parc > 1 ? parv[1] : NULL;
	char *message = parc > 2 ? parv[2] : NULL;
	char userhost[USERLEN+HOSTLEN+6];
	char *user, *host;
#ifdef G_LINES
	aConfItem *aconf;
	time_t seconds = 0;
	char buffer[1024];
	int i, out;
	aClient *acptr;
	char *reason = NULL;
#endif

	if (check_registered_user(sptr))
		return 0;
	if (!IsOper(sptr) || IsServer(sptr)) /* local opers can't gline */
	{
		if (MyClient(sptr) && !IsServer(sptr))
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES),
				me.name, parv[0]);
		return 0;
	}
#ifndef G_LINES
	sendto_one(sptr, err_str(ERR_UNKNOWNCOMMAND),
		me.name, parv[0], "GLINE");
	return 0;
#endif
	if (BadPtr(who) || !message || !*message || !strchr(who, '@'))
	{
		if (MyClient(sptr))
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
				me.name, parv[0], "GLINE");
		return 0;
	}
	if (strlen(message) > TOPICLEN)
		message[TOPICLEN] = (char) 0;
	user = who;
	host = strchr(who, '@');
	*host++ = (char) 0;
	if (!*user || !*host)
	{
		if (MyClient(sptr))
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
				me.name, parv[0], "GLINE");
		return 0;
	}
	if (!match(user, "........") && !match(host, "......."))
	{
		if (MyClient(sptr))
			sendto_one(sptr, ":%s NOTICE %s :Can't G-Line *@*",
				me.name, parv[0]);
		return 0;
	}
	*(host-1) = '@';
	sendto_serv_butone(IsServer(cptr) ? cptr : NULL, ":%s GLINE %s :%s",
		   parv[0], who, message);
#ifdef G_LINES
	seconds = GlineConf.seconds ? GlineConf.seconds : 60*10;
	sprintf(userhost, "%s@%s", sptr->user->username,
			sptr->user->host);
	if (!rule_allow(GlineConf.fromoper, userhost, 0) ||
		!rule_allow(GlineConf.fromserver, sptr->user->server, 0) ||
		!rule_allow(GlineConf.foruser, who, 1))
	{
		*(host-1) = (char) 0;
		if (MyClient(sptr))
		{
		sendto_flagops(UFLAGS_OPERS,
			"IGNORED G-Line: %s for [%s@%s]: %s",
			parv[0], user, host, message);
		sendto_one(sptr, ":%s NOTICE %s :G-line not added.",
			me.name, parv[0]);
		}
		else
		sendto_flagops(UFLAGS_OPERS,
			"IGNORED G-Line: %s from %s for [%s@%s]: %s",
			parv[0], sptr->user->server, user, host,
			message);
		if ((out = open(FNAME_GLINELOG, O_RDWR|O_APPEND|O_CREAT))!=-1)
		{
			fchmod(out, 432);
			sprintf(buffer,
			"%li IGNORED %s!%s@%s from %s for %s@%s: %s\n",
				NOW, parv[0], sptr->user->username,
				sptr->user->host, sptr->user->server,
				user, host, message);
			write(out, buffer, strlen(buffer));
			close(out);
		}
		return 0;
	}
	*(host-1) = (char) 0;
	aconf = make_conf();
	aconf->status = CONF_GLINE;
	DupString(aconf->host, host);
	DupString(aconf->passwd, message);
	DupString(aconf->name, user);
	aconf->port = 0;
	aconf->expires = seconds+NOW;
	Class(aconf) = find_class(0);
	addto_dichconf(Glines, aconf);
	if (MyClient(sptr))
		sendto_flagops(UFLAGS_OPERS,"%s added G-Line for [%s@%s]: %s",
			parv[0], user, host, message);
        else
		sendto_flagops(UFLAGS_OPERS,
				"%s from %s added G-Line for [%s@%s]: %s",
		parv[0], sptr->user->server, user, host, message);
	if ((out = open(FNAME_GLINELOG, O_RDWR|O_APPEND|O_CREAT))!=-1)
	{
		fchmod(out, 432);
		sprintf(buffer,
			"%li ADDED   %s!%s@%s from %s for %s@%s: %s [%li]\n",
			NOW, parv[0], sptr->user->username,
			sptr->user->host, sptr->user->server,
			user, host, message, aconf->expires);
		write(out, buffer, strlen(buffer));
		close(out);
	}
	for (i = 0; i <= highest_fd; i++)
	{
		if (!(acptr = local[i]) || IsMe(acptr) || IsLog(acptr))
			continue;
		if (!IsPerson(acptr))
			continue;
		if (find_gline(acptr, 1, &reason))
		{
			sendto_ops("Gline active for %s",
				get_client_name(acptr, FALSE));
			(void)exit_client(acptr, acptr, &me, reason);
			i=0;
			continue;
		}
	}
#endif
	return 0;
}

int	m_operwall(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	char *message = parc > 1 ? parv[1] : NULL;

	if (check_registered_user(sptr))
		return 0;
	if (!IsAnOper(sptr) || IsServer(sptr))
	{
		if (MyClient(sptr) && !IsServer(sptr))
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES),
				me.name, parv[0]);
		return 0;
	}
	if (BadPtr(message))
	{
		if (MyClient(sptr))
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
				me.name, parv[0], "OPERWALL");
		return 0;
	}
	if (strlen(message) > TOPICLEN)
		message[TOPICLEN] = (char) 0;
	sendto_serv_butone(IsServer(cptr) ? cptr : NULL, ":%s OPERWALL :%s",
		   parv[0], message);
	send_operwall(sptr, message);
	return 0;
}

int	m_ext(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{

	if (check_registered_user(sptr))
		return 0;
	if (MyClient(sptr))
	{
		sendto_one(sptr, err_str(ERR_UNKNOWNCOMMAND),
			me.name, parv[0], "EXT");
		return 0;
	}
	if (parc < 2)
		return 0;
	sendto_serv_butone(IsServer(cptr) ? cptr : NULL, ":%s EXT %s%s%s",
		parv[0], parv[1], parv[2] ? " :" : "", parv[2]?parv[2]:"");
	/* process it */
}

