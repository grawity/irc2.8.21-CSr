/************************************************************************
 *   IRC - Internet Relay Chat, common/send.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *		      University of Oulu, Computing Center
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

/* -- Jto -- 16 Jun 1990
 * Added Armin's PRIVMSG patches...
 */

#ifndef lint
static  char rcsid[] = "@(#)$Id: send.c,v 1.2 1998/01/05 05:55:48 cbehrens Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "numeric.h"

#include "fdlist.h"
void sendto_fdlist();
 
#ifdef	IRCII_KLUDGE
#define	NEWLINE	"\n"
#else
#define NEWLINE	"\r\n"
#endif

static	int	send_message PROTO((aClient *, char *, int));
static	int	sendprep();

/*
** dead_link
**	An error has been detected. The link *must* be closed,
**	but *cannot* call ExitClient (m_bye) from here.
**	Instead, mark it with FLAGS_DEADSOCKET. This should
**	generate ExitClient from the main loop.
**
**	If 'notice' is not NULL, it is assumed to be a format
**	for a message to local opers. I can contain only one
**	'%s', which will be replaced by the sockhost field of
**	the failing link.
**
**	Also, the notice is skipped for "uninteresting" cases,
**	like Persons and yet unknown connections...
*/
static	int	dead_link(to, notice)
aClient *to;
char	*notice;
{
	long int connected;

	to->flags |= FLAGS_DEADSOCKET;

	/*
	 * If because of BUFFERPOOL problem then clean dbuf's now so that
	 * notices don't hurt operators below.
	 */
	DBufClear(&to->recvQ);
	DBufClear(&to->sendQ);
	if (!IsPerson(to) && !IsUnknown(to) && !(to->flags & FLAGS_CLOSING))
		sendto_ops(notice, get_client_name(to, FALSE));
	if (IsServer(to))
	{
		connected = NOW - to->firsttime;
		sendto_ops("%s had been connected for %d day%s, %2d:%02d:%02d",
			to->name && *to->name ? to->name : "Server",
			connected/86400,
			(connected/86400 == 1) ? "" : "s",
			(connected % 86400) / 3600,
			(connected % 3600) / 60,
			connected % 60);
	}
	Debug((DEBUG_ERROR, notice, get_client_name(to, FALSE)));
	return -1;
}

/*
** flush_connections
**	Used to empty all output buffers for all connections. Should only
**	be called once per scan of connections. There should be a select in
**	here perhaps but that means either forcing a timeout or doing a poll.
**	When flushing, all we do is empty the obuffer array for each local
**	client and try to send it. if we cant send it, it goes into the sendQ
**	-avalon
*/

void	flush_connections(fd)
int	fd;
{
#ifdef SENDQ_ALWAYS
	Reg1	int	i;
	Reg2	aClient *cptr;
	int	length = 1;

	if (fd == me.fd)
	{
		for (i = highest_fd; i >= 0; i--)
		{
			if ((cptr = local[i]) && DBufLength(&cptr->sendQ) > 0)
				(void)send_queued(cptr);
			if (!cptr)
				continue;
			length = 1;
			if (!NoNewLine(cptr))
				length = read_packet(cptr, 0);
			if (length == FLUSH_BUFFER)
				continue;
			if (IsDead(cptr))
			{
				(void)exit_client(cptr, cptr, &me,
					strerror(get_sockerr(cptr)));
				continue;
			}
			if (length > 0)
				continue;
			if (IsServer(cptr) || IsHandshake(cptr))
			{
				int connected = NOW - cptr->firsttime;

				if (length == 0)
				sendto_ops("Server %s closed the connection",
						get_client_name(cptr,FALSE));
				else
					report_error("Lost connection to %s:%s",
						cptr);

				sendto_ops("%s had been connected for %d day%s, %2d:%02d:%02d",
					cptr->name,
					connected/86400,
					(connected/86400 == 1) ? "" : "s",
					(connected % 86400) / 3600,
					(connected % 3600) / 60,
					connected % 60);
			}
			(void)exit_client(cptr, cptr, &me, length >= 0 ?
				"EOF From client" :
					strerror(get_sockerr(cptr)));
		}
	}
	else if (fd >= 0 && (cptr = local[fd]) && DBufLength(&cptr->sendQ) > 0)
		(void)send_queued(cptr);
#endif
}

/*
** send_message
**	Internal utility which delivers one message buffer to the
**	socket. Takes care of the error handling and buffering, if
**	needed.
*/
static	int	send_message(to, msg, len)
aClient	*to;
char	*msg;	/* if msg is a null pointer, we are flushing connection */
int	len;
#ifdef SENDQ_ALWAYS
{
	if (IsDead(to))
		return 0; /* This socket has already been marked as dead */
	if (DBufLength(&to->sendQ) > get_sendq(to))
	    {
		if (IsServer(to))
			sendto_ops("Max SendQ limit exceeded for %s: %d > %d",
			   	get_client_name(to, FALSE),
				DBufLength(&to->sendQ), get_sendq(to));
		return dead_link(to, "Max Sendq exceeded");
	    }
	else if (dbuf_put(&to->sendQ, msg, len) < 0)
		return dead_link(to, "Buffer allocation error for %s");
	/*
	** Update statistics. The following is slightly incorrect
	** because it counts messages even if queued, but bytes
	** only really sent. Queued bytes get updated in SendQueued.
	*/
	to->sendM += 1;
	me.sendM += 1;
	if (to->acpt != &me)
		to->acpt->sendM += 1;
	/*
	** This little bit is to stop the sendQ from growing too large when
	** there is no need for it to. Thus we call send_queued() every time
	** 2k has been added to the queue since the last non-fatal write.
	** Also stops us from deliberately building a large sendQ and then
	** trying to flood that link with data (possible during the net
	** relinking done by servers with a large load).
	*/
	if (DBufLength(&to->sendQ)/1024 > to->lastsq)
		send_queued(to);
	return 0;
}
#else
{
	int	rlen = 0;

	if (IsDead(to))
		return 0; /* This socket has already been marked as dead */

	/*
	** DeliverIt can be called only if SendQ is empty...
	*/
	if ((DBufLength(&to->sendQ) == 0) &&
	    (rlen = deliver_it(to, msg, len)) < 0)
		return dead_link(to,"Write error to %s, closing link");
	else if (rlen < len)
	    {
		/*
		** Was unable to transfer all of the requested data. Queue
		** up the remainder for some later time...
		*/
		if (DBufLength(&to->sendQ) > get_sendq(to))
		    {
			sendto_ops("Max SendQ limit exceeded for %s : %d > %d",
				   get_client_name(to, FALSE),
				   DBufLength(&to->sendQ), get_sendq(to));
			return dead_link(to, "Max Sendq exceeded");
		    }
		else if (dbuf_put(&to->sendQ,msg+rlen,len-rlen) < 0)
			return dead_link(to,"Buffer allocation error for %s");
	    }
	/*
	** Update statistics. The following is slightly incorrect
	** because it counts messages even if queued, but bytes
	** only really sent. Queued bytes get updated in SendQueued.
	*/
	to->sendM += 1;
	me.sendM += 1;
	if (to->acpt != &me)
		to->acpt->sendM += 1;
	return 0;
}
#endif

/*
** send_queued
**	This function is called from the main select-loop (or whatever)
**	when there is a chance the some output would be possible. This
**	attempts to empty the send queue as far as possible...
*/
int	send_queued(to)
aClient *to;
{
	char	*msg;
	int	len, rlen;

	/*
	** Once socket is marked dead, we cannot start writing to it,
	** even if the error is removed...
	*/
	if (IsDead(to))
	    {
		/*
		** Actually, we should *NEVER* get here--something is
		** not working correct if send_queued is called for a
		** dead socket... --msa
		*/
#ifndef SENDQ_ALWAYS
		return dead_link(to, "send_queued called for a DEADSOCKET:%s");
#else
		return -1;
#endif
	    }
	while (DBufLength(&to->sendQ) > 0)
	    {
		msg = dbuf_map(&to->sendQ, &len);
					/* Returns always len > 0 */
		if ((rlen = deliver_it(to, msg, len)) < 0)
			return dead_link(to,"Write error to %s, closing link");
		(void)dbuf_delete(&to->sendQ, rlen);
		to->lastsq = DBufLength(&to->sendQ)/1024;
		if (rlen < len) /* ..or should I continue until rlen==0? */
			break;
	    }

	return (IsDead(to)) ? -1 : 0;
}

/*
** send message to single client
*/
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_one(to, pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
aClient *to;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
{
#else
void	sendto_one(to, pattern, va_alist)
aClient	*to;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	char	sendbuf[2048];

#ifdef VMS
	extern int goodbye;
	
	if (StrEq("QUIT", pattern)) 
		goodbye = 1;
#endif

#ifdef	USE_VARARGS
	va_start(vl);
	(void)vsprintf(sendbuf, pattern, vl);
	va_end(vl);
#else
	(void)irc_sprintf(sendbuf, pattern, p1, p2, p3, p4, p5, p6,
		p7, p8, p9, p10, p11);
#endif
	Debug((DEBUG_SEND,"Sending [%s] to %s", sendbuf,to->name));

	if (to->from)
		to = to->from;
	if (to->fd < 0)
	{
		Debug((DEBUG_ERROR,
		      "Local socket %s with negative fd... AARGH!",
		      to->name));
	}
	else if (IsMe(to))
	{
		sendto_ops("Trying to send [%s] to myself!", sendbuf);
		return;
	}
	(void)strcat(sendbuf, NEWLINE);
#ifndef	IRCII_KLUDGE
	sendbuf[510] = '\r';
#endif
	sendbuf[511] = '\n';
	sendbuf[512] = '\0';
	(void)send_message(to, sendbuf, strlen(sendbuf));
}

# ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_channel_butone(one, from, chptr, pattern,
			      p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
aChannel *chptr;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
# else
void	sendto_channel_butone(one, from, chptr, pattern, va_alist)
aClient	*one, *from;
aChannel *chptr;
char	*pattern;
va_dcl
{
	va_list	vl;
# endif
	Reg1	Link	*lp;
	Reg2	aClient *acptr;
	Reg3	int	i;
	int	sentalong[MAXCONNECTIONS];

# ifdef	USE_VARARGS
	va_start(vl);
# endif
        bzero((char *)sentalong,sizeof(sentalong));
	for (lp = chptr->members; lp; lp = lp->next)
	    {
		acptr = lp->value.cptr;
		if (acptr->from == one)
			continue;	/* ...was the one I should skip */
		i = acptr->from->fd;
		if (i < 0)
			continue;
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		    {
# ifdef	USE_VARARGS
			sendto_prefix_one(acptr, from, pattern, vl);
# else
			sendto_prefix_one(acptr, from, pattern, p1, p2,
					  p3, p4, p5, p6, p7, p8);
# endif
			sentalong[i] = 1;
		    }
		else
		    {
		/* Now check whether a message has been sent to this
		 * remote link already */
			if (sentalong[i] == 0)
			    {
# ifdef	USE_VARARGS
	  			sendto_prefix_one(acptr, from, pattern, vl);
# else
	  			sendto_prefix_one(acptr, from, pattern,
						  p1, p2, p3, p4,
						  p5, p6, p7, p8);
# endif
				sentalong[i] = 1;
			    }
		    }
	    }
# ifdef	USE_VARARGS
	va_end(vl);
# endif
	return;
}

/*
 * sendto_server_butone
 *
 * Send a message to all connected servers except the client 'one'.
 */
# ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_serv_butone(one, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
# else
void	sendto_serv_butone(one, pattern, va_alist)
aClient	*one;
char	*pattern;
va_dcl
{
	va_list	vl;
# endif
	Reg1	int	i;
	Reg2	aClient *cptr;
	register int j,k=0;
	fdlist send_fdlist;

# ifdef	USE_VARARGS
	va_start(vl);
# endif

	for (i=new_servfdlist.entry[j=1];j<=new_servfdlist.last_entry;
		i=new_servfdlist.entry[++j])
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef USE_VARARGS
			sendto_one(cptr, pattern, vl);
	}
	va_end(vl);
#else
		send_fdlist.entry[++k] = i;
	}
	send_fdlist.last_entry = k;
	if (k)
		sendto_fdlist(&send_fdlist,pattern,p1,p2,p3,p4,p5,p6,p7,p8);
#endif
	return;
}


#ifndef TS_ONLY

/*
 * sendto_TS_server_butone
 *
 * Send a message to all connected TS servers except the 'one', if ts==1,
 * and to all connected non-TS servers except the 'one', if ts==0.
 */
# ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_TS_serv_butone(ts, one, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
int	ts;
aClient *one;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
# else
void	sendto_TS_serv_butone(ts, one, pattern, va_alist)
int	ts;
aClient	*one;
char	*pattern;
va_dcl
{
	va_list	vl;
# endif
	Reg1	int	i;
	Reg2	aClient *cptr;
	register int j, k=0;
	fdlist send_fdlist;

#ifdef	USE_VARARGS
	va_start(vl);
#endif

	for (i=new_servfdlist.entry[j=1];j<=new_servfdlist.last_entry;
		i=new_servfdlist.entry[++j])
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
		if ((DoesTS(cptr) != 0) == (ts != 0))
#ifdef USE_VARARGS
			sendto_one(cptr, pattern, vl);
	}
	va_end(vl);
#else
			send_fdlist.entry[++k] = i;
	}
	send_fdlist.last_entry = k;
	if (k)
		sendto_fdlist(&send_fdlist,pattern,p1,p2,p3,p4,p5,p6,p7,p8);
#endif
	return;
}

#endif /* TS_ONLY */

/*
 * sendto_common_channels()
 *
 * Sends a message to all people (inclusing user) on local server who are
 * in same channel with user.
 */
# ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_common_channels(user, pattern, p1, p2, p3, p4,
				p5, p6, p7, p8)
aClient *user;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
# else
void	sendto_common_channels(user, pattern, va_alist)
aClient	*user;
char	*pattern;
va_dcl
{
	va_list	vl;
# endif
	register Link *channels;
	register Link *users;
	register aClient *cptr;
	int	sentalong[MAXCONNECTIONS];

# ifdef	USE_VARARGS
	va_start(vl);
# endif
	bzero((char *)sentalong,sizeof(sentalong));
	if (user->fd >= 0)
		sentalong[user->fd] = 1;
	if (user->user)
	for (channels=user->user->channel;channels;channels=channels->next)
		for(users=channels->value.chptr->members;users;users=users->next)
		{
			cptr = users->value.cptr;
			if (!MyConnect(cptr) || sentalong[cptr->fd])
				continue;
			sentalong[cptr->fd]++;
# ifdef	USE_VARARGS
			sendto_prefix_one(cptr, user, pattern, vl);
# else
			sendto_prefix_one(cptr, user, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8);
# endif
		}

/* maybe (cptr == user) could be taken out above and this
   part of code removed?  -- CS */

	if (MyConnect(user))
# ifdef	USE_VARARGS
		sendto_prefix_one(user, user, pattern, vl);
	va_end(vl);
# else
		sendto_prefix_one(user, user, pattern, p1, p2, p3, p4,
					p5, p6, p7, p8);
# endif
	return;
}

/*
 * sendto_channel_butserv
 *
 * Send a message to all members of a channel that are connected to this
 * server.
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_channel_butserv(chptr, from, pattern, p1, p2, p3,
			       p4, p5, p6, p7, p8)
aChannel *chptr;
aClient *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_channel_butserv(chptr, from, pattern, va_alist)
aChannel *chptr;
aClient *from;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	Reg1	Link	*lp;
	Reg2	aClient	*acptr;

#ifdef	USE_VARARGS
	for (va_start(vl), lp = chptr->members; lp; lp = lp->next)
		if (MyConnect(acptr = lp->value.cptr))
			sendto_prefix_one(acptr, from, pattern, vl);
	va_end(vl);
#else
	for (lp = chptr->members; lp; lp = lp->next)
		if (MyConnect(acptr = lp->value.cptr))
			sendto_prefix_one(acptr, from, pattern,
					  p1, p2, p3, p4,
					  p5, p6, p7, p8);
#endif

	return;
}

/*
** send a msg to all ppl on servers/hosts that match a specified mask
** (used for enhanced PRIVMSGs)
**
** addition -- Armin, 8jun90 (gruner@informatik.tu-muenchen.de)
*/

static	int	match_it(one, mask, what)
aClient *one;
char	*mask;
int	what;
{
	switch (what)
	{
	case MATCH_HOST:
		return (match(mask, one->user->host)==0);
	case MATCH_SERVER:
	default:
		return (match(mask, one->user->server)==0);
	}
}

/*
 * sendto_match_servs
 *
 * send to all servers which match the mask at the end of a channel name
 * (if there is a mask present) or to all if no mask.
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_match_servs(chptr, from, format, p1,p2,p3,p4,p5,p6,p7,p8,p9)
aChannel *chptr;
aClient	*from;
char	*format, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9;
{
#else
void	sendto_match_servs(chptr, from, format, va_alist)
aChannel *chptr;
aClient	*from;
char	*format;
va_dcl
{
	va_list	vl;
#endif
	Reg1	int	i;
	Reg2	aClient	*cptr;
	char	*mask;
	register int j,k=0;
	fdlist send_fdlist;

#ifdef	USE_VARARGS
	va_start(vl);
#endif

	if (chptr)
	{
		if (*chptr->chname == '&')
			return;
		if (mask = (char *)rindex(chptr->chname, ':'))
			mask++;
	}
	else
		mask = (char *)NULL;

	for (i=new_servfdlist.entry[j=1];j<=new_servfdlist.last_entry;
		i=new_servfdlist.entry[++j])
	{
		if (!(cptr = local[i]))
			continue;
		if (cptr == from)
			continue;
		if (!BadPtr(mask) &&
			match(mask, cptr->name))
				continue;
#ifdef	USE_VARARGS
		sendto_one(cptr, format, vl);
	}
	va_end(vl);
#else
		send_fdlist.entry[++k] = i;
	}
	send_fdlist.last_entry=k;
	if (k)
		sendto_fdlist(&send_fdlist,format,p1,p2,p3,p4,p5,p6,p7,p8,p9);
#endif
}

#ifndef TS_ONLY

/*
 * sendto_match_TS_servs
 *
 * if ts==0, send to all non-TS servers matching the mask
 * if ts==1, send to all TS servers matching the mask
 * (or to all if no mask)
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_match_TS_servs(ts, chptr, from, format, 
				p1,p2,p3,p4,p5,p6,p7,p8,p9)
int	ts;
aChannel *chptr;
aClient	*from;
char	*format, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9;
{
#else
void	sendto_match_TS_servs(ts, chptr, from, format, va_alist)
int	ts;
aChannel *chptr;
aClient	*from;
char	*format;
va_dcl
{
	va_list	vl;
#endif
	Reg1	int	i;
	Reg2	aClient	*cptr;
	char	*mask;
	register int j, k=0;
	fdlist send_fdlist;

#ifdef	USE_VARARGS
	va_start(vl);
#endif

	if (chptr)
	{
		if (*chptr->chname == '&')
			return;
		if (mask = (char *)rindex(chptr->chname, ':'))
			mask++;
	}
	else
		mask = (char *)NULL;

	for (i=new_servfdlist.entry[j=1];j<=new_servfdlist.last_entry;
		i=new_servfdlist.entry[++j])
	{
		if (!(cptr = local[i]))
			continue;
		if ((cptr == from) || (DoesTS(cptr) != 0) != (ts != 0))
			continue;
		if (!BadPtr(mask) &&
			match(mask, cptr->name))
			continue;
#ifdef	USE_VARARGS
		sendto_one(cptr, format, vl);
	}
	va_end(vl);
#else
		send_fdlist.entry[++k] = i;
	}
	send_fdlist.last_entry=k;
	if (k)
		sendto_fdlist(&send_fdlist,format,p1,p2,p3,p4,p5,p6,p7,p8,p9);
#endif
}

#endif  /* TS_ONLY */


/*
 * sendto_match_butone
 *
 * Send to all clients which match the mask in a way defined on 'what';
 * either by user hostname or user servername.
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_match_butone(one, from, mask, what, pattern,
			    p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
int	what;
char	*mask, *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_match_butone(one, from, mask, what, pattern, va_alist)
aClient *one, *from;
int	what;
char	*mask, *pattern;
va_dcl
{
	va_list	vl;
#endif
	Reg1	int	i;
	Reg2	aClient *cptr, *acptr;
  
#ifdef	USE_VARARGS
	va_start(vl);
#endif
	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]))
			continue;       /* that clients are not mine */
 		if (cptr == one)	/* must skip the origin !! */
			continue;
		if (IsServer(cptr))
		    {
			for (acptr = client; acptr; acptr = acptr->next)
				if (IsRegisteredUser(acptr)
				    && match_it(acptr, mask, what)
				    && acptr->from == cptr)
					break;
			/* a person on that server matches the mask, so we
			** send *one* msg to that server ...
			*/
			if (acptr == NULL)
				continue;
			/* ... but only if there *IS* a matching person */
		    }
		/* my client, does he match ? */
		else if (!(IsRegisteredUser(cptr) &&
			 match_it(cptr, mask, what)))
			continue;
#ifdef	USE_VARARGS
		sendto_prefix_one(cptr, from, pattern, vl);
	    }
	va_end(vl);
#else
		sendto_prefix_one(cptr, from, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8);
	    }
#endif
	return;
}

/*
 * sendto_all_butone.
 *
 * Send a message to all connections except 'one'. The basic wall type
 * message generator.
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_all_butone(one, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_all_butone(one, from, pattern, va_alist)
aClient *one, *from;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	Reg1	int	i;
	Reg2	aClient *cptr;

#ifdef	USE_VARARGS
	for (va_start(vl), i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsMe(cptr) && one != cptr)
			sendto_prefix_one(cptr, from, pattern, vl);
	va_end(vl);
#else
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsMe(cptr) && one != cptr)
			sendto_prefix_one(cptr, from, pattern,
					  p1, p2, p3, p4, p5, p6, p7, p8);
#endif

	return;
}

/*
 * sendto_flagops
 *
 *      Send to certain *local* clients.
 */
#ifndef USE_VARARGS
/*VARARGS*/
void    sendto_flagops(flag, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
int	flag;
char    *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void    sendto_flagops(flag, pattern, va_alist)
int	flag;
char    *pattern;
va_dcl
{
        va_list vl;
#endif
        Reg1    aClient *cptr;
        Reg2    int     i;
	char    nbuf[1024];
	static int nbufsize = 1024;
	int len;

#ifdef  USE_VARARGS
        va_start(vl);
#endif
	nbuf[nbufsize-1] = (char) 0;
#ifndef FK_USERMODES
	if ((flag == 3) || (flag == 4) || (flag == 6))
	{
#ifndef USE_VARARGS
		sendto_ops(pattern, p1, p2, p3, p4, p5, p6, p7, p8);
#else
		sendto_ops(pattern, va_alist);
#endif /* USE_VARARGS */
		return;
	}
#endif /* FK_USERMODES */
/* 
1 == Opers
2 == Opers and +c
3 == +k
4 = +f
5 = +b
6 = +u
7 = +d
8 = +l
*/
        for (i = 0; i <= highest_fd; i++)
                if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
			(((flag == UFLAGS_OPERS) && IsAnOper(cptr)) ||
			 ((flag == UFLAGS_CMODE) && IsAnOper(cptr) && IsCMode(cptr)) ||
			((flag == UFLAGS_KMODE) && IsKMode(cptr)) ||
			((flag == UFLAGS_FMODE) && IsFMode(cptr)) ||
			((flag == UFLAGS_BMODE) && IsBMode(cptr)) ||
			((flag == UFLAGS_UMODE) && IsUMode(cptr)) ||
			((flag == UFLAGS_DMODE) && IsDMode(cptr)) ||
#ifdef SHOW_NICKCHANGES
			((flag == UFLAGS_NMODE) && IsNMode(cptr) &&
					IsAnOper(cptr)) ||
#endif
#ifdef G_MODE
			((flag == UFLAGS_GMODE) && IsGMode(cptr) &&
					IsAnOper(cptr)) ||
#endif
			((flag == UFLAGS_LMODE) && IsLMode(cptr))))
		{
                        len = irc_sprintf(nbuf, ":%s NOTICE %s :*** Notice -- ",
                                        me.name, cptr->name);
			(void)strncat(nbuf, pattern, nbufsize - len);
#ifdef  USE_VARARGS
                        sendto_one(cptr, nbuf, va_alist);
#else
                        sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6, p7, p8);
#endif
		}
#ifdef  USE_SERVICES
                else if (cptr && IsService(cptr) &&
                         (cptr->service->wanted & SERVICE_WANT_SERVNOTE))
		{
			len = irc_sprintf(nbuf, "NOTICE %s :*** Notice -- ",
				cptr->name);
                        (void)strncat(nbuf, pattern, nbufsize - len);
# ifdef USE_VARARGS
                        sendto_one(cptr, nbuf, vl);
		}
        va_end(vl);
# else
                        sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6, p7, p8);
		}
# endif
#endif
	return;
}

/*
 * sendto_ops
 *
 *	Send to *local* ops only.
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_ops(pattern, p1, p2, p3, p4, p5, p6, p7, p8)
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_ops(pattern, va_alist)
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	Reg1	aClient *cptr;
	Reg2	int	i;
	char	nbuf[1024];
	static	int nbufsize = 1024;
	int	len;

#ifdef	USE_VARARGS
	va_start(vl);
#endif
	nbuf[nbufsize-1] = (char) 0;
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    SendServNotice(cptr))
		{
			len = irc_sprintf(nbuf, ":%s NOTICE %s :*** Notice -- ",
					me.name, cptr->name);
			(void)strncat(nbuf, pattern, nbufsize - len);
#ifdef	USE_VARARGS
			sendto_one(cptr, nbuf, va_alist);
#else
			sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6, p7, p8);
#endif
		}
#ifdef	USE_SERVICES
		else if (cptr && IsService(cptr) &&
			 (cptr->service->wanted & SERVICE_WANT_SERVNOTE))
		{
			len = irc_sprintf(nbuf, "NOTICE %s :*** Notice -- ",
					cptr->name);
			(void)strncat(nbuf, pattern, nbufsize - len);
# ifdef	USE_VARARGS
			sendto_one(cptr, nbuf, vl);
		}
	va_end(vl);
# else
			sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6, p7, p8);
		}
# endif
#endif
	return;
}

/*
** ts_warn
**	Call sendto_ops, with some flood checking (at most 5 warnings
**	every 5 seconds)
*/

#ifndef	USE_VARARGS
/*VARARGS*/
void	ts_warn(pattern, p1, p2, p3, p4, p5, p6, p7, p8)
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	ts_warn(pattern, va_alist)
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	static	ts_val last = 0;
	static	warnings = 0;
	register ts_val now;

#ifdef	USE_VARARGS
	va_start(vl);
#endif

	/*
	** if we're runnign with TS_WARNINGS enabled and someone does
	** something silly like (remotely) connecting a nonTS server,
	** we'll get a ton of warnings, so we make sure we don't send
	** more than 5 every 5 seconds.  -orabidoo
	*/
	now = time(NULL);
	if (now - last < 5)
	    {
		if (++warnings > 5)
			return;
	    }
	else
	    {
		last = now;
		warnings = 0;
	    }

#ifdef	USE_VARARGS
	sendto_ops(pattern, va_alist);
#else
	sendto_ops(pattern, p1, p2, p3, p4, p5, p6, p7, p8);
#endif
	return;
}

/*
** sendto_ops_butone
**	Send message to all operators.
** one - client not to send message to
** from- client which message is from *NEVER* NULL!!
*/
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_ops_butone(one, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_ops_butone(one, from, pattern, va_alist)
aClient *one, *from;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	Reg1	int	i;
	Reg2	aClient *cptr;
	int	sentalong[MAXCONNECTIONS];

#ifdef	USE_VARARGS
	va_start(vl);
#endif
        bzero((char *)sentalong,sizeof(sentalong));
	for (cptr = client; cptr; cptr = cptr->next)
	    {
		if (!SendWallops(cptr))
			continue;
		if (MyClient(cptr) && !(IsServer(from) || IsMe(from)))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (i < 0)
			continue;
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		sentalong[i] = 1;
# ifdef	USE_VARARGS
      		sendto_prefix_one(cptr->from, from, pattern, vl);
	    }
	va_end(vl);
# else
      		sendto_prefix_one(cptr->from, from, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8);
	    }
# endif
	return;
}

/*
** sendto_wallops_butone
**      Send message to all operators.
** one - client not to send message to
** from- client which message is from *NEVER* NULL!!
*/
#ifndef USE_VARARGS
/*VARARGS*/
void    sendto_wallops_butone(one, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
char    *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void    sendto_wallops_butone(one, from, pattern, va_alist)
aClient *one, *from;
char    *pattern;
va_dcl
{
	va_list vl;
#endif
	Reg1    int     i;
	Reg2    aClient *cptr;
	int	sentalong[MAXCONNECTIONS];

#ifdef  USE_VARARGS
	va_start(vl);
#endif
	bzero((char *)sentalong,sizeof(sentalong));
	for (cptr = client; cptr; cptr = cptr->next)
	{
		if (!IsClient(cptr) && !IsServer(cptr))
			continue;
		if (!SendWallops(cptr))
			continue;
		i = cptr->from->fd;     /* find connection oper is on */
		if (sentalong[i])       /* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;       /* ...was the one I should skip */
		sentalong[i] = 1;
# ifdef USE_VARARGS
		sendto_prefix_one(cptr->from, from, pattern, vl);
	}
	va_end(vl);
# else
		sendto_prefix_one(cptr->from, from, pattern,
			p1, p2, p3, p4, p5, p6, p7, p8);
	}
# endif
	return;
}


/*
** send_operwall -- Send Wallop to All Opers on this server
**
*/

void    send_operwall(from, message)
aClient *from;
char *message;
{
	register int i, j;
	char sender[NICKLEN+USERLEN+HOSTLEN+5];
	aClient *acptr;
	anUser *user;

	if (!from || !message)
		return;
	if (!IsPerson(from))
		return;
	user = from->user;
	(void)strcpy(sender, from->name);
	if (*user->username)
	{
		(void)strcat(sender, "!");
		(void)strcat(sender, user->username);
	}
	if (*user->host && !MyConnect(from))
	{
		(void)strcat(sender, "@");
		(void)strcat(sender, user->host);
	}
	else if (*user->host && MyConnect(from))
	{
		(void)strcat(sender, "@");
		if (IsUnixSocket(from))
			(void)strcat(sender, user->host);
		else
			(void)strcat(sender, from->sockhost);
	}
	for (i=new_operfdlist.entry[j=1];j<=new_operfdlist.last_entry;
		i=new_operfdlist.entry[++j])
	{
		if (!(acptr=local[i]))
			continue;
		if (!IsAnOper(acptr) || !IsZMode(acptr))
			continue; /* should be oper, might as well check */
		sendto_one(acptr, ":%s WALLOPS :%s", sender, message);
	}
}

/*
 * to - destination client
 * from - client which message is from
 *
 * NOTE: NEITHER OF THESE SHOULD *EVER* BE NULL!!
 * -avalon
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_prefix_one(to, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
Reg1	aClient *to;
Reg2	aClient *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_prefix_one(to, from, pattern, va_alist)
Reg1	aClient *to;
Reg2	aClient *from;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	char	sender[HOSTLEN+NICKLEN+USERLEN+5];
	Reg3	anUser	*user;
	char	*par;
	int	flag = 0;

#ifdef	USE_VARARGS
	va_start(vl);
	par = va_arg(vl, char *);
#else
	par = p1;
#endif
/*      This if() checks to make sure we don't send the same
        message back to the place we received it from.  This
        occurs when a client becomes ghosted and 2 servers
        have different information about the client.
               -- Taner & Comstud
*/
	if (from && to && !MyClient(from) &&
		IsPerson(to) && (to->from == from->from))
	{
		if (IsServer(from))
		{
			sendto_ops("Send message to %s[%s] from %s would have caused Fake Direction",
				to->name, to->from->name, from->name);
			return;
		}
		sendto_ops("Send message failed to ghosted %s[%s] from %s[%s]",
			to->name, to->from->name, from->name,
			from->from->name);
		sendto_serv_butone(NULL, ":%s KILL %s :%s (%s[%s@%s] Ghosted)",
			me.name, to->name, me.name, to->name,
			to->user->username, to->user->host);
		to->flags |= FLAGS_KILLED;
		(void)exit_client(NULL, to, &me, "Ghost");
		if (IsPerson(from))
			sendto_one(from, err_str(ERR_GHOSTEDCLIENT),
				me.name, from->name, to->name);
                return;
	} 
	if (to && from && MyClient(to) && IsPerson(from) &&
	    !mycmp(par, from->name))
	    {
		user = from->user;
		(void)strcpy(sender, from->name);
		if (user)
		    {
			if (*user->username)
			    {
				(void)strcat(sender, "!");
				(void)strcat(sender, user->username);
			    }
			if (*user->host && !MyConnect(from))
			    {
				(void)strcat(sender, "@");
				(void)strcat(sender, user->host);
				flag = 1;
			    }
		    }
		/*
		** flag is used instead of index(sender, '@') for speed and
		** also since username/nick may have had a '@' in them. -avalon
		*/
		if (!flag && MyConnect(from) && *user->host)
		    {
			(void)strcat(sender, "@");
			if (IsUnixSocket(from))
				(void)strcat(sender, user->host);
			else
				(void)strcat(sender, from->sockhost);
		    }
#ifdef	USE_VARARGS
		par = sender;
	    }
	sendto_one(to, pattern, par, vl);
	va_end(vl);
#else
		par = sender;
	    }
	sendto_one(to, pattern, par, p2, p3, p4, p5, p6, p7, p8);
#endif
}

void sendto_fdlist(listp,formp,p1,p2,p3,p4,p5,p6,p7,p8,p9)
fdlist  *listp;
char    *formp;
char *p1,*p2,*p3,*p4,*p5,*p6,*p7,*p8,*p9;
{
	register int len,j,i,fd;
	char	sendbuf[2048];

	len = sendprep(sendbuf,formp,p1,p2,p3,p4,p5,p6,p7,p8,p9);

	for(fd=listp->entry[j=1]; j<= listp->last_entry ; fd=listp->entry[++j])
		send_message(local[fd],sendbuf,len);
} 

static  int     sendprep(sendbuf, pattern,
			p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
char	*sendbuf;
char    *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
{
        int     len;

	Debug((DEBUG_L10, "sendprep(%s)", pattern));
	len = irc_sprintf(sendbuf, pattern, p1, p2, p3, p4, p5, p6,
		p7, p8, p9, p10, p11);
	if (len > 510)
#ifdef  IRCII_KLUDGE
		len = 511;
#else
		len = 510;
	sendbuf[len++] = '\r';
#endif
	sendbuf[len++] = '\n';
	sendbuf[len] = '\0';
	return len;
}

