/************************************************************************
 *   IRC - Internet Relay Chat, include/msg.h
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
**  $Id: msg.h,v 1.2 1997/07/24 05:16:29 cbehrens Exp $
*/

#ifndef	__msg_include__
#define __msg_include__

#include "comstud.h"

#define MSG_PRIVATE  "PRIVMSG"	/* PRIV */
#define MSG_WHO      "WHO"	/* WHO  -> WHOC */
#define MSG_WHOIS    "WHOIS"	/* WHOI */
#define MSG_WHOWAS   "WHOWAS"	/* WHOW */
#define MSG_USER     "USER"	/* USER */
#define MSG_NICK     "NICK"	/* NICK */
#define MSG_SERVER   "SERVER"	/* SERV */
#define MSG_LIST     "LIST"	/* LIST */
#define MSG_TOPIC    "TOPIC"	/* TOPI */
#define MSG_INVITE   "INVITE"	/* INVI */
#define MSG_VERSION  "VERSION"	/* VERS */
#define MSG_QUIT     "QUIT"	/* QUIT */
#define MSG_SQUIT    "SQUIT"	/* SQUI */
#define MSG_KILL     "KILL"	/* KILL */
#define MSG_INFO     "INFO"	/* INFO */
#define MSG_LINKS    "LINKS"	/* LINK */
#define MSG_STATS    "STATS"	/* STAT */
#define MSG_HELP     "HELP"	/* HELP */
#define MSG_ERROR    "ERROR"	/* ERRO */
#define MSG_AWAY     "AWAY"	/* AWAY */
#define MSG_CONNECT  "CONNECT"	/* CONN */
#define MSG_PING     "PING"	/* PING */
#define MSG_PONG     "PONG"	/* PONG */
#define MSG_OPER     "OPER"	/* OPER */
#define MSG_PASS     "PASS"	/* PASS */
#define MSG_WALLOPS  "WALLOPS"	/* WALL */
#define MSG_TIME     "TIME"	/* TIME */
#define MSG_NAMES    "NAMES"	/* NAME */
#define MSG_ADMIN    "ADMIN"	/* ADMI */
#define MSG_TRACE    "TRACE"	/* TRAC */
#define MSG_NOTICE   "NOTICE"	/* NOTI */
#define MSG_JOIN     "JOIN"	/* JOIN */
#define MSG_PART     "PART"	/* PART */
#define MSG_LUSERS   "LUSERS"	/* LUSE */
#define MSG_MOTD     "MOTD"	/* MOTD */
#define MSG_MODE     "MODE"	/* MODE */
#define MSG_KICK     "KICK"	/* KICK */
#define MSG_SERVICE  "SERVICE"	/* SERV -> SRVI */
#define MSG_USERHOST "USERHOST"	/* USER -> USRH */
#define MSG_ISON     "ISON"	/* ISON */
#define MSG_NOTE     "NOTE"	/* NOTE */
#define MSG_SQUERY   "SQUERY"	/* SQUE */
#define MSG_SERVLIST "SERVLIST"	/* SERV -> SLIS */
#define MSG_SERVSET  "SERVSET"	/* SERV -> SSET */
#define	MSG_REHASH   "REHASH"	/* REHA */
#define	MSG_RESTART  "RESTART"	/* REST */
#define	MSG_CLOSE    "CLOSE"	/* CLOS */
#define	MSG_SVINFO   "SVINFO"	/* SVINFO */
#define	MSG_SJOIN    "SJOIN"	/* SJOIN */
#ifdef	TSDEBUG
#define	MSG_TS	     "TS"	/* TS */
#endif
#define	MSG_DIE	     "DIE"
#define	MSG_HASH     "HSAH"	/* HASH */
#define	MSG_DNS      "DNS"	/* DNS  -> DNSS */
#define MSG_GLINE    "GLINE"    /* GLINE */
#define MSG_EXT      "EXT"      /* EXT */
#define MSG_OPERWALL "OPERWALL" /* OPERWALL */
#ifndef PUT_KLINES_IN_IRCD_CONF
#ifdef SEPARATE_QUOTE_KLINES_BY_DATE
#define MSG_REHASHADD "REHASHADD" /* REHASHADD */
#define MSG_ADDKLINES "ADDKLINES" /* ADDKLINES */
#endif /* SEPARATE...*/
#endif /* PUT... */
#ifdef QUOTE_KLINE
#define MSG_KLINE    "KLINE"    /* KLINE */
#endif
#ifdef IDLE_CHECK
#define MSG_IDLE     "IDLE"     /* IDLE */
#endif
#ifdef G_LINES
#define MSG_GLINELT "GLINELIFETIME"
#define MSG_CLEARGL "CLEARGLINES"
#endif

#ifdef DOG3
#define MSG_HTM	"HTM"
#define MSG_DOG3FREQ "DOG3FREQ"
#define MSG_DOG3LOAD "DOG3LOAD"
#endif

#define MAXPARA    15 

#ifdef DOG3
extern int m_htm(), m_dog3freq(), m_dog3load();
#endif

#ifdef G_LINES
extern int m_clearglines();
extern int m_glinelifetime();
#endif
#ifdef QUOTE_KLINE
extern int m_kline();
#endif
#ifdef IDLE_CHECK
extern int m_idle();
#endif
extern int m_gline(), m_operwall(), m_ext();
extern int m_private(), m_topic(), m_join(), m_part(), m_mode();
extern int m_ping(), m_pong(), m_wallops(), m_kick();
extern int m_nick(), m_error(), m_notice();
extern int m_invite(), m_quit(), m_kill();
#ifndef CLIENT_COMPILE
extern int m_motd(), m_who(), m_whois(), m_user(), m_list();
extern int m_server(), m_info(), m_links(), m_stats();
extern int m_version(), m_help();
extern int m_squit(), m_away(), m_connect();
extern int m_oper(), m_pass(), m_trace();
extern int m_time(), m_names(), m_admin();
extern int m_lusers(), m_umode(), m_note(), m_close();
extern int m_motd(), m_whowas();
extern int m_service(), m_userhost(), m_ison(), m_svinfo(), m_sjoin();
extern int m_service(), m_servset(), m_servlist(), m_squery();
#ifdef TSDEBUG
extern int m_ts();
#endif
#ifndef PUT_KLINES_IN_IRCD_CONF
#ifdef SEPARATE_QUOTE_KLINES_BY_DATE 
extern  int	m_addklines();
#endif
#endif
#if defined(OPER_REHASH) || defined(LOCOP_REHASH)
extern	int	m_rehash();
extern	int	m_rehashadd();
#endif
#if defined(OPER_RESTART) || defined(LOCOP_RESTART)
extern	int	m_restart();
#endif
#if defined(OPER_DIE) || defined(LOCOP_DIE)
extern	int	m_die();
#endif
extern int m_hash(), m_dns();
#endif /* !CLIENT_COMPILE */

#ifdef MSGTAB
struct Message msgtab[] = {
	{ MSG_PRIVATE,	m_private,	0, MAXPARA, 1, 0L },
	{ MSG_NICK,	m_nick,		0, MAXPARA, 1, 0L },
	{ MSG_NOTICE,	m_notice,	0, MAXPARA, 1, 0L },
	{ MSG_JOIN,	m_join,		0, MAXPARA, 1, 0L },
	{ MSG_MODE,	m_mode,		0, MAXPARA, 1, 0L },
	{ MSG_QUIT,	m_quit,		0, MAXPARA, 1, 0L },
	{ MSG_PART,	m_part,		0, MAXPARA, 1, 0L },
	{ MSG_TOPIC,	m_topic,	0, MAXPARA, 1, 0L },
	{ MSG_INVITE,	m_invite,	0, MAXPARA, 1, 0L },
	{ MSG_KICK,	m_kick,		0, MAXPARA, 1, 0L },
	{ MSG_WALLOPS,	m_wallops,	0, MAXPARA, 1, 0L },
	{ MSG_PING,	m_ping,		0, MAXPARA, 1, 0L },
	{ MSG_PONG,	m_pong,		0, MAXPARA, 1, 0L },
	{ MSG_ERROR,	m_error,	0, MAXPARA, 1, 0L },
	{ MSG_KILL,	m_kill,		0, MAXPARA, 1, 0L },
	{ MSG_USER,	m_user,		0, MAXPARA, 1, 0L },
	{ MSG_AWAY,	m_away,		0, MAXPARA, 1, 0L },
	{ MSG_ISON,	m_ison,		0, 1,       1, 0L },
	{ MSG_SERVER,	m_server,	0, MAXPARA, 1, 0L },
	{ MSG_SQUIT,	m_squit,	0, MAXPARA, 1, 0L },
	{ MSG_WHOIS,	m_whois,	0, MAXPARA, 1, 0L },
	{ MSG_WHO,	m_who,		0, MAXPARA, 1, 0L },
	{ MSG_WHOWAS,	m_whowas,	0, MAXPARA, 1, 0L },
	{ MSG_LIST,	m_list,		0, MAXPARA, 1, 0L },
	{ MSG_NAMES,	m_names,	0, MAXPARA, 1, 0L },
	{ MSG_USERHOST,	m_userhost,	0, 1,       1, 0L },
	{ MSG_TRACE,	m_trace,	0, MAXPARA, 1, 0L },
	{ MSG_PASS,	m_pass,		0, MAXPARA, 1, 0L },
	{ MSG_LUSERS,	m_lusers,	0, MAXPARA, 1, 0L },
	{ MSG_TIME,	m_time,		0, MAXPARA, 1, 0L },
	{ MSG_OPER,	m_oper,		0, MAXPARA, 1, 0L },
	{ MSG_CONNECT,	m_connect,	0, MAXPARA, 1, 0L },
	{ MSG_VERSION,	m_version,	0, MAXPARA, 1, 0L },
	{ MSG_STATS,	m_stats,	0, MAXPARA, 1, 0L },
	{ MSG_LINKS,	m_links,	0, MAXPARA, 1, 0L },
	{ MSG_ADMIN,	m_admin,	0, MAXPARA, 1, 0L },
	{ MSG_HELP,	m_help,		0, MAXPARA, 1, 0L },
	{ MSG_INFO,	m_info,		0, MAXPARA, 1, 0L },
	{ MSG_MOTD,	m_motd,		0, MAXPARA, 1, 0L },
	{ MSG_CLOSE,	m_close,	0, MAXPARA, 1, 0L },
	{ MSG_SVINFO,	m_svinfo,	0, MAXPARA, 1, 0L },
	{ MSG_SJOIN,	m_sjoin,	0, MAXPARA, 1, 0L },
#ifdef DOG3
	{ MSG_HTM,	m_htm,		0, MAXPARA, 1, 0L },
	{ MSG_DOG3LOAD,	m_dog3load,	0, MAXPARA, 1, 0L },
	{ MSG_DOG3FREQ, m_dog3freq,	0, MAXPARA, 1, 0L },
#endif
	{ MSG_GLINE,	m_gline,	0, MAXPARA, 1, 0L },
	{ MSG_EXT,	m_ext,		0, MAXPARA, 1, 0L },
	{ MSG_OPERWALL,	m_operwall,	0, MAXPARA, 1, 0L },
#ifdef TSDEBUG
	{ MSG_TS,	m_ts,		0, MAXPARA, 1, 0L },
#endif
#ifdef QUOTE_KLINE
	{ MSG_KLINE,	m_kline,	0, MAXPARA, 1, 0L },
#endif
#ifdef G_LINES
	{ MSG_GLINELT,	m_glinelifetime,0, MAXPARA, 1, 0L },
	{ MSG_CLEARGL,	m_clearglines,	0, MAXPARA, 1, 0L },
#endif
#ifdef IDLE_CHECK
	{ MSG_IDLE,	m_idle,		0, MAXPARA, 1, 0L },
#endif
#undef USE_SERVICES
#ifdef USE_SERVICES
	{ MSG_SERVICE,	m_service,	0, MAXPARA, 1, 0L },
	{ MSG_SERVSET,	m_servset,	0, MAXPARA, 1, 0L },
	{ MSG_SQUERY,	m_squery,	0, MAXPARA, 1, 0L },
	{ MSG_SERVLIST,	m_servlist,	0, MAXPARA, 1, 0L },
#endif
	{ MSG_HASH,	m_hash,		0, MAXPARA, 1, 0L },
	{ MSG_DNS,	m_dns,		0, MAXPARA, 1, 0L },
#if defined(OPER_REHASH) || defined(LOCOP_REHASH)
	{ MSG_REHASH,	m_rehash,	0, MAXPARA, 1, 0L },
#ifndef PUT_KLINES_IN_IRCD_CONF
#ifdef SEPARATE_QUOTE_KLINES_BY_DATE
	{ MSG_REHASHADD,m_rehashadd,	0, MAXPARA,1,0L},
#endif
#endif
#endif
#ifndef PUT_KLINES_IN_IRCD_CONF
#ifdef SEPARATE_QUOTE_KLINES_BY_DATE 
	{ MSG_ADDKLINES,m_addklines,	0, MAXPARA, 1, 0L },
#endif
#endif
#if defined(OPER_RESTART) || defined(LOCOP_RESTART)
	{ MSG_RESTART,	m_restart,	0, MAXPARA, 1, 0L },
#endif
#if defined(OPER_DIE) || defined(LOCOP_DIE)
	{ MSG_DIE,	m_die,		0, MAXPARA, 1, 0L },
#endif
	{ (char *) 0, (int (*)()) 0 , 0, 0, 0, 0L}
};
#else
extern struct Message msgtab[];
#endif
#endif /* __msg_include__ */
