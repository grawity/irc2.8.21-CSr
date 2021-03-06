/************************************************************************
 *   IRC - Internet Relay Chat, ircd/channel.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
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

/* -- Jto -- 09 Jul 1990
 * Bug fix
 */

/* -- Jto -- 03 Jun 1990
 * Moved m_channel() and related functions from s_msg.c to here
 * Many changes to start changing into string channels...
 */

/* -- Jto -- 24 May 1990
 * Moved is_full() from list.c
 */

#ifndef lint
static  char rcsid[] = "@(#)$Id: channel.c,v 1.1.1.1 1997/07/23 18:02:04 cbehrens Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "channel.h"
#include "h.h"

aChannel *channel = NullChn;

static	void	add_invite PROTO((aClient *, aChannel *));
static	int	add_banid PROTO((aClient *, aChannel *, char *));
static	int	can_join PROTO((aClient *, aChannel *, char *));
static	void	channel_modes PROTO((aClient *, char *, char *, aChannel *));
static	int	check_channelmask PROTO((aClient *, aClient *, char *));
static	int	del_banid PROTO((aChannel *, char *));
static	Link	*is_banned PROTO((aClient *, aChannel *));
static	int	set_mode PROTO((aClient *, aClient *, aChannel *, int,\
			        char **, char *,char *));
static	void	sub1_from_channel PROTO((aChannel *));

static void	clean_channelname PROTO((unsigned char *));
void	del_invite PROTO((aClient *, aChannel *));

#ifdef ORATIMING
struct timeval tsdnow, tsdthen; 
unsigned long tsdms;
#endif

/*
** number of seconds to add to all readings of time() when making TS's
** -orabidoo
*/
ts_val	timedelta = 0;

static	char	*PartFmt = ":%s PART %s";
/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */
static	char	nickbuf[BUFSIZE], buf[BUFSIZE];
static	char	modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];

/*
 * return the length (>=0) of a chain of links.
 */
static	int	list_length(lp)
Reg1	Link	*lp;
{
	Reg2	int	count = 0;

	for (; lp; lp = lp->next)
		count++;
	return count;
}

/*
** find_chasing
**	Find the client structure for a nick name (user) using history
**	mechanism if necessary. If the client is not found, an error
**	message (NO SUCH NICK) is generated. If the client was found
**	through the history, chasing will be 1 and otherwise 0.
*/
aClient *find_chasing(sptr, user, chasing)
aClient *sptr;
char	*user;
Reg1	int	*chasing;
{
	Reg2	aClient *who = find_client(user, (aClient *)NULL);

	if (chasing)
		*chasing = 0;
	if (who)
		return who;
	if (!(who = get_history(user, (long)KILLCHASETIMELIMIT)))
	{
		if (sptr != NULL)
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
				me.name, sptr->name, user);
		return NULL;
	}
	if (chasing)
		*chasing = 1;
	return who;
}

/*
 *  Fixes a string so that the first white space found becomes an end of
 * string marker (`\-`).  returns the 'fixed' string or "*" if the string
 * was NULL length or a NULL pointer.
 */
static	char	*check_string(s)
Reg1	char *s;
{
	static	char	star[2] = "*";
	char	*str = s;

	if (BadPtr(s))
		return star;

	for ( ;*s; s++)
		if (isspace(*s))
		{
			*s = '\0';
			break;
		}

	return (BadPtr(str)) ? star : str;
}

/*
 * create a string of form "foo!bar@fubar" given foo, bar and fubar
 * as the parameters.  If NULL, they become "*".
 */
static	char *make_nick_user_host(nick, name, host)
Reg1	char	*nick, *name, *host;
{
	static char namebuf[NICKLEN+USERLEN+HOSTLEN+6];
	register char *ptr1, *ptr2;
	register int n;

	ptr1 = namebuf;
	for(ptr2=check_string(nick),n=NICKLEN;*ptr2 && n--;)
		*ptr1++ = *ptr2++;
	*ptr1++ = '!';
	for(ptr2=check_string(name),n=USERLEN;*ptr2 && n--;)
		*ptr1++ = *ptr2++;
	*ptr1++ = '@';
	for(ptr2=check_string(host),n=HOSTLEN;*ptr2 && n--;)
		*ptr1++ = *ptr2++;
	*ptr1 = (char) 0;
	return namebuf;
#if 0
	bzero(namebuf, sizeof(namebuf));
	strncpy(namebuf, check_string(nick), NICKLEN);
	strcat(namebuf, "!");
	strncat(namebuf, check_string(name), USERLEN);
	strcat(namebuf, "@");
	strncat(namebuf, check_string(host), HOSTLEN);
	return namebuf;
#endif
}

/*
 * Ban functions to work with mode +b
 */
/* add_banid - add an id to be banned to the channel  (belongs to cptr) */

static	int	add_banid(cptr, chptr, banid)
aClient	*cptr;
aChannel *chptr;
char	*banid;
{
	Reg1	Link	*ban;
	Reg2	int	cnt = 0, len = 0;

	if (MyClient(cptr))
		(void)collapse(banid);
	for (ban = chptr->banlist; ban; ban = ban->next)
	{
#ifdef BAN_INFO
		len += strlen(ban->value.ban.banstr);
#else
		len += strlen(ban->value.cp);
#endif
		if (MyClient(cptr) &&
		    ((len > MAXBANLENGTH) || (++cnt >= MAXBANS) ||
#ifdef BAN_INFO
                     !match(ban->value.ban.banstr, banid) ||
                     !match(banid, ban->value.ban.banstr)))
#else
		     !match(ban->value.cp, banid) ||
		     !match(banid, ban->value.cp)))
#endif
			return -1;
#ifdef BAN_INFO
		else if (!mycmp(ban->value.ban.banstr, banid))
#else
		else if (!mycmp(ban->value.cp, banid))
#endif
			return -1;
		
	}
	ban = make_link();
	bzero((char *)ban, sizeof(Link));
	ban->flags = CHFL_BAN;
	ban->next = chptr->banlist;
#ifdef BAN_INFO
	ban->value.ban.banstr = (char *)MyMalloc(strlen(banid)+1);
	(void)strcpy(ban->value.ban.banstr, banid);
#ifdef USE_UH
	if (IsPerson(cptr))
	{
		ban->value.ban.who =
			(char *)MyMalloc(strlen(cptr->name)+
			strlen(cptr->user->username)+strlen(cptr->user->host)+3);
		(void)sprintf(ban->value.ban.who, "%s!%s@%s",
			cptr->name, cptr->user->username, cptr->user->host);
	}
	else
	{
#endif
		ban->value.ban.who = (char *)MyMalloc(strlen(cptr->name)+1);
		(void)strcpy(ban->value.ban.who, cptr->name);
#ifdef USE_UH
	}
#endif
        ban->value.ban.when = NOW;
#else
        ban->value.cp = (char *)MyMalloc(strlen(banid)+1);
        (void)strcpy(ban->value.cp, banid);
#endif
	chptr->banlist = ban;
	return 0;
}

/*
 * del_banid - delete an id belonging to cptr
 * if banid is null, deleteall banids belonging to cptr.
 */
static	int	del_banid(chptr, banid)
aChannel *chptr;
char	*banid;
{
	Reg1 Link **ban;
	Reg2 Link *tmp;

	if (!banid)
		return -1;
	for (ban = &(chptr->banlist); *ban; ban = &((*ban)->next))
#ifdef BAN_INFO
		if (mycmp(banid, (*ban)->value.ban.banstr)==0)
#else
		if (mycmp(banid, (*ban)->value.cp)==0)
#endif
		    {
			tmp = *ban;
			*ban = tmp->next;
#ifdef BAN_INFO
			MyFree(tmp->value.ban.banstr);
			MyFree(tmp->value.ban.who);
#else
			MyFree(tmp->value.cp);
#endif
			free_link(tmp);
			break;
		    }
	return 0;
}

/*
 * is_banned - returns a pointer to the ban structure if banned else NULL
 */
static	Link	*is_banned(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*tmp;
#ifndef IP_BAN_ALL
	char	*s;
#else
	static	char	s[NICKLEN+USERLEN+HOSTLEN+6];
	char	*s2;
#endif

	if (!IsPerson(cptr))
		return NULL;

#ifdef IP_BAN_ALL
        strcpy(s, make_nick_user_host(cptr->name, cptr->user->username,
                cptr->user->host));

        s2 = make_nick_user_host(cptr->name, cptr->user->username,
		cptr->ipstr);
#else
	s = make_nick_user_host(cptr->name, cptr->user->username,
				  cptr->user->host);
#endif

	for (tmp = chptr->banlist; tmp; tmp = tmp->next)
#ifdef IP_BAN_ALL
#ifdef BAN_INFO
		if ((match(tmp->value.ban.banstr, s) == 0) ||
			(match(tmp->value.ban.banstr, s2) == 0))
#else
		if ((match(tmp->value.cp, s) == 0) ||
			(match(tmp->value.cp, s2) == 0))
#endif /* BAN_INFO */
#else
#ifdef BAN_INFO
		if (match(tmp->value.ban.banstr, s) == 0)
#else
		if (match(tmp->value.cp, s) == 0)
#endif /* BAN_INFO */
#endif /* IP_BAN_ALL */
			break;
	return (tmp);
}

/*
 * adds a user to a channel by adding another link to the channels member
 * chain.
 */
static	void	add_user_to_channel(chptr, who, flags)
aChannel *chptr;
aClient *who;
int	flags;
{
	Reg1	Link *ptr;

	if (who->user)
	    {
		ptr = make_link();
		ptr->value.cptr = who;
		ptr->flags = flags;
		ptr->next = chptr->members;
		chptr->members = ptr;
		chptr->users++;

		ptr = make_link();
		ptr->value.chptr = chptr;
		ptr->next = who->user->channel;
		who->user->channel = ptr;
		who->user->joined++;
	    }
}

void	remove_user_from_channel(sptr, chptr)
aClient *sptr;
aChannel *chptr;
{
	Reg1	Link	**curr;
	Reg2	Link	*tmp;

	for (curr = &chptr->members; (tmp = *curr); curr = &tmp->next)
		if (tmp->value.cptr == sptr)
		    {
			*curr = tmp->next;
			free_link(tmp);
			break;
		    }
	for (curr = &sptr->user->channel; (tmp = *curr); curr = &tmp->next)
		if (tmp->value.chptr == chptr)
		    {
			*curr = tmp->next;
			free_link(tmp);
			break;
		    }
	sptr->user->joined--;
	sub1_from_channel(chptr);
}

static	int	change_chan_flag(lp, chptr, mine)
Link	*lp;
aChannel *chptr;
int mine;
{
	Reg1	Link *tmp;

	if ((tmp = find_user_link(chptr->members, lp->value.cptr)))
                if (lp->flags & MODE_ADD)
                {
#ifdef NO_RED_MODES
                        if (mine && tmp->flags & (lp->flags & MODE_FLAGS))
                                return 0;
#endif
			tmp->flags |= lp->flags & MODE_FLAGS;
			if (lp->flags & MODE_CHANOP)
				tmp->flags &= ~MODE_DEOPPED;
                }
                else
                {
#ifdef NO_RED_MODES
                        if (mine && !(tmp->flags & (lp->flags & MODE_FLAGS)))
                                return 0;
#endif
			tmp->flags &= ~lp->flags & MODE_FLAGS;
                }
	return 1;
}

static  void    set_deopped(lp, chptr)
Link    *lp;
aChannel *chptr;
{
        Reg1    Link    *tmp;

	if ((tmp = find_user_link(chptr->members, lp->value.cptr)))
		if ((tmp->flags & MODE_CHANOP) == 0)
			tmp->flags |= MODE_DEOPPED;
}

int	is_chan_op(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*lp;

	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & CHFL_CHANOP);

	return 0;
}

int	is_deopped(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*lp;

	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & CHFL_DEOPPED);

	return 0;
}

int	has_voice(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*lp;

	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & CHFL_VOICE);

	return 0;
}

int	can_send(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*lp;
	Reg2	int	member;

	member = IsMember(cptr, chptr);
	lp = find_user_link(chptr->members, cptr);

	if (chptr->mode.mode & MODE_MODERATED &&
	    (!lp || !(lp->flags & (CHFL_CHANOP|CHFL_VOICE))))
			return (MODE_MODERATED);

	if (chptr->mode.mode & MODE_NOPRIVMSGS && !member)
		return (MODE_NOPRIVMSGS);

	return 0;
}

aChannel *find_channel(chname, chptr)
Reg1	char	*chname;
Reg2	aChannel *chptr;
{
	return hash_find_channel(chname, chptr);
}

/*
 * write the "simple" list of channel modes for channel chptr onto buffer mbuf
 * with the parameters in pbuf.
 */
static	void	channel_modes(cptr, mbuf, pbuf, chptr)
aClient	*cptr;
Reg1	char	*mbuf, *pbuf;
aChannel *chptr;
{
	*mbuf++ = '+';
	if (chptr->mode.mode & MODE_SECRET)
		*mbuf++ = 's';
	else if (chptr->mode.mode & MODE_PRIVATE)
		*mbuf++ = 'p';
	if (chptr->mode.mode & MODE_MODERATED)
		*mbuf++ = 'm';
	if (chptr->mode.mode & MODE_TOPICLIMIT)
		*mbuf++ = 't';
	if (chptr->mode.mode & MODE_INVITEONLY)
		*mbuf++ = 'i';
	if (chptr->mode.mode & MODE_NOPRIVMSGS)
		*mbuf++ = 'n';
	if (chptr->mode.limit)
	    {
		*mbuf++ = 'l';
		if (IsMember(cptr, chptr) || IsServer(cptr))
			(void)irc_sprintf(pbuf, "%d ", chptr->mode.limit);
	    }
	if (*chptr->mode.key)
	    {
		*mbuf++ = 'k';
		if (IsMember(cptr, chptr) || IsServer(cptr))
			(void)strcat(pbuf, chptr->mode.key);
	    }
	*mbuf++ = '\0';
	return;
}
static	void	send_mode_list(cptr, chname, top, mask, flag)
aClient	*cptr;
Link	*top;
int	mask;
char	flag, *chname;
{
	Reg1	Link	*lp;
	Reg2	char	*cp, *name;
	int	count = 0, send = 0;

	cp = modebuf + strlen(modebuf);
	if (*parabuf)	/* mode +l or +k xx */
		count = 1;
	for (lp = top; lp; lp = lp->next)
	    {
		if (!(lp->flags & mask))
			continue;
		if (mask == CHFL_BAN)
#ifdef BAN_INFO
			name = lp->value.ban.banstr;
#else
			name = lp->value.cp;
#endif
		else
			name = lp->value.cptr->name;
		if (strlen(parabuf) + strlen(name) + 10 < (size_t) MODEBUFLEN)
		    {
			(void)strcat(parabuf, " ");
			(void)strcat(parabuf, name);
			count++;
			*cp++ = flag;
			*cp = '\0';
		    }
		else if (*parabuf)
			send = 1;
		if (count == 3)
			send = 1;
		if (send)
		    {
			sendto_one(cptr, ":%s MODE %s %s %s",
				   me.name, chname, modebuf, parabuf);
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != 3)
			    {
				(void)strcpy(parabuf, name);
				*cp++ = flag;
			    }
			count = 0;
			*cp = '\0';
		    }
	    }
}
/*
 * send "cptr" a full list of the modes for channel chptr.
 */
void	send_channel_modes(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	if (*chptr->chname != '#')
		return;

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);

	if (DoesTS(cptr))
	{
		Link	*l, *anop = NULL, *skip = NULL;
		int	n = 0;
		char	*t;

		if (*parabuf)
			strcat(parabuf, " ");
		sprintf(buf, ":%s SJOIN %ld %s %s %s:", me.name,
			chptr->channelts, chptr->chname, modebuf, parabuf);
		t = buf + strlen(buf);
		for (l = chptr->members; l && l->value.cptr; l = l->next)
			if (l->flags & MODE_CHANOP)
			{
				anop = l;
				break;
			}
		/* follow the channel, but doing anop first if it's defined
		**  -orabidoo
		*/
		l = NULL;
		for (;;)
		{
			if (anop)
			{
				l = skip = anop;
				anop = NULL;
			}
			else 
			{
				if (l == NULL || l == skip)
					l = chptr->members;
				else
					l = l->next;
				if (l && l == skip)
					l = l->next;
				if (l == NULL)
					break;
			}
			if (l->flags & MODE_CHANOP)
				*t++ = '@';
			if (l->flags & MODE_VOICE)
				*t++ = '+';
			strcpy(t, l->value.cptr->name);
			t += strlen(t);
			*t++ = ' ';
			n++;
			if (t - buf > BUFSIZE - 80)
			{
				*t++ = '\0';
				if (t[-1] == ' ') t[-1] = '\0';
				sendto_one(cptr, "%s", buf);
				sprintf(buf, ":%s SJOIN %ld %s 0 :",
					me.name, chptr->channelts,
					chptr->chname);
				t = buf + strlen(buf);
				n = 0;
			}
		}

		if (n)
		{
			*t++ = '\0';
			if (t[-1] == ' ') t[-1] = '\0';
			sendto_one(cptr, "%s", buf);
		}
		*parabuf = '\0';
		*modebuf = '+';
		modebuf[1] = '\0';
		send_mode_list(cptr, chptr->chname, chptr->banlist, CHFL_BAN,
				'b');
		if (modebuf[1] || *parabuf)
			sendto_one(cptr, ":%s MODE %s %s %s",
				   me.name, chptr->chname, modebuf, parabuf);
	}
	else
	{
		send_mode_list(cptr, chptr->chname, chptr->members, 
				CHFL_CHANOP, 'o');
		if (modebuf[1] || *parabuf)
			sendto_one(cptr, ":%s MODE %s %s %s",
				   me.name, chptr->chname, modebuf, parabuf);

		*parabuf = '\0';
		*modebuf = '+';
		modebuf[1] = '\0';
		send_mode_list(cptr, chptr->chname, chptr->banlist, CHFL_BAN,
				'b');
		if (modebuf[1] || *parabuf)
			sendto_one(cptr, ":%s MODE %s %s %s",
				   me.name, chptr->chname, modebuf, parabuf);

		*parabuf = '\0';
		*modebuf = '+';
		modebuf[1] = '\0';
		send_mode_list(cptr, chptr->chname, chptr->members, CHFL_VOICE,
				'v');
		if (modebuf[1] || *parabuf)
			sendto_one(cptr, ":%s MODE %s %s %s",
				   me.name, chptr->chname, modebuf, parabuf);
	}
}

/*
 * m_mode
 * parv[0] - sender
 * parv[1] - channel
 */

int	m_mode(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int	parc;
char	*parv[];
{
	int	mcount = 0;
	aChannel *chptr;

	if (check_registered(sptr))
		return 0;

	/* Now, try to find the channel in question */
	if (parc > 1)
	    {
		chptr = find_channel(parv[1], NullChn);
		if (chptr == NullChn)
			return m_umode(cptr, sptr, parc, parv);
	    }
	else
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "MODE");
 	 	return 0;
	    }

#if 0
	/*
	   causes parv[1] to be truncated for a remote mode, etc.
        bad.
	*/
	clean_channelname(parv[1]);
#endif
	if (check_channelmask(sptr, cptr, parv[1]))
		return 0;

	if (parc < 3)
	    {
		*modebuf = *parabuf = '\0';
		modebuf[1] = '\0';
		channel_modes(sptr, modebuf, parabuf, chptr);
		sendto_one(sptr, rpl_str(RPL_CHANNELMODEIS), me.name, parv[0],
			   chptr->chname, modebuf, parabuf);
		sendto_one(sptr, rpl_str(RPL_CREATIONTIME), me.name, parv[0],
				chptr->chname, chptr->channelts);
		return 0;
	    }
	mcount = set_mode(cptr, sptr, chptr, parc - 2, parv + 2,
			  modebuf, parabuf);
	if (strlen(modebuf) > (size_t)1)
		switch (mcount)
		    {
			case 0:
			    break;
			case -1:
			    if (MyClient(sptr))
				    sendto_one(sptr,
					       err_str(ERR_CHANOPRIVSNEEDED),
					       me.name, parv[0], chptr->chname);
			    else
				{
#ifndef DONT_SEND_FAKES
#ifdef FK_USERMODES
				    sendto_flagops(FMODE,"Fake: %s MODE %s %s %s",
					   parv[0], parv[1], modebuf, parabuf);
#else
				    sendto_ops("Fake: %s MODE %s %s %s",
					   parv[0], parv[1], modebuf, parabuf);
#endif
#endif
				    ircstp->is_fake++;
				}
			    break;
			default:
			    sendto_channel_butserv(chptr, sptr,
						":%s MODE %s %s %s", parv[0],
						chptr->chname, modebuf,
						parabuf);
			    sendto_match_servs(chptr, cptr,
						":%s MODE %s %s %s",
						parv[0], chptr->chname,
						modebuf, parabuf);
		    }
	return 0;
}

/*
 * Check and try to apply the channel modes passed in the parv array for
 * the client ccptr to channel chptr.  The resultant changes are printed
 * into mbuf and pbuf (if any) and applied to the channel.
 */
static	int	set_mode(cptr, sptr, chptr, parc, parv, mbuf, pbuf)
Reg2	aClient *cptr, *sptr;
aChannel *chptr;
int	parc;
char	*parv[], *mbuf, *pbuf;
{
	static	Link	chops[MAXMODEPARAMS];
	static	int	flags[] = {
				MODE_PRIVATE,    'p', MODE_SECRET,     's',
				MODE_MODERATED,  'm', MODE_NOPRIVMSGS, 'n',
				MODE_TOPICLIMIT, 't', MODE_INVITEONLY, 'i',
				0x0, 0x0 };

	Reg1	Link	*lp;
	Reg2	char	*curr = parv[0], *cp;
	Reg3	int	*ip;
	u_int	whatt = MODE_ADD;
	int	limitset = 0, count = 0, chasing = 0;
	int	nusers, ischop, isok, isdeop, new, len;
	int	keychange = 0, opcnt = 0;
	char	fm = '\0';
	aClient *who;
	Mode	*mode, oldm;

	*mbuf = *pbuf = '\0';
	if (parc < 1)
		return 0;

	mode = &(chptr->mode);
	bcopy((char *)mode, (char *)&oldm, sizeof(Mode));
	ischop = IsServer(sptr) || is_chan_op(sptr, chptr);
	isdeop = !ischop && !IsServer(sptr) && is_deopped(sptr, chptr);
	isok = ischop || (!isdeop && IsServer(cptr) && DoesTS(cptr) && 
	       chptr->channelts);
	new = mode->mode;

	while (curr && *curr && count >= 0)
	    {
		switch (*curr)
		{
		case '+':
			whatt = MODE_ADD;
			break;
		case '-':
			whatt = MODE_DEL;
			break;
		case 'o' :
		case 'v' :
			if (--parc <= 0)
				break;
			parv++;
			*parv = check_string(*parv);
			if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
				break;
			/*
			 * Check for nickname changes and try to follow these
			 * to make sure the right client is affected by the
			 * mode change.
			 */
			if (!(who = find_chasing(sptr, parv[0], &chasing)))
				break;
	  		if (!IsMember(who, chptr))
			    {
	    			sendto_one(cptr, err_str(ERR_USERNOTINCHANNEL),
					   me.name, cptr->name,
					   parv[0], chptr->chname);
				break;
			    }
			/*
			** If this server noticed the nick change, the
			** information must be propagated back upstream.
			** This is a bit early, but at most this will generate
			** just some extra messages if nick appeared more than
			** once in the MODE message... --msa
			*/
			/*
			** This is actually useless - if we notice a nick
			** change it means the server the mode comes from       
			** dealt with the mode with the old nick, and already
			** applied it, and the nick change must be coming
			** from another server; besides, we don't want to
			** be resetting the channel's TS -orabidoo
			*/

			if (who == cptr && whatt == MODE_ADD && *curr == 'o')
				break;
			/*
			 * to stop problems, don't allow +v and +o to mix
			 * into the one message if from a client.
			 */
			if (!fm)
				fm = *curr;
			else if (MyClient(sptr) && (*curr != fm))
				break;
			if (!ischop)
				sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
					me.name, sptr->name, chptr->chname);
			if (whatt == MODE_ADD)
			    {
				lp = &chops[opcnt++];
				lp->value.cptr = who;
				lp->flags = (*curr == 'o') ? MODE_CHANOP:
								  MODE_VOICE;
				lp->flags |= MODE_ADD;
			    }
			else if (whatt == MODE_DEL)
			    {
				lp = &chops[opcnt++];
				lp->value.cptr = who;
				lp->flags = (*curr == 'o') ? MODE_CHANOP:
								  MODE_VOICE;
				lp->flags |= MODE_DEL;
			    }
			count++;
			break;
		case 'k':
			if (--parc <= 0)
				break;
			parv++;
			/* check now so we eat the parameter if present */
			if (keychange)
				break;
			{
				register u_char *s;

				for (s = (u_char *)*parv; *s; s++)
					*s &= 0x7f;
			}
			*parv = check_string(*parv);
			if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
				break;
			if (!fm)
				fm = *curr;
			else if (MyClient(sptr) && (*curr != fm))
				break;
			if (whatt == MODE_ADD)
			    {
				if (*mode->key && !IsServer(cptr))
					sendto_one(cptr, err_str(ERR_KEYSET),
						   me.name, cptr->name,
						   chptr->chname);
				else if (ischop &&
				    (!*mode->key || IsServer(cptr)))
				    {
					lp = &chops[opcnt++];
					lp->value.cp = *parv;
					if (strlen(lp->value.cp) >
					    (size_t) KEYLEN)
						lp->value.cp[KEYLEN] = '\0';
					lp->flags = MODE_KEY|MODE_ADD;
					keychange = 1;
				    }
			    }
			else if (whatt == MODE_DEL)
			    {
				if (ischop && (mycmp(mode->key, *parv) == 0 ||
				     IsServer(cptr)))
				    {
					lp = &chops[opcnt++];
					lp->value.cp = mode->key;
					lp->flags = MODE_KEY|MODE_DEL;
					keychange = 1;
				    }
			    }
			count++;
			break;
		case 'b':
			if (--parc <= 0)
			    {
				for (lp = chptr->banlist; lp; lp = lp->next)
					sendto_one(cptr, rpl_str(RPL_BANLIST),
					     me.name, cptr->name,
						chptr->chname,
#ifdef BAN_INFO
						lp->value.ban.banstr,
						lp->value.ban.who,
						lp->value.ban.when);
#else
						lp->value.cp);
#endif
				sendto_one(cptr, rpl_str(RPL_ENDOFBANLIST),
					   me.name, cptr->name, chptr->chname);
				break;
			    }
			parv++;
			if (BadPtr(*parv))
				break;
			if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
				break;
			if (!ischop)
				sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
					me.name, parv[0], chptr->chname);
			if (whatt == MODE_ADD)
			    {
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_ADD|MODE_BAN;
			    }
			else if (whatt == MODE_DEL)
			    {
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_DEL|MODE_BAN;
			    }
			count++;
			break;
		case 'l':
			/*
			 * limit 'l' to only *1* change per mode command but
			 * eat up others.
			 */
			if (limitset || !ischop)
			    {
				if (!ischop)
					sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
						me.name, parv[0], chptr->chname);
				if (whatt == MODE_ADD && --parc > 0)
					parv++;
				break;
			    }
			if (whatt == MODE_DEL)
			    {
				limitset = 1;
				nusers = 0;
				count++;
				break;
			    }
			if (--parc > 0)
			    {
				if (BadPtr(*parv))
					break;
				if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
					break;
				if (!(nusers = atoi(*++parv)))
					continue;
				lp = &chops[opcnt++];
				lp->flags = MODE_ADD|MODE_LIMIT;
				limitset = 1;
				count++;
				break;
			    }
			sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS),
					   me.name, cptr->name, "MODE +l");
			break;
		case 'i' : /* falls through for default case */
			if (whatt == MODE_DEL)
				while (lp = chptr->invites)
					del_invite(lp->value.cptr, chptr);
		default:
			for (ip = flags; *ip; ip += 2)
				if (*(ip+1) == *curr)
					break;

			if (*ip)
			    {
				if (whatt == MODE_ADD)
				    {
					if (*ip == MODE_PRIVATE)
						new &= ~MODE_SECRET;
					else if (*ip == MODE_SECRET)
						new &= ~MODE_PRIVATE;
					new |= *ip;
				    }
				else
					new &= ~*ip;
				count++;
			    }
			else
				sendto_one(cptr, err_str(ERR_UNKNOWNMODE),
					    me.name, cptr->name, *curr);
			break;
		}
		curr++;
		/*
		 * Make sure modes strings such as "+m +t +p +i" are parsed
		 * fully.
		 */
		if (!*curr && parc > 0)
		    {
			curr = *++parv;
			parc--;
		    }
	    } /* end of while loop for MODE processing */

	whatt = 0;

	/* if it's a non-deopped non-op on a TS channel from a TS
	** server, allow only ops and deops and ignore everything
	** else (to avoid having desynched ops) -orabidoo
	*/ 
	if (!ischop && isok)
	    {
		Reg1	int 	i;
		Reg2	int	j;

		new = oldm.mode;
		for (i = 0, j = 0; i < opcnt; i++)
			if (chops[i].flags == (MODE_CHANOP|MODE_VOICE))
				chops[j++] = chops[i];
			else
				count--;
		opcnt = j;
	    }

	for (ip = flags; *ip; ip += 2)
		if ((*ip & new) && !(*ip & oldm.mode))
		    {
			if (whatt == 0)
			    {
				*mbuf++ = '+';
				whatt = 1;
			    }
			if (ischop)
				mode->mode |= *ip;
			*mbuf++ = *(ip+1);
		    }

	for (ip = flags; *ip; ip += 2)
		if ((*ip & oldm.mode) && !(*ip & new))
		    {
			if (whatt != -1)
			    {
				*mbuf++ = '-';
				whatt = -1;
			    }
			if (ischop)
				mode->mode &= ~*ip;
			*mbuf++ = *(ip+1);
		    }

	if (limitset && !nusers && mode->limit)
	    {
		if (whatt != -1)
		    {
			*mbuf++ = '-';
			whatt = -1;
		    }
		mode->mode &= ~MODE_LIMIT;
		mode->limit = 0;
		*mbuf++ = 'l';
	    }

	/*
	 * Reconstruct "+bkov" chain.
	 */
	if (opcnt)
	    {
		Reg1	int	i = 0;
		Reg2	char	c;
		char	*user, *host, numeric[16];

		/* restriction for 2.7 servers */
		if (!IsServer(cptr) && opcnt > (MAXMODEPARAMS-2))
			opcnt = (MAXMODEPARAMS-2);

		for (; i < opcnt; i++)
		    {
			lp = &chops[i];
			/*
			 * make sure we have correct mode change sign
			 */
			if (whatt != (lp->flags & (MODE_ADD|MODE_DEL)))
				if (lp->flags & MODE_ADD)
				    {
					*mbuf++ = '+';
					whatt = MODE_ADD;
				    }
				else
				    {
					*mbuf++ = '-';
					whatt = MODE_DEL;
				    }
			len = strlen(pbuf);
			/*
			 * get c as the mode char and tmp as a pointer to
			 * the paramter for this mode change.
			 */
			switch(lp->flags & MODE_WPARAS)
			{
			case MODE_CHANOP :
				c = 'o';
				cp = lp->value.cptr->name;
				break;
			case MODE_VOICE :
				c = 'v';
				cp = lp->value.cptr->name;
				break;
			case MODE_BAN :
				c = 'b';
				cp = lp->value.cp;
				if ((user = index(cp, '!')))
					*user++ = '\0';
				if ((host = rindex(user ? user : cp, '@')))
					*host++ = '\0';
				cp = make_nick_user_host(cp, user, host);
				break;
			case MODE_KEY :
				c = 'k';
				cp = lp->value.cp;
				break;
			case MODE_LIMIT :
				c = 'l';
				(void)irc_sprintf(numeric, "%-15d", nusers);
				if ((cp = index(numeric, ' ')))
					*cp = '\0';
				cp = numeric;
				break;
			}

			if (len + strlen(cp) + 2 > (size_t) MODEBUFLEN)
				break;
			/*
			 * pass on +/-o/v regardless of whether they are
			 * redundant or effective but check +b's to see if
			 * it existed before we created it.
			 */
			switch(lp->flags & MODE_WPARAS)
			{
			case MODE_KEY :
				*mbuf++ = c;
				(void)strcat(pbuf, cp);
				len += strlen(cp);
				(void)strcat(pbuf, " ");
				len++;
				if (!ischop)
					break;
				if (strlen(cp) > (size_t) KEYLEN)
					*(cp+KEYLEN) = '\0';
				if (whatt == MODE_ADD)
					strncpyzt(mode->key, cp,
						  sizeof(mode->key));
				else
					*mode->key = '\0';
				break;
			case MODE_LIMIT :
				*mbuf++ = c;
				(void)strcat(pbuf, cp);
				len += strlen(cp);
				(void)strcat(pbuf, " ");
				len++;
				if (!ischop)
					break;
				mode->limit = nusers;
				break;
			case MODE_CHANOP :
			case MODE_VOICE :
                                /* pass fakes along? nawww */
				/* uh-huh.. if they're fake, they'd go
				   into a FAKE, not get sent along as
				   a MODE  -orabidoo
				*/
				if (IsServer(sptr) && c == 'o' &&
				    whatt == MODE_ADD)
				    {
					chptr->channelts = 0;
#ifdef TS_WARNINGS
        ts_warn("Server %s setting +o and blasting TS on %s", sptr->name,
					chptr->chname);
#endif
				    }

				if (c == 'o' && whatt == MODE_ADD && isdeop &&
				    !is_chan_op(lp->value.cptr, chptr))
					set_deopped(lp, chptr);
				if (isok &&
				    change_chan_flag(lp, chptr, MyClient(sptr)))
				    {
					*mbuf++ = c;
					(void)strcat(pbuf, cp);
					len += strlen(cp);
					(void)strcat(pbuf, " ");
					len++;
				    }
				break;
			case MODE_BAN :
				if (ischop && ((whatt & MODE_ADD) &&
				     !add_banid(sptr, chptr, cp) ||
				     (whatt & MODE_DEL) &&
				     !del_banid(chptr, cp)))
				    {
					*mbuf++ = c;
					(void)strcat(pbuf, cp);
					len += strlen(cp);
					(void)strcat(pbuf, " ");
					len++;
				    }
				break;
			}
		    } /* for (; i < opcnt; i++) */
	    } /* if (opcnt) */

	*mbuf++ = '\0';

	/* returns:
	** -1  = mode changes were generated but ignored, and a FAKE
	**       must be sent for remote clients and a CHANOPRIVSNEEDED
	**       for local ones
	**  0  = ignore whatever was generated, do not propagate anything
	** >0  = modes were accepted, propagate
	*/
	if (isok)
		return count;
	if (isdeop && !MyClient(sptr))
		return 0;
	else
		return -1;
}

static	int	can_join(sptr, chptr, key)
aClient	*sptr;
Reg2	aChannel *chptr;
char	*key;
{
	Reg1	Link	*lp;

	if (is_banned(sptr, chptr))
		return (ERR_BANNEDFROMCHAN);
	if (chptr->mode.mode & MODE_INVITEONLY)
	    {
		for (lp = sptr->user->invited; lp; lp = lp->next)
			if (lp->value.chptr == chptr)
				break;
		if (!lp)
			return (ERR_INVITEONLYCHAN);
	    }

	if (*chptr->mode.key && (BadPtr(key) || mycmp(chptr->mode.key, key)))
		return (ERR_BADCHANNELKEY);

	if (chptr->mode.limit && chptr->users >= chptr->mode.limit)
		return (ERR_CHANNELISFULL);

	return 0;
}

/*
** Remove bells and commas from channel name
*/

static void	clean_channelname(unsigned char *cn)
{
	for (;*cn;cn++)
	{
		if ((*cn == '\007') || (*cn == ' ') || 
					(*cn == ',') || ((*cn > 127) && (*cn <= 160)))
		{
			*cn = '\0';
			return;
		}
	}
}

/*
** Return -1 if mask is present and doesnt match our server name.
*/
static	int	check_channelmask(sptr, cptr, chname)
aClient	*sptr, *cptr;
char	*chname;
{
	Reg1	char	*s;

	if (*chname == '&' && IsServer(cptr))
		return -1;
	s = rindex(chname, ':');
	if (!s)
		return 0;

	s++;
	if (match(s, me.name) || (IsServer(cptr) && match(s, cptr->name)))
	    {
		if (MyClient(sptr))
			sendto_one(sptr, err_str(ERR_BADCHANMASK),
				   me.name, sptr->name, chname);
		return -1;
	    }
	return 0;
}

/*
**  Get Channel block for i (and allocate a new channel
**  block, if it didn't exists before).
*/
static	aChannel *get_channel(cptr, chname, flag)
aClient *cptr;
char	*chname;
int	flag;
    {
	Reg1	aChannel *chptr;
	int	len;

	if (BadPtr(chname))
		return NULL;

	len = strlen(chname);
	if (MyClient(cptr) && len > CHANNELLEN)
	    {
		len = CHANNELLEN;
		*(chname+CHANNELLEN) = '\0';
	    }
	if ((chptr = find_channel(chname, (aChannel *)NULL)))
		return (chptr);
	if (flag == CREATE)
	    {
		chptr = (aChannel *)MyMalloc(sizeof(aChannel) + len);
		bzero((char *)chptr, sizeof(aChannel));
		strncpyzt(chptr->chname, chname, len+1);
		if (channel)
			channel->prevch = chptr;
		chptr->prevch = NULL;
		chptr->nextch = channel;
		channel = chptr;
		(void)add_to_channel_hash_table(chname, chptr);
		ch_count++;
	    }
	return chptr;
    }

static	void	add_invite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*inv, **tmp;

	del_invite(cptr, chptr);
	/*
	 * delete last link in chain if the list is max length
	 */
	if (list_length(cptr->user->invited) >= MAXCHANNELSPERUSER)
	    {
/*		This forgets the channel side of invitation     -Vesa
		inv = cptr->user->invited;
		cptr->user->invited = inv->next;
		free_link(inv);
*/
		del_invite(cptr, cptr->user->invited->value.chptr);
 
	    }
	/*
	 * add client to channel invite list
	 */
	inv = make_link();
	inv->value.cptr = cptr;
	inv->next = chptr->invites;
	chptr->invites = inv;
	/*
	 * add channel to the end of the client invite list
	 */
	for (tmp = &(cptr->user->invited); *tmp; tmp = &((*tmp)->next))
		;
	inv = make_link();
	inv->value.chptr = chptr;
	inv->next = NULL;
	(*tmp) = inv;
}

/*
 * Delete Invite block from channel invite list and client invite list
 */
void	del_invite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	**inv, *tmp;

	for (inv = &(chptr->invites); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.cptr == cptr)
		    {
			*inv = tmp->next;
			free_link(tmp);
			break;
		    }

	for (inv = &(cptr->user->invited); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.chptr == chptr)
		    {
			*inv = tmp->next;
			free_link(tmp);
			break;
		    }
}

/*
**  Subtract one user from channel i (and free channel
**  block, if channel became empty).
*/
static	void	sub1_from_channel(chptr)
Reg1	aChannel *chptr;
{
	Reg2	Link *tmp;
	Link	*obtmp;

	if (--chptr->users <= 0)
	    {
		/*
		 * Now, find all invite links from channel structure
		 */
		while ((tmp = chptr->invites))
			del_invite(tmp->value.cptr, chptr);

		tmp = chptr->banlist;
		while (tmp)
		    {
			obtmp = tmp;
			tmp = tmp->next;
#ifdef BAN_INFO
			MyFree(obtmp->value.ban.banstr);
			MyFree(obtmp->value.ban.who);
#else
			MyFree(obtmp->value.cp);
#endif
			free_link(obtmp);
		    }
		if (chptr->prevch)
			chptr->prevch->nextch = chptr->nextch;
		else
			channel = chptr->nextch;
		if (chptr->nextch)
			chptr->nextch->prevch = chptr->prevch;
		(void)del_from_channel_hash_table(chptr->chname, chptr);
		MyFree((char *)chptr);
		ch_count--;
	    }
}

/*
** m_join
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel password (key)
*/
int	m_join(cptr, sptr, parc, parv)
Reg2	aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	static	char	jbuf[BUFSIZE];
	Reg1	Link	*lp;
	Reg3	aChannel *chptr;
	Reg4	char	*name, *key = NULL;
	int	i, flags = 0;
	char	*p = NULL, *p2 = NULL;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "JOIN");
		return 0;
	}

	*jbuf = '\0';
	jbuf[BUFSIZE-1] = (char) 0;
	/*
	** Rebuild list of channels joined to be the actual result of the
	** JOIN.  Note that "JOIN 0" is the destructive problem.
	*/
	for (i = 0, name = strtoken(&p, parv[1], ","); name;
	     name = strtoken(&p, NULL, ","))
	{
		if (!IsServer(cptr))		/* has to be true for TS servers */
			clean_channelname(name);
		if (check_channelmask(sptr, cptr, name)==-1)
			continue;
		if (*name == '&' && !MyConnect(sptr))
			continue;
		if (*name == '0' && !atoi(name))
			(void)strcpy(jbuf, "0");
		else if (!IsChannelName(name))
		{
			if (MyClient(sptr))
				sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
					   me.name, parv[0], name);
			continue;
		}
		if (*jbuf)
			(void)strcat(jbuf, ",");
		(void)strncat(jbuf, name, sizeof(jbuf) - i - 1);
		i += strlen(name)+1;
	}
	(void)strcpy(parv[1], jbuf);

	p = NULL;
	if (parv[2])
		key = strtoken(&p2, parv[2], ",");
	parv[2] = NULL;	/* for m_names call later, parv[parc] must == NULL */
	for (name = strtoken(&p, jbuf, ","); name;
	     key = (key) ? strtoken(&p2, NULL, ",") : NULL,
	     name = strtoken(&p, NULL, ","))
	{
		/*
		** JOIN 0 sends out a part for all channels a user
		** has joined.
		*/
		if (*name == '0' && !atoi(name))
		{
			while ((lp = sptr->user->channel))
			{
				chptr = lp->value.chptr;
				sendto_channel_butserv(chptr, sptr, PartFmt,
						parv[0], chptr->chname);
				remove_user_from_channel(sptr, chptr);
			}
			sendto_match_servs(NULL, cptr, ":%s JOIN 0", parv[0]);
			continue;
		}

		if (MyConnect(sptr))
		{
			/*
			** local client is first to enter prviously nonexistant
			** channel so make them (rightfully) the Channel
			** Operator.
			*/
			flags = (ChannelExists(name)) ? 0 : CHFL_CHANOP;

			if (sptr->user->joined >= MAXCHANNELSPERUSER)
			{
				sendto_one(sptr, err_str(ERR_TOOMANYCHANNELS),
					   me.name, parv[0], name);
				return 0;
			}
		}
		else
		{
#ifdef TS_WARNINGS
                        /*
                        ** complain for remote JOINs to existing channels
                        ** (they should be SJOINs) -orabidoo
                        */
                        if (!ChannelExists(name) && sptr->user)
                ts_warn("User on %s remotely JOINing new channel", 
                                sptr->user->server);
#endif
		}

		chptr = get_channel(sptr, name, CREATE);

		if (!chptr ||
		    (MyConnect(sptr) && (i = can_join(sptr, chptr, key))))
		{
			sendto_one(sptr, err_str(i), me.name, parv[0], name);
/*
			sendto_one(sptr,
				   ":%s %d %s %s :Sorry, cannot join channel.",
				   me.name, i, parv[0], name);
*/
			continue;
		}
		if (IsMember(sptr, chptr))
			continue;

		/*
		**  Complete user entry to the new channel (if any)
		*/
		add_user_to_channel(chptr, sptr, flags);
		/*
		**  Set timestamp if appropriate, and propagate
		*/
		if (MyClient(sptr) && flags == CHFL_CHANOP)
		{
			chptr->channelts = NOW + timedelta;
#ifdef TS_ONLY
                        sendto_match_servs(chptr, cptr,
                                ":%s SJOIN %ld %s + :@%s", me.name, 
                                chptr->channelts, name, parv[0]);
#else
                        sendto_match_TS_servs(0, chptr, cptr, ":%s JOIN :%s",
                                                parv[0], name);
                        sendto_match_TS_servs(1, chptr, cptr,
                                ":%s SJOIN %ld %s + :@%s", me.name, 
                                chptr->channelts, name, parv[0]);
#endif
		}
                else if (MyClient(sptr))
		{
#ifdef TS_ONLY
                        sendto_match_servs(chptr, cptr, 
                                ":%s SJOIN %ld %s + :%s", me.name, 
                                chptr->channelts, name, parv[0]);
#else
                        sendto_match_TS_servs(0, chptr, cptr, ":%s JOIN :%s", 
                                                parv[0], name);
                        sendto_match_TS_servs(1, chptr, cptr, 
                                ":%s SJOIN %ld %s + :%s", me.name, 
                                chptr->channelts, name, parv[0]);
#endif
		}
                else
                    sendto_match_servs(chptr, cptr, ":%s JOIN :%s", parv[0], 
                                        name);
		/*
		** notify all other users on the new channel
		*/
		sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s",
					parv[0], name);

		if (MyClient(sptr))
		{
			del_invite(sptr, chptr);
#ifndef TS_ONLY
			if (flags == CHFL_CHANOP)
				sendto_match_TS_servs(0, chptr, cptr,
						   ":%s MODE %s +o %s",
						   me.name, name, parv[0]);
#endif
			if (chptr->topic[0] != '\0')
			{
				sendto_one(sptr, rpl_str(RPL_TOPIC), me.name,
					   parv[0], name, chptr->topic);
#ifdef TOPIC_INFO
                                sendto_one(sptr, rpl_str(RPL_TOPICWHOTIME),
                                           me.name, parv[0], name,
                                           chptr->topic_nick,
                                           chptr->topic_time);
#endif
			}
			parv[1] = name;
			(void)m_names(cptr, sptr, 2, parv);
		}
	}
	return 0;
}

/*
** m_part
**	parv[0] = sender prefix
**	parv[1] = channel
*/
int	m_part(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	aChannel *chptr;
	char	*p = NULL, *name;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 2 || parv[1][0] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "PART");
		return 0;
	}

#ifdef	V28PlusOnly
	*buf = '\0';
#endif

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		chptr = get_channel(sptr, name, 0);
		if (!chptr)
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
				   me.name, parv[0], name);
			continue;
		    }
		if (check_channelmask(sptr, cptr, name))
			continue;
		if (!IsMember(sptr, chptr))
		    {
			sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
		    		   me.name, parv[0], name);
			continue;
		    }
		/*
		**  Remove user from the old channel (if any)
		*/
#ifdef	V28PlusOnly
		if (*buf)
			(void)strcat(buf, ",");
		(void)strcat(buf, name);
#else
		sendto_match_servs(chptr, cptr, PartFmt, parv[0], name);
#endif
		sendto_channel_butserv(chptr, sptr, PartFmt, parv[0], name);
		remove_user_from_channel(sptr, chptr);
	    }
#ifdef	V28PlusOnly
	if (*buf)
		sendto_serv_butone(cptr, PartFmt, parv[0], buf);
#endif
	return 0;
    }

/*
** m_kick
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = client to kick
**	parv[3] = kick comment
*/
int	m_kick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *who;
	aChannel *chptr;
	int	chasing = 0;
	char	*comment, *name, *p = NULL, *user, *p2 = NULL;
#ifdef	V28PlusOnly
	int	mlen, len = 0, nlen;
#endif

	if (check_registered(sptr))
		return 0;

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "KICK");
		return 0;
	    }
	if (IsServer(sptr))
		sendto_ops("KICK from %s for %s %s",
			   parv[0], parv[1], parv[2]);
	comment = (BadPtr(parv[3])) ? parv[0] : parv[3];
	if (strlen(comment) > (size_t) TOPICLEN)
		comment[TOPICLEN] = '\0';

	*nickbuf = *buf = '\0';
#ifdef	V28PlusOnly
	mlen = 7 + strlen(parv[0]);
#endif

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		chptr = get_channel(sptr, name, !CREATE);
		if (!chptr)
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
				   me.name, parv[0], name);
			continue;
		    }
		if (check_channelmask(sptr, cptr, name))
			continue;
		if (!IsServer(sptr) && !is_chan_op(sptr, chptr) &&
		    (MyConnect(sptr) || !DoesTS(cptr) || !chptr->channelts))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				   me.name, parv[0], chptr->chname);
			continue;
		    }
#ifdef	V28PlusOnly
		if (len + mlen + strlen(name) < (size_t) BUFSIZE / 2)
		    {
			if (*buf)
				(void)strcat(buf, ",");
			(void)strcat(buf, name);
			len += strlen(name) + 1;
		    }
		else
			continue;
		nlen = 0;
#endif

		for (; (user = strtoken(&p2, parv[2], ",")); parv[2] = NULL)
		    {
			if (!(who = find_chasing(sptr, user, &chasing)))
				continue; /* No such user left! */
#ifdef	V28PlusOnly
			if (nlen + mlen + strlen(who->name) >
			    (size_t) BUFSIZE - NICKLEN)
				continue;
#endif
			if (IsMember(who, chptr))
			    {
				sendto_channel_butserv(chptr, sptr,
						":%s KICK %s %s :%s", parv[0],
						name, who->name, comment);
#ifdef	V28PlusOnly
				if (*nickbuf)
					(void)strcat(nickbuf, ",");
				(void)strcat(nickbuf, who->name);
				nlen += strlen(who->name);
#else
				sendto_match_servs(chptr, cptr,
						   ":%s KICK %s %s :%s",
						   parv[0], name,
						   who->name, comment);
#endif
				remove_user_from_channel(who, chptr);
			    }
			else
				sendto_one(sptr, err_str(ERR_USERNOTINCHANNEL),
					   me.name, parv[0], user, name);
#ifndef	V28PlusOnly
			if (!IsServer(cptr))
				break;
#endif
		    } /* loop on parv[2] */
#ifndef	V28PlusOnly
		if (!IsServer(cptr))
			break;
#endif
	    } /* loop on parv[1] */

#ifdef	V28PlusOnly
	if (*buf && *nickbuf)
		sendto_serv_butone(cptr, ":%s KICK %s %s :%s",
				   parv[0], buf, nickbuf, comment);
#endif
	return (0);
}

int	count_channels()
{
	Reg1	aChannel	*chptr;
	Reg2	int	count = 0;

	for (chptr = channel; chptr; chptr = chptr->nextch)
			count++;
	return (count);
}

/*
** m_topic
**	parv[0] = sender prefix
**	parv[1] = topic text
*/
int	m_topic(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aChannel *chptr = NullChn;
	char	*topic = NULL, *name, *p = NULL;
	
	if (check_registered(sptr))
		return 0;

	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "TOPIC");
		return 0;
	    }

	parv[1] = canonize(parv[1], NULL);

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		if (parc > 1 && IsChannelName(name))
		    {
			chptr = find_channel(name, NullChn);
			if (!chptr || !IsMember(sptr, chptr))
			    {
				sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
					   me.name, parv[0], name);
				continue;
			    }
			if (parc > 2)
				topic = parv[2];
		    }

		if (!chptr)
		    {
			sendto_one(sptr, rpl_str(RPL_NOTOPIC),
				   me.name, parv[0], name);
			return 0;
		    }

		if (check_channelmask(sptr, cptr, name))
			continue;
	
		if (!topic)  /* only asking  for topic  */
		    {
			if (chptr->topic[0] == '\0')
			sendto_one(sptr, rpl_str(RPL_NOTOPIC),
				   me.name, parv[0], chptr->chname);
			else
			{
				sendto_one(sptr, rpl_str(RPL_TOPIC),
					   me.name, parv[0],
					   chptr->chname, chptr->topic);
#ifdef TOPIC_INFO
                                sendto_one(sptr, rpl_str(RPL_TOPICWHOTIME),
                                           me.name, parv[0], chptr->chname,
                                           chptr->topic_nick,
                                           chptr->topic_time);
#endif
			}
		    } 
		else if (((chptr->mode.mode & MODE_TOPICLIMIT) == 0 ||
			  is_chan_op(sptr, chptr)) && topic)
		    {
			/* setting a topic */
			strncpyzt(chptr->topic, topic, sizeof(chptr->topic));
#ifdef TOPIC_INFO
                        strcpy(chptr->topic_nick, sptr->name);
                        chptr->topic_time = NOW;
#endif
			sendto_match_servs(chptr, cptr,":%s TOPIC %s :%s",
					   parv[0], chptr->chname,
					   chptr->topic);
			sendto_channel_butserv(chptr, sptr, ":%s TOPIC %s :%s",
					       parv[0],
					       chptr->chname, chptr->topic);
		    }
		else
		      sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				 me.name, parv[0], chptr->chname);
	    }
	return 0;
    }

/*
** m_invite
**	parv[0] - sender prefix
**	parv[1] - user to invite
**	parv[2] - channel number
*/
int	m_invite(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aClient *acptr;
	aChannel *chptr;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "INVITE");
		return -1;
	    }

	if (!(acptr = find_person(parv[1], (aClient *)NULL)))
	    {
		sendto_one(sptr, err_str(ERR_NOSUCHNICK),
			   me.name, parv[0], parv[1]);
		return 0;
	    }
	if (!IsServer(cptr))
		clean_channelname(parv[2]);
	if (check_channelmask(sptr, cptr, parv[2]))
		return 0;
	if (!(chptr = find_channel(parv[2], NullChn)))
	    {

		sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",
				  parv[0], parv[1], parv[2]);
		return 0;
	    }

	if (chptr && !IsMember(sptr, chptr))
	    {
		sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
			   me.name, parv[0], parv[2]);
		return -1;
	    }

	if (IsMember(acptr, chptr))
	    {
		sendto_one(sptr, err_str(ERR_USERONCHANNEL),
			   me.name, parv[0], parv[1], parv[2]);
		return 0;
	    }
	if (chptr && (chptr->mode.mode & MODE_INVITEONLY))
	    {
		if (!is_chan_op(sptr, chptr))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				   me.name, parv[0], chptr->chname);
			return -1;
		    }
		else if (!IsMember(sptr, chptr))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				   me.name, parv[0],
				   ((chptr) ? (chptr->chname) : parv[2]));
			return -1;
		    }
	    }

	if (MyConnect(sptr))
	    {
		sendto_one(sptr, rpl_str(RPL_INVITING), me.name, parv[0],
			   acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
		if (acptr->user->away)
			sendto_one(sptr, rpl_str(RPL_AWAY), me.name, parv[0],
				   acptr->name, acptr->user->away);
	    }
	if (MyConnect(acptr))
		if (chptr && (chptr->mode.mode & MODE_INVITEONLY) &&
		    sptr->user && is_chan_op(sptr, chptr))
			add_invite(acptr, chptr);
	sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",parv[0],
			  acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
	return 0;
    }


/*
** m_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
int	m_list(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aChannel *chptr;
	char	*name, *p = NULL;
	int sc;

#if defined(DOG3) && defined(RESTRICT)
	if (lifesux && !IsAnOper(sptr))
	{
		sendto_one(sptr, rpl_str(RPL_LOAD2HI), me.name,
			parv[0]);
		return 0;
	}
#endif
	sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, parv[0]);

	if (parc < 2 || BadPtr(parv[1]))
	{
		for (chptr = channel; chptr; chptr = chptr->nextch)
		{
			if (!sptr->user ||
			    (SecretChannel(chptr) && !IsMember(sptr, chptr)))
				continue;
			sc = ShowChannel(sptr, chptr);
			sendto_one(sptr, rpl_str(RPL_LIST), me.name, parv[0],
				sc ? chptr->chname : "*", chptr->users,
				sc ? chptr->topic : "");
		}
		sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);
		return 0;
	}

	if (hunt_server(cptr, sptr, ":%s LIST %s %s", 2, parc, parv))
		return 0;

	if (!MyConnect(sptr) && !IsAnOper(sptr))
	{
		if (flood_check(sptr, NOW))
			return 0;
	}

	parv[1] = canonize(parv[1], NULL);

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	{
		chptr = find_channel(name, NullChn);
		if (chptr && ShowChannel(sptr, chptr) && sptr->user)
			sendto_one(sptr, rpl_str(RPL_LIST), me.name, parv[0],
				   ShowChannel(sptr,chptr) ? name : "*",
				   chptr->users, chptr->topic);
	}
	sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);
	return 0;
}


/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 ************************************************************************/

/*
** m_names
**	parv[0] = sender prefix
**	parv[1] = channel
*/
int	m_names(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{ 
	Reg1	aChannel *chptr;
	Reg2	aClient *c2ptr;
	Reg3	Link	*lp;
	aChannel *ch2ptr = NULL;
	int	idx, flag, len, mlen;
	char	*s, *para = parc > 1 ? parv[1] : NULL;

	if (parc > 1 &&
	    hunt_server(cptr, sptr, ":%s NAMES %s %s", 2, parc, parv))
		return 0;

	if (!MyConnect(sptr) && !IsAnOper(sptr))
	{
		if (flood_check(sptr, NOW))
			return 0;
	}

	mlen = strlen(me.name) + NICKLEN + 7;

	if (!BadPtr(para))
	{
		s = index(para, ',');
		if (s)
		    {
			parv[1] = ++s;
			(void)m_names(cptr, sptr, parc, parv);
		    }
		if (!IsServer(cptr))
			clean_channelname(para);
		ch2ptr = find_channel(para, (aChannel *)NULL);
	}

	bzero((char *)buf, sizeof(buf));

	/* Allow NAMES without registering
	 *
	 * First, do all visible channels (public and the one user self is)
	 */

	for (chptr = channel; chptr; chptr = chptr->nextch)
	{
		if ((chptr != ch2ptr) && !BadPtr(para))
			continue; /* -- wanted a specific channel */
		if (!MyConnect(sptr) && BadPtr(para))
			continue;
		if (!ShowChannel(sptr, chptr))
			continue; /* -- users on this are not listed */

		/* Find users on same channel (defined by chptr) */

		(void)strcpy(buf, "* ");
		len = strlen(chptr->chname);
		(void)strcpy(buf + 2, chptr->chname);
		(void)strcpy(buf + 2 + len, " :");

		if (PubChannel(chptr))
			*buf = '=';
		else if (SecretChannel(chptr))
			*buf = '@';
		idx = len + 4;
		flag = 1;
		for (lp = chptr->members; lp; lp = lp->next)
		{
			c2ptr = lp->value.cptr;
			if (IsInvisible(c2ptr) && !IsMember(sptr,chptr))
				continue;
		        if (lp->flags & CHFL_CHANOP)
			{
				idx++;
				(void)strcat(buf, "@");
			}
			else if (lp->flags & CHFL_VOICE)
			{
				(void)strcat(buf, "+");
				idx++;
			}
			(void)strncat(buf, c2ptr->name, NICKLEN);
			idx += strlen(c2ptr->name) + 1;
			flag = 1;
			(void)strcat(buf," ");
			if (mlen + idx + NICKLEN > BUFSIZE - 3)
			{
				sendto_one(sptr, rpl_str(RPL_NAMREPLY),
					   me.name, parv[0], buf);
				(void)strcpy(buf, "* ");
				(void)strncpy(buf+2, chptr->chname, len + 1);
				buf[2+len+1] = (char) 0;
				(void)strcat(buf, " :");
				if (PubChannel(chptr))
					*buf = '=';
				else if (SecretChannel(chptr))
					*buf = '@';
				idx = len + 4;
				flag = 0;
			}
		}
		if (flag)
			sendto_one(sptr, rpl_str(RPL_NAMREPLY),
				   me.name, parv[0], buf);
	    }
	if (!BadPtr(para))
	    {
		sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name, parv[0],
			   para);
		return(1);
	    }

	/* Second, do all non-public, non-secret channels in one big sweep */

	(void)strcpy(buf, "* * :");
	idx = 5;
	flag = 0;
	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	{
  	        aChannel *ch3ptr;
		int	showflag = 0, secret = 0;

		if (!IsPerson(c2ptr) || IsInvisible(c2ptr))
			continue;
		lp = c2ptr->user->channel;
		/*
		 * dont show a client if they are on a secret channel or
		 * they are on a channel sptr is on since they have already
		 * been show earlier. -avalon
		 */
		while (lp)
		    {
			ch3ptr = lp->value.chptr;
			if (PubChannel(ch3ptr) || IsMember(sptr, ch3ptr))
				showflag = 1;
			if (SecretChannel(ch3ptr))
				secret = 1;
			lp = lp->next;
		    }
		if (showflag) /* have we already shown them ? */
			continue;
		if (secret) /* on any secret channels ? */
			continue;
		(void)strncat(buf, c2ptr->name, NICKLEN);
		idx += strlen(c2ptr->name) + 1;
		(void)strcat(buf," ");
		flag = 1;
		if (mlen + idx + NICKLEN > BUFSIZE - 2)
		{
			sendto_one(sptr, rpl_str(RPL_NAMREPLY),
				   me.name, parv[0], buf);
			(void)strcpy(buf, "* * :");
			idx = 5;
			flag = 0;
		}
	}
	if (flag)
		sendto_one(sptr, rpl_str(RPL_NAMREPLY), me.name, parv[0], buf);
	sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name, parv[0], "*");
	return(1);
}


void	send_user_joins(cptr, user)
aClient	*cptr, *user;
{
	Reg1	Link	*lp;
	Reg2	aChannel *chptr;
	Reg3	int	cnt = 0, len = 0, clen;
	char	 *mask;

	*buf = ':';
	(void)strcpy(buf+1, user->name);
	(void)strcat(buf, " JOIN ");
	len = strlen(user->name) + 7;

	for (lp = user->user->channel; lp; lp = lp->next)
	    {
		chptr = lp->value.chptr;
		if ((mask = index(chptr->chname, ':')))
			if (match(++mask, cptr->name))
				continue;
		if (*chptr->chname == '&')
			continue;
		clen = strlen(chptr->chname);
		if (clen > (size_t) BUFSIZE - 4 - len)
		    {
			if (cnt)
				sendto_one(cptr, "%s", buf);
			*buf = ':';
			(void)strcpy(buf+1, user->name);
			(void)strcat(buf, " JOIN ");
			len = strlen(user->name) + 7;
			cnt = 0;
		    }
		(void)strcpy(buf + len, chptr->chname);
		cnt++;
		len += clen;
		if (lp->next)
		    {
			len++;
			(void)strcat(buf, ",");
		    }
	    }
	if (*buf && cnt)
		sendto_one(cptr, "%s", buf);

	return;
}

static	void sjoin_sendit(cptr, sptr, chptr, from, modebuf, parabuf)
aClient *cptr;
aClient *sptr;
aChannel *chptr;
char	*from;
char	*modebuf;
char	*parabuf;
{
	sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s", from,
				chptr->chname, modebuf, parabuf);
#ifndef TS_ONLY
	sendto_match_TS_servs(0, chptr, cptr, ":%s MODE %s %s %s",
				from, chptr->chname, modebuf, parabuf);
#endif
}

/*
 * m_sjoin
 * parv[0] - sender
 * parv[1] - TS
 * parv[2] - channel
 * parv[3] - modes + n arguments (key and/or limit)
 * parv[4+n] - flags+nick list (all in one parameter)

 * 
 * process a SJOIN, taking the TS's into account to either ignore the
 * incomign modes or undo the existing ones or merge them, and JOIN
 * all the specified users while sending JOIN/MODEs to non-TS servers
 * and to clients
 */

int	m_sjoin(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int	parc;
char	*parv[];
{
	aChannel *chptr;
	aClient	*acptr;
	ts_val	newts, oldts, tstosend;
	static	Mode mode, *oldmode;
	Link	*l;
	int	args = 0, haveops = 0, keepourmodes = 1, keepnewmodes = 1,
		doesop = 0, mewhat = 0, pargs = 0, *ip, fl, people = 0, isnew,
		mpargs = 0, opargs = 0, othwhat = 0;
	Reg1	char *s, *s0;
	static	char numeric[16], sjbuf[BUFSIZE];
	char	*mbuf = modebuf, *t = sjbuf, *p;
	char	othmodebuf[MODEBUFLEN], othparabuf[MODEBUFLEN];
	char	memodebuf[MODEBUFLEN],	meparabuf[MODEBUFLEN];
	char	*ombuf = othmodebuf;
	char	*mmbuf = memodebuf;

	static	int	flags[] = {
				MODE_PRIVATE,    'p', MODE_SECRET,     's',
				MODE_MODERATED,  'm', MODE_NOPRIVMSGS, 'n',
				MODE_TOPICLIMIT, 't', MODE_INVITEONLY, 'i',
				0x0, 0x0 };

	
	if (check_registered(sptr) || IsClient(sptr) || parc < 5)
		return 0;
	if (!IsChannelName(parv[2]) ||
	    check_channelmask(sptr, cptr, parv[2]) == -1)
		return 0;
	newts = atol(parv[1]);
	bzero((char *)&mode, sizeof(mode));
	*ombuf = *mmbuf = (char) 0;
	meparabuf[0] = othparabuf[0] = (char) 0;
	s = parv[3];
	while (*s)
		switch(*(s++))
		{
		    case 'i':
			mode.mode |= MODE_INVITEONLY;
			break;
		    case 'n':
			mode.mode |= MODE_NOPRIVMSGS;
			break;
		    case 'p':
			mode.mode |= MODE_PRIVATE;
			break;
		    case 's':
			mode.mode |= MODE_SECRET;
			break;
		    case 'm':
			mode.mode |= MODE_MODERATED;
			break;
		    case 't':
			mode.mode |= MODE_TOPICLIMIT;
			break;
		    case 'k':
			strncpyzt(mode.key, parv[4+args], KEYLEN+1);
			args++;
			if (parc < 5+args) return 0;
			break;
		    case 'l':
			mode.limit = atoi(parv[4+args]);
			args++;
			if (parc < 5+args) return 0;
			break;
		}

	*parabuf = '\0';

	isnew = ChannelExists(parv[2]) ? 0 : 1;
	chptr = get_channel(sptr, parv[2], CREATE);
	oldts = chptr->channelts;

 
       /* we only look if any ops are introduced if we'll actually need
        * to use the info
        */
	if (!isnew && newts > 0 && oldts > 0 && newts != oldts)
	{
		static char nicks[BUFSIZE];
 
		strcpy(nicks, parv[4+args]);
		for (s=strtoken(&p, nicks, " "); s; s=strtoken(&p, NULL, " "))
			if (*s == '@' || s[1] == '@')
			{
                               while (*s == '@' || *s == '+')
                                       s++;
                               if (!(acptr = find_chasing(sptr, s, NULL)))
                                       continue;
                               if (acptr->from != cptr)
                                       continue;
                               doesop = 1;
                               break;
			}
	}


	for (l = chptr->members; l && l->value.cptr; l = l->next)
		if (l->flags & MODE_CHANOP)
		{
			haveops++;
			break;
		}

	oldmode = &chptr->mode;

#if 0
 Enables TS
        if (newts == 0)
                if (haveops || !doesop)
                        tstosend = oldts;
                else
                        chptr->channelts = tstosend = 0;
        else if (oldts == 0)
                if (doesop || !haveops)
                        chptr->channelts = tstosend = newts;
                else
                        tstosend = 0;
#endif
/* Disables TS: */
	if (isnew)
		chptr->channelts = tstosend = newts;
	else if (newts == 0 || oldts == 0)
		chptr->channelts = tstosend = 0;
  	else if (newts == oldts)
		tstosend = oldts;
	else if (newts < oldts)
	{
		if (doesop)
			keepourmodes = 0;
		if (haveops && !doesop)
			tstosend = oldts;
		else
			chptr->channelts = tstosend = newts;
	}
	else
	{
		if (haveops)
			keepnewmodes = 0;
		if (doesop && !haveops)
		{
			chptr->channelts = tstosend = newts;
			if (MyConnect(sptr))
				ts_warn("Hacked ops on opless channel: %s",
					    chptr->chname);
		}
		else
			tstosend = oldts;
	}

	if (!keepnewmodes)
		mode = *oldmode;
	else if (keepourmodes)
	{
		mode.mode |= oldmode->mode;
		if (oldmode->limit > mode.limit)
			mode.limit = oldmode->limit;
		if (strcmp(mode.key, oldmode->key) < 0)
			strcpy(mode.key, oldmode->key);
	}

	for (ip = flags; *ip; ip += 2)
		if ((*ip & mode.mode) && !(*ip & oldmode->mode))
		{
			if (othwhat != 1)
			{
				*ombuf++ = '+';
				othwhat = 1;
		    	}
			*ombuf++ = *(ip+1);
		}
	for (ip = flags; *ip; ip += 2)
		if ((*ip & oldmode->mode) && !(*ip & mode.mode))
		{
			if (mewhat != -1)
			{
				*mmbuf++ = '-';
				mewhat = -1;
			}
			*mmbuf++ = *(ip+1);
		}
	if (oldmode->limit && !mode.limit)
	{
		if (mewhat != -1)
		{
			*mmbuf++ = '-';
			mewhat = -1;
		}
		*mmbuf++ = 'l';
	}
	if (oldmode->key[0] && !mode.key[0])
	{
		if (mewhat != -1)
		{
			*mmbuf++ = '-';
			mewhat = -1;
		}
		*mmbuf++ = 'k';
		strcat(meparabuf, oldmode->key);
		strcat(meparabuf, " ");
                mpargs++;
	}
	if (mode.limit && oldmode->limit != mode.limit)
	{
		if (othwhat != 1)
		{
			*ombuf++ = '+';
			othwhat = 1;
		}
		*ombuf++ = 'l';
		(void)sprintf(numeric, "%-15d", mode.limit);
		if ((s = index(numeric, ' ')))
			*s = '\0';
		strcat(othparabuf, numeric);
		strcat(othparabuf, " ");
		opargs++;
	}
	if (mode.key[0] && strcmp(oldmode->key, mode.key))
	{
		if (othwhat != 1)
		{
			*ombuf++ = '+';
			othwhat = 1;
		}
		*ombuf++ = 'k';
		strcat(othparabuf, mode.key);
		strcat(othparabuf, " ");
		opargs++;
	}

	chptr->mode = mode;

	if (!keepourmodes)
	{
	    	mewhat = 0;
		for (l = chptr->members; l && l->value.cptr; l = l->next)
		{
			if (l->flags & MODE_CHANOP)
			{
				if (mewhat != -1)
				{
					*mmbuf++ = '-';
					mewhat = -1;
				}
				*mmbuf++ = 'o';
				strcat(meparabuf, l->value.cptr->name);
				strcat(meparabuf, " ");
				mpargs++;
				if (mpargs >= (MAXMODEPARAMS-2))
				{
					*mmbuf = '\0';
					sjoin_sendit(cptr, sptr, chptr,
						me.name,
						memodebuf, meparabuf);
					mmbuf = memodebuf;
					*mmbuf = meparabuf[0] = '\0';
					mpargs = mewhat = 0;
				}
				l->flags &= ~MODE_CHANOP;
			}
			if (l->flags & MODE_VOICE)
			{
				if (mewhat != -1)
				{
					*mmbuf++ = '-';
					mewhat = -1;
				}
				*mmbuf++ = 'v';
				strcat(meparabuf, l->value.cptr->name);
				strcat(meparabuf, " ");
				mpargs++;
				if (mpargs >= (MAXMODEPARAMS-2))
				{
					*mmbuf = '\0';
					sjoin_sendit(cptr, sptr, chptr,
						    me.name, memodebuf,
							meparabuf);
					mmbuf = memodebuf;
					*mmbuf = meparabuf[0] = '\0';
					mpargs = mewhat = 0;
				}
				l->flags &= ~MODE_VOICE;
			    }
		    } /* for() */
	} /* if (!keepourmodes) */

        if (mmbuf != memodebuf)
	{
                *mmbuf = '\0';
                sjoin_sendit(cptr, sptr, chptr, me.name, memodebuf,
			meparabuf);
	}
	if (ombuf != othmodebuf)
	{
                *ombuf = '\0';
                sjoin_sendit(cptr, sptr, chptr, parv[0], othmodebuf,
                       othparabuf);
	}
 
	*modebuf = *parabuf = '\0';
	if (parv[3][0] != '0' && keepnewmodes)
		channel_modes(sptr, modebuf, parabuf, chptr);
	else
	{
		modebuf[0] = '0';
		modebuf[1] = '\0';
	}

	sprintf(t, ":%s SJOIN %ld %s %s %s :", parv[0], tstosend, parv[2],
		modebuf, parabuf);
	t += strlen(t);

	mbuf = modebuf;
	parabuf[0] = '\0';
	pargs = 0;
	*mbuf++ = '+';

	for (s = s0 = strtoken(&p, parv[args+4], " "); s;
	     s = s0 = strtoken(&p, NULL, " "))
	{
		fl = 0;
		if (*s == '@' || s[1] == '@')
			fl |= MODE_CHANOP;
		if (*s == '+' || s[1] == '+')
			fl |= MODE_VOICE;
		if (!keepnewmodes)
			if (fl & MODE_CHANOP)
				fl = MODE_DEOPPED;
			else
				fl = 0;
		while (*s == '@' || *s == '+')
			s++;
		if (!(acptr = find_chasing(sptr, s, NULL)))
			continue;
		if (acptr->from != cptr)
			continue;
		people++;
		if (!IsMember(acptr, chptr))
		{
			add_user_to_channel(chptr, acptr, fl);
			sendto_channel_butserv(chptr, acptr, ":%s JOIN :%s",
						s, parv[2]);
#ifndef TS_ONLY
			sendto_match_TS_servs(0, chptr, cptr, ":%s JOIN :%s",
						s, parv[2]);
#endif
		}
		if (keepnewmodes)
			strcpy(t, s0);
		else
			strcpy(t, s);
		t += strlen(t);
		*t++ = ' ';
		if (fl & MODE_CHANOP)
		{
			*mbuf++ = 'o';
			strcat(parabuf, s);
			strcat(parabuf, " ");
			pargs++;
			if (pargs >= (MAXMODEPARAMS-2))
			{
				*mbuf = '\0';
				sjoin_sendit(cptr, sptr, chptr, parv[0],
					modebuf, parabuf);
				mbuf = modebuf;
				*mbuf++ = '+';
				parabuf[0] = '\0';
				pargs = 0;
			    }
		    }
		if (fl & MODE_VOICE)
		{
			*mbuf++ = 'v';
			strcat(parabuf, s);
			strcat(parabuf, " ");
			pargs++;
			if (pargs >= (MAXMODEPARAMS-2))
			{
				*mbuf = '\0';
				sjoin_sendit(cptr, sptr, chptr, parv[0],
					modebuf, parabuf);
				mbuf = modebuf;
				*mbuf++ = '+';
				parabuf[0] = '\0';
				pargs = 0;
			}
		}
	}

	*mbuf = '\0';
	if (pargs)
		sjoin_sendit(cptr, sptr, chptr, parv[0], modebuf, parabuf);
	if (people)
	{
		if (t[-1] == ' ')
			t[-1] = '\0';
		else
			*t = '\0';
#ifdef TS_ONLY
                sendto_match_servs(chptr, cptr, "%s", sjbuf);
#else
		sendto_match_TS_servs(1, chptr, cptr, "%s", sjbuf);
#endif
	}
}


#ifdef	TSDEBUG
/* this code is for debugging purposes only and should be removed in the
** definitive version of TSora... besides, it's illegible  -orabidoo
*/
int	m_ts(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient	*acptr;
	aChannel *chptr;

	if (check_registered_user(sptr))
		return 0;
	sendto_one(sptr, ":%s 999 %s :Time delta is: %ld", me.name, sptr->name, timedelta);
	if (parc > 1)
	{
		if (*parv[1] == '#' || *parv[1] == '&') 
		{
			chptr = get_channel(sptr, parv[1], 0);
			if (!chptr)
			{
				sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
					    me.name, parv[0], parv[1]);
				return 0;
			}
			sendto_one(sptr, ":%s 999 %s :Channel - %s : %ld", 
				   me.name, sptr->name, chptr->chname,
				   chptr->channelts);
		}
		else
		{
			acptr = find_chasing(sptr, parv[1], NULL);
			if (!acptr)
				return 0;
			sendto_one(sptr, ":%s 999 %s :User - %s : %ld",
				   me.name, sptr->name, acptr->name,
				   acptr->tsinfo);
		}
		return 0;
	}
	for (acptr = client; acptr; acptr = acptr->next)
	{
	    if (IsPerson(acptr))
	    	sendto_one(sptr, ":%s 999 %s :User - %s : %ld", me.name,
			     sptr->name, acptr->name, acptr->tsinfo);
	    else if (IsServer(acptr) && MyConnect(acptr))
		sendto_one(sptr, ":%s 999 %s :Server - %s (%s)", me.name,
			    sptr->name, acptr->name,
			    (DoesTS(acptr) ? "does TS" : "no TS"));
	}
	for (chptr = channel; chptr; chptr = chptr->nextch)
		sendto_one(sptr, ":%s 999 %s :Channel - %s : %ld", me.name,
			     sptr->name, chptr->chname, chptr->channelts);
	return 0;
}

#endif


