/************************************************************************
 *   IRC - Internet Relay Chat, include/struct.h
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

/*
**  $Id: struct.h,v 1.2 1997/07/24 05:13:34 cbehrens Exp $
*/

#ifndef	__struct_include__
#define __struct_include__

#include "config.h"
#include "setup.h"

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef USE_SYSLOG
# include <syslog.h>
# ifdef HAVE_SYS_SYSLOG_H
#  include <sys/syslog.h>
# endif
#endif

#include "hash.h"
#include "fdlist.h"

#include "zlib.h"

typedef	struct	ConfItem aConfItem;
typedef	struct 	Client	aClient;
typedef	struct	Channel	aChannel;
typedef	struct	User	anUser;
typedef	struct	Server	aServer;
typedef	struct	SLink	Link;
typedef	struct	SMode	Mode;
typedef	struct	CPass	ClearPass;
#ifdef KEEP_OPS
typedef	struct	kept_ops	KeptOps;
typedef	struct	id_kept_ops	IdKeptOps;
#endif
#ifdef ZIP_LINKS
typedef	struct	Zdata	aZdata;
#endif

typedef	long	ts_val;

typedef struct	IdleItem anIdle;
typedef struct  CloneItem aClone;
typedef struct	MotdItem aMotd;

typedef struct Whowas aWhowas;

#ifndef VMSP
#include "class.h"
#include "dbuf.h"	/* THIS REALLY SHOULDN'T BE HERE!!! --msa */
#endif

#ifdef MAJORLOGGING
typedef struct commandlog
{
	char line[520];
	time_t	thetime;
	struct commandlog *next;
} aCommandlog;

#endif



/* These set of defines are for sendto_flagops */
#define UFLAGS_OPERS 1
#define UFLAGS_CMODE 2
#define UFLAGS_KMODE 3
#define UFLAGS_FMODE 4
#define UFLAGS_BMODE 5
#define UFLAGS_UMODE 6
#define UFLAGS_DMODE 7
#define UFLAGS_LMODE 8
#ifdef G_MODE
#define UFLAGS_GMODE 9
#endif
#define UFLAGS_NMODE 10
/* */


#define	HOSTLEN		63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

#define	NICKLEN		9	/* Necessary to put 9 here instead of 10
				** if s_msg.c/m_nick has been corrected.
				** This preserves compatibility with old
				** servers --msa
				*/
#define	USERLEN		10
#define	REALLEN	 	50
#define	TOPICLEN	80
#define	CHANNELLEN	200
#define	PASSWDLEN 	20	/* connection password, not channel */
#define	KEYLEN		23
#define	CHPASSLEN	23
#define IDLEN		12	/* this is the maximum length, not the actual
				   generated length; DO NOT CHANGE! */
#define	CHPASSHASHLEN	12	/* same comment...  DO NOT CHANGE! */

#define COOKIELEN	IDLEN
#define	BUFSIZE		512		/* WARNING: *DONT* CHANGE THIS!!!! */
#define	MAXRECIPIENTS 	20
#define	MAXBANS		20	/* bans + exceptions together */
#define	MAXBANLENGTH	1024

#define	USERHOST_REPLYLEN	(NICKLEN+HOSTLEN+USERLEN+5)

#define	READBUF_SIZE	16384	/* used in s_bsd *AND* s_zip.c ! */

#ifdef USE_SERVICES
#include "service.h"
#endif

/*
** 'offsetof' is defined in ANSI-C. The following definition
** is not absolutely portable (I have been told), but so far
** it has worked on all machines I have needed it. The type
** should be size_t but...  --msa
*/

/* added the ifndef linux  -- Comstud
*/

#ifndef linux
# ifndef offsetof
# define offsetof(t,m) (int)((&((t *)0L)->m))
# endif
#endif

#define	elementsof(x) (sizeof(x)/sizeof(x[0]))

/*
** flags for bootup options (command line flags)
*/
#define	BOOT_CONSOLE	1
#define	BOOT_QUICK	2
#define	BOOT_DEBUG	4
#define	BOOT_INETD	8
#define	BOOT_TTY	16
#define	BOOT_OPER	32
#define	BOOT_AUTODIE	64

#define	STAT_LOG	-6	/* logfile for -x */
#define	STAT_MASTER	-5	/* Local ircd master before identification */
#define	STAT_CONNECTING	-4
#define	STAT_HANDSHAKE	-3
#define	STAT_ME		-2
#define	STAT_UNKNOWN	-1
#define	STAT_SERVER	0
#define	STAT_CLIENT	1
#define	STAT_SERVICE	2	/* Services not implemented yet */

/*
 * status macros.
 */
#define	IsRegisteredUser(x)	((x)->status == STAT_CLIENT)
#define	IsRegistered(x)		((x)->status >= STAT_SERVER)
#define	IsConnecting(x)		((x)->status == STAT_CONNECTING)
#define	IsHandshake(x)		((x)->status == STAT_HANDSHAKE)
#define	IsMe(x)			((x)->status == STAT_ME)
#define	IsUnknown(x)		((x)->status == STAT_UNKNOWN || \
				 (x)->status == STAT_MASTER)
#define	IsServer(x)		((x)->status == STAT_SERVER)
#define	IsClient(x)		((x)->status == STAT_CLIENT)
#define	IsLog(x)		((x)->status == STAT_LOG)
#define	IsService(x)		((x)->status == STAT_SERVICE)

#define UserHasID(x)		((x)->user && (x)->user->id[0] == '.')

#define	SetMaster(x)		((x)->status = STAT_MASTER)
#define	SetConnecting(x)	((x)->status = STAT_CONNECTING)
#define	SetHandshake(x)		((x)->status = STAT_HANDSHAKE)
#define	SetMe(x)		((x)->status = STAT_ME)
#define	SetUnknown(x)		((x)->status = STAT_UNKNOWN)
#define	SetServer(x)		((x)->status = STAT_SERVER)
#define	SetClient(x)		((x)->status = STAT_CLIENT)
#define	SetLog(x)		((x)->status = STAT_LOG)
#define	SetService(x)		((x)->status = STAT_SERVICE)

/* aConfItem->flags  */

#define FLAGS_LIMIT_IP		0x0001
#define FLAGS_NO_TILDE		0x0002
#define FLAGS_NEED_IDENTD	0x0004
#define FLAGS_PASS_IDENTD	0x0008
#define FLAGS_NOMATCH_IP	0x0010

/* aClient->flags */

#define	FLAGS_PINGSENT   0x0001	/* Unreplied ping sent */
#define	FLAGS_DEADSOCKET 0x0002	/* Local socket is dead--Exiting soon */
#define	FLAGS_KILLED     0x0004	/* Prevents "QUIT" from being sent for this */
#define FLAGS_NORMALEX	 0x0008 /* Normal exit */
#define	FLAGS_BLOCKED    0x0010	/* socket is in a blocked condition */
#define	FLAGS_UNIX	 0x0020	/* socket is in the unix domain, not inet */
#define	FLAGS_CLOSING    0x0040	/* set when closing to suppress errors */
#define	FLAGS_LISTEN     0x0080 /* used to mark clients which we listen() on */
#define	FLAGS_CHKACCESS  0x0100 /* ok to check clients access if set */
#define	FLAGS_DOINGDNS	 0x0200 /* client is waiting for a DNS response */
#define	FLAGS_AUTH	 0x0400 /* client is waiting on rfc931 response */
#define	FLAGS_WRAUTH	 0x0800	/* set if we havent writen to ident server */
#define	FLAGS_LOCAL	 0x1000 /* set for local clients */
#define	FLAGS_GOTID	 0x2000	/* successful ident lookup achieved */
#define	FLAGS_NONL	 0x4000 /* No \n in buffer */
#define FLAGS_NKILLED  0x100000 /* killed by a server;  never set on a client
                                   that isn't mine */
#define FLAGS_ZIP      0x200000 /* link is zipped */
#define FLAGS_ZIPFIRST 0x400000 /* start of zip (ignore any CRLF) */
#define FLAGS_CBURST   0x800000 /* set to mark connection burst being sent */
#define FLAGS_NKILL   0x1000000 /* do recover this client's kill even
                                   if exit_client fired from &me */

/* aClient->extraflags */

#define	FLAGS_OPER		0x0001	/* Operator */
#define	FLAGS_LOCOP		0x0002 /* Local operator -- SRB */
#define	FLAGS_INVISIBLE		0x0004 /* makes user invisible */
#define	FLAGS_WALLOP		0x0008 /* send wallops to them */
#define	FLAGS_SERVNOTICE	0x0010 /* server notices such as kill */
#define FLAGS_FMODE		0x0020 /* +f usermode */
#define FLAGS_CMODE		0x0040 /* +c usermode */
#define FLAGS_KMODE		0x0080 /* +k usermode */
#define FLAGS_UMODE		0x0100 /* +u usermode */
#define FLAGS_BMODE		0x0200 /* +b usermode */
#define FLAGS_DMODE		0x0400 /* +d usermode */
#define FLAGS_LMODE		0x0800 /* +l usermode */
#define FLAGS_ZMODE		0x1000 /* +z usermode */
#define FLAGS_GMODE		0x2000 /* +g usermode */
#define FLAGS_NMODE		0x4000 /* +n usermode */

#define	SEND_UMODES	(FLAGS_INVISIBLE|FLAGS_OPER|FLAGS_WALLOP)


/* ARGH...THIS IS UGLY...I'M SURE THERES SOMETHING EASIER,
   BUT MY BRAIN CANT THINK RIGHT NOW -CS
*/

#ifdef G_MODE
# ifdef SHOW_NICKCHANGES
# define	ALL_UMODES	(SEND_UMODES|FLAGS_SERVNOTICE|FLAGS_CMODE|FLAGS_KMODE|FLAGS_FMODE|FLAGS_UMODE|FLAGS_LMODE|FLAGS_DMODE|FLAGS_BMODE|FLAGS_ZMODE|FLAGS_GMODE|FLAGS_NMODE)
# else
# define	ALL_UMODES	(SEND_UMODES|FLAGS_SERVNOTICE|FLAGS_CMODE|FLAGS_KMODE|FLAGS_FMODE|FLAGS_UMODE|FLAGS_LMODE|FLAGS_DMODE|FLAGS_BMODE|FLAGS_ZMODE|FLAGS_GMODE)
#endif /* SHOW_NICKCHANGES */
#else
# ifdef SHOW_NICKCHANGES
# define	ALL_UMODES	(SEND_UMODES|FLAGS_SERVNOTICE|FLAGS_CMODE|FLAGS_KMODE|FLAGS_FMODE|FLAGS_UMODE|FLAGS_LMODE|FLAGS_DMODE|FLAGS_BMODE|FLAGS_ZMODE|FLAGS_NMODE)
# else
# define	ALL_UMODES	(SEND_UMODES|FLAGS_SERVNOTICE|FLAGS_CMODE|FLAGS_KMODE|FLAGS_FMODE|FLAGS_UMODE|FLAGS_LMODE|FLAGS_DMODE|FLAGS_BMODE|FLAGS_ZMODE)
# endif
#endif /* G_MODE */

/*
 * aClient->extraflags macros.
 */
#define	IsOper(x)		((x)->extraflags & FLAGS_OPER)
#define	IsLocOp(x)		((x)->extraflags & FLAGS_LOCOP)
#define	IsInvisible(x)		((x)->extraflags & FLAGS_INVISIBLE)
#define	IsAnOper(x)		((x)->extraflags & (FLAGS_OPER|FLAGS_LOCOP))
#define IsFMode(x)		((x)->extraflags & FLAGS_FMODE)
#define IsCMode(x)		((x)->extraflags & FLAGS_CMODE)
#define IsUMode(x)		((x)->extraflags & FLAGS_UMODE)
#define IsKMode(x)		((x)->extraflags & FLAGS_KMODE)
#define IsLMode(x)		((x)->extraflags & FLAGS_LMODE)
#define IsBMode(x)		((x)->extraflags & FLAGS_BMODE)
#define IsDMode(x)		((x)->extraflags & FLAGS_DMODE)
#define IsGMode(x)		((x)->extraflags & FLAGS_GMODE)
#define IsNMode(x)		((x)->extraflags & FLAGS_NMODE)
#define IsZMode(x)		((x)->extraflags & FLAGS_ZMODE)

#define	SendWallops(x)		((x)->extraflags & FLAGS_WALLOP)
#define	SendServNotice(x)	((x)->extraflags & FLAGS_SERVNOTICE)

#define	SetOper(x)		((x)->extraflags |= FLAGS_OPER)
#define	SetLocOp(x)    		((x)->extraflags |= FLAGS_LOCOP)
#define	SetInvisible(x)		((x)->extraflags |= FLAGS_INVISIBLE)
#define	SetWallops(x)  		((x)->extraflags |= FLAGS_WALLOP)

#define	ClearOper(x)		((x)->extraflags &= ~FLAGS_OPER)
#define ClearLocOp(x)		((x)->extraflags &= ~FLAGS_LOCOP)
#define	ClearInvisible(x)	((x)->extraflags &= ~FLAGS_INVISIBLE)
#define	ClearWallops(x)		((x)->extraflags &= ~FLAGS_WALLOP)

/*
 * aClient->flags macros
*/

#define	IsPerson(x)		((x)->user && IsClient(x))
#define	IsPrivileged(x)		(IsAnOper(x) || IsServer(x))
#define	IsUnixSocket(x)		((x)->flags & FLAGS_UNIX)
#define	IsListening(x)		((x)->flags & FLAGS_LISTEN)
#define	DoAccess(x)		((x)->flags & FLAGS_CHKACCESS)
#define	IsLocal(x)		((x)->flags & FLAGS_LOCAL)
#define	IsDead(x)		((x)->flags & FLAGS_DEADSOCKET)
#define	CBurst(x)		((x)->flags & FLAGS_CBURST)

#define	SetUnixSock(x)		((x)->flags |= FLAGS_UNIX)
#define	SetDNS(x)		((x)->flags |= FLAGS_DOINGDNS)
#define	DoingDNS(x)		((x)->flags & FLAGS_DOINGDNS)
#define	SetAccess(x)		((x)->flags |= FLAGS_CHKACCESS)
#define	DoingAuth(x)		((x)->flags & FLAGS_AUTH)
#define	NoNewLine(x)		((x)->flags & FLAGS_NONL)

#define	ClearDNS(x)		((x)->flags &= ~FLAGS_DOINGDNS)
#define	ClearAuth(x)		((x)->flags &= ~FLAGS_AUTH)
#define	ClearAccess(x)		((x)->flags &= ~FLAGS_CHKACCESS)

/*
 * defined debugging levels
 */
#define	DEBUG_FATAL  0
#define	DEBUG_ERROR  1	/* report_error() and other errors that are found */
#define	DEBUG_NOTICE 3
#define	DEBUG_DNS    4	/* used by all DNS related routines - a *lot* */
#define	DEBUG_INFO   5	/* general usful info */
#define	DEBUG_NUM    6	/* numerics */
#define	DEBUG_SEND   7	/* everything that is sent out */
#define	DEBUG_DEBUG  8	/* anything to do with debugging, ie unimportant :) */
#define	DEBUG_MALLOC 9	/* malloc/free calls */
#define	DEBUG_L10   10	/* debug level 10 */

/*
 * defines for curses in client
 */
#define	DUMMY_TERM	0
#define	CURSES_TERM	1
#define	TERMCAP_TERM	2

#ifdef G_LINES
typedef	struct _glineconf
{
	time_t	seconds; /* length of ban */
	struct _rule *fromoper;
	struct _rule *fromserver;
	struct _rule *foruser;
} aGlineConf;
#endif

/* aRule flags */
#define FLAGS_ALLOW             0x001
#define FLAGS_DENY              0x002

#define IsAllow(x)              ((x)->flags & FLAGS_ALLOW)
#define IsDeny(x)               ((x)->flags & FLAGS_DENY)

typedef struct _rule
{
        char *string;
        int flags;
        struct _except *excepts;
        struct _rule *next;
} aRule;

typedef struct _except
{
        char *string;
        struct _except *next;
} anExcept;

struct	IdleItem	{
	char	username[USERLEN+1];
	struct	in_addr	ip;
	long	last;
	struct IdleItem *prev;
	struct IdleItem *next;
};

struct  CloneItem       {
	struct	in_addr	ip;
        int     num;
        long    last;
        struct CloneItem *prev;
        struct CloneItem *next;
};

struct	MotdItem	{
	char	line[82];
	struct	MotdItem *next;
};

struct Whowas
{
	int hashv;
	char *name;
	char *username;
	char *hostname;
	char *servername;
	char *realname;
	char *away;
	time_t logoff;
	struct Client *online; /* Pointer to new nickname for chasing or NULL */
	struct Whowas *next;  /* for hash table... */
	struct Whowas *prev;  /* for hash table... */
	struct Whowas *cnext; /* for client struct linked list */
	struct Whowas *cprev; /* for client struct linked list */
};

struct	ConfItem	{
	unsigned int	status;	/* If CONF_ILLEGAL, delete when no clients */
	int	clients;	/* Number of *LOCAL* clients using this */
	struct	in_addr ipnum;	/* ip number of host field */
	char	*host;
	char	*passwd;
	char	*name;
	int	port;
	int	flags;  /* Special flags... */
	time_t	hold;	/* Hold action until this time (calendar time) */
	time_t	expires; /* Expire time (glines */
#ifndef VMSP
	aClass	*class;  /* Class of connection */
#endif
	struct	ConfItem *next;
};

#define	CONF_ILLEGAL		0x80000000
#define	CONF_MATCH		0x40000000
#define	CONF_QUARANTINED_SERVER	0x000001
#define	CONF_CLIENT		0x000002
#define	CONF_CONNECT_SERVER	0x000004
#define	CONF_NOCONNECT_SERVER	0x000008
#define	CONF_NZCONNECT_SERVER	0x000010
#define	CONF_LOCOP		0x000020
#define	CONF_OPERATOR		0x000040
#define	CONF_ME			0x000080
#define	CONF_KILL		0x000100
#define	CONF_ADMIN		0x000200
#ifdef 	R_LINES
#define	CONF_RESTRICT		0x000400
#endif
#define	CONF_CLASS		0x000800
#define	CONF_SERVICE		0x001000
#define	CONF_LEAF		0x002000
#define	CONF_LISTEN_PORT	0x004000
#define	CONF_HUB		0x008000
#define CONF_BOT_IGNORE		0x010000
#define CONF_ELINE		0x020000
#define CONF_DLINE		0x040000
#define CONF_JLINE		0x080000
#define CONF_GLINE		0x100000

#define	CONF_OPS		(CONF_OPERATOR | CONF_LOCOP)
#define	CONF_SERVER_MASK	(CONF_CONNECT_SERVER | CONF_NOCONNECT_SERVER |\
				 CONF_NZCONNECT_SERVER)
#define	CONF_CLIENT_MASK	(CONF_CLIENT | CONF_SERVICE | CONF_OPS | \
				 CONF_SERVER_MASK)

#define	IsIllegal(x)	((x)->status & CONF_ILLEGAL)

#ifdef  ZIP_LINKS

/* the minimum amount of data needed to trigger compression */
#define ZIP_MINIMUM     4096

/* the maximum amount of data to be compressed (can actually be a bit more) */
#define ZIP_MAXIMUM     8192    /* WARNING: *DON'T* CHANGE THIS!!!! */

struct Zdata {
        z_stream        *in;            /* input zip stream data */
        z_stream        *out;           /* output zip stream data */
        char            inbuf[ZIP_MAXIMUM]; /* incoming zipped buffer */
        char            outbuf[ZIP_MAXIMUM]; /* outgoing (unzipped) buffer */
        int             incount;        /* size of inbuf content */
        int             outcount;       /* size of outbuf content */
};

#endif /* ZIP_LINKS */


/*
 * Client structures
 */
struct	User	{
	struct	User	*nextu;
	Link	*channel;	/* chain of channel pointer blocks */
	Link	*invited;	/* chain of invite pointer blocks */
	char	*away;		/* pointer to away message */
	time_t	last;
	time_t  last2;		/* for new priority stuff... */
	int	refcnt;		/* Number of times this block is referenced */
	int	joined;		/* number of channels joined */
	char	username[USERLEN+1];
	char	host[HOSTLEN+1];
	char	id[IDLEN+1];	/* ID if available, nick otherwise */
        char	server[HOSTLEN+1];
				/*
				** In a perfect world the 'server' name
				** should not be needed, a pointer to the
				** client describing the server is enough.
				** Unfortunately, in reality, server may
				** not yet be in links while USER is
				** introduced... --msa
				*/
#ifdef	LIST_DEBUG
	aClient	*bcptr;
#endif
};

struct	Server	{
	struct	Server	*nexts;
	anUser	*user;		/* who activated this connection */
	char	up[HOSTLEN+1];	/* uplink for this server */
	char	by[NICKLEN+1];
	aConfItem *nline;	/* N-line pointer for this server */
	aClient	*servers;	/* Servers on this server */
	aClient *users;		/* Users on this server */
#ifdef	LIST_DEBUG
	aClient	*bcptr;
#endif
};

struct Client
{
	struct	Client *next;	/* Next client */
	struct	Client *prev;	/* Previous client */
	struct	Client *hnext;	/* Next client in this hash bucket */
	struct	Client *idhnext;/* Next client in this id hash bucket */
	struct	Client *lnext;	/* Used for Server->servers/users */
	struct	Client *lprev;	/* Used for Server->servers/users */

	anUser	*user;		/* ...defined, if this is a User */
	aServer	*serv;		/* ...defined, if this is a server */
	aClient	*servptr;	/* Points to server this Client is on */
	aWhowas *whowas;	/* Pointers to whowas structs */
#ifdef USE_SERVICES
	aService *service;
#endif
	int	hashv;		/* raw hash value */
	time_t	lasttime;	/* ...should be only LOCAL clients? --msa */
	time_t	firsttime;	/* time client was created */
	time_t	since;		/* last time we parsed something */
	ts_val	tsinfo;		/* TS on the nick, SVINFO on servers */
	unsigned long flags;	/* client flags */
	unsigned long extraflags;	/* client extra flags */
	aClient	*from;		/* == self, if Local Client, *NEVER* NULL! */
	int	fd;		/* >= 0, for local clients */
	int	hopcount;	/* number of servers to this 0 = local */
	int	ping;		/* max ping...holder for get_client_ping() */
	short	status;		/* Client type */
	char	nicksent;
	char	name[HOSTLEN+1]; /* Unique name of the client, nick or host */
	char	username[USERLEN+1]; /* username here now for auth stuff */
	char	info[REALLEN+1]; /* Free form additional client information */
	/*
	** The following fields are allocated only for local clients
	** (directly connected to *this* server with a socket.
	** The first of them *MUST* be the "count"--it is the field
	** to which the allocation is tied to! *Never* refer to
	** these fields, if (from != self).
	*/
	int	count;		/* Amount of data in buffer */
	fdlist	*fdlist;
	char	buffer[BUFSIZE]; /* Incoming message buffer */
#ifdef	ZIP_LINKS
	aZdata	*zip;		/* zip data */
#endif
	short	lastsq;		/* # of 2k blocks when sendqueued called last*/
	dbuf	sendQ;		/* Outgoing message queue--if socket full */
	dbuf	recvQ;		/* Hold for data incoming yet to be parsed */
	long	sendM;		/* Statistics: protocol messages send */
	long	sendK;		/* Statistics: total k-bytes send */
	long	receiveM;	/* Statistics: protocol messages received */
#ifdef DOG3
	long lastrecvM;		/* to check for activity --Mika */
#endif 
#ifdef NO_NICK_FLOODS
	long	lastnick;
	int	numnicks;
#endif
	long	receiveK;	/* Statistics: total k-bytes received */
	u_short	sendB;		/* counters to count upto 1-k lots of bytes */
	u_short	receiveB;	/* sent and received. */
	aClient	*acpt;		/* listening client which we accepted from */
	Link	*confs;		/* Configuration record associated */
	int	authfd;		/* fd for rfc931 authentication */
	struct	in_addr	bindip;	/* for listeners */
	struct	in_addr	ip;	/* keep real ip# too */
	unsigned short	port;	/* and the remote port# too :-) */
	struct	hostent	*hostp;
#ifdef	pyr
	struct	timeval	lw;
#endif
	char	sockhost[HOSTLEN+1]; /* This is the host name from the socket
				  ** and after which the connection was
				  ** accepted.
				  */
	char	ipstr[HOSTLEN+1];
	char	passwd[PASSWDLEN+1];
        char    oldnick[NICKLEN+1];  /* This is for local NKILLed clients */
	int	caps;		/* capabilities bit-field */
#ifdef KEEP_OPS
        KeptOps *ko;            /* list of kept ops */
        char    cookie[COOKIELEN+1];
#endif
};

#define	CLIENT_LOCAL_SIZE sizeof(aClient)
#define	CLIENT_REMOTE_SIZE offsetof(aClient,count)

#ifdef KEEP_OPS
struct  kept_ops {
        struct  kept_ops *next, *prev;
        char    *chname;
        ts_val  ts;
        ts_val  expires;
	int	flags;
};

struct  id_kept_ops {
        char    cookie[COOKIELEN+1];
        ts_val  expires;
        struct  id_kept_ops *next, *prev;
        struct  kept_ops *first;
};

#define MAXKEPTCHANS    10
#endif /* KEEP_OPS */

/*
 * statistics structures
 */
struct	stats {
	unsigned int	is_cl;	/* number of client connections */
	unsigned int	is_sv;	/* number of server connections */
	unsigned int	is_ni;	/* connection but no idea who it was */
	unsigned short	is_cbs;	/* bytes sent to clients */
	unsigned short	is_cbr;	/* bytes received to clients */
	unsigned short	is_sbs;	/* bytes sent to servers */
	unsigned short	is_sbr;	/* bytes received to servers */
	unsigned long	is_cks;	/* k-bytes sent to clients */
	unsigned long	is_ckr;	/* k-bytes received to clients */
	unsigned long	is_sks;	/* k-bytes sent to servers */
	unsigned long	is_skr;	/* k-bytes received to servers */
	time_t 		is_cti;	/* time spent connected by clients */
	time_t		is_sti;	/* time spent connected by servers */
	unsigned int	is_ac;	/* connections accepted */
	unsigned int	is_ref;	/* accepts refused */
	unsigned int	is_unco; /* unknown commands */
	unsigned int	is_wrdi; /* command going in wrong direction */
	unsigned int	is_unpf; /* unknown prefix */
	unsigned int	is_empt; /* empty message */
	unsigned int	is_num;	/* numeric message */
	unsigned int	is_kill; /* number of kills generated on collisions */
	unsigned int	is_fake; /* MODE 'fakes' */
	unsigned int	is_asuc; /* successful auth requests */
	unsigned int	is_abad; /* bad auth requests */
	unsigned int	is_udp;	/* packets recv'd on udp port */
	unsigned int	is_loc;	/* local connections made */
};

/* mode structure for channels */

struct	SMode	{
	unsigned int	mode;
	int	limit;

	char	key[KEYLEN+1];
	char	pass[CHPASSHASHLEN+1];
	ClearPass	*clear;
};

/* Clear passwords for channels */

struct	CPass	{
	ts_val	expires;
	char	pass[1];		/* variable size */
};

/* Message table structure */

struct	Message	{
	char	*cmd;
	int	(* func)();
	unsigned int	count;
	int	parameters;
	char	flags;
		/* bit 0 set means that this command is allowed to be used
		 * only on the average of once per 2 seconds -SRB */
	unsigned long	bytes;
};

/* general link structure used for chains */

struct	SLink	{
	struct	SLink	*next;
	union {
		aClient	*cptr;
		aChannel *chptr;
		aConfItem *aconf;
		char	*cp;
#ifdef BAN_INFO
                struct {
                  char *banstr;
                  char *who;
                  time_t when;
                } ban;
#endif
	} value;
	int	flags;
};

/* this should eliminate a lot of ifdef's in the main code... -orabidoo */
#ifdef BAN_INFO
#  define BANSTR(l)  ((l)->value.ban.banstr)
#else
#  define BANSTR(l)  ((l)->value.cp)
#endif

/* channel structure */

struct Channel	{
	struct	Channel *nextch, *prevch, *hnextch;
	int	hashv;		/* raw hash value */
	Mode	mode;
	char	topic[TOPICLEN+1];
#ifdef TOPIC_INFO
	char	topic_nick[NICKLEN+1];
	time_t	topic_time; 
#endif
	int	users;
	Link	*members;
	Link	*invites;
	Link	*banlist;
	Link	*exceptlist;
	ts_val	channelts;
	ts_val	passwd_ts;
	ts_val	opless_ts;
	char	chname[1];
};

#define TS_CURRENT      4       /* current TS protocol version */

#ifdef TS4_ONLY
#define TS_MIN          4       /* minimum supported TS protocol version */
#else
#define TS_MIN          3
#endif

#define CAP_TS          0x00000001
#define CAP_TS4         0x00000002
#define CAP_ZIP         0x00000004

#define DoesTS(x)       ((x)->caps & (CAP_TS|CAP_TS4))
/* we can't #define DoesTS to 1, it's used to check that the server sent
** the "TS" extra arg to "PASS" (or "CAPAB TS4" or both).
*/

#ifdef TS4_ONLY
#define DoesTS4(x)      1
#else
#define DoesTS4(x)      ((x)->caps & CAP_TS4)
#endif

typedef int (*testcap_t)(aClient *, int);
#define SEND_DEFAULT    (testcap_t)0

struct  Capability      {
        char    *name;
        int     cap;
        int     wehaveit;
        int     required;
        testcap_t  sendit;      /* SEND_DEFAULT means: send it if we have it */
};

/*
 * Capability macros.
 */
#define IsCapable(x, cap)       ((x)->caps & (cap))
#define SetCapable(x, cap)      ((x)->caps |= (cap))
#define ClearCap(x, cap)        ((x)->caps &= ~(cap))

extern  int     test_send_zipcap PROTO((aClient *, int));

/*
** list of recognized server capabilities.  "TS" is not on the list
** because all servers that we talk to already do TS, and the kludged
** extra argument to "PASS" takes care of checking that.  -orabidoo
*/

#ifdef CAPTAB
struct Capability captab[] = {
/*  name        cap           have   required  send function */
#ifdef  TS4_ONLY
  { "TS4",      CAP_TS4,        1,      1,      SEND_DEFAULT },
#else
  { "TS4",      CAP_TS4,        1,      0,      SEND_DEFAULT },
#endif
#ifdef  ZIP_LINKS
  { "ZIP",      CAP_ZIP,        1,      0,      test_send_zipcap },
#else
  { "ZIP",      CAP_ZIP,        0,      0,      SEND_DEFAULT },
#endif
  { (char*)0,   0,              0,      0,      SEND_DEFAULT }
};
#else
extern  struct Capability captab[];
#endif

#define MIN_NOW         (880000000)     /* minimal acceptable time() value */
#define MAX_NOW         (2147000000)    /* max acceptable time() value */

/*
** Channel Related macros follow
*/

#undef DEBUGGING_TIMES
#ifdef DEBUGGING_TIMES

#define	CHPASS_MIN_TS (300)
#define	CHPASS_EXPIRE (300)
#define	CLEANUP_DELAY (60)
#define	CLEARPASS_EXPIRE (300)
#define	CLEARCHAN_EXPIRE (120)
#define	TS_WARN_OVERRIDE (600)
#define KEPT_OPS_EXPIRE 900
#define KEEP_OPS_MIN_TS 10

#else
#define	CHPASS_MIN_TS	(48*3600)    /* no +c on channels less than 48h old */
#define	CHPASS_EXPIRE	(48*3600)    /* expire +c after 48 hours opless */
#define	CLEANUP_DELAY	(5*60)	     /* check expired +c's every 5 mins */
#define	CLEARPASS_EXPIRE (15*60)     /* forget clear password after 15 mins */
#define	CLEARCHAN_EXPIRE (15*60)     /* expire +z after 15 mins */
#define	TS_WARN_OVERRIDE (4*24*3600) /* warn of a possible hack when a TS 
				      * older than 4 days gets overriden */
#define KEPT_OPS_EXPIRE 900          /* kept ops lists expire after 15 mins */
#define KEEP_OPS_MIN_TS 3600         /* no keptops on chans less than 1h old */

#endif

/* Channel related flags */

#define	CHFL_CHANOP     0x0001 /* Channel operator */
#define	CHFL_VOICE      0x0002 /* the power to speak */
#define	CHFL_DEOPPED	0x0004 /* deopped by us, modes need to be ignored */
#define	CHFL_HALFOP	0x0008 /* channel half-operator */
#define	CHFL_BAN	0x0010 /* ban channel flag */
#define	CHFL_EXCEPTION	0x0020 /* exception to ban channel flag */

/* Channel Visibility macros */

#define	MODE_CHANOP	CHFL_CHANOP
#define	MODE_VOICE	CHFL_VOICE
#define	MODE_DEOPPED	CHFL_DEOPPED
#define	MODE_HALFOP	CHFL_HALFOP
#define	MODE_PRIVATE	0x00010
#define	MODE_SECRET	0x00020
#define	MODE_MODERATED  0x00040
#define	MODE_TOPICLIMIT 0x00080
#define	MODE_INVITEONLY 0x00100
#define	MODE_NOPRIVMSGS 0x00200
#define MODE_CHPASSWD   0x00400 /* not actually used !*/
#define	MODE_KEY	0x00800
#define	MODE_BAN	0x01000
#define MODE_EXCEPTION  0x02000 /* not actually used !*/
#define	MODE_LIMIT	0x04000
#define	MODE_NOPLUSC	0x08000
#define	MODE_ZAP	0x10000
#define	MODE_FLAGS	0x1ffff

#define	MODE_WPARAS	(MODE_CHANOP|MODE_VOICE|MODE_BAN|MODE_KEY|MODE_LIMIT)
/*
 * Undefined here, these are used in conjunction with the above modes in
 * the source.
#define MODE_QUERY      0x10000000
#define MODE_ADD        0x40000000
#define MODE_DEL        0x20000000
 */

#define IsLimitIp(x)		((x)->flags & FLAGS_LIMIT_IP)
#define IsNoTilde(x)		((x)->flags & FLAGS_NO_TILDE)
#define IsNeedIdentd(x)		((x)->flags & FLAGS_NEED_IDENTD)
#define IsPassIdentd(x)		((x)->flags & FLAGS_PASS_IDENTD)
#define IsNoMatchIp(x)		((x)->flags & FLAGS_NOMATCH_IP)

#define	HoldChannel(x)		(!(x))
/* zapped channels are automatically invisible */
#define SecretMask      (MODE_SECRET|MODE_ZAP)
/* name invisible */
#define	SecretChannel(x)	((x) && ((x)->mode.mode & SecretMask))
/* channel not shown but names are */
#define	HiddenChannel(x)	((x) && ((x)->mode.mode & MODE_PRIVATE))
/* channel visible */
#define	ShowChannel(v,c)	(PubChannel(c) || IsMember((v),(c)))
#define	PubChannel(x)		((!x) || ((x)->mode.mode &\
				 (MODE_PRIVATE | SecretMask)) == 0)

/* channel cleared by opers, joinable by opers only */
#ifdef CLEAR_CHAN
#define ZappedChannel(x)        ((x) && ((x)->mode.mode & MODE_ZAP))
#else
#define ZappedChannel(x)        0
#endif

/* either of +o or +h resets the opless counter */
#define OpsMask (CHFL_CHANOP|CHFL_HALFOP)

#ifdef THIS_ONE_IS_MUCH_BETTER
#define	IsMember(user,chan) (find_user_link((chan)->members,user) ? 1 : 0)
#endif
#define	IsMember(blah,chan) ((blah && blah->user && \
		find_channel_link((blah->user)->channel, chan)) ? 1 : 0)

#ifdef PLUS_CHANNELS
#define	IsChannelName(name) ((name) && (*(name) == '#' || *(name) == '&' || \
							 *(name) == '+' ))
#else
#define IsChannelName(name) ((name) && (*(name) == '#' || *(name) == '&'))
#endif

/* Misc macros */

#define	BadPtr(x) (!(x) || (*(x) == '\0'))

#define	isvalid(c) (((c) >= 'A' && (c) <= '~') || isdigit(c) || (c) == '-')

#define	MyConnect(x)			((x)->fd >= 0)
#define	MyClient(x)			(MyConnect(x) && IsClient(x))
#define	MyOper(x)			(MyConnect(x) && IsOper(x))

#ifdef TS4_ONLY
#define ID(x)			(((x)->user && *(x)->user->id == '.') ? \
				    (x)->user->id : (x)->name)
#define IDORNICK(x)		(MyConnect((x)) ? (x)->name : ID(x))
#else
#define ID(x)			((x)->name)
#define IDORNICK(x)		((x)->name)
#endif

/* String manipulation macros */

/* strncopynt --> strncpyzt to avoid confusion, sematics changed
   N must be now the number of bytes in the array --msa */
#define	strncpyzt(x, y, N) do{(void)strncpy(x,y,N);x[N-1]='\0';}while(0)
#define	StrEq(x,y)	(!strcmp((x),(y)))

/* used in SetMode() in channel.c and m_umode() in s_msg.c */

#define	MODE_NULL	0
#define	MODE_QUERY	0x10000000
#define	MODE_ADD	0x40000000
#define	MODE_DEL	0x20000000

/* return values for hunt_server() */

#define	HUNTED_NOSUCH	(-1)	/* if the hunted server is not found */
#define	HUNTED_ISME	0	/* if this server should execute the command */
#define	HUNTED_PASS	1	/* if message passed onwards successfully */

/* used when sending to #mask or $mask */

#define	MATCH_SERVER  1
#define	MATCH_HOST    2

/* used for async dns values */

#define	ASYNC_NONE	(-1)
#define	ASYNC_CLIENT	0
#define	ASYNC_CONNECT	1
#define	ASYNC_CONF	2
#define	ASYNC_SERVER	3

/* misc variable externs */

extern	char	*version, *infotext[];
extern	char	*generation, *creation;

#ifdef RELIABLE_TIME
#define	timedelta	0
#else
extern	ts_val	timedelta;
#endif

/* misc defines */

#define	FLUSH_BUFFER	-2
#define GO_ON		-3	/* for m_nick/m_client's benefit */
#define	UTMP		"/etc/utmp"
#define	COMMA		","

#ifdef ORATIMING
/* Timing stuff (for performance measurements): compile with -DORATIMING
   and put a TMRESET where you want the counter of time spent set to 0,
   a TMPRINT where you want the accumulated results, and TMYES/TMNO pairs
   around the parts you want timed -orabidoo
*/
extern struct timeval tsdnow, tsdthen;
extern unsigned long tsdms;
#define TMRESET tsdms=0;
#define TMYES gettimeofday(&tsdthen, NULL);
#define TMNO gettimeofday(&tsdnow, NULL); if (tsdnow.tv_sec!=tsdthen.tv_sec) tsdms+=1000000*(tsdnow.tv_sec-tsdthen.tv_sec); tsdms+=tsdnow.tv_usec; tsdms-=tsdthen.tv_usec;
#define TMPRINT sendto_ops("Time spent: %ld ms", tsdms);
#else
#define TMRESET
#define TMYES
#define TMNO
#define TMPRINT
#endif

/* IRC client structures */

#ifdef	CLIENT_COMPILE
typedef	struct	Ignore {
	char	user[NICKLEN+1];
	char	from[USERLEN+HOSTLEN+2];
	int	flags;
	struct	Ignore	*next;
} anIgnore;

#define	IGNORE_PRIVATE	1
#define	IGNORE_PUBLIC	2
#define	IGNORE_TOTAL	3

#define	HEADERLEN	200

#endif

#ifndef IRC_CSR31PL1_TS4_BETA_2_2
#error "Please use (and edit) the config.h that came with this server."
#endif

#endif /* __struct_include__ */
