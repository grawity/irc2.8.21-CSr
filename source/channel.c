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
#ifdef KEEP_OPS
IdKeptOps       *first_kept_ops = NULL, *last_kept_ops = NULL;
#endif

/* MODE_NOPLUSC is not a public flag, so it doesn't go here -orabidoo
 */
static  int     channel_flags[] = {
        MODE_PRIVATE,           'p',
        MODE_SECRET,            's',
        MODE_MODERATED,         'm',
        MODE_NOPRIVMSGS,        'n',
        MODE_TOPICLIMIT,        't',
        MODE_INVITEONLY,        'i',
        0x0,                    0x0
};

static  int     channel_uflags[] = {
        MODE_CHANOP,    'o',
        MODE_VOICE,     'v',
        MODE_HALFOP,    'h',
        0x0,            0x0
};

static	void	add_invite PROTO((aClient *, aChannel *));
static	int	add_banid PROTO((aClient *, aChannel *, char *));
static	int	add_exceptid PROTO((aClient *, aChannel *, char *));
static	int	can_join PROTO((aClient *, aChannel *, char *));
static	void	channel_modes PROTO((aClient *, char *, char *, aChannel *));
static	int	check_channelmask PROTO((aClient *, aClient *, char *));
static	int	del_banid PROTO((aChannel *, char *));
static	int	del_exceptid PROTO((aChannel *, char *));
static	Link	*is_banned PROTO((aClient *, aChannel *));
static	void	set_mode PROTO((aClient *,aClient *, aChannel *, int, char **));
static	void	sub1_from_channel PROTO((aChannel *));
static  void    drop_plus_c PROTO((aChannel *));
static  void    nuke_channel PROTO((aChannel *));

#ifdef CLEAR_CHAN
static  void    zap_channel PROTO((aClient *, aChannel *, ts_val, char *));
#endif

#ifdef KEEP_OPS
static  int     ko_join PROTO((aClient *, char *, int *));
#endif

void	clean_channelname PROTO((char *));
void	del_invite PROTO((aClient *, aChannel *));

#ifdef ORATIMING
struct timeval tsdnow, tsdthen; 
unsigned long tsdms;
#endif

static	char	*PartFmt = ":%s PART %s";
/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */
static	char	nickbuf[BUFSIZE], buf[BUFSIZE];
static	char	modebuf[MODEBUFLEN], modebuf2[MODEBUFLEN];
static	char	parabuf[MODEBUFLEN], parabuf2[MODEBUFLEN];

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
	if (who || *user == '.')
		return who;
	if (!(who = get_history(user, (long)KILLCHASETIMELIMIT)))
	{
		if (sptr != NULL && IsClient(sptr))
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
	Reg1	Link	*ban, *ex;
	Reg2	int	cnt = 0, len = 0;

	if (MyClient(cptr))
	{
		for (ex = chptr->exceptlist; ex; ex = ex->next)
		{
			len += strlen(BANSTR(ex));
			if (len > MAXBANLENGTH || ++cnt >= MAXBANS)
				return -1;
		}
	}
	for (ban = chptr->banlist; ban; ban = ban->next)
	{
		len += strlen(BANSTR(ban));
		if (MyClient(cptr) &&
		    ((len > MAXBANLENGTH) || (++cnt >= MAXBANS) ||
                     !match(BANSTR(ban), banid) ||
                     !match(banid, BANSTR(ban))))
			return -1;
		else if (!mycmp(BANSTR(ban), banid))
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

/* add_exceptid - add an id to the exception list for the channel
** (belongs to cptr) 
*/

static	int	add_exceptid(cptr, chptr, eid)
aClient	*cptr;
aChannel *chptr;
char	*eid;
{
	Reg1	Link	*ex, *ban;
	Reg2	int	cnt = 0, len = 0;

	if (MyClient(cptr))
	{
		for (ban = chptr->banlist; ban; ban = ban->next)
		{
			len += strlen(BANSTR(ban));
			if (len > MAXBANLENGTH || ++cnt >= MAXBANS)
				return -1;
		}
	}
	for (ex = chptr->exceptlist; ex; ex = ex->next)
	{
		len += strlen(BANSTR(ex));
		if (MyClient(cptr) &&
		    ((len > MAXBANLENGTH) || (++cnt >= MAXBANS) ||
                     !match(BANSTR(ex), eid) ||
                     !match(eid, BANSTR(ex))))
			return -1;
		else if (!mycmp(BANSTR(ex), eid))
			return -1;
	}
	ex = make_link();
	bzero((char *)ex, sizeof(Link));
	ex->flags = CHFL_BAN;
	ex->next = chptr->exceptlist;
#ifdef BAN_INFO
	ex->value.ban.banstr = (char *)MyMalloc(strlen(eid)+1);
	(void)strcpy(ex->value.ban.banstr, eid);
#ifdef USE_UH
	if (IsPerson(cptr))
	{
		ex->value.ban.who =
			(char *)MyMalloc(strlen(cptr->name)+
			strlen(cptr->user->username)+strlen(cptr->user->host)+3);
		(void)sprintf(ex->value.ban.who, "%s!%s@%s",
			cptr->name, cptr->user->username, cptr->user->host);
	}
	else
	{
#endif
		ex->value.ban.who = (char *)MyMalloc(strlen(cptr->name)+1);
		(void)strcpy(ex->value.ban.who, cptr->name);
#ifdef USE_UH
	}
#endif
        ex->value.ban.when = NOW;
#else
        ex->value.cp = (char *)MyMalloc(strlen(eid)+1);
        (void)strcpy(ex->value.cp, eid);
#endif
	chptr->exceptlist = ex;
	return 0;
}

/*
 * del_banid - delete an id belonging to cptr
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
		if (mycmp(banid, BANSTR(*ban))==0)
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
 * del_exceptid - delete an exception on chptr
 */
static	int	del_exceptid(chptr, eid)
aChannel *chptr;
char	*eid;
{
	Reg1 Link **ex;
	Reg2 Link *tmp;

	if (!eid)
		return -1;
	for (ex = &(chptr->exceptlist); *ex; ex = &((*ex)->next))
		if (mycmp(eid, BANSTR(*ex))==0)
		    {
			tmp = *ex;
			*ex = tmp->next;
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
	Reg1	Link	*tmp, *t2;
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
		if ((match(BANSTR(tmp), s) == 0)
#ifdef IP_BAN_ALL
		    || (match(BANSTR(tmp), s2) == 0)
#endif
		    )
			break;
	
	if (tmp)
	{
		for (t2 = chptr->exceptlist; t2; t2 = t2->next)
			if ((match(BANSTR(t2), s) == 0)
#ifdef IP_BAN_ALL
			    || (match(BANSTR(t2), s2) == 0)
#endif
			    )
			return NULL;
	}
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

static	int	change_chan_flag(chptr, cptr, flag, mine)
aChannel *chptr;
aClient *cptr;
int flag, mine;
{
	Reg1	Link *tmp;

	if ((tmp = find_user_link(chptr->members, cptr)))
                if (flag & MODE_ADD)
                {
#ifdef NO_RED_MODES
                        if (mine && (tmp->flags & flag & MODE_FLAGS))
                                return 0;
#endif
			tmp->flags |= flag & MODE_FLAGS;
			if (flag & (MODE_CHANOP|MODE_HALFOP))
				tmp->flags &= ~MODE_DEOPPED;
                }
                else
                {
#ifdef NO_RED_MODES
                        if (mine && !(tmp->flags & flag & MODE_FLAGS))
                                return 0;
#endif
			tmp->flags &= ~flag & MODE_FLAGS;
                }
	return 1;
}

static  void    set_deopped(cptr, chptr, flag)
aClient *cptr;
aChannel *chptr;
int	flag;
{
        Reg1    Link    *tmp;

	if ((tmp = find_user_link(chptr->members, cptr)))
		if ((tmp->flags & flag) == 0)
			tmp->flags |= MODE_DEOPPED;
}

/* now returns both CHFL_CHANOP and CHFL_HALFOP status! */
int	is_chan_op(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg1	Link	*lp;

	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & (CHFL_CHANOP|CHFL_HALFOP));

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

int	can_send(cptr, chptr, flag)
aClient *cptr;
aChannel *chptr;
int	flag;
{
	Reg1	Link	*lp;

	lp = find_user_link(chptr->members, cptr);

	if (flag && (chptr->mode.mode & MODE_NOPRIVMSGS) &&
	    !(lp && (lp->flags & flag)))
		return -1;

	if ((chptr->mode.mode & MODE_MODERATED) &&
	    (!lp || !(lp->flags & (CHFL_CHANOP|CHFL_VOICE|CHFL_HALFOP))))
			return -1;

	if (chptr->mode.mode & (MODE_NOPRIVMSGS|MODE_ZAP) && !lp)
		return -1;

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
 *
 * kludge: if cptr is NULL, write the list for a non-TS4 server; if it's
 * &me, write it for a TS4 server (m_sjoin uses this)  -orabidoo
 */
static	void	channel_modes(cptr, mbuf, pbuf, chptr)
aClient	*cptr;
Reg1	char	*mbuf, *pbuf;
aChannel *chptr;
{
	char tmpbuf[32+CHPASSHASHLEN];

	*mbuf++ = '+';
	if (chptr->mode.mode & MODE_SECRET)
		*mbuf++ = 's';
	if (chptr->mode.mode & MODE_PRIVATE)
		*mbuf++ = 'p';
	if (chptr->mode.mode & MODE_MODERATED)
		*mbuf++ = 'm';
	if (chptr->mode.mode & MODE_TOPICLIMIT)
		*mbuf++ = 't';
	if (chptr->mode.mode & MODE_INVITEONLY)
		*mbuf++ = 'i';
	if (chptr->mode.mode & MODE_NOPRIVMSGS)
		*mbuf++ = 'n';
#ifdef CLEAR_CHAN
        if ((chptr->mode.mode & MODE_ZAP) && cptr && 
            (!IsServer(cptr) || DoesTS4(cptr)))
                *mbuf++ = 'z';
#endif
	if (chptr->mode.limit)
	    {
		*mbuf++ = 'l';
		if (!cptr || IsMe(cptr) || IsServer(cptr) || 
		    IsMember(cptr, chptr))
			(void)irc_sprintf(pbuf, "%d ", chptr->mode.limit);
	    }
	if (*chptr->mode.key)
	    {
		*mbuf++ = 'k';
		if (!cptr || IsMe(cptr) || IsServer(cptr) ||
		    IsMember(cptr, chptr))
		{
			(void)strcat(pbuf, chptr->mode.key);
			(void)strcat(pbuf, " ");
		}
	    }
#ifdef PLUS_C_OPT_OUT
	if (cptr && (chptr->mode.mode & MODE_NOPLUSC) &&
	    (IsMe(cptr) || (IsServer(cptr) && DoesTS4(cptr))))
		*mbuf++ = 'C';
	else 
#endif
	if (*chptr->mode.pass && cptr &&
	    (IsClient(cptr) || 
	      (((IsServer(cptr) && DoesTS4(cptr)) || IsMe(cptr)) && 
	       chptr->channelts && chptr->passwd_ts <= chptr->channelts &&
	       !(chptr->mode.mode & MODE_NOPLUSC))))
	    {
		*mbuf++ = 'c';
		if (IsServer(cptr) || IsMe(cptr))
		    {
		    	sprintf(tmpbuf, "%s %d", chptr->mode.pass,
				(int)(chptr->channelts - chptr->passwd_ts));
			(void)strcat(pbuf, tmpbuf);
		    }
	    }

	*mbuf++ = '\0';
	return;
}

/* only used for banlists anymore  -orabidoo */
#ifndef TS4_ONLY

static	void	send_mode_list(cptr, chname, top, flag)
aClient	*cptr;
Link	*top;
char	flag, *chname;
{
	Reg1	Link	*lp;
	Reg2	char	*mbw, *pbw, *name;
	int	len = 0, count = 0, sl;

	mbw = modebuf;
	pbw = parabuf;
	*mbw++ = '+';

	for (lp = top; lp; lp = lp->next)
	    {
		name = lp->value.cp;
		sl = strlen(name);
		if (count >= MAXMODEPARAMS || (count && 
		    len + sl + 10 >= (size_t)MODEBUFLEN))
		    {
			*pbw = *mbw = '\0';
			sendto_one(cptr, ":%s MODE %s %s %s",
				   me.name, chname, modebuf, parabuf);
			count = len = 0;
			mbw = modebuf;
			pbw = parabuf;
			*mbw++ = '+';
		    }
		if (len + sl + 10 < (size_t) MODEBUFLEN)
		    {
		    	if (count++)
				*pbw++ = ' ';
		    	strcpy(pbw, name);
			pbw += sl;
			len += sl + 1;
			*mbw++ = flag;
		    }
	    }
	*pbw = *mbw = '\0';
	if (count)
		sendto_one(cptr, ":%s MODE %s %s %s", me.name, chname, 
			   modebuf, parabuf);
}
#endif /* TS4_ONLY */

/*
 * send "cptr" a full list of the modes for channel chptr.
 */
void	send_channel_modes(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Link	*l;
	int	n = 0, ops = 1, len, sl, any = 0;
	char	*t, *name, c = 'b';

	if (*chptr->chname != '#' &&
	    (*chptr->chname != '+' || !DoesTS4(cptr)))
		return;

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);

	if ((len = strlen(parabuf)) && parabuf[len-1] != ' ')
		strcat(parabuf, " ");

	sprintf(buf, ":%s SJOIN %ld %s %s %s:", me.name,
		(long)chptr->channelts, chptr->chname, modebuf, parabuf);
	t = buf + strlen(buf);

	/* follow the channel, doing the ops first   -orabidoo
	 */
	for (l = chptr->members; l; )
	    {
		if (((l->flags & MODE_CHANOP) != 0) == ops)
		    {
			if (l->flags & MODE_CHANOP)
				*t++ = '@';
			if (l->flags & MODE_VOICE)
				*t++ = '+';
			if (DoesTS4(cptr) && (l->flags & MODE_HALFOP))
				*t++ = '%';
#ifdef TS4_ONLY
			strcpy(t, ID(l->value.cptr));
#else
			if (DoesTS4(cptr) && UserHasID(l->value.cptr))
				strcpy(t, l->value.cptr->user->id);
			else
				strcpy(t, l->value.cptr->name);
#endif
			t += strlen(t);
			*t++ = ' ';
			n++;
			any++;
			if (t - buf > BUFSIZE - 80)
			    {
				*t = '\0';
				if (t[-1] == ' ') 
					t[-1] = '\0';
				sendto_one(cptr, "%s", buf);
				sprintf(buf, ":%s SJOIN %ld %s 0 :",
					me.name, (long)chptr->channelts,
					chptr->chname);
				t = buf + strlen(buf);
				n = 0;
			    }
		    }
		l = l->next;
		if (l == NULL && ops)
		    {
			l = chptr->members;
			ops = 0;
		    }
	    }

	/* 
	 * stick the bans and the exceptions there too, if the server
	 * understands TS4  -orabidoo
	 */

#ifndef TS4_ONLY
	if (DoesTS4(cptr))
#endif
	    {
		l = chptr->banlist;
		if (!l)
		    {
		    	c = 'e';
			l = chptr->exceptlist;
		    }
	    	while (l)
		    {
		    	name = l->value.cp;
			sl = strlen(name);
			if ((t - buf) + sl + 5 >= BUFSIZE - 80)
			    {
			    	*t = '\0';
				if (t[-1] == ' ') t[-1] = '\0';
				sendto_one(cptr, "%s", buf);
				sprintf(buf, ":%s SJOIN %ld %s 0 :",
					me.name, (long)chptr->channelts,
					chptr->chname);
				t = buf + strlen(buf);
				n = 0;
			    }
			if ((t - buf) + sl + 5 < BUFSIZE - 80)
			    {
			    	*t++ = c;
				*t++ = ')';
			    	strcpy(t, name);
				t += strlen(t);
				*t++ = ' ';
				n++;
				any++;
			    }

		/* don't we wish C had perl's " } continue { " syntax here? */
			l = l->next;
			if (l == NULL && c == 'b')
			    {
			    	l = chptr->exceptlist;
				c = 'e';
			    }
		    }
	    }

        if (!any && DoesTS4(cptr))
            {
                any = n = 1;
                *t++ = '*';
            }

	if (n)
	    {
		*t++ = '\0';
		if (t[-1] == ' ')
			t[-1] = '\0';
		sendto_one(cptr, "%s", buf);
	    }

#ifndef TS4_ONLY
	if (!DoesTS4(cptr))
	    	send_mode_list(cptr, chptr->chname, chptr->banlist, 'b');
#endif

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
		if (MyConnect(sptr))
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
				   me.name, parv[0], "MODE");
 	 	return 0;
	    }

	clean_channelname(parv[1]);
	if (check_channelmask(sptr, cptr, parv[1]))
		return 0;

	if (parc < 3)
	    {
		*modebuf = *parabuf = '\0';
		modebuf[1] = '\0';
		channel_modes(sptr, modebuf, parabuf, chptr);
		sendto_one(sptr, rpl_str(RPL_CHANNELMODEIS), me.name, parv[0],
			   chptr->chname, modebuf, parabuf);

		/* Channels that can never have a +c get two -1's in the
		** creation time reply
		*/
		if (!chptr->passwd_ts && (
#if defined(PLUS_CHANNELS) && defined(MODE_C_PLUS_CHANS_ONLY)
			(*chptr->name != '+')
#else
			0
#endif
			||
#ifdef PLUS_C_START
			(chptr->channelts < (PLUS_C_START))
#else
			0
#endif
			||
#ifdef PLUS_C_OPT_OUT
			(chptr->mode.mode & MODE_NOPLUSC)
#else
			0
#endif
		   ))
			sendto_one(sptr, rpl_str(RPL_CREATIONTIME), me.name, 
				   parv[0], chptr->chname, chptr->channelts, 
				   -1, -1);
		else
			sendto_one(sptr, rpl_str(RPL_CREATIONTIME), me.name,
				   parv[0], chptr->chname, chptr->channelts, 
				   chptr->passwd_ts, chptr->opless_ts);
	    }
	else
		set_mode(cptr, sptr, chptr, parc - 2, parv + 2);
	return 0;
}

/*
 * remove clear pass on a channel mode struct.  -orabidoo
 */
static  void    drop_clear_pass(mode)
Mode    *mode;
{
        if (mode->clear)
            {
                MyFree(mode->clear);
                mode->clear = NULL;
            }
}

/*
 * set (or remove) a password on a channel mode struct.  -orabidoo
 */
static  void    set_pass(mode, clear, pw)
Mode    *mode;
char    *clear, *pw;
{
        if (pw && *pw)
            {
                /* m_sjoin needs this test */
                if (pw != mode->pass)
                        strncpyzt(mode->pass, pw, CHPASSHASHLEN+1);
                drop_clear_pass(mode);
                if (clear)
                    {
                        mode->clear = (ClearPass *)MyMalloc(1 +
                                        sizeof(ClearPass) + strlen(clear));
                        strcpy(mode->clear->pass, clear);
                        mode->clear->expires = make_ts() + CLEARPASS_EXPIRE;
                    }
            }
        else
            {
                drop_clear_pass(mode);
                *mode->pass = '\0';
            }
}

/* stolen from Undernet's ircd  -orabidoo
 */
static  char    *pretty_mask(mask)
char    *mask;
{
        Reg1    char    *cp, *user, *host;

        if ((user = index((cp = mask), '!')))
                *user++ = '\0';
        if ((host = rindex(user ? user : cp, '@')))
            {
                *host++ = '\0';
                if (!user)
                        return make_nick_user_host(NULL, cp, host);
            }
        else if (!user && index(cp, '.'))
                return make_nick_user_host(NULL, NULL, cp);
        return make_nick_user_host(cp, user, host);
}

static  char    *fix_key(arg)
char    *arg;
{
        Reg1    u_char  *s, *t, c;

        /* No more stripping the 8th bit or checking
        ** for the +k bug... it's long dead.  -orab
        */
        for (s = t = (u_char *)arg; (c = *s); s++)
            {
                if (c != ':' && c > 0x20 &&
                    (c < 0x7f || c > 0xa0))
                        *t++ = c;
            }
        *t = '\0';
        return arg;
}

/*
 * like the name says...  take out the redundant signs in a modechange list
 */
static  void    collapse_signs(s)
char    *s;
{
        char    plus = '\0', *t = s, c;
        while ((c = *s++))
            {
                if (c != plus)
                        *t++ = c;
                if (c == '+' || c == '-')
                        plus = c;
            }
        *t = '\0';
}

/* little helper function to avoid returning duplicate errors */
static  int     errsent(err, errs)
int     err, *errs;
{
        if (err & *errs)
                return 1;
        *errs |= err;
        return 0;
}


/* bitmasks for various error returns that set_mode should only return
 * once per call  -orabidoo
 */

#define SM_ERR_NOTS     0x00000001
#define SM_ERR_NOOPS    0x00000002
#define SM_ERR_UNKNOWN  0x00000004
#define SM_ERR_RPL_C    0x00000008
#define SM_ERR_RPL_B    0x00000010
#define SM_ERR_RPL_E    0x00000020

/*
** Apply the mode changes passed in parv to chptr, sending any error
** messages and MODE commands out.  Rewritten to do the whole thing in
** one pass, in a desperate attempt to keep the code sane.  -orabidoo
*/
static	void	set_mode(cptr, sptr, chptr, parc, parv)
Reg2	aClient *cptr, *sptr;
aChannel *chptr;
int	parc;
char	*parv[];
{
	/* do the various modes take an arg? */
	static	char	argmodes[] = {
	     /* a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z   */
		0,1,1,0,1,0,0,1,0,0,1,1,0,0,1,0,0,0,0,0,0,1,0,0,0,1 
	};

	int	errors_sent = 0, opcnt = 0, len = 0, tmp, nusers, *ip;
	int	keychange = 0, passet = 0, limitset = 0;
	int	whatt = MODE_ADD, the_mode;
	aClient	*who;
	Link	*lp;
	char	*curr = parv[0], c, cc, *arg, plus = '+', *tmpc, *clear;
	char	numeric[16], star[] = "*";
	/* mbufw gets the param-less mode chars, always with their sign
	 * mbuf2w gets the paramed mode chars, always with their sign
	 * pbufw gets the params, in ID form whenever possible
	 * pbuf2w gets the params, no ID's
	 */
	char	*mbufw = modebuf, *mbuf2w = modebuf2;
	char	*pbufw = parabuf, *pbuf2w = parabuf2;
	ts_val	zapts;
	int	curr_type, fm_type = 0;  
		/* 0 = none, 'c' = +c, ['h' = +h, 'e' = +e], 
		 * 1 = other modes of different types that can mix 
		 * on a single mode change from the user
		 */
	int	ischop, isok, isdeop, isok_h, isok_c, chan_op;


	chan_op = is_chan_op(sptr, chptr);

	/* has ops or is a server */
	ischop = IsServer(sptr) || (chan_op & MODE_CHANOP);

	/* is client marked as deopped */
	isdeop = !ischop && !IsServer(sptr) && is_deopped(sptr, chptr);

	/* is an op or server or remote user on a TS channel */
	isok = ischop || (!isdeop && IsServer(cptr) && chptr->channelts);

	/* is an op or halfop or server or remote user on TS channel */
	isok_h = isok || (!isdeop && (chan_op & MODE_HALFOP));

	/* isok_c calculated later, only if needed */

	/* parc is the number of _remaining_ args (where <0 means 0);
	** parv points to the first remaining argument
	*/
	parc--;
	parv++;

	while (1)
	    {
	    	if (BadPtr(curr))
		    {
		    	/*
			 * Deal with mode strings like "+m +o blah +i"
			 */
		    	if (parc-- > 0)
			    {
				curr = *parv++;
				continue;
			    }
			break;
		    }
		c = *curr++;

		if ((c == '+' && (whatt = MODE_ADD)) ||
		    (c == '-' && (whatt = MODE_DEL)) ||
		    (c == '=' && (whatt = MODE_QUERY)))
		    {
		    	plus = c;
			continue;
		    }

#ifdef	TS4_GLOBAL_ONLY
		/* only +c doesn't mix with anything else, and will stay
		 * this way   -orabidoo 
		 */
		curr_type = ((c == 'c' || c == 'z') ? c : 1);
#else
		/* all modes mix freely except for +z, +h, +c and +e */
	    	if (c == 'c' || c == 'e' || c == 'h' || c == 'z')
			curr_type = c;
		else
			curr_type = 1;
#endif

		/* silently ignore modes that can't mix with previously 
		 * parsed ones, if they come from a local user _or_ if
		 * the conflict involves mode +c or +z   -orabidoo
		 */
		if (!fm_type)
			fm_type = curr_type;
		else if ((fm_type != curr_type && MyClient(sptr)) ||
			 ((curr_type == 'c') != (fm_type == 'c')) ||
			 ((curr_type == 'z') != (fm_type == 'z')))
		    {
		        /* chomp param if appropriate */
		    	if (c >= 'a' && c <= 'z' && argmodes[c - 'a'] != 0)
				parc--;
			continue;
		    }

		switch (c)
		{
		case 'h' :
			if (MyConnect(sptr) && !chptr->channelts && 
				whatt == MODE_ADD)
			    {
				if (!errsent(SM_ERR_NOTS, &errors_sent))
					sendto_one(sptr, 
						err_str(ERR_TSLESSCHAN),
						me.name, sptr->name,
						chptr->chname, 'h');
				break;
			    }
			/* falls through */
		case 'o' :
		case 'v' :
			if (MyClient(sptr) && !IsMember(sptr, chptr))
			    {
				sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
			    		   me.name, sptr, chptr->chname);
				break;
			    }
			if (whatt == MODE_QUERY)
				break;
			if (parc-- <= 0)
				break;
			arg = check_string(*parv++);

			if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
				break;

			if (!(who = find_chasing(sptr, arg, NULL)))
				break;
			/* no more of that mode bouncing crap */
	  		if (!IsMember(who, chptr))
			    {
			    	if (MyClient(sptr))
	    				sendto_one(sptr, 
					   err_str(ERR_USERNOTINCHANNEL),
					   me.name, sptr->name,
					   arg, chptr->chname);
				break;
			    }

			if (who == sptr && whatt == MODE_ADD && (c == 'o' ||
			    (c == 'h' && !isok)))
				break;

			/* ignore server-generated MODE +-ovh */
			if (IsServer(sptr))
			    {
				ts_warn(
				    "MODE %c%c on %s from server %s (ignored)", 
					(whatt == MODE_ADD ? '+' : '-'), 
					c, chptr->chname, sptr->name);
				      break;
			    }

			if (c == 'o')
				the_mode = MODE_CHANOP;
			else if (c == 'v')
				the_mode = MODE_VOICE;
			else if (c == 'h')
				the_mode = MODE_HALFOP;

			if (isdeop && (c == 'o' || c == 'h') && 
			    whatt == MODE_ADD)
				set_deopped(who, chptr, the_mode);

			if (! (isok || (isok_h && (c == 'v' || (c == 'h' &&
				!(chptr->mode.mode & MODE_PRIVATE))))))
			    {
				/* halfops can only give out voices, and
				 * give and take halfops on non-+p channels.
				 */
				if (MyClient(sptr) &&
				    !errsent(SM_ERR_NOOPS, &errors_sent))
					sendto_one(sptr,
						err_str(ERR_CHANOPRIVSNEEDED),
						me.name, sptr->name, 
						chptr->chname);
				break;
			    }
			
			if (len + IDLEN + 2 >= MODEBUFLEN)
				break;

			*mbuf2w++ = plus;
			*mbuf2w++ = c;
			/* we bypass the ID() macro b/c we want the actual
			** ID even if not TS4_ONLY   -orabidoo
			*/
			if (who->user && *who->user->id == '.')
				strcpy(pbufw, who->user->id);
			else
				strcpy(pbufw, who->name);
			pbufw += strlen(pbufw);
			strcpy(pbuf2w, who->name);
			pbuf2w += strlen(pbuf2w);
			*pbufw++ = ' ';
			*pbuf2w++ = ' ';
			len += IDLEN + 1;
			opcnt++;

			change_chan_flag(chptr, who, the_mode|whatt);

			if ((the_mode & OpsMask) && whatt == MODE_ADD &&
                            !ZappedChannel(chptr) &&
			    (!chptr->passwd_ts || 
			     chptr->channelts <= chptr->passwd_ts))
				chptr->opless_ts = 0;
			break;


		case 'k':
			if (whatt == MODE_QUERY)
				break;
			if (parc-- <= 0)
			    {
			    	/* allow arg-less mode -k */
			    	if (whatt == MODE_DEL)
					arg = star;
				else
					break;
			    }
			else
				arg = fix_key(check_string(*parv++));

			if (keychange++)
				break;
			if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
				break;
			if (!*arg)
				break;

			if (!isok_h)
			    {
				if (!errsent(SM_ERR_NOOPS, &errors_sent) &&
				    MyClient(sptr))
					sendto_one(sptr,
						err_str(ERR_CHANOPRIVSNEEDED),
						me.name, sptr->name, 
						chptr->chname);
				break;
			    }


			/* 
			** this is restrictive and useless.  -orabidoo 
			*/
#if 0
			if (whatt == MODE_DEL && !*chptr->mode.key)
					break;

			if (whatt == MODE_ADD && *chptr->mode.key && 
			    MyClient(sptr))
			    {
				sendto_one(sptr, err_str(ERR_KEYSET), me.name, 
					   sptr->name, chptr->chname);
				break;
			    }
#endif

			if (strlen(arg) > KEYLEN)
				arg[KEYLEN] = '\0';

			/* don't check the arg when removing the key */
			if (whatt == MODE_DEL && *chptr->mode.key)
				arg = chptr->mode.key;

			tmp = strlen(arg);
			if (len + tmp + 2 >= MODEBUFLEN)
				break;

			*mbuf2w++ = plus;
			*mbuf2w++ = 'k';
			strcpy(pbufw, arg);
			pbufw += strlen(pbufw);
			strcpy(pbuf2w, arg);
			pbuf2w += strlen(pbuf2w);
			*pbufw++ = ' ';
			*pbuf2w++ = ' ';
			len += tmp + 1;
			opcnt++;

			if (whatt == MODE_DEL)
				*chptr->mode.key = '\0';
			else
				strncpyzt(chptr->mode.key, arg, KEYLEN+1);

			break;

		case 'c':
                        if (whatt == MODE_ADD && (
#ifdef MODE_C_PLUS_CHANS_ONLY
                             *chptr->chname != '+'
#else
                             0
#endif
                               ||
#if !defined(MODE_PLUS_C)
                                1
#elif defined(PLUS_C_START)
                            ((PLUS_C_START) > chptr->channelts)
#else
                                0
#endif
				||
#ifdef PLUS_C_OPT_OUT
			    (chptr->mode.mode & MODE_NOPLUSC)
#else
				0
#endif
                            ))
			    {
			    	if (MyClient(sptr))
					sendto_one(sptr, 
						   err_str(ERR_UNKNOWNMODE),
						   me.name, sptr->name, 'c');
				/* eat the param if it's there */
				if (parc-- > 0)
					parv++;
				break;
			    }

			if (passet++)
			    {
				if (parc-- > 0)
					parv++;
				break;
			    }

			/* check whether the user is an op and the channel's 
			 * ok to set/see a +c 
			 */
			isok_c = ischop && !IsServer(sptr) && 
			    chptr->channelts && 
			    (chptr->channelts <= make_ts() - CHPASS_MIN_TS) &&
	      		    (chptr->channelts <= chptr->passwd_ts || 
			     !chptr->passwd_ts);

			/*
			** MODE_QUERY and MODE_ADD do the same if there's
			** no arg here
			*/
			if (parc-- <= 0 && whatt != MODE_DEL)
			    {
			    	if (!MyClient(sptr) ||
				    errsent(SM_ERR_RPL_C, &errors_sent))
					;
			    	else if (!*chptr->mode.pass)
				    {
			    		sendto_one(sptr, 
						   rpl_str(RPL_NOCHANPASS),
						   me.name, sptr->name,
						   chptr->chname);
				    }
			    	else if (isok_c && chptr->mode.clear)
				    {
				    	sendto_one(sptr, 
						rpl_str(RPL_CHANNELPASSIS),
						me.name, sptr->name, 
						chptr->chname, 
						chptr->mode.clear->pass);
				    }
				else if (isok_c)
				    {
				    	sendto_one(sptr,
						rpl_str(RPL_CHPASSUNKNOWN),
						me.name, sptr->name, 
						chptr->chname);
				    }
				else if (chptr->channelts > chptr->passwd_ts)
				    {
					sendto_one(sptr, 
						err_str(ERR_CHANTOORECENT),
						me.name, sptr->name,
						chptr->chname);
				    }
				else if (!isok)
				    {
					sendto_one(sptr,
						err_str(ERR_CHANOPRIVSNEEDED),
						me.name, sptr->name, 
						chptr->chname);
				    }
				break;
			    }

			arg = check_string(*parv++);
			/* returns a '*' if no arg */

			if (whatt == MODE_QUERY)
			    {
			    	/* has to be mine, we don't pass the '='
				** crap to other servers.  and anyone is
				** allowed to check a pass, even if they're
				** not ops, or for that matter not on the
				** channel  -orabidoo
				*/
		    		sendto_one(sptr, (match_pw(arg, chptr) == 0 ?
						   rpl_str(RPL_CHANPASSOK) :
						   rpl_str(RPL_BADCHANPASS)),
						 me.name, sptr->name,
						 chptr->chname);
				break;
			    }

			if (whatt == MODE_DEL)
			    {
				arg = star;
				clear = NULL;
			    }
			else if (MyClient(sptr))
			    {
				clear = fix_key(arg);
				if (!*clear)
					break;
				arg = hash_pw(clear, NULL); 
			    }
			else
			    {
				if ((clear = index(arg, ':')))
					*clear++ = '\0';
			    }

			if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
				break;

			if (!isok_c)
			    {
			    	if (!MyClient(sptr))
					;
				else if (ischop && 
					 ((chptr->passwd_ts && 
				         chptr->channelts > chptr->passwd_ts) ||
				         chptr->channelts >
				     	 make_ts() - CHPASS_MIN_TS))
					sendto_one(sptr, 
						err_str(ERR_CHANTOORECENT),
						me.name, sptr->name,
						chptr->chname);
				else if (ischop && !chptr->channelts)
					sendto_one(sptr, 
						err_str(ERR_TSLESSCHAN),
						me.name, sptr->name,
						chptr->chname, 'c');
				else
					sendto_one(sptr, 
					       err_str(ERR_CHANOPRIVSNEEDED),
					       me.name, sptr->name, 
					       chptr->chname);
				break;
			    }

			if (clear && strlen(clear) > CHPASSLEN)
				clear[CHPASSLEN] = '\0';

			tmp = strlen(arg);
			if (clear)
				tmp += strlen(clear) + 2;
			if (len + tmp + 2 >= MODEBUFLEN)
				break;

			if (whatt == MODE_DEL)
			    {
				set_pass(&chptr->mode, NULL, NULL);
				chptr->passwd_ts = 0;
			    }
			else
			    {
			    	set_pass(&chptr->mode, clear, arg);
				chptr->passwd_ts = chptr->channelts;
				chptr->opless_ts = 0;
			    }

			/* +c only uses parabuf */
			*mbuf2w++ = plus;
			*mbuf2w++ = 'c';
			strcpy(pbufw, arg);
			pbufw += strlen(pbufw);
			if (clear && whatt == MODE_ADD)
			    {
				*pbufw++ = ':';
				strcpy(pbufw, clear);
				pbufw += strlen(pbufw);
			    }
			*pbufw++ = ' ';
			len += tmp + 1;
			opcnt++;

			break;

#ifdef CLEAR_CHAN
		case 'z':
		    	if (!IsServer(sptr) && !IsOper(sptr))
			    {
				if (whatt == MODE_ADD && parc-- > 0)
					parv++;
				if (MyClient(sptr))
					sendto_one(sptr, 
						err_str(ERR_UNKNOWNMODE),
						me.name, sptr->name, 'z');
				break;
			    }
			if (whatt == MODE_DEL && (chptr->mode.mode & MODE_ZAP))
			    {
				chptr->mode.mode &= ~MODE_ZAP;
			    	chptr->opless_ts = 0;
				*mbuf2w++ = '-';
				*mbuf2w++ = 'z';
				break;
			    }
			if (whatt != MODE_ADD)
				break;

			if (MyConnect(sptr) && !IsServer(sptr))
			    {
#ifdef OPER_CLEARCHAN
				sendto_one(sptr, 
			  ":%s NOTICE %s :*** Please use /clearchan instead",
		  			   me.name, sptr->name);
#else
				sendto_one(sptr,
			  ":%s NOTICE %s :*** You are not allowed to use +z",
		  			   me.name, sptr->name);
#endif
				break;
			    }

			if (parc-- <= 0)
			    {
				sendto_ops("Parameterless mode +z from %s (ignored)",
					   parv[0]);
				break;
			    }
			arg = check_string(*parv++);
			zapts = (ts_val)atol(arg);
			if (zapts < MIN_NOW || zapts > MAX_NOW)
			    {
				sendto_ops("Mode +z from with bad TS from %s (ignored)",
					   parv[0]);
				break;
			    }

			zap_channel(sptr, chptr, zapts, "(no reason given)");

        /* we send it as a clearchan rather than as a mode */
#ifdef TS4_ONLY
			sendto_match_servs(chptr, cptr,
                           ":%s CLEARCHAN %s %lu :(no reason given)", ID(sptr),
					   chptr->chname, (long)zapts);
#else
			sendto_match_TS4_servs(1, chptr, cptr,
				    ":%s CLEARCHAN %s %lu :(no reason given)",
					       ID(sptr), chptr->chname, 
					       (long)zapts);
#endif
			break;
#endif /* CLEAR_CHAN */

		case 'e':
			if (MyConnect(sptr) && isok && !chptr->channelts && 
				whatt == MODE_ADD && parc > 0)
			    {
				if (!errsent(SM_ERR_NOTS, &errors_sent))
					sendto_one(sptr, 
						err_str(ERR_TSLESSCHAN),
						me.name, sptr->name,
						chptr->chname, 'e');
				break;
			    }
			/* falls through */

		case 'b':
			if (whatt == MODE_QUERY || parc-- <= 0)
			    {
			    	if (!MyClient(sptr))
					break;
				if (c == 'b')
				    {
			    		if (errsent(SM_ERR_RPL_B, &errors_sent))
						break;
					for (lp = chptr->banlist; lp; 
					     lp = lp->next)
						sendto_one(sptr, 
							rpl_str(RPL_BANLIST),
						        me.name, sptr->name,
						        chptr->chname,
#ifdef BAN_INFO
							lp->value.ban.banstr,
							lp->value.ban.who,
							lp->value.ban.when);
#else
							lp->value.cp);
#endif
					sendto_one(sptr, 
						   rpl_str(RPL_ENDOFBANLIST),
						   me.name, sptr->name, 
						   chptr->chname);
				    }
				else
				    {
			    		if (errsent(SM_ERR_RPL_E, &errors_sent))
						break;
					for (lp = chptr->exceptlist; lp; 
					     lp = lp->next)
						sendto_one(sptr, 
							rpl_str(RPL_EXCEPTLIST),
							me.name, sptr->name,
							chptr->chname, 
#ifdef BAN_INFO
							lp->value.ban.banstr,
							lp->value.ban.who,
							lp->value.ban.when);
#else
							lp->value.cp);
#endif
					sendto_one(sptr, 
						   rpl_str(RPL_ENDOFEXCEPTLIST),
						   me.name, sptr->name, 
						   chptr->chname);
				    }
				break;
			    }
			arg = check_string(*parv++);
			if (*arg == ':')
				break;

			if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
				break;

			if (!isok_h)
			    {
				if (!errsent(SM_ERR_NOOPS, &errors_sent) &&
				    MyClient(sptr))
					sendto_one(sptr,
						err_str(ERR_CHANOPRIVSNEEDED),
						me.name, sptr->name, 
						chptr->chname);
				break;
			    }

			/* user-friendly ban mask generation, taken
			** from Undernet's ircd  -orabidoo
			*/
			if (MyClient(sptr))
				arg = collapse(pretty_mask(arg));

			tmp = strlen(arg);
			if (len + tmp + 2 >= MODEBUFLEN)
				break;

			if (c == 'b')
			    {
				if (!(((whatt & MODE_ADD) &&
				    !add_banid(sptr, chptr, arg)) ||
				    ((whatt & MODE_DEL) && 
				    !del_banid(chptr, arg))))
					break;
			    }
			else
			    {
				if (!(((whatt & MODE_ADD) &&
				    !add_exceptid(sptr, chptr, arg)) ||
				    ((whatt & MODE_DEL) &&
				    !del_exceptid(chptr, arg))))
					break;
			    }

			*mbuf2w++ = plus;
			*mbuf2w++ = c;
			strcpy(pbufw, arg);
			pbufw += strlen(pbufw);
			strcpy(pbuf2w, arg);
			pbuf2w += strlen(pbuf2w);
			*pbufw++ = ' ';
			*pbuf2w++ = ' ';
			len += tmp + 1;
			opcnt++;

			break;


		case 'l':
			if (whatt == MODE_QUERY)
				break;
			if (!isok_h || limitset++)
			    {
				if (whatt == MODE_ADD && parc-- > 0)
					parv++;
				break;
			    }

			if (whatt == MODE_ADD)
			    {
			    	if (parc-- <= 0)
				    {
				    	if (MyClient(sptr))
						sendto_one(sptr, 
						    err_str(ERR_NEEDMOREPARAMS),
					   	    me.name, sptr->name, 
						    "MODE +l");
				    	break;
				    }

				arg = check_string(*parv++);
				if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
					break;
			    	if (!(nusers = atoi(arg)))
					break;
				sprintf(numeric, "%-15d", nusers);
				if ((tmpc = index(numeric, ' ')))
					*tmpc = '\0';
				arg = numeric;

				tmp = strlen(arg);
				if (len + tmp + 2 >= MODEBUFLEN)
					break;

				chptr->mode.limit = nusers;
				chptr->mode.mode |= MODE_LIMIT;

				*mbuf2w++ = '+';
				*mbuf2w++ = 'l';
				strcpy(pbufw, arg);
				pbufw += strlen(pbufw);
				strcpy(pbuf2w, arg);
				pbuf2w += strlen(pbuf2w);
				*pbufw++ = ' ';
				*pbuf2w++ = ' ';
				len += tmp + 1;
				opcnt++;

			    }
			else
			    {
			    	chptr->mode.limit = 0;
				chptr->mode.mode &= ~MODE_LIMIT;
				*mbufw++ = '-';
				*mbufw++ = 'l';
			    }

			break;

		default:
			if (whatt == MODE_QUERY)
				break;

			for (ip = channel_flags; *ip; ip += 2)
				if (ip[1] == c)
					break;

			if ((the_mode = *ip))
			    {
#ifndef TS4_GLOBAL_ONLY
			    	/* redundant, and possibly a hole in a 
				 * mixed net.
				 */
			    	if (isok_h && !isok && c == 's' && 
				    whatt == MODE_ADD && 
				    (chptr->mode.mode & 
				     (MODE_PRIVATE|MODE_SECRET)))
					break;
#endif

			    	if (! (isok || (isok_h && c != 'p')))
				    {
					if (!errsent(SM_ERR_NOOPS, 
						     &errors_sent))
						sendto_one(sptr,
						  err_str(ERR_CHANOPRIVSNEEDED),
						  me.name, sptr->name, 
						  chptr->chname);
					break;
				    }

				*mbufw = '\0';
				if ((tmpc = index(modebuf, c)))
				    	tmpc[-1] = plus;
				else
				    {
				    	*mbufw++ = plus;
					*mbufw++ = c;
				    }

				if (whatt == MODE_ADD)
					chptr->mode.mode |= the_mode;
				else
					chptr->mode.mode &= ~the_mode;

#ifdef PRIV_SEC_EXCLUSIVE /* notdefined */
				/* this is sheer braindmage, but it's the
				** traditional way...  -orabidoo
				*/
				if (whatt == MODE_ADD &&
				    ((c == 's' && (tmp = MODE_PRIVATE) && 
				     (cc = 'p')) ||
				    (c == 'p' && (tmp = MODE_SECRET) && 
				     (cc = 's'))) &&
				    (chptr->mode.mode & tmp))
				    {
					*mbufw = '\0';
					if ((tmpc = index(modebuf, cc)))
						tmpc[-1] = '-';
					else
					    {
						*mbufw++ = '-';
						*mbufw++ = cc;
					    }
					chptr->mode.mode &= ~tmp;
				    }
#endif
			    }
			/* only one "UNKNOWNMODE" per mode... we don't want
			** to generate a storm, even if it's just to a 
			** local client  -orabidoo
			*/
			else if (MyClient(sptr) && !errsent(SM_ERR_UNKNOWN, 
				 &errors_sent))
				sendto_one(sptr, err_str(ERR_UNKNOWNMODE),
					   me.name, sptr->name, c);
			break;
		}
	}

	/*
	** WHEW!!  now all that's left to do is put the various bufs
	** together and send it along.
	*/

	*mbufw = *mbuf2w = *pbufw = *pbuf2w = '\0';
	collapse_signs(modebuf);
	collapse_signs(modebuf2);

	if (!*modebuf && !*modebuf2)
		return;

	strcat(modebuf, modebuf2);

#ifndef TS4_ONLY
	if (fm_type == 'h' || fm_type == 'e')
	    {
		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s", 
				       sptr->name, chptr->chname, modebuf,
				       parabuf2);
		sendto_match_TS4_servs(1, chptr, cptr, ":%s MODE %s %s %s",
				       ID(sptr), chptr->chname, modebuf, 
				       parabuf);
	    }
	else
#endif
	if (fm_type == 'c' || fm_type == 'z')
	    {
		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s", 
				       sptr->name, chptr->chname, modebuf);
#ifdef TS4_ONLY
		sendto_match_servs(chptr, cptr, ":%s MODE %s %s %s", ID(sptr), 
				   chptr->chname, modebuf, parabuf);
#else
		sendto_match_TS4_servs(1, chptr, cptr, ":%s MODE %s %s %s",
				    ID(sptr), chptr->chname, modebuf, parabuf);
#endif
	    }
	else
	    {
		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s", 
				       sptr->name, chptr->chname, modebuf,
				       parabuf2);
#ifdef TS4_ONLY
		sendto_match_servs(chptr, cptr, ":%s MODE %s %s %s", ID(sptr), 
				       chptr->chname, modebuf, parabuf);
#else
		sendto_match_TS4_servs(1, chptr, cptr, ":%s MODE %s %s %s",
				    ID(sptr), chptr->chname, modebuf, parabuf);
		sendto_match_TS4_servs(0, chptr, cptr, ":%s MODE %s %s %s",
				    sptr->name, chptr->chname, modebuf, 
				    parabuf2);
#endif
	    }
        if (!chptr->members && !ZappedChannel(chptr) && !*chptr->mode.pass)
                nuke_channel(chptr);

	return;
}

static	int	can_join(sptr, chptr, key)
aClient	*sptr;
Reg2	aChannel *chptr;
char	*key;
{
	Reg1	Link	*lp;

#ifdef CLEAR_CHAN
        /* I don't feel like making up a numeric just for this  -orabilazy */
        if ((chptr->mode.mode & MODE_ZAP) && !IsOper(sptr))
                return (ERR_INVITEONLYCHAN);
#endif
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

void	clean_channelname(cn)
Reg1	char *cn;
{
	for (; *cn; cn++)
		if (*cn == '\007' || *cn == ' ' || *cn == ',')
		    {
			*cn = '\0';
			return;
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
		chptr->channelts = make_ts(); /* just in case */
		chptr->mode.clear = NULL;
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
** Remove a channel completely, assumes its user list is empty.  -orabidoo
*/
static  void    nuke_channel(chptr)
Reg1    aChannel *chptr;
{
        Reg2    Link *tmp;
	        Link    *obtmp;

	/*
	 * Find all invite links from channel structure
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
	tmp = chptr->exceptlist;
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
	    	if (*chptr->mode.pass || ZappedChannel(chptr))
		    {
		    	/*
			** +c/+z channel has become empty -- clear modes,
			** bans and exceptions -orabidoo
			*/
	  		chptr->mode.mode &= (MODE_ZAP|MODE_SECRET|MODE_PRIVATE|
					     MODE_NOPRIVMSGS|MODE_TOPICLIMIT|
					     MODE_NOPLUSC);
			chptr->mode.limit = 0;
			*chptr->mode.key = '\0';

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
			tmp = chptr->exceptlist;
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
			chptr->banlist = chptr->exceptlist = NULL;
			if (!chptr->opless_ts)
				chptr->opless_ts = make_ts();
		    }
		else
			nuke_channel(chptr);
	    }
}

/*
** locally remove all modes from a channel, and set deopped flag on any 
** that used to have ops/halfops -orabidoo
*/ 
static void deop_channel(chptr, flags)
aChannel *chptr;
int	flags;
{
	char *mbuf = modebuf, *name, c;
	Mode *mode = &chptr->mode;
	int	*ip, pargs = 0, the_mode, plen;
	Link	*l;

	*parabuf = '\0';
	*mbuf++ = '-';

	if (flags & MODE_NOPLUSC)
		mode->mode |= MODE_NOPLUSC;
	else  
		mode->mode &= ~MODE_NOPLUSC;

	for (ip = channel_flags; *ip; ip += 2)
	    {
		if (*ip & mode->mode)
		    {
			*mbuf++ = *(ip+1);
			mode->mode &= ~(*ip);
		    }
	    }

	if (mode->limit)
	    {
		*mbuf++ = 'l';
	    	mode->limit = 0;
		mode->mode &= ~MODE_LIMIT;
	    }

	if (*mode->key)
	    {
		*mbuf++ = 'k';
		strcat(parabuf, mode->key);
		strcat(parabuf, " ");
		pargs++;
		*mode->key = '\0';
	    }

	if (mbuf > modebuf+1)
		*mbuf++ = '-';

	for (l = chptr->members; l && l->value.cptr; l = l->next)
	    {
	    	for (ip = channel_uflags; (the_mode = *ip); ip += 2)
		    {
			if (!(l->flags & the_mode))
				continue;

			*mbuf++ = (char)ip[1];
			strcat(parabuf, l->value.cptr->name);
			strcat(parabuf, " ");
			pargs++;
			if (pargs >= (MAXMODEPARAMS))
			    {
				*mbuf = '\0';
				sendto_channel_butserv(chptr, &me, 
						":%s MODE %s %s %s", me.name,
					      chptr->chname, modebuf, parabuf);
				mbuf = modebuf;
				*mbuf++ = '-';
				parabuf[0] = '\0';
				pargs = 0;
			    }
			l->flags &= ~the_mode;
			if (the_mode != MODE_VOICE)
				l->flags |= MODE_DEOPPED;
		    }
	    }

	plen = strlen(parabuf);

	while ((chptr->banlist && 
		(name = chptr->banlist->value.cp) && (c = 'b')) ||
	       (chptr->exceptlist && 
		(name = chptr->exceptlist->value.cp) && (c = 'e')))
	    {
		int sl = strlen(name);

		if (pargs >= MAXMODEPARAMS || 
		    (pargs && plen + sl + 10 >= (size_t)MODEBUFLEN))
		    {
			*mbuf = '\0';
			sendto_channel_butserv(chptr, &me, ":%s MODE %s %s %s",
						me.name, chptr->chname, 
						modebuf, parabuf);
			pargs = plen = 0;
			mbuf = modebuf;
			*mbuf++ = '-';
			parabuf[0] = '\0';
		    }
		*mbuf++ = c;
		strcat(parabuf, name);
		strcat(parabuf, " ");
		pargs++;
		plen += sl + 1;

		if (c == 'b')
			del_banid(chptr, name);
		else
			del_exceptid(chptr, name);

	    }

	if (mbuf > modebuf+1)
	    {
		*mbuf = '\0';
		sendto_channel_butserv(chptr, &me, ":%s MODE %s %s %s", 
				me.name, chptr->chname, modebuf, parabuf);
	    }
}

#ifdef CLEAR_CHAN
/* Remove all non-opers, set the given TS, propagating messages only to 
** local clients.
*/
static	void	zap_channel(sptr, chptr, zapts, reason)
aClient	*sptr;
aChannel *chptr;
ts_val	zapts;
char	*reason;
{
	Reg1	aClient	*cptr;
	char	rbuf[TOPICLEN+2], *r = reason;

	if (strlen(reason) > TOPICLEN)
	    {
		strncpyzt(rbuf, reason, TOPICLEN+1);
		r = rbuf;
	    }

	chptr->mode.mode = MODE_ZAP;
	while (chptr->members)
	    {
	    	cptr = chptr->members->value.cptr;
	    	if (MyClient(cptr))
			sendto_one(cptr, ":%s KICK %s %s :Clearing channel",
				   me.name, chptr->chname, cptr->name);
		remove_user_from_channel(cptr, chptr);
		/* removing the last user also clears bans, etc */
	    }
	drop_plus_c(chptr);
	chptr->channelts = chptr->opless_ts = zapts;
	/* opless_ts is used for +z expiration too */

	sendto_ops("Channel %s has been CLEARED", chptr->chname);
	if (IsPerson(sptr))
		sendto_flagops(UFLAGS_OPERS, 
				"Channel %s CLEARED by %s!%s@%s[%s] (%s)",
				chptr->chname, sptr->name, sptr->user->username,
				sptr->user->host, sptr->servptr->name, r);
	else
		sendto_flagops(UFLAGS_OPERS, 
				"Channel %s CLEARED by %s[%s] (%s)",
				chptr->chname, sptr->name,
				sptr->servptr->name, r);
}

int	m_clearchan(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int	parc;
char	*parv[];
{
	aChannel *chptr;
	ts_val	zapts;
	char *reason = "(no reason given)";
	Link	*lp;
	aConfItem *aconf = NULL;

	if (MyClient(sptr) && IsOper(sptr))
	    {
#ifndef OPER_CLEARCHAN
		sendto_one(sptr,
		  ":%s NOTICE %s :*** /clearchan is not allowed on this server",
		   me.name, sptr->name);
		return 0;
#else
		for (lp = sptr->confs; lp; lp = lp->next)
		    {
		    	aconf = lp->value.aconf;
			if ((aconf->status & CONF_OPERATOR) && aconf->port == 8)
				break;
			else
			    	aconf = NULL;
		    }
		if (!aconf)
		    {
		    	sendto_one(sptr,
	            ":%s NOTICE %s :*** You are not allowed to use /clearchan",
				   me.name, sptr->name);
			return 0;
		    }

	    	if (parc < 2)
		    {
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
				   me.name, parv[0], "CLEARCHAN");
			return 0;
		    }
		chptr = get_channel(sptr, parv[1], 0);
	    	if (!chptr)
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
				   me.name, parv[0], parv[1]);
			return 0;
		    }
		if (parc >= 3)
			reason = parv[2];

		/* now we use the existing channel TS - 1, unless its 
		 * TS is already 0 or 1   -orabidoo
		 */
		if (chptr->channelts > 1)
			zapts = chptr->channelts - 1;
		else
			zapts = make_ts();
#endif
	    }
	else if (IsServer(sptr) || IsOper(sptr))
	    {
	    	if (parc < 3 || !IsChannelName(parv[1]))
			return 0;
		zapts = (ts_val)atol(parv[2]);
		if (zapts)
			chptr = get_channel(sptr, parv[1], CREATE);
		if (!zapts || !chptr)
		    {
			sendto_ops("Invalid CLEARCHAN from %s[%s]", sptr->name,
				   cptr->name);
			return 0;
		    }
		if (parc >= 4)
			reason = parv[3];
	    }
	else return 0;

	if (strlen(reason) >= TOPICLEN)
		reason[TOPICLEN] = '\0';

	/* ignore if zapped before */
	if ((chptr->mode.mode & MODE_ZAP) && chptr->channelts <= zapts)
		return 0;

	zap_channel(sptr, chptr, zapts, reason);

#ifdef TS4_ONLY
        sendto_match_servs(chptr, cptr, ":%s CLEARCHAN %s %lu :%s", ID(sptr), 
                           parv[1], (long)zapts, reason);
#else
        sendto_match_TS4_servs(1, chptr, cptr, ":%s CLEARCHAN %s %lu :%s",
                                  ID(sptr), parv[1], (long)zapts, reason);
#endif /* TS4_ONLY */

	return 0;
}
#endif /* CLEAR_CHAN */

#ifdef KEEP_OPS
/*
** Checks if the client is joining a channel in its keep_ops list.  If so,
** removes it from the list, and sets the associated TS.   -orabidoo
*/
static	int	ko_join(sptr, chan, jts)
aClient	*sptr;
char	*chan;
int	*jts;
{
	KeptOps *ko, *next;
	int	flags = 0;

	for (ko = sptr->ko; ko; ko = next)
	    {
	    	next = ko->next;
		if (ko->expires <= NOW)
		    {
		    	if (ko->prev)
				ko->prev->next = next;
			else
				sptr->ko = next;
			if (next)
				next->prev = ko->prev;
			MyFree(ko);
		    	continue;
		    }
	    	if (mycmp(ko->chname, chan) != 0)
			continue;
	    	*jts = ko->ts;
		flags = ko->flags;
	    	if (ko->prev)
			ko->prev->next = next;
		else
			sptr->ko = next;
		if (next)
			next->prev = ko->prev;
		MyFree(ko);
		return flags;
	    }
	return 0;
}
#endif /* KEEP_OPS */

/*
** m_join
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel password (or key)
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
	int	i, flags = 0, passwd_join, jts, koflags;
	char	*p = NULL, *p2 = NULL;

	/*
	** As of TS4, server-to-server JOIN is ignored, except for
	** ":user JOIN 0".  -orabidoo
	*/
	if (!IsPerson(sptr) || parc != 2 || *parv[1] != '0' ||
	    parv[1][1] != '\0')
	{
		if (IsServer(cptr) && sptr->user)
		    	ts_warn("server-to-server JOIN from %s(%s) ignored", 
				sptr->name, sptr->user->server);
		if (check_registered_user(sptr) || !MyClient(sptr))
			return 0;
	}

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
		clean_channelname(name);
		if (check_channelmask(sptr, cptr, name)==-1)
			continue;
		if (*name == '&' && !MyConnect(sptr))
			continue;
		if (*name == '0' && !atoi(name))
			(void)strcpy(jbuf, "0");
		else if (!IsChannelName(name) ||
#ifdef PLUS_C_START
			(*name == '+' && !(ChannelExists(name)) &&
			 (PLUS_C_START) > time(NULL))
#else
			0
#endif
		        )
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
		passwd_join = 0;
		koflags = CHFL_CHANOP;
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
			sendto_match_servs(NULL, cptr, ":%s JOIN 0", ID(sptr));
			continue;
		}

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

		chptr = get_channel(sptr, name, CREATE);

		if (!chptr || IsMember(sptr, chptr))
			continue;

		if (0 
#ifdef MODE_PLUS_C
		    || (key && *key && chptr->channelts &&
		    match_pw(key, chptr) == 0 &&
		    !ZappedChannel(chptr) &&
		    (jts = chptr->passwd_ts) && 
		    jts <= chptr->channelts)
#endif
#ifdef KEEP_OPS
		    || (chptr->channelts &&
		    (koflags = ko_join(sptr, name, &jts)) &&
		    !ZappedChannel(chptr) &&
		    jts && jts <= chptr->channelts &&
		    (jts < chptr->channelts || can_join(sptr, chptr, key)==0))
#endif
		   )
		    {
		    	flags = CHFL_CHANOP;
                        if (!ZappedChannel(chptr))
                                chptr->opless_ts = 0;
			passwd_join = 1;
			if (jts < chptr->channelts)
			    {
				chptr->channelts = jts;
				deop_channel(chptr, koflags);
			    }
		    }
		else if ((i = can_join(sptr, chptr, key)))
		{
			sendto_one(sptr, err_str(i), me.name, parv[0], name);
/*
			sendto_one(sptr,
				   ":%s %d %s %s :Sorry, cannot join channel.",
				   me.name, i, parv[0], name);
*/
			continue;
		}

#ifdef CLEAR_CHAN
                if (ZappedChannel(chptr) && IsOper(sptr))
                    {
                        flags = CHFL_CHANOP;
                        passwd_join = 1;
                    }
#endif

		/*
		**  Complete user entry to the new channel (if any)
		*/
		add_user_to_channel(chptr, sptr, flags);
		/*
		**  Set timestamp if appropriate, and propagate
		*/
                if (flags == CHFL_CHANOP)
                    {
			char *Plus = "+", *PlusC = "+C";

                        if (!passwd_join)
                                chptr->channelts = make_ts();
#ifdef PLUS_C_OPT_OUT
			if (key && !passwd_join && !strcmp(key, "none"))
				chptr->mode.mode |= MODE_NOPLUSC;

			/* no this isn't redundant, it could also have
			 * this mode after a cookie  -orabidoo
			 */
			if (chptr->mode.mode & MODE_NOPLUSC)
				Plus = PlusC;
#endif
                        sendto_match_servs(chptr, cptr,
                                ":%s SJOIN %ld %s %s :@%s", me.name,
                                chptr->channelts, name,
                                (passwd_join ? "0" : Plus), ID(sptr));
                    }
                else
                    {
                        sendto_match_servs(chptr, cptr,
                                ":%s SJOIN %ld %s 0 :%s", me.name,
                                chptr->channelts, name, ID(sptr));
                    }

		/*
		** notify all other users on the new channel
		*/
		sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s",
					parv[0], name);
                if (passwd_join)
                        sendto_channel_butserv(chptr, sptr, ":%s MODE %s +o %s",
                                                me.name, name, parv[0]);
		del_invite(sptr, chptr);
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
		sendto_match_servs(chptr, cptr, PartFmt, ID(sptr), name);
#endif
		sendto_channel_butserv(chptr, sptr, PartFmt, parv[0], name);
		remove_user_from_channel(sptr, chptr);
	    }
#ifdef	V28PlusOnly
	if (*buf)
		sendto_serv_butone(cptr, PartFmt, ID(sptr), buf);
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
	int	non_ops_only = 0, chan_op, err_sent = 0;
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
		chan_op = is_chan_op(sptr, chptr);
		if (!IsServer(sptr) && chan_op == 0 &&
		    (MyConnect(sptr) || !chptr->channelts))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				   me.name, parv[0], chptr->chname);
			continue;
		    }
                non_ops_only = MyClient(sptr) && (chan_op & MODE_HALFOP)
                                && !(chan_op & MODE_CHANOP);
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
			if (!(who = find_chasing(sptr, user, NULL)))
				continue; /* No such user left! */
#ifdef	V28PlusOnly
			if (nlen + mlen + strlen(who->name) >
			    (size_t) BUFSIZE - NICKLEN)
				continue;
#endif
			if (non_ops_only && 
			    (is_chan_op(who, chptr) & MODE_CHANOP))
                            {
                                if (!err_sent++)
                                        sendto_one(sptr,
                                                err_str(ERR_CHANOPRIVSNEEDED),
                                              me.name, parv[0], chptr->chname);
                                continue;
                             }
			if (IsMember(who, chptr))
			    {
				sendto_channel_butserv(chptr, sptr,
						":%s KICK %s %s :%s", parv[0],
						name, who->name, comment);
#ifdef	V28PlusOnly
				if (*nickbuf)
					(void)strcat(nickbuf, ",");
				(void)strcat(nickbuf, ID(who));
				nlen += strlen(ID(who));
#else
				sendto_match_servs(chptr, cptr,
						   ":%s KICK %s %s :%s",
						   ID(sptr), name,
						   ID(who), comment);
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
				   ID(sptr), buf, nickbuf, comment);
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
					   ID(sptr), chptr->chname,
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
		if (*parv[1] != '.')
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
				   me.name, parv[0], parv[1]);
		return 0;
	    }
	clean_channelname(parv[2]);
	if (check_channelmask(sptr, cptr, parv[2]))
		return 0;
	if (!(chptr = find_channel(parv[2], NullChn)))
	    {

		sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s", parv[0], 
				  IDORNICK(acptr), parv[2]);
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
			   me.name, parv[0], acptr->name, parv[2]);
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
#ifndef SHOW_EMPTY_CHANS
                            chptr->users <= 0 ||
#endif
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

	mlen = strlen(me.name) + NICKLEN + 7;

	if (!BadPtr(para))
	{
		s = index(para, ',');
		if (s)
		    {
			parv[1] = ++s;
			(void)m_names(cptr, sptr, parc, parv);
		    }
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
#ifdef NO_HALFOP_FLAGS_IN_NAMES
		        if (lp->flags & CHFL_CHANOP)
			{
				idx++;
				(void)strcat(buf, "@");
			}
			else if (lp->flags & (CHFL_HALFOP|CHFL_VOICE))
			{
				(void)strcat(buf, "+");
				idx++;
			}
#else
		        if (lp->flags & CHFL_CHANOP)
			{
				idx++;
				(void)strcat(buf, "@");
			}
			if (lp->flags & CHFL_HALFOP)
			{
				(void)strcat(buf, "%");
				idx++;
			}
			if (lp->flags & CHFL_VOICE)
			{
				(void)strcat(buf, "+");
				idx++;
			}
#endif
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

#define sjoin_sendit(sptr, chptr, from, modebuf, parabuf) \
        sendto_channel_butserv((chptr), (sptr), ":%s MODE %s %s %s", (from), \
                                (chptr)->chname, (modebuf), (parabuf))

static  void drop_plus_c(chptr)
aChannel *chptr;
{
        if (*chptr->mode.pass)
            {
                drop_clear_pass(&chptr->mode);
                *chptr->mode.pass = '\0';
                sendto_channel_butserv(chptr, &me, ":%s MODE %s -c",
                                        me.name, chptr->chname);
            }
        chptr->passwd_ts = 0;
}


/*
 * m_sjoin
 * parv[0] - sender
 * parv[1] - TS
 * parv[2] - channel
 * parv[3] - modes + n arguments (key and/or limit)
 * parv[4+n] - flags+nick list (all in one parameter)
 *
 * 
 * process a SJOIN, taking the TS's into account to either ignore the
 * incomign modes or undo the existing ones or merge them, and JOIN
 * all the specified users while sending JOIN/MODEs to clients
 */

int	m_sjoin(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int	parc;
char	*parv[];
{
	aChannel *chptr;
	aClient	*acptr;
	ts_val	newts, oldts, tstosend, newpassts;
	Mode	mode, *oldmode;
	Link	*l;
	int	args = 0, haveops = 0, keepourmodes = 1, keepnewmodes = 1,
		doesop = 0, mewhat = 0, *ip, fl, any = 0, isnew,
		the_mode, plen = 0, pargs = 0, mpargs = 0, opargs = 0,
		othwhat = 0, mplen, nouserjoins = 0;
	Reg1	char *s, *s0;
	static	char numeric[16], sjbuf[BUFSIZE];
	static	char othmodebuf[MODEBUFLEN], othparabuf[MODEBUFLEN];
	static	char memodebuf[MODEBUFLEN],  meparabuf[MODEBUFLEN];
	char	*mbuf = modebuf, *pbuf = parabuf, *t = sjbuf, *p, *name,
		*arg_pass = NULL, *clear = NULL, c;
	char	*ombuf = othmodebuf, *opbuf = othparabuf;
	char	*mmbuf = memodebuf, *mpbuf = meparabuf;

#ifndef TS4_ONLY
	static	char sjbuf2[BUFSIZE], 
		sjbanbuf[BUFSIZE], sjbanm[MAXMODEPARAMS+3];
	char	*t2 = sjbuf2, *banm, *banargs;
	int	bargs = 0, blen = 0;
#endif

	if (check_registered(sptr) || IsClient(sptr) || parc < 5)
		return 0;
	if (!IsChannelName(parv[2]) ||
	    check_channelmask(sptr, cptr, parv[2]) == -1)
		return 0;
	newts = atol(parv[1]);
	newpassts = 0;
	bzero((char *)&mode, sizeof(mode));
	*ombuf = *mmbuf = *mmbuf = *mpbuf = (char)0;

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
#ifdef CLEAR_CHAN
                    case 'z':
                        mode.mode |= MODE_ZAP;
                        break;
#endif
		    case 'k':
			strncpyzt(mode.key, parv[4+args], KEYLEN+1);
			args++;
			if (parc < 5+args) return 0;
			break;
#ifdef PLUS_C_OPT_OUT
		    case 'C':
		    	mode.mode |= MODE_NOPLUSC;
			break;
#endif
		    case 'c':
			arg_pass = parv[4+args];
			if ((clear = index(arg_pass, ':')))
				*clear++ = '\0';
		    	strncpyzt(mode.pass, arg_pass, CHPASSHASHLEN+1);
			args++;
			if (parc < 6+args) return 0;
			newpassts = newts - atol(parv[4+args]);
			args++;
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

#ifdef CLEAR_CHAN
	if ((mode.mode & MODE_ZAP) || (oldmode->mode & MODE_ZAP))
	    {
	    	if ((oldmode->mode & MODE_ZAP) && 
		    (!(mode.mode & MODE_ZAP) || newts > oldts))
		    {
			chptr->channelts = tstosend = oldts;
			if (!(mode.mode & MODE_ZAP))
				nouserjoins++;
			mode.mode = oldmode->mode;
		    }
		else
			chptr->channelts = tstosend = newts;
		if (!(oldmode->mode & MODE_ZAP))
			zap_channel(sptr, chptr, chptr->channelts, "netjoin clearchan");
		goto sjoin_user_list;  /* yeah, yuck */
	    }
	else
#endif /* CLEAR_CHAN */

	if (isnew || !chptr->users)
		chptr->channelts = tstosend = newts;
	else if (newts == 0 || oldts == 0)
	    {
		chptr->channelts = tstosend = newpassts = 0;
		drop_plus_c(chptr);
		*mode.pass = '\0';
	    }
	else if (newts == oldts)
		tstosend = oldts;
	else if (newts < oldts)
	    {
		if (doesop)
			keepourmodes = 0;
		if (haveops && !doesop)
			tstosend = oldts;
		else
		    {
			chptr->channelts = tstosend = newts;
			if (make_ts() - oldts > TS_WARN_OVERRIDE)
				ts_warn("possible HACK: old TS (%ld) on %s overriden by %s (incoming TS = %ld)", 
					(long)oldts, chptr->chname, 
					sptr->name, (long)newts);
		    }
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
	    {
		mode = *oldmode;
	    	if (newpassts && arg_pass && (newpassts < chptr->passwd_ts ||
		     (!chptr->passwd_ts && newpassts <= oldts)))
		    {
			chptr->passwd_ts = newpassts;
			set_pass(&mode, arg_pass, clear);
			mode.mode &= ~MODE_NOPLUSC;
		    }
	    }
	else 
	    {
	    	if (keepourmodes)
		    {
			mode.mode |= oldmode->mode;
			if (oldmode->limit > mode.limit)
				mode.limit = oldmode->limit;
			if (strcmp(mode.key, oldmode->key) < 0)
				strcpy(mode.key, oldmode->key);
	    	    }
		if (chptr->passwd_ts && newpassts && 
		    chptr->passwd_ts < newpassts)
		    	strcpy(mode.pass, oldmode->pass);
		else if (newpassts && chptr->passwd_ts == newpassts)
		    {
			if (strcmp(mode.pass, oldmode->pass) < 0)
				strcpy(mode.pass, oldmode->pass);
		    }
		else if (tstosend)
		    {
			if (*mode.pass)
				chptr->passwd_ts = newpassts;
			else if (!chptr->passwd_ts || newts < chptr->passwd_ts)
			    {
				drop_plus_c(chptr);
				*mode.pass = '\0';
				*oldmode->pass = '\0';
			    }
			else
				strcpy(mode.pass, oldmode->pass);
			/* keep old pass if there isn't an incoming 
			 * pass, and the TS's are the same  -orabidoo
			 */
		    }
		if (mode.mode & MODE_NOPLUSC)
		    {
		    	if (chptr->passwd_ts && 
			    chptr->passwd_ts < chptr->channelts)
				mode.mode &= ~MODE_NOPLUSC;
			else
			    {
				drop_plus_c(chptr);
				*mode.pass = '\0';
				*oldmode->pass = '\0';
			    }
		    }
	    }

#ifdef PRIV_SEC_EXCLUSIVE /* notdefined */
	if ((mode.mode & MODE_PRIVATE) && (mode.mode & MODE_SECRET))
		mode.mode &= ~MODE_SECRET;
#endif

	for (ip = channel_flags; *ip; ip += 2)
	    {
		if ((*ip & mode.mode) && !(*ip & oldmode->mode))
		    {
			if (othwhat != 1)
			    {
				*ombuf++ = '+';
				othwhat = 1;
			    }
			*ombuf++ = *(ip+1);
		    }
		 else if ((*ip & oldmode->mode) && !(*ip & mode.mode))
		    {
			if (mewhat != -1)
			    {
				*mmbuf++ = '-';
				mewhat = -1;
			    }
			*mmbuf++ = *(ip+1);
		    }
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
	else if (mode.limit && oldmode->limit != mode.limit)
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
		strcpy(opbuf, numeric);
		opbuf += strlen(opbuf);
		*opbuf++ = ' ';
		opargs++;
		/* plen is not kept up to date in this part */
	    }
	if (oldmode->key[0] && !mode.key[0])
	    {
		if (mewhat != -1)
		    {
			*mmbuf++ = '-';
			mewhat = -1;
		    }
		*mmbuf++ = 'k';
		strcpy(mpbuf, oldmode->key);
		mpbuf += strlen(mpbuf);
		*mpbuf++ = ' ';
		mpargs++;
	    }
	else if (mode.key[0] && strcmp(oldmode->key, mode.key))
	    {
		if (othwhat != 1)
		    {
			*ombuf++ = '+';
			othwhat = 1;
		    }
		*ombuf++ = 'k';
		strcpy(opbuf, mode.key);
		opbuf += strlen(opbuf);
		*opbuf++ = ' ';
		opargs++;
	    }
	if (oldmode->pass[0] && !mode.pass[0])
	    {
		if (mewhat != -1)
		    {
			*mmbuf++ = '-';
			mewhat = -1;
		    }
		*mmbuf++ = 'c';
		if (oldmode->clear)
			drop_clear_pass(oldmode);
	    }
	else if (mode.pass[0] && strcmp(oldmode->pass, mode.pass))
	    {
		if (othwhat != 1)
		    {
			*ombuf++ = '+';
			othwhat = 1;
		    }
		*ombuf++ = 'c';
		/* if we're here, the pass has changed, so it _has_ to be
		** the arriving one, so "clear" is right  -orabidoo
		*/
		set_pass(&mode, clear, mode.pass);
	    }

	chptr->mode = mode;

	if (!keepourmodes)
	    {
	    	mewhat = 0;
		for (l = chptr->members; l && l->value.cptr; l = l->next)
		    {
		    	for (ip = channel_uflags; (the_mode = *ip); ip += 2)
			    {
				if (!(l->flags & the_mode))
					continue;

				if (mewhat != -1)
				    {
					*mmbuf++ = '-';
					mewhat = -1;
				    }
				*mmbuf++ = (char)ip[1];
				strcpy(mpbuf, l->value.cptr->name);
				mpbuf += strlen(mpbuf);
				*mpbuf++ = ' ';
				mpargs++;
				if (mpargs >= (MAXMODEPARAMS))
				    {
					*mmbuf = *mpbuf = '\0';
					sjoin_sendit(sptr, chptr,
							me.name, memodebuf,
							meparabuf);
					mmbuf = memodebuf;
					mpbuf = meparabuf;
					mpargs = mewhat = 0;
				    }
				l->flags &= ~the_mode;
				if (the_mode != MODE_VOICE)
					l->flags |= MODE_DEOPPED;
			    }
		    }

		*mmbuf = *mpbuf = '\0';
		if (mmbuf != memodebuf)
			sjoin_sendit(sptr, chptr, me.name, 
					memodebuf, meparabuf);
		mmbuf = memodebuf;
		mpbuf = meparabuf;
		plen = mpargs = mplen = mewhat = 0;

		while ((chptr->banlist && 
			(name = BANSTR(chptr->banlist)) && (c = 'b')) ||
		       (chptr->exceptlist && 
			(name = BANSTR(chptr->exceptlist)) && (c = 'e')))
		    {
		    	/* here we should probably send -b's to non-TS4
			 * servers too, but I'm feeling lazy and it 
			 * smells of bouncy TS, which I don't like -orabidoo
			 */

		    	int sl = strlen(name);

		    	if (mpargs >= MAXMODEPARAMS || 
			    (mpargs && mplen + sl + 10 >= (size_t)MODEBUFLEN))
			    {
			    	*mpbuf = *mmbuf = '\0';
				sjoin_sendit(sptr, chptr, me.name,
					     memodebuf, meparabuf);
				mpargs = mplen = mewhat = 0;
				mmbuf = memodebuf;
				mpbuf = meparabuf;
			    }
			if (mewhat != -1)
			    {
				*mmbuf++ = '-';
				mewhat = -1;
			    }
			*mmbuf++ = c;
			strcpy(mpbuf, name);
			mpbuf += sl;
			*mpbuf++ = ' ';
			mpargs++;

			if (c == 'b')
				del_banid(chptr, name);
			else
				del_exceptid(chptr, name);

		    }
	    }

	if (mmbuf != memodebuf)
	    {
	    	*mmbuf = *mpbuf = '\0';
		sjoin_sendit(sptr, chptr, me.name, memodebuf, meparabuf);
	    }
	if (ombuf != othmodebuf)
	    {
		*ombuf = *opbuf = '\0';
		sjoin_sendit(sptr, chptr, parv[0], othmodebuf, othparabuf);
	    }

sjoin_user_list:
	*modebuf = *parabuf = '\0';
	if (parv[3][0] != '0' && keepnewmodes)
		channel_modes(&me, modebuf, parabuf, chptr);
	else
	    {
		modebuf[0] = '0';
		modebuf[1] = '\0';
	    }

	sprintf(sjbuf, ":%s SJOIN %ld %s %s %s :", parv[0], tstosend, parv[2],
		modebuf, parabuf);
	t = sjbuf + strlen(sjbuf);

#ifndef TS4_ONLY
	*parabuf = '\0';
	if (parv[3][0] != '0' && keepnewmodes)
		channel_modes(NULL, modebuf, parabuf, chptr);
	sprintf(sjbuf2, ":%s SJOIN %ld %s %s %s :", parv[0], tstosend, parv[2],
		modebuf, parabuf);
	t2 = sjbuf2 + strlen(sjbuf2);
#endif

	mbuf = modebuf;
	pbuf = parabuf;
	pargs = plen = 0;
	*mbuf++ = '+';

#ifndef TS4_ONLY
	banm = sjbanm;
	banargs = sjbanbuf;
	bargs = blen = 0;
	*banm++ = '+';
#endif

	for (s = s0 = strtoken(&p, parv[args+4], " "); s;
	     s = s0 = strtoken(&p, NULL, " "))
	    {
	    	/* 
		** a single "*" passes an empty user/ban list -- useful
		** to pass modes/TS along even if the incoming users were
		** all invalid  -orabidoo
		*/
	    	if (*s == '*' && s[1] == 0)
			continue;

	    	if (s[1] == ')' && *s != '.')
		    {
		    	int sl;

		    	if (!keepnewmodes)
				continue;

			strcpy(t, s);
			t += strlen(t);
			*t++ = ' ';
			any++;

			c = *s;
			if (c != 'b' && c != 'e')
				continue;
			s += 2;
			sl = strlen(s);

			if (c == 'b')
				add_banid(sptr, chptr, s);
			else
				add_exceptid(sptr, chptr, s);

			if (pargs >= (MAXMODEPARAMS) ||
			    (pargs && plen + sl + 10 >= (size_t)MODEBUFLEN))
			    {
				*mbuf = *pbuf = '\0';
				sjoin_sendit(sptr, chptr, parv[0], modebuf,
						parabuf);
				mbuf = modebuf;
				*mbuf++ = '+';
				pbuf = parabuf;
				pargs = plen = 0;
			    }
			if (plen + sl + 10 < (size_t)MODEBUFLEN)
			    {
			    	*mbuf++ = c;
				strcpy(pbuf, s);
				pbuf += sl;
				*pbuf++ = ' ';
				pargs++;
				plen += sl + 1;
			    }

#ifndef TS4_ONLY
			/*
			 * translate sjoin'ed bans for the benefit of non-TS4
			 * servers  -orabidoo
			 */
			if (c == 'e')
				continue;
			if (bargs >= (MAXMODEPARAMS) ||
			    (bargs && blen + sl + 10 >= (size_t)MODEBUFLEN))
			    {
			    	*banm = *banargs = '\0';
                		sendto_match_TS4_servs(0, chptr, cptr, 
						":%s MODE %s %s %s",
		                                sptr->name, chptr->chname, 
						sjbanm, sjbanbuf);
				bargs = blen = 0;
				banm = sjbanm;
				banargs = sjbanbuf;
				*banm++ = '+';
			    }
			if (blen + sl + 10 < (size_t)MODEBUFLEN)
			    {
			    	if (bargs++)
					*banargs++ = ' ';
				strcpy(banargs, s);
				banargs += sl;
				blen += sl + 1;
			    	*banm++ = 'b';
			    }
#endif
			continue;
		    }

		fl = 0;
		while ((*s == '@' && (fl |= MODE_CHANOP) && ++s) ||
		       (*s == '+' && (fl |= MODE_VOICE) && ++s) ||
		       (*s == '%' && (fl |= MODE_HALFOP) && ++s))
			;
		if (!keepnewmodes)
		    {
			if (fl & (MODE_CHANOP|MODE_HALFOP))
				fl = MODE_DEOPPED;
			else
				fl = 0;
		    }
		if (!(acptr = find_chasing(sptr, s, NULL)))
			continue;
		if (acptr->from != cptr)
			continue;
#ifdef CLEAR_CHAN
                if (nouserjoins && !IsOper(acptr))
                        continue;
#endif
		any++;
		if (!IsMember(acptr, chptr))
		    {
			add_user_to_channel(chptr, acptr, fl);
			sendto_channel_butserv(chptr, acptr, ":%s JOIN :%s",
						acptr->name, parv[2]);
		    }

		if (keepnewmodes)
		    {
			strcpy(t, s0);
#ifndef TS4_ONLY
			while(*s0 == '@' || *s0 == '+')
				*t2++ = *s0++;
			strcpy(t2, acptr->name);
#endif
		    }
		else
		    {
			strcpy(t, s);
#ifndef TS4_ONLY
			strcpy(t2, acptr->name);
#endif
		    }
		t += strlen(t);
		*t++ = ' ';
#ifndef TS4_ONLY
		t2 += strlen(t2);
		*t2++ = ' ';
#endif

		if ((fl & OpsMask) && !ZappedChannel(chptr) &&
		    (!chptr->passwd_ts || chptr->passwd_ts == tstosend))
		    	chptr->opless_ts = 0;

	    	for (ip = channel_uflags; (the_mode = *ip); ip += 2)
		    {
		    	int sl = strlen(acptr->name);

			if (!(fl & the_mode))
				continue;

			if (pargs >= (MAXMODEPARAMS) ||
			    (pargs && plen + sl + 10 >= (size_t)MODEBUFLEN))
			    {
				*mbuf = *pbuf = '\0';
				sjoin_sendit(sptr, chptr, parv[0], modebuf,
					     parabuf);
				mbuf = modebuf;
				*mbuf++ = '+';
				pbuf = parabuf;
				pargs = plen = 0;
			    }
			if (plen + sl + 10 < (size_t)MODEBUFLEN)
			    {
			    	*mbuf++ = (char)ip[1];
				strcpy(pbuf, acptr->name);
				pbuf += sl;
				*pbuf++ = ' ';
				pargs++;
				plen += sl + 1;
			    }
		    }
	    }

	*mbuf = *pbuf = '\0';
	if (pargs)
		sjoin_sendit(sptr, chptr, parv[0], modebuf, parabuf);

	if (t[-1] == ' ')
		*--t = '\0';
	else
		*t = '\0';

	/* empty list -> add a single '*', only for TS4 servers */
	if (!any)
	    {
	    	*t++ = '*';
		*t = '\0';
	    }

#ifdef TS4_ONLY
	sendto_match_servs(chptr, cptr, "%s", sjbuf);
#else
	sendto_match_TS4_servs(1, chptr, cptr, "%s", sjbuf);

	if (any)
	    {
		if (t2[-1] == ' ')
			t2[-1] = '\0';
		else
			*t2 = '\0';
		sendto_match_TS4_servs(0, chptr, cptr, "%s", sjbuf2);
	    }
    	*banm = *banargs = '\0';
	if (bargs)
		sendto_match_TS4_servs(0, chptr, cptr, ":%s MODE %s %s %s",
					sptr->name, chptr->chname, sjbanm, 
					sjbanbuf);
#endif

	/* just in case... */
	if (!*chptr->mode.pass && !chptr->members && !ZappedChannel(chptr))
		nuke_channel(chptr);

	return 0;
}

/*
**  remove +c passwords locally on TS-less channels, as well as channels
**  that have been opless for too long.  remove them from the list
**  altogether if they are empty.  also forget clear passwords after a
**  while  -orabidoo
*/
void	expire_channel_passwords()
{
	Reg1	aChannel *chptr, *nextch;
	Reg2	Link *l;
	Reg3	Mode	*mode;
	time_t	now = make_ts();

	for (chptr = channel; chptr; chptr = nextch)
	    {
	    	nextch = chptr->nextch;
	    	mode = &chptr->mode;

#ifdef CLEAR_CHAN
		if (ZappedChannel(chptr))
		    {
			if (!chptr->opless_ts || 
			    chptr->opless_ts < now - CLEARCHAN_EXPIRE)
			    {
				chptr->mode.mode &= ~MODE_ZAP;
				sendto_channel_butserv(chptr, &me,
					":%s MODE %s -z", me.name,
					chptr->chname);

				/* This does NOT generate a storm:
				** whichever first notices it has
				** expired sends it to others, which
				** take the mode off too.
				*/
#ifdef TS4_ONLY
				sendto_match_servs(chptr, NULL,
					":%s MODE %s -z", me.name,
					chptr->chname);
#else
				sendto_match_TS4_servs(1, chptr, NULL,
					":%s MODE %s -z", me.name,
					chptr->chname);
#endif
				chptr->opless_ts = 0;
				if (!chptr->members)
				    {
					nuke_channel(chptr);
					continue;
				    }
			    }
		    }
#endif /* CLEAR_CHAN */

		if (*mode->pass)
		    {
			if (!chptr->channelts || 
			    (chptr->mode.mode & MODE_NOPLUSC) ||
			    chptr->channelts < chptr->passwd_ts)
				drop_plus_c(chptr);
			else if (!chptr->opless_ts)
			    {
				Reg3	int ops = 0;

				if (chptr->channelts <= chptr->passwd_ts)
					for (l = chptr->members; 
					     l && l->value.cptr; 
					     l = l->next)
						if (l->flags & OpsMask)
						    {
							ops = 1;
							break;
						    }
				if (!ops)
					chptr->opless_ts = now;
			    }
			else if (chptr->opless_ts < now - CHPASS_EXPIRE)
			    {
				if (chptr->members)
				    {
					drop_plus_c(chptr);
					chptr->opless_ts = 0;
				    }
				else
				    {
					nuke_channel(chptr);
					continue;
				    }
			    }
		    }

		/* no "else" ... the pass could have fallen off in a 
		** drop_plus_c above
		*/
		if (!*mode->pass && !chptr->members && !ZappedChannel(chptr))
		    {
			/* can this ever happen? */
			nuke_channel(chptr);
			continue;
		    }

	    	if (mode->clear && mode->clear->expires < now)
			drop_clear_pass(&chptr->mode);
	    }
}

#ifdef KEEP_OPS

void	free_ko_chain(ko)
KeptOps	*ko;
{
	KeptOps	*tmp;

	for (; ko; ko = tmp)
	    {
	    	tmp = ko->next;
		MyFree(ko->chname);
		MyFree(ko);
	    }
}

static	void	expire_saved_ops()
{
	IdKeptOps	*tmp;
	ts_val	now = (ts_val)time(NULL);

	while ((tmp = first_kept_ops) && tmp->expires <= now)
	    {
	    	free_ko_chain(tmp->first);
		if ((first_kept_ops = tmp->next))
			first_kept_ops->prev = NULL;
		else
			last_kept_ops = NULL;
	    	MyFree(tmp);
	    }
}

void	save_client_ops(sptr)
aClient	*sptr;
{
	IdKeptOps *iko;
	KeptOps	*ko, *prev = NULL, *tmp;
	Link	*lp;
	aChannel *chptr;
	int	chans = 0;
	ts_val	now = (ts_val)time(NULL);

	iko = (IdKeptOps *)MyMalloc(sizeof(IdKeptOps));
	strncpyzt(iko->cookie, sptr->cookie, COOKIELEN+1);
	iko->expires = now + KEPT_OPS_EXPIRE;

	for (lp = sptr->user->channel; lp; lp = lp->next)
	    {
	    	chptr = lp->value.chptr;
		if (chptr->channelts && 
		    now - chptr->channelts >= KEEP_OPS_MIN_TS &&
		    (is_chan_op(sptr, chptr) & MODE_CHANOP))
		    {
			if (chans++ >= MAXKEPTCHANS)
				break;
		    	ko = (KeptOps *)MyMalloc(sizeof(KeptOps));
			ko->chname = MyMalloc(2+strlen(chptr->chname));
			strcpy(ko->chname, chptr->chname);
			ko->ts = chptr->channelts;
			ko->flags = CHFL_CHANOP;
			if (chptr->mode.mode & MODE_NOPLUSC)
				ko->flags |= MODE_NOPLUSC;
			ko->expires = now + KEPT_OPS_EXPIRE;
			ko->next = NULL;
			ko->prev = prev;
			if (prev)
				prev->next = ko;
			else
				iko->first = ko;
			prev = ko;
		    }
	    }
	for (ko = sptr->ko; ko; ko = tmp)
	    {
	    	tmp = ko->next;
		if (ko->expires <= now || chans++ >= MAXKEPTCHANS)
			MyFree(ko);
		else
		    {
		    	ko->next = NULL;
			ko->prev = prev;
			if (prev)
				prev->next = ko;
			else
				iko->first = ko;
		    	prev = ko;
		    }
	    }
	sptr->ko = NULL;
	if (chans && (sptr->flags & FLAGS_NKILLED))
	    {
	    	sptr->ko = iko->first;
		MyFree(iko);
	    }
	else if (chans)
	    {
		iko->next = NULL;
		if ((iko->prev = last_kept_ops))
			iko->prev->next = iko;
		else
			first_kept_ops = iko;
	    	last_kept_ops = iko;
	    }
	else
		MyFree(iko);

	expire_saved_ops();
}

IdKeptOps	*find_cookie(cookie)
char	*cookie;
{
	IdKeptOps *tmp;

	for (tmp = first_kept_ops; tmp; tmp = tmp->next)
		if (strcmp(cookie, tmp->cookie) == 0)
			return tmp;
	
	return NULL;
}

int	m_cookie(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	IdKeptOps *iko;
	KeptOps	*ko, *next;
	aChannel *chptr;
	ts_val	now;

	if (check_registered_user(sptr) || !MyClient(sptr) || 
	    parc <= 1 || sptr->ko != NULL)
		return 0;

	if (!(iko = find_cookie(parv[1])))
	    {
#ifndef SILENT_COOKIE
                sendto_one(sptr, ":%s NOTICE %s :*** No matching cookie",
                                me.name, sptr->name);
#endif
		return 0;
	    }
	now = (ts_val)time(NULL);
	if (iko->expires <= now)
	    {
#ifndef SILENT_COOKIE
                sendto_one(sptr, ":%s NOTICE %s :*** Expired cookie",
                                me.name, sptr->name);
#endif
		free_ko_chain(iko->first);
	    }
	else
	    {
		sptr->ko = iko->first;
		for (ko = sptr->ko; ko; ko = next)
		    {
	    		next = ko->next;
			if ((chptr = find_channel(ko->chname, NullChn)) &&
		    	    IsMember(sptr, chptr))
			    {
		    		if (ko->prev)
					ko->prev->next = next;
				else
					sptr->ko = next;
				if (next)
					next->prev = ko->prev;
		    		MyFree(ko);
			    }
		    }
#ifndef SILENT_COOKIE
                sendto_one(sptr, ":%s NOTICE %s :*** Cookie matched",
                                me.name, sptr->name);
#endif
	    }

	if (iko->prev)
		iko->prev->next = iko->next;
	else
		first_kept_ops = iko->next;
	if (iko->next)
		iko->next->prev = iko->prev;
	else
		last_kept_ops = iko->prev;
    	MyFree(iko);
	return 0;
}

#endif /* KEEP_OPS */

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
		if (IsChannelName(parv[1]))
		    {
			chptr = get_channel(sptr, parv[1], 0);
			if (!chptr)
			    {
				sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
					    me.name, parv[0], parv[1]);
				return 0;
			    }
			sendto_one(sptr, 
     ":%s 999 %s :Channel: `%s', TS: %ld, pass_ts: %ld, opless_ts: %ld, passhash: `%s', clearpass: `%s'",
				   me.name, sptr->name, chptr->chname,
				   chptr->channelts, chptr->passwd_ts,
				   chptr->opless_ts, chptr->mode.pass,
		   (chptr->mode.clear ? chptr->mode.clear->pass : "(none)"));
		    }
		else
		    {
			acptr = find_client(parv[1], NULL);
			if (!acptr)
			    {
				sendto_one(sptr, err_str(ERR_NOSUCHNICK),
						me.name, parv[0], parv[1]);
				return 0;
			    }
			sendto_one(sptr, 
				":%s 999 %s :Nick: `%s', ID: `%s', TS: %ld, ",
				   me.name, sptr->name, acptr->name,
				   acptr->user->id, acptr->tsinfo);
		   }
	       return 0;
	   }
	for (acptr = client; acptr; acptr = acptr->next)
	{
	    if (IsPerson(acptr))
		sendto_one(sptr, 
			    ":%s 999 %s :Nick: `%s', ID: `%s', TS: %ld, ",
			     me.name, sptr->name, acptr->name,
			     acptr->user->id, acptr->tsinfo);
	    else if (IsServer(acptr) && MyConnect(acptr))
		sendto_one(sptr, ":%s 999 %s :Server - %s (%s)", me.name,
			    sptr->name, acptr->name, 
			    (DoesTS4(acptr) ? "TS4" : "TS3"));
	}
	for (chptr = channel; chptr; chptr = chptr->nextch)
	sendto_one(sptr, 
     	     ":%s 999 %s :Channel: `%s', TS: %ld, pass_ts: %ld, opless_ts: %ld, passhash: `%s', clearpass: `%s'",
	      me.name, sptr->name, chptr->chname,
	      chptr->channelts, chptr->passwd_ts,
	      chptr->opless_ts, chptr->mode.pass,
	      (chptr->mode.clear ? chptr->mode.clear->pass : "(none)"));
	return 0;
}

#endif


