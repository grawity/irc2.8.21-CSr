/************************************************************************
 *   IRC - Internet Relay Chat, common/parse.c
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

/* -- Jto -- 03 Jun 1990
 * Changed the order of defines...
 */

#ifndef lint
static  char rcsid[] = "@(#)$Id: parse.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $";
#endif

#include "struct.h"
#include "common.h"
#define MSGTAB
#include "msg.h"
#undef MSGTAB
#include "sys.h"
#include "numeric.h"
#include "h.h"

/*
 * NOTE: parse() should not be called recursively by other functions!
 */
static	char	*para[MAXPARA+1];

static	char	sender[HOSTLEN+1];
static	int	cancel_clients PROTO((aClient *, aClient *, char *));
static	void	remove_unknown PROTO((aClient *, char *));

/*
**  Find a client (server or user) by name.
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string and the search is the for server and user.
*/
aClient *find_client(name, cptr)
char	*name;
Reg1	aClient *cptr;
{
	if (name)
		cptr = hash_find_client(name, cptr);

	return cptr;
}

aClient	*find_nickserv(name, cptr)
char	*name;
Reg1	aClient *cptr;
{
	if (name)
		cptr = hash_find_nickserver(name, cptr);

	return cptr;
}

/*
**  Find a user@host (server or user).
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string and the search is the for server and user.
*/
aClient *find_userhost(user, host, cptr, count)
char	*user, *host;
aClient *cptr;
int	*count;
{
	Reg1	aClient	*c2ptr;
	Reg2	aClient	*res = cptr;

	*count = 0;
	if (collapse(user))
		for (c2ptr = client; c2ptr; c2ptr = c2ptr->next) 
		{
			if (!MyClient(c2ptr)) /* implies mine and a user */
				continue;
			if ((!host || !match(host, c2ptr->user->host)) &&
			     mycmp(user, c2ptr->user->username) == 0)
			{
				(*count)++;
				res = c2ptr;
			}
		}
	return res;
}

/*
**  Find server by name.
**
**	This implementation assumes that server and user names
**	are unique, no user can have a server name and vice versa.
**	One should maintain separate lists for users and servers,
**	if this restriction is removed.
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string.
*/
aClient *find_server(name, cptr)
char	*name;
Reg1	aClient *cptr;
{
	if (name)
		cptr = hash_find_server(name, cptr);
	return cptr;
}

aClient *find_name(name, cptr)
char	*name;
aClient *cptr;
{
	Reg1 aClient *c2ptr = cptr;

	if (!collapse(name))
		return c2ptr;

	if ((c2ptr = hash_find_server(name, cptr)))
		return (c2ptr);
	if (!index(name, '*'))
		return c2ptr;
	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	{
		if (!IsServer(c2ptr) && !IsMe(c2ptr))
			continue;
		if (match(name, c2ptr->name) == 0)
			break;
		if (index(c2ptr->name, '*'))
			if (match(c2ptr->name, name) == 0)
					break;
	}
	return (c2ptr ? c2ptr : cptr);
}

/*
**  Find person by (nick)name.
*/
aClient *find_person(name, cptr)
char	*name;
aClient *cptr;
{
	Reg1	aClient	*c2ptr = cptr;

	c2ptr = find_client(name, c2ptr);

	if (c2ptr && IsClient(c2ptr) && c2ptr->user)
		return c2ptr;
	else
		return cptr;
}

/*
 * parse a buffer.
 *
 * NOTE: parse() should not be called recusively by any other fucntions!
 */
int	parse(cptr, buffer, bufend, mptr)
aClient *cptr;
char	*buffer, *bufend;
struct	Message *mptr;
{
	Reg1	aClient *from = cptr;
	Reg2	char *ch, *s;
	Reg3	int	len, i, numeric, paramcount;

	Debug((DEBUG_DEBUG,"Parsing: %s", buffer));
	if (IsDead(cptr))
		return 0;

	s = sender;
	*s = '\0';
	for (ch = buffer; *ch == ' '; ch++)
		;
	para[0] = from->name;
	if (*ch == ':')
	{
		/*
		** Copy the prefix to 'sender' assuming it terminates
		** with SPACE (or NULL, which is an error, though).
		*/
		for (++ch, i = 0; *ch && *ch != ' '; ++ch )
			if (s < (sender + sizeof(sender)-1))
				*s++ = *ch; /* leave room for NULL */
		*s = '\0';
		/*
		** Actually, only messages coming from servers can have
		** the prefix--prefix silently ignored, if coming from
		** a user client...
		**
		** ...sigh, the current release "v2.2PL1" generates also
		** null prefixes, at least to NOTIFY messages (e.g. it
		** puts "sptr->nickname" as prefix from server structures
		** where it's null--the following will handle this case
		** as "no prefix" at all --msa  (": NOTICE nick ...")
		*/
		if (*sender && IsServer(cptr))
		{
 			from = find_client(sender, (aClient *) NULL);
			if (!from || match(from->name, sender))
				from = find_server(sender, (aClient *)NULL);
			else if (!from && index(sender, '@'))
				from = find_nickserv(sender, (aClient *)NULL);

			para[0] = sender;

			/* Hmm! If the client corresponding to the
			 * prefix is not found--what is the correct
			 * action??? Now, I will ignore the message
			 * (old IRC just let it through as if the
			 * prefix just wasn't there...) --msa
			 */
			if (!from)
			{
				Debug((DEBUG_ERROR,
					"Unknown prefix (%s)(%s) from (%s)",
					sender, buffer, cptr->name));
				ircstp->is_unpf++;
				remove_unknown(cptr, sender);
				return -1;
			}
			if (from->from != cptr)
			{
				ircstp->is_wrdi++;
				Debug((DEBUG_ERROR,
					"Message (%s) coming from (%s)",
					buffer, cptr->name));
				return cancel_clients(cptr, from, buffer);
			}
		}
		while (*ch == ' ')
			ch++;
	}
	if (*ch == '\0')
	{
		ircstp->is_empt++;
		Debug((DEBUG_NOTICE, "Empty message from host %s:%s",
		      cptr->name, from->name));
		return(-1);
	}
	/*
	** Extract the command code from the packet.  Point s to the end
	** of the command code and calculate the length using pointer
	** arithmetic.  Note: only need length for numerics and *all*
	** numerics must have paramters and thus a space after the command
	** code. -avalon
	*/
	s = (char *)index(ch, ' '); /* s -> End of the command code */
	len = (s) ? (s - ch) : 0;
	if (len == 3 &&
	    isdigit(*ch) && isdigit(*(ch + 1)) && isdigit(*(ch + 2)))
	{
		mptr = NULL;
		numeric = (*ch - '0') * 100 + (*(ch + 1) - '0') * 10
			+ (*(ch + 2) - '0');
		paramcount = MAXPARA;
		ircstp->is_num++;
	}
	else
	{
		if (s)
			*s++ = '\0';
		for (; mptr->cmd; mptr++) 
			if (mycmp(mptr->cmd, ch) == 0)
				break;

		if (!mptr->cmd)
		{
			/*
			** Note: Give error message *only* to recognized
			** persons. It's a nightmare situation to have
			** two programs sending "Unknown command"'s or
			** equivalent to each other at full blast....
			** If it has got to person state, it at least
			** seems to be well behaving. Perhaps this message
			** should never be generated, though...  --msa
			** Hm, when is the buffer empty -- if a command
			** code has been found ?? -Armin
			*/
			if (buffer[0] != '\0')
			{
				if (IsPerson(from))
					sendto_one(from,
					    ":%s %d %s %s :Unknown command",
					    me.name, ERR_UNKNOWNCOMMAND,
					    from->name, ch);
				Debug((DEBUG_ERROR,"Unknown (%s) from %s",
					ch, get_client_name(cptr, TRUE)));
			}
			ircstp->is_unco++;
			return(-1);
		}
		paramcount = mptr->parameters;
		i = bufend - ((s) ? s : ch);
		mptr->bytes += i;
		if ((mptr->flags & 1) && !(IsServer(cptr) || IsService(cptr)))
		{
			if (mptr->func != m_ison && 
				mptr->func != m_userhost &&
				mptr->func != m_kill)
/* m_kill stuff is done in m_kill itself */
			cptr->since += (2 + i / 120);
					/* Allow only 1 msg per 2 seconds
					 * (on average) to prevent dumping.
					 * to keep the response rate up,
					 * bursts of up to 5 msgs are allowed
					 * -SRB
					 */
		}
	}
	/*
	** Must the following loop really be so devious? On
	** surface it splits the message to parameters from
	** blank spaces. But, if paramcount has been reached,
	** the rest of the message goes into this last parameter
	** (about same effect as ":" has...) --msa
	*/

	/* Note initially true: s==NULL || *(s-1) == '\0' !! */

	i = 0;
	if (s)
	{
		if (paramcount > MAXPARA)
			paramcount = MAXPARA;
		for (;;)
		{
			/*
			** Never "FRANCE " again!! ;-) Clean
			** out *all* blanks.. --msa
			*/
			while (*s == ' ')
				*s++ = '\0';

			if (*s == '\0')
				break;
			if (*s == ':')
			{
				/*
				** The rest is single parameter--can
				** include blanks also.
				*/
				para[++i] = s + 1;
				break;
			}
			para[++i] = s;
			if (i >= paramcount)
				break;
			for (; *s != ' ' && *s; s++)
				;
		}
	}
	para[++i] = NULL;
	if (mptr == NULL)
		return (do_numeric(numeric, cptr, from, i, para));
	mptr->count++;
	return (mptr->func)(cptr, from, i, para);
}

/*
 * field breakup for ircd.conf file.
 */
char	*getfield(newline)
char	*newline;
{
	static	char *line = NULL;
	char	*end, *field;
	
	if (newline)
		line = newline;
	if (line == NULL)
		return(NULL);

	field = line;
	if ((end = (char *)index(line,':')) == NULL)
	    {
		line = NULL;
		if ((end = (char *)index(field,'\n')) == NULL)
			end = field + strlen(field);
	    }
	else
		line = end + 1;
	*end = '\0';
	return(field);
}

static	int	cancel_clients(cptr, sptr, cmd)
aClient	*cptr, *sptr;
char	*cmd;
{
	if (IsServer(sptr) || IsMe(sptr) || (IsServer(cptr) &&
	    !DoesTS(cptr)))
		sendto_ops("Message from %s[%s] != %s", sptr->name,
			   sptr->from->name, get_client_name(cptr, TRUE));
	if (IsServer(sptr) || IsMe(sptr))
	{ 
		sendto_ops("Dropping link for fake direction: %s", cptr->name);
		return exit_client(cptr, cptr, &me, "Fake Direction");
	}
	/*
	** with TS, fake prefixes are a common thing, during the
	** connect burst when there's a nick collision, and they
	** must be ignored rather than killed because one of the
	** two is surviving.. so we don't bother sending them to
	** all ops everytime, as this could send 'private' stuff
	** from lagged clients. we do send the ones that cause
	** servers to be dropped though, as well as the ones from
	** non-TS servers -orabidoo
	*/
	if (IsServer(cptr))
	{
	/*
	** If the fake prefix from a client is coming from a TS server,
	** discard it silently -orabidoo
	*/
		if (DoesTS(cptr))
			return 0;
		else
		    {
                	sendto_serv_butone(NULL,
					   ":%s KILL %s :%s (%s[%s] != %s)",
					   me.name, sptr->name, me.name,
					   sptr->name, sptr->from->name,
					   get_client_name(cptr, TRUE));
	                sptr->flags |= FLAGS_KILLED;
			return exit_client(cptr, sptr, &me, "Fake Prefix");
		    }
	}
	/* Fake prefix came from a client of mine...something is screwed
	   with it, so we can exit this one
	*/
	return exit_client(cptr, cptr, &me, "Fake prefix");
}

static	void	remove_unknown(cptr, sender)
aClient	*cptr;
char	*sender;
{
	if (!IsRegistered(cptr) || IsClient(cptr))
		return;
	/*
	 * Not from a server so don't need to worry about it.
	 */
	if (!IsServer(cptr))
		return;
	/*
	 * Do kill if it came from a server because it means there is a ghost
	 * user on the other server which needs to be removed. -avalon
	 */
	if (!index(sender, '.'))
		sendto_one(cptr, ":%s KILL %s :%s (%s(?) <- %s)",
			   me.name, sender, me.name, sender,
			   get_client_name(cptr, FALSE));
	else
	{
		sendto_ops("Unknown sender %s came from %s", sender,
			get_client_name(cptr, TRUE));
		sendto_one(cptr, ":%s SQUIT %s :(Unknown from %s)",
			   me.name, sender, get_client_name(cptr, FALSE));
	}
}
