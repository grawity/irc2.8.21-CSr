/************************************************************************
 *   IRC - Internet Relay Chat, ircd/ircd.c
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

#ifndef lint
static  char rcsid[] = "@(#)$Id: ircd.c,v 1.2 1997/07/29 19:49:02 cbehrens Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "h.h"
#include "dich_conf.h"

#ifdef SHOW_HEADERS
int     R_do_dns, R_fin_dns, R_fin_dnsc, R_fail_dns,
		R_do_id, R_fin_id, R_fail_id;
#endif

aDichConf	*Ilines = NULL; /* I lines */
aDichConf	*Klines = NULL; /* Kill lines */
aDichConf	*Blines = NULL; /* Ban passing lines */
aDichConf	*Dlines = NULL; /* Deny on accept() lines */
aDichConf	*Elines = NULL; /* Exceptions to Kill lines */
aDichConf	*Glines = NULL; /* Global kill lines */
aDichConf	*Jlines = NULL; /* Unused */

#ifdef G_LINES
	aGlineConf GlineConf;
#endif

#ifdef BETTER_MOTD
	aMotd *motd;
	struct tm motd_tm;
#endif

int	s_count = 1;    /* All servers */
int	c_count = 0;    /* All clients */
int	ch_count = 0;   /* All channels */
int	i_count = 0;    /* All invisible users */
int	o_count = 0;    /* All operators */
int	m_clients = 0;  /* My clients */
int	m_servers = 0;  /* My servers */
int	m_invis = 0;    /* My invisible users */

#include "fdlist.h"

int lifesux = 0;

fdlist new_fdlists[32];
fdlist new_servfdlist;
fdlist new_operfdlist;
fdlist new_miscfdlist;

time_t check_fdlists();
void send_high_sendq();

#ifdef DOG3
time_t	lastrecvtime = 0;
int	dog3loadrecv = DEFAULT_LOADRECV;
int	dog3loadcfreq = DEFAULT_LOADCFREQ;
long	lastrecvK = 0;
float	currentrate = 0;
#endif


#ifdef CLONE_CHECK
	aClone *Clones = NULL;
	char clonekillhost[100];
#endif

#ifdef IDLE_CHECK
	anIdle *Idles = NULL;
	int idlelimit = DEFAULT_IDLELIMIT;
#endif

time_t	NOW;
aClient me;			/* That's me */
aClient *client = &me;		/* Pointer to beginning of Client list */
int	rehashed = 1;

void	server_reboot();
void	restart PROTO((char *));
static	void	open_debugfile(), setup_signals();

char	**myargv;
int	portnum = -1;		    /* Server port number, listening this */
char	*configfile = CONFIGFILE;	/* Server configuration file */
#ifndef PUT_KLINES_IN_IRCD_CONF
char	*klinefile = KLINEFILE;	        /* Config file for klines */
#endif
int	debuglevel = -1;		/* Server debug level */
int	bootopt = 0;			/* Server boot option flags */
char	*debugmode = "";		/*  -"-    -"-   -"-  */
char	*sbrk0;				/* initial sbrk(0) */
static	int	dorehash = 0;
static	char	*dpath = DPATH;

time_t	nextconnect = 1;	/* time for next try_connections call */
time_t	nextping = 1;		/* same as above for check_pings() */
time_t	nextdnscheck = 0;	/* next time to poll dns to force timeouts */
time_t  nextpasscleanup = 0;    /* next time to cleanup passwds on empty #'s */
time_t	nextexpire = 1;	/* next expire run on the dns cache */

#ifndef RELIABLE_TIME
/*
** number of seconds to add to all readings of time() when making TS's
** -orabidoo
*/
ts_val  timedelta = 0;
#endif

#ifdef	PROFIL
extern	etext();

RETSIGTYPE s_monitor()
{
	static	int	mon = 0;
#ifdef	POSIX_SIGNALS
	struct	sigaction act;
#endif

	(void)moncontrol(mon);
	mon = 1 - mon;
#ifdef	POSIX_SIGNALS
	act.sa_handler = s_rehash;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGUSR1);
	(void)sigaction(SIGUSR1, &act, NULL);
#else
	(void)signal(SIGUSR1, s_monitor);
#endif
}
#endif

RETSIGTYPE s_die()
{
#ifdef	USE_SYSLOG
	(void)syslog(LOG_CRIT, "Server Killed By SIGTERM");
#endif
	flush_connections(me.fd);
	exit(-1);
}

static RETSIGTYPE s_rehash()
{
#ifdef	POSIX_SIGNALS
	struct	sigaction act;
#endif
	dorehash = 1;
#ifdef	POSIX_SIGNALS
	act.sa_handler = s_rehash;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGHUP);
	(void)sigaction(SIGHUP, &act, NULL);
#else
	(void)signal(SIGHUP, s_rehash);	/* sysV -argv */
#endif
}

void	restart(mesg)
char	*mesg;
{
#ifdef	USE_SYSLOG
	(void)syslog(LOG_WARNING, "Restarting Server because: %s",mesg);
#endif
	server_reboot();
}

RETSIGTYPE s_restart()
{
	static int restarting = 0;

#ifdef	USE_SYSLOG
	(void)syslog(LOG_WARNING, "Server Restarting on SIGINT");
#endif
	if (restarting == 0)
	    {
		/* Send (or attempt to) a dying scream to oper if present */

		restarting = 1;
		server_reboot();
	    }
}

void	server_reboot()
{
	Reg1	int	i;

	sendto_ops("Aieeeee!!!  Restarting server...");
	Debug((DEBUG_NOTICE,"Restarting server..."));
	flush_connections(me.fd);
	/*
	** fd 0 must be 'preserved' if either the -d or -i options have
	** been passed to us before restarting.
	*/
#ifdef USE_SYSLOG
	(void)closelog();
#endif
	for (i = 3; i < MAXCONNECTIONS; i++)
		(void)close(i);
	if (!(bootopt & (BOOT_TTY|BOOT_DEBUG)))
		(void)close(2);
	(void)close(1);
	if ((bootopt & BOOT_CONSOLE) || isatty(0))
		(void)close(0);
	if (!(bootopt & (BOOT_INETD|BOOT_OPER)))
		(void)execv(MYNAME, myargv);
#ifdef USE_SYSLOG
	/* Have to reopen since it has been closed above */

	openlog(myargv[0], LOG_PID|LOG_NDELAY, LOG_FACILITY);
	syslog(LOG_CRIT, "execv(%s,%s) failed: %m\n", MYNAME, myargv[0]);
	closelog();
#endif
	Debug((DEBUG_FATAL,"Couldn't restart server: %s", strerror(errno)));
	exit(-1);
}


/*
** try_connections
**
**	Scan through configuration and try new connections.
**	Returns the calendar time when the next call to this
**	function should be made latest. (No harm done if this
**	is called earlier or later...)
*/
static	time_t	try_connections(currenttime)
time_t	currenttime;
{
	Reg1	aConfItem *aconf;
	Reg2	aClient *cptr;
	aConfItem **pconf;
	int	connecting, confrq;
	time_t	next = 0;
	aClass	*cltmp;
	aConfItem *con_conf;
	int	con_class = 0;

	connecting = FALSE;
	Debug((DEBUG_NOTICE,"Connection check at   : %s",
		myctime(currenttime)));
	for (aconf = conf; aconf; aconf = aconf->next )
	    {
		/* Also when already connecting! (update holdtimes) --SRB */
		if (!(aconf->status & 
			(CONF_CONNECT_SERVER|CONF_NZCONNECT_SERVER)) || 
		    aconf->port <= 0)
			continue;
		cltmp = Class(aconf);
		/*
		** Skip this entry if the use of it is still on hold until
		** future. Otherwise handle this entry (and set it on hold
		** until next time). Will reset only hold times, if already
		** made one successfull connection... [this algorithm is
		** a bit fuzzy... -- msa >;) ]
		*/

		if ((aconf->hold > currenttime))
		    {
			if ((next > aconf->hold) || (next == 0))
				next = aconf->hold;
			continue;
		    }

		confrq = get_con_freq(cltmp);
		aconf->hold = currenttime + confrq;
		/*
		** Found a CONNECT config with port specified, scan clients
		** and see if this server is already connected?
		*/
		cptr = find_name(aconf->name, (aClient *)NULL);

		if (!cptr && (Links(cltmp) < MaxLinks(cltmp)) &&
		    (!connecting || (Class(cltmp) > con_class)))
		    {
			con_class = Class(cltmp);
			con_conf = aconf;
			/* We connect only one at time... */
			connecting = TRUE;
		    }
		if ((next > aconf->hold) || (next == 0))
			next = aconf->hold;
	    }
	if (connecting)
	    {
		if (con_conf->next)  /* are we already last? */
		    {
			for (pconf = &conf; (aconf = *pconf);
			     pconf = &(aconf->next))
				/* put the current one at the end and
				 * make sure we try all connections
				 */
				if (aconf == con_conf)
					*pconf = aconf->next;
			(*pconf = con_conf)->next = 0;
		    }
		if (connect_server(con_conf, (aClient *)NULL,
				   (struct hostent *)NULL) == 0)
			sendto_ops("Connection to %s[%s] activated.",
				   con_conf->name, con_conf->host);
	    }
	Debug((DEBUG_NOTICE,"Next connection check : %s", myctime(next)));
	return (next);
}

/* These can be removed when we know static lusers works */
extern int s_ct, c_ct, i_ct, o_ct, m_cs, m_ss, m_is; 

static	time_t	check_pings(currenttime)
time_t	currenttime;
{
	Reg1	aClient	*cptr;
	int	ping = 0, i, checkit = 0;
	time_t	oldest = 0, timeout;
	static time_t  lastcheck = 0;
	register int reg;
#ifdef IDLE_CHECK
	register int checkit2 = 0;
	static	time_t	lastidlecheck = 0;
#endif

#ifdef IDLE_CHECK
	if (currenttime-lastidlecheck > 120)
	{
		update_idles();
		lastidlecheck = currenttime;
		checkit2 = 1;
	}
#endif /* IDLE_CHECK */
	if (rehashed || (currenttime-lastcheck > KLINE_CHECK))
	{
		int ch_ct;

		lastcheck = currenttime;
		checkit = 1;
		rehashed = 0;
/* Can be removed when we know static lusers works */
		compute_lusers(NULL);
		ch_ct = count_channels();
		if (ch_ct != ch_count)
			sendto_ops("Channel count updated, was off by %i",
				ch_count-ch_ct);
		if (s_ct != s_count)
			sendto_ops("Server count updated, was off by %i",
				s_count-s_ct);
		if (c_ct != c_count)
			sendto_ops("Client count updated, was off by %i",
				c_count-c_ct);
		if (i_ct != i_count)
			sendto_ops("Invis count updated, was off by %i",
				i_count-i_ct);
		if (o_ct != o_ct)
			sendto_ops("Oper count updated, was off by %i",
				o_count-o_ct);
		if (m_cs != m_clients)
			sendto_ops("My clients updated, was off by %i",
				m_clients-m_cs);
		if (m_ss != m_servers)
			sendto_ops("My servers updated, was off by %i",
				m_servers-m_ss);
		ch_count = ch_ct;
		s_count = s_ct;
		c_count = c_ct;
		i_count = i_ct;
		o_count = o_ct;
		m_clients = m_cs;
		m_servers = m_ss;
		m_invis = m_is;
	}
	for (i = 0; i <= highest_fd; i++)
	{
		if (!(cptr = local[i]) || IsMe(cptr) || IsLog(cptr))
			continue;

		/*
		** Note: No need to notify opers here. It's
		** already done when "FLAGS_DEADSOCKET" is set.
		*/
		if (cptr->flags & FLAGS_DEADSOCKET)
		{
			(void)exit_client(cptr, cptr, cptr, "Dead socket");
			i=0;
			continue;
		}
		if ((checkit
#ifdef IDLE_CHECK
			|| checkit2
#endif
			) && IsPerson(cptr)
#ifdef E_LINES
				&& !find_eline(cptr->sockhost, cptr->ipstr,
					cptr->user->username))
#else
			)
#endif
		{
			char *reason;
#ifdef IDLE_CHECK
			if (checkit2 && idlelimit && !IsAnOper(cptr) &&
				(currenttime - cptr->user->last > idlelimit))
		{ /* indenting wrong */
			if (!find_idle(cptr))
			{
				anIdle *crap;
				char *temp = cptr->user->username;

				crap = make_idle();
				if (*temp == '~')
					temp++;
				strcpy(crap->username, temp);
				crap->ip = cptr->ip;
			}
			sendto_ops("Idle time limit exceeded for %s",
				get_client_name(cptr, FALSE));
			(void)exit_client(cptr, cptr, cptr,
				"Idle time limit exceeded");
			i=0;
			continue;
		} /* if */
#endif
			if (find_kline(cptr, cptr->sockhost, cptr->ipstr,
				cptr->user->username, 0, 0, &reason))
			{
                        /*
                         * this is used for KILL lines with time restrictions
                         * on them - send a messgae to the user being killed
                         * first.
                         */
				sendto_ops("Kill line active for %s",
					get_client_name(cptr, FALSE));
				(void)exit_client(cptr, cptr, &me, reason);
				i=0;
				continue;
			}
#if defined(R_LINES) && defined(R_LINES_OFTEN)
			if (find_restrict(cptr))
			{
                                sendto_ops("Restricting %s, closing link.",
                                           get_client_name(cptr,FALSE));
				(void)exit_client(cptr, cptr, &me, "R-Lined");
				i=0;
				continue;
			}
#endif
		}
		reg = IsRegistered(cptr);
		if (reg)
		{
                /*
                 * Ok, so goto's are ugly and can be avoided here but this code
                 * is already indented enough so I think its justified. -avalon
                 */
			ping = get_client_ping(cptr);
			if (ping >= (currenttime - cptr->lasttime))
				goto ping_timeout;
		}
		else
			ping = CONNECTTIMEOUT;

		/*
		 * If the server hasnt talked to us in 2*ping seconds
		 * and it has a ping time, then close its connection.
		 * If the client is a user and a KILL line was found
		 * to be active, close this connection too.
		 */
		if ( (((currenttime - cptr->lasttime) >= ping*2) &&
			(cptr->flags & FLAGS_PINGSENT)) ||
			((IsUnknown(cptr) || !reg) &&
			((currenttime - cptr->firsttime) >= ping)) )
		{
			if (!reg || (cptr->flags & FLAGS_PINGSENT))
			{
				if (!reg && (DoingDNS(cptr) || DoingAuth(cptr)))
				{
					if (cptr->authfd >= 0)
					{
						(void)close(cptr->authfd);
						cptr->authfd = -1;
						cptr->count = 0;
						*cptr->buffer = '\0';
					}
#ifdef SHOW_HEADERS
					if (DoingDNS(cptr))
						sendheader(cptr, REPORT_FAIL_DNS1,
							R_fail_dns);
					else
						sendheader(cptr, REPORT_FAIL_ID,
							R_fail_id);
#endif
					Debug((DEBUG_NOTICE,
						"DNS/AUTH timeout %s",
						get_client_name(cptr,TRUE)));
					del_queries((char *)cptr);
					ClearAuth(cptr);
					ClearDNS(cptr);
					SetAccess(cptr);
					cptr->firsttime = currenttime;
					cptr->lasttime = currenttime;
					continue;
				}
			if (IsServer(cptr) || IsConnecting(cptr) ||
			    IsHandshake(cptr))
				sendto_ops("No response from %s, closing link",
					   get_client_name(cptr, FALSE));
{
			char buffer[512];

			sprintf(buffer, "Ping timeout: %i seconds",
				currenttime - cptr->lasttime);
			(void)exit_client(cptr, cptr, cptr, buffer);
}
			i = 0;
			continue;
			}
		}
		else if (reg && !(cptr->flags & FLAGS_PINGSENT))
		{
			/*
			 * if we havent PINGed the connection and we havent
			 * heard from it in a while, PING it to make sure
			 * it is still alive.
			 */
			cptr->flags |= FLAGS_PINGSENT;
			/* not nice but does the job */
			cptr->lasttime = currenttime - ping;
			sendto_one(cptr, "PING :%s", me.name);
		}
		if (IsClient(cptr) || IsServer(cptr))
		{
			if (cptr->servptr == NULL)
			sendto_flagops(UFLAGS_OPERS, "Possible ghost: %s!%s@%s[%s] no servptr",
				cptr->name,
				cptr->user ? cptr->user->username : "<NULL>",
				cptr->user ? cptr->user->host : "<NULL>",
				cptr->user ? cptr->user->server : "<NULL>");
			else
			{
				/* just lame debug to make sure
					servptr is valid */
				if (*cptr->servptr->name)
					;
				if (cptr->servptr->from == NULL)
					;
			}
		}
ping_timeout:
	;
#ifdef blahblahblah
		timeout = cptr->lasttime + ping;
		while (timeout <= currenttime)
			timeout += ping;
		if (timeout < oldest || !oldest)
			oldest = timeout;
#endif
#if defined(CLONE_CHECK) && defined(KILL_CLONES)
        if (*clonekillhost)
        {
                if (cptr->user && cptr->user->host &&
                        !mycmp(cptr->user->host, clonekillhost) &&
                        ((currenttime - cptr->firsttime) < CLONE_TIME))
                {
                        ircstp->is_kill++;
                        sendto_ops("Clonebot killed: %s [%s@%s]",
                           cptr->name, cptr->user->username,
                                cptr->user->host);
                        (void)exit_client(cptr, cptr, &me, "CloneBot");
                        i = 0;
                        continue;
                }
        }
#endif /* CLONE_CHECK */
	    } /* must end the for loop */
#if defined(CLONE_CHECK) && defined(KILL_CLONES)
        *clonekillhost = (char) 0;
#endif
#ifdef blahblahblah
	if (!oldest || oldest < currenttime)
		oldest = currenttime + PINGFREQUENCY;
	Debug((DEBUG_NOTICE,"Next check_ping() call at: %s, %d %d %d",
		myctime(oldest), ping, oldest, currenttime));
	return (oldest);
#else
	return currenttime+PINGFREQUENCY;
#endif
}

/*
** bad_command
**	This is called when the commandline is not acceptable.
**	Give error message and exit without starting anything.
*/
static	int	bad_command()
{
  (void)printf(
	 "Usage: ircd %s[-h servername] [-p portnumber] [-x loglevel] [-t]\n",
#ifdef CMDLINE_CONFIG
	 "[-f config] "
#else
	 ""
#endif
	 );
  (void)printf("Server not started\n\n");
  return (-1);
}

void init_me()
{
	me.port = portnum;
        me.flags = FLAGS_LISTEN;
        if (bootopt & BOOT_INETD)
        {
                me.fd = 0;
                local[0] = &me;
                me.flags = FLAGS_LISTEN;
        }
        else
                me.fd = -1;
}
 
int	main(argc, argv)
int	argc;
char	*argv[];
{
	int	i;
	int	portarg = 0;
	uid_t	uid, euid;
	fdlist	tempfdlist;
#if 0
	time_t	delay = 0;
#endif
	time_t	curdiff, lastdiff;
	time_t  lastflush, lastmisc;
#ifdef RLIMIT_NOFILE
	struct rlimit r;
#endif
	time_t nextfdlistcheck=0;
	NOW = time(NULL);
#ifdef SHOW_HEADERS
	R_do_dns = strlen(REPORT_DO_DNS);
	R_fin_dns = strlen(REPORT_FIN_DNS);
	R_fin_dnsc = strlen(REPORT_FIN_DNSC);
	R_fail_dns = strlen(REPORT_FAIL_DNS1);
	R_do_id = strlen(REPORT_DO_ID);
	R_fin_id = strlen(REPORT_FIN_ID);
	R_fail_id = strlen(REPORT_FAIL_ID);
#endif
#ifdef DBUF_INIT
        dbuf_init(); /* set up some dbuf stuff to control paging */
#endif
#ifdef RLIMIT_NOFILE
        r.rlim_cur = MAXCONNECTIONS;
        r.rlim_max = MAXCONNECTIONS;
        setrlimit(RLIMIT_NOFILE, &r);
#endif
	init_dichconf(&Ilines);
	init_dichconf(&Klines);
	init_dichconf(&Dlines);
	init_dichconf(&Elines);
	init_dichconf(&Blines);
	init_dichconf(&Jlines);
#ifdef G_LINES
	GlineConf.seconds = 0;
	GlineConf.fromoper = NULL;
	GlineConf.fromserver = NULL;
	GlineConf.foruser = NULL;
	init_glineconf(GLINE_CONFFILE);
	init_dichconf(&Glines);
#endif
	sbrk0 = (char *)sbrk((size_t)0);
	uid = getuid();
	euid = geteuid();
#ifdef	PROFIL
	(void)monstartup(0, etext);
	(void)moncontrol(1);
	(void)signal(SIGUSR1, s_monitor);
#endif

#ifdef	CHROOTDIR
	if (chdir(dpath))
	{
		perror("chdir");
		exit(-1);
	}
	res_init();
	if (chroot(DPATH))
	{
	    (void)fprintf(stderr,"ERROR:  Cannot chdir/chroot\n");
	    exit(5);
	}
#endif /*CHROOTDIR*/

#ifdef	ZIP_LINKS
	if (zlib_version[0] == '0')
	    {
		fprintf(stderr, "zlib version 1.0 or higher required\n");
		exit(1);
	    }
	if (zlib_version[0] != ZLIB_VERSION[0])
	    {
        	fprintf(stderr, "incompatible zlib version\n");
		exit(1);
	    }
	if (strcmp(zlib_version, ZLIB_VERSION) != 0)
	    {
		fprintf(stderr, "warning: different zlib version\n");
	    }
#endif

	myargv = argv;
	(void)umask(077);                /* better safe than sorry --SRB */
	bzero((char *)&me, sizeof(me));

	setup_signals();

	/*
	** All command line parameters have the syntax "-fstring"
	** or "-f string" (e.g. the space is optional). String may
	** be empty. Flag characters cannot be concatenated (like
	** "-fxyz"), it would conflict with the form "-fstring".
	*/
	while (--argc > 0 && (*++argv)[0] == '-')
	{
		char	*p = argv[0]+1;
		int	flag = *p++;

		if (flag == '\0' || *p == '\0')
			if (argc > 1 && argv[1][0] != '-')
			{
				p = *++argv;
				argc -= 1;
			}
			else
				p = "";

		switch (flag)
		    {
                    case 'a':
			bootopt |= BOOT_AUTODIE;
			break;
		    case 'c':
			bootopt |= BOOT_CONSOLE;
			break;
		    case 'q':
			bootopt |= BOOT_QUICK;
			break;
		    case 'd' :
                        (void)setuid((uid_t)uid);
			dpath = p;
			break;
		    case 'o': /* Per user local daemon... */
                        (void)setuid((uid_t)uid);
			bootopt |= BOOT_OPER;
		        break;
#ifdef CMDLINE_CONFIG
		    case 'f':
                        (void)setuid((uid_t)uid);
			configfile = p;
			break;
#endif
		    case 'h':
			strncpyzt(me.name, p, sizeof(me.name));
			break;
		    case 'i':
			bootopt |= BOOT_INETD|BOOT_AUTODIE;
		        break;
		    case 'p':
			if ((portarg = atoi(p)) > 0 )
				portnum = portarg;
			break;
		    case 't':
                        (void)setuid((uid_t)uid);
			bootopt |= BOOT_TTY;
			break;
		    case 'v':
			(void)printf("ircd %s\n\tzlib %s\n\tircd_dir: %s\n", 
				     version, 
#ifndef ZIP_LINKS
				     "not used",
#else
				     zlib_version,
#endif
				     dpath);
			exit(0);
		    case 'x':
#ifdef	DEBUGMODE
                        (void)setuid((uid_t)uid);
			debuglevel = atoi(p);
			debugmode = *p ? p : "0";
			bootopt |= BOOT_DEBUG;
			break;
#else
			(void)fprintf(stderr,
				"%s: DEBUGMODE must be defined for -x y\n",
				myargv[0]);
			exit(0);
#endif
		    default:
			bad_command();
			break;
		}
	}

#ifndef	CHROOT
	if (chdir(dpath))
	{
		perror("chdir");
		exit(-1);
	}
#endif
#ifdef BETTER_MOTD
        motd = NULL;
        read_motd(MOTD);
#endif
#ifndef IRC_UID
	if ((uid != euid) && !euid)
	{
		(void)fprintf(stderr,
			"ERROR: do not run ircd setuid root. Make it setuid a\
 normal user.\n");
		exit(-1);
	}
#endif

#if !defined(CHROOTDIR) || (defined(IRC_UID) && defined(IRC_GID))
# ifndef	AIX
	(void)setuid((uid_t)euid);
# endif

	if ((int)getuid() == 0)
	{
# if defined(IRC_UID) && defined(IRC_GID)

		/* run as a specified user */
		(void)fprintf(stderr,"WARNING: running ircd with uid = %d\n",
			IRC_UID);
		(void)fprintf(stderr,"         changing to gid %d.\n",IRC_GID);
		(void)setuid(IRC_UID);
		(void)setgid(IRC_GID);
#else
		/* check for setuid root as usual */
		(void)fprintf(stderr,
			"ERROR: do not run ircd setuid root. Make it setuid a\
 normal user.\n");
		exit(-1);
# endif	
	} 
#endif /*CHROOTDIR/UID/GID*/

	/* didn't set debuglevel */
	/* but asked for debugging output to tty */
	if ((debuglevel < 0) &&  (bootopt & BOOT_TTY))
	    {
		(void)fprintf(stderr,
			"you specified -t without -x. use -x <n>\n");
		exit(-1);
	    }

	if (argc > 0)
		return bad_command(); /* This should exit out */

	clear_client_hash_table();
	clear_channel_hash_table();
	initlists();
	initclass();
	initwhowas();
	initstats();
	id_init(); /* before init_sys! */
	NOW = time(NULL);
	open_debugfile();
	NOW = time(NULL);
	init_fdlist(&new_servfdlist);
	init_fdlist(&new_operfdlist);
	init_fdlist(&new_miscfdlist);
	for(i=30;i >= 0; i--)
		init_fdlist(&new_fdlists[i]);
	if (portnum < 0)
		portnum = PORTNUM;
	(void)init_sys();
	init_me();

#ifdef USE_SYSLOG
	openlog(myargv[0], LOG_PID|LOG_NDELAY, LOG_FACILITY);
#endif
	if (initconf(bootopt,configfile) == -1)
	{
		Debug((DEBUG_FATAL, "Failed in reading configuration file %s",
			configfile));
		(void)printf("Couldn't open configuration file %s\n",
			configfile);
		exit(-1);
	}
#ifndef PUT_KLINES_IN_IRCD_CONF
	initconf(bootopt,klinefile);
#ifdef SEPARATE_QUOTE_KLINES_BY_DATE
{
	struct tm *tmptr;
	char timebuffer[20], filename[200];

	tmptr = localtime(&NOW);
	strftime(timebuffer, 20, "%y%m%d", tmptr);
	sprintf(filename, "%s.%s", klinefile, timebuffer);
	initconf(0,filename);
}
#endif
#endif /* PUT_KLINES_IN_IRCD_CONF */
	if (!(bootopt & BOOT_INETD))
	{
		aConfItem	*aconf;

		if ((aconf = find_me()) && portarg <= 0 && aconf->port > 0)
			portnum = aconf->port;
		Debug((DEBUG_ERROR, "Port = %d", portnum));
		if (inetport(&me, NULL, NULL, portnum))
		{
			fprintf(stderr, "Couldn't bind to port %d\n",
				portnum);
			exit(1);
		}
	}
	else if (inetport(&me, NULL, NULL, 0))
		exit(1);
/* This turns udp off...why is it even here? - Comstud
	(void)setup_ping();
*/
	(void)get_my_name(&me, me.sockhost, sizeof(me.sockhost)-1);
	if (me.name[0] == '\0')
		strncpyzt(me.name, me.sockhost, sizeof(me.name));
	me.hopcount = 0;
	me.authfd = -1;
	me.confs = NULL;
	me.next = NULL;
	me.user = NULL;
	me.from = &me;
	me.servptr = &me;
	SetMe(&me);
	make_server(&me);
	(void)strcpy(me.serv->up, me.name);

	id_reseed(me.name, HOSTLEN);
	me.lasttime = me.since = me.firsttime = NOW;
	(void)add_to_client_hash_table(me.name, &me);

	check_class();
	if (bootopt & BOOT_OPER)
	{
		aClient *tmp = add_connection(&me, 0);

		if (!tmp)
			exit(1);
		SetMaster(tmp);
	}
	else
		write_pidfile();

	Debug((DEBUG_NOTICE,"Server ready..."));
#ifdef USE_SYSLOG
	syslog(LOG_NOTICE, "Server Ready");
#endif
	NOW = time(NULL);
	curdiff = 0;
	lastdiff = 0;
	check_fdlists();
	lastmisc = NOW;
	lastflush = NOW;
	for (;;)
	{
		NOW = time(NULL);
#ifdef DOG3
{
		static time_t loadcfreq=DEFAULT_LOADCFREQ;

		if (NOW-lastrecvtime < loadcfreq)
			goto done_check;
		if (me.receiveK - dog3loadrecv > lastrecvK)
		{
			if (!lifesux)
		{
			sendto_ops("Entering high-traffic mode - %uk/%us > %uk/%us",
				me.receiveK-lastrecvK, NOW-lastrecvtime,
				dog3loadrecv, NOW-lastrecvtime);
			loadcfreq *= 2; /* add hysteresis */
			lifesux = TRUE;
		}
	}
	else
	{
		loadcfreq = dog3loadcfreq;
		if (lifesux)
		{
			lifesux = 0;
			sendto_ops("Resuming standard operation - %uk/%us <= %uk/%us",
				me.receiveK-lastrecvK, NOW-lastrecvtime,
				dog3loadrecv, NOW-lastrecvtime);
		}
	}
	lastrecvtime = NOW;
	lastrecvK = me.receiveK;
done_check:
	if (lastrecvtime != NOW)
		currentrate = ((float)(me.receiveK-lastrecvK))/((float)(NOW-lastrecvtime));
}
#endif /* DOG3 */

		/*
		** We only want to connect if a connection is due,
		** not every time through.  Note, if there are no
		** active C lines, this call to Tryconnections is
		** made once only; it will return 0. - avalon
		*/
		if (nextconnect && (NOW >= nextconnect))
			nextconnect = try_connections(NOW);
		/*
		** DNS checks. One to timeout queries, one for cache expiries.
		*/
		if (NOW >= nextdnscheck)
			nextdnscheck = timeout_query_list(NOW);
		if (NOW >= nextexpire)
			nextexpire = expire_cache(NOW);
		/*
		** take the smaller of the two 'timed' event times as
		** the time of next event (stops us being late :) - avalon
		** WARNING - nextconnect can return 0!
		*/
#if 0
		if (nextconnect)
			delay = MIN(nextping, nextconnect);
		else
			delay = nextping;
		delay = MIN(nextdnscheck, delay);
		delay = MIN(nextexpire, delay);
		delay = MIN(nextpasscleanup, delay);
		delay -= NOW;
		/*
		** Adjust delay to something reasonable [ad hoc values]
		** (one might think something more clever here... --msa)
		** We don't really need to check that often and as long
		** as we don't delay too long, everything should be ok.
		** waiting too long can cause things to timeout...
		** i.e. PINGS -> a disconnection :(
		** - avalon
		*/
		if (delay < 1)
			delay = 1;
		else
			delay = MIN(delay, TIMESEC);
#endif

		read_message(0, &new_servfdlist); /* servers */
		read_message(0, &new_operfdlist); /* opers */
		if (!lifesux || (lastmisc+1 < NOW))
		{
			read_message(0, &new_miscfdlist); /* listen, misc */
			lastmisc = NOW;
		}
		read_message(1, &new_fdlists[0]); /* busy clients */
						/* or all clients if
							NO_PRIORITY
						*/
#ifdef NO_PRIORITY
		curdiff = (NOW-me.since);
		if (curdiff != lastdiff)
		{
			time_t temp;

			tempfdlist.last_entry = 0;
/* this should be a bit better than my loops that are if 0'd out below */
			for(i=1;i<=30;i++)
			{
				if (!(temp=(lastdiff+1)%i) ||
						((curdiff % i) < temp))
				{
				bcopy(&new_fdlists[i].entry[1],
					&tempfdlist.entry[
						tempfdlist.last_entry+1],
					new_fdlists[i].last_entry*sizeof(int));
				tempfdlist.last_entry +=
						new_fdlists[i].last_entry;
				}
			}
			if (tempfdlist.last_entry)
				read_message(1, &tempfdlist);
#if 0
			for(i=1;i<=30;i++)
			{
				for (temp=lastdiff+1;temp<=curdiff;temp++)
					if (!(temp % i))
						break;
				if (temp != (curdiff+1))
					read_message(0, &new_fdlists[i]);
			}
#endif
			lastdiff = curdiff;
		}
#endif
	
		/* read servs more often */
		if (lifesux)
			(void)read_message(0, &new_servfdlist);
		/* screw it...send what you can to high sendq'd
			servers - CS */
		send_high_sendq(&new_servfdlist);

		Debug((DEBUG_DEBUG ,"Got message(s)"));
		
		/*
		** ...perhaps should not do these loops every time,
		** but only if there is some chance of something
		** happening (but, note that conf->hold times may
		** be changed elsewhere--so precomputed next event
		** time might be too far away... (similarly with
		** ping times) --msa
		*/
		if ((!lifesux && (NOW >= nextping)) || rehashed)
			nextping = check_pings(NOW);

		if (dorehash)
		{
			(void)rehash(&me, &me, 1);
#ifndef PUT_KLINES_IN_IRCD_CONF
#ifdef SEPARATE_QUOTE_KLINES_BY_DATE
{
        struct tm *tmptr;
        char timebuffer[20], filename[200];

        tmptr = localtime(&NOW);
        strftime(timebuffer, 20, "%y%m%d", tmptr);
        sprintf(filename, "%s.%s", klinefile, timebuffer);
        initconf(0,filename);
}
#endif
#endif
			dorehash = 0;
		}

		if (NOW >= nextpasscleanup)
		    {
		    	nextpasscleanup = NOW + CLEANUP_DELAY;
			expire_channel_passwords();
			save_random();
		    }

		/*
		** Flush output buffers on all connections now if they
		** have data in them (or at least try to flush)
		** -avalon
		*/

		if ((lastflush+1) < NOW)
		{
			flush_connections(me.fd);
			lastflush = NOW;
		}
#ifndef NO_PRIORITY
		/* check which clients are active */
		if (NOW > nextfdlistcheck)
			nextfdlistcheck = check_fdlists();
#endif
	}
}

/*
 * open_debugfile
 *
 * If the -t option is not given on the command line when the server is
 * started, all debugging output is sent to the file set by LPATH in config.h
 * Here we just open that file and make sure it is opened to fd 2 so that
 * any fprintf's to stderr also goto the logfile.  If the debuglevel is not
 * set from the command line by -x, use /dev/null as the dummy logfile as long
 * as DEBUGMODE has been defined, else dont waste the fd.
 */
static	void	open_debugfile()
{
#ifdef	DEBUGMODE
	int	fd;
	aClient	*cptr;

	if (debuglevel >= 0)
	{
		cptr = make_client(NULL);
		cptr->fd = 2;
		SetLog(cptr);
		cptr->port = debuglevel;
		cptr->flags = 0;
		cptr->acpt = cptr;
		local[2] = cptr;
		(void)strcpy(cptr->sockhost, me.sockhost);

		(void)printf("isatty = %d ttyname = %#x\n",
			isatty(2), (u_int)ttyname(2));
		if (!(bootopt & BOOT_TTY)) /* leave debugging output on fd 2 */
		{
			(void)truncate(LOGFILE, 0);
			if ((fd = open(LOGFILE, O_WRONLY | O_CREAT, 0600)) < 0) 
				if ((fd = open("/dev/null", O_WRONLY)) < 0)
					exit(-1);
			if (fd != 2)
			{
				(void)dup2(fd, 2);
				(void)close(fd); 
			}
			strncpyzt(cptr->name, LOGFILE, sizeof(cptr->name));
		}
		else if (isatty(2) && ttyname(2))
			strncpyzt(cptr->name, ttyname(2), sizeof(cptr->name));
		else
			(void)strcpy(cptr->name, "FD2-Pipe");
		Debug((DEBUG_FATAL, "Debug: File <%s> Level: %d at %s",
			cptr->name, cptr->port, myctime(NOW)));
	}
	else
		local[2] = NULL;
#endif
	return;
}

static	void	setup_signals()
{
#ifdef	POSIX_SIGNALS
	struct	sigaction act;

	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGPIPE);
	(void)sigaddset(&act.sa_mask, SIGALRM);
# ifdef	SIGWINCH
	(void)sigaddset(&act.sa_mask, SIGWINCH);
	(void)sigaction(SIGWINCH, &act, NULL);
# endif
	(void)sigaction(SIGPIPE, &act, NULL);
	act.sa_handler = dummy;
	(void)sigaction(SIGALRM, &act, NULL);
	act.sa_handler = s_rehash;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGHUP);
	(void)sigaction(SIGHUP, &act, NULL);
	act.sa_handler = s_restart;
	(void)sigaddset(&act.sa_mask, SIGINT);
	(void)sigaction(SIGINT, &act, NULL);
	act.sa_handler = s_die;
	(void)sigaddset(&act.sa_mask, SIGTERM);
	(void)sigaction(SIGTERM, &act, NULL);

#else
# ifndef	HAVE_RELIABLE_SIGNALS
	(void)signal(SIGPIPE, dummy);
#  ifdef	SIGWINCH
	(void)signal(SIGWINCH, dummy);
#  endif
# else
#  ifdef	SIGWINCH
	(void)signal(SIGWINCH, SIG_IGN);
#  endif
	(void)signal(SIGPIPE, SIG_IGN);
# endif
	(void)signal(SIGALRM, dummy);   
	(void)signal(SIGHUP, s_rehash);
	(void)signal(SIGTERM, s_die); 
	(void)signal(SIGINT, s_restart);
#endif

#ifdef RESTARTING_SYSTEMCALLS
	/*
	** At least on Apollo sr10.1 it seems continuing system calls
	** after signal is the default. The following 'siginterrupt'
	** should change that default to interrupting calls.
	*/
	(void)siginterrupt(SIGALRM, 1);
#endif
}

void send_high_sendq(listp)
fdlist *listp;
{
	int i, j;
	aClient *cptr;

	for (i=listp->entry[j=1];j<=listp->last_entry;
		i=listp->entry[++j])
	{
                if (!(cptr = local[i]) || IsMe(cptr) || IsConnecting(cptr))
                        continue;
		if (DBufLength(&cptr->sendQ) > 10240)
			send_queued(cptr);
	}
} 

time_t check_fdlists()
{
	register aClient *cptr;
	register int i;
	int pri; /* temp. for priority */

	for(i=0;i<=30;i++)
		new_fdlists[i].last_entry = 0;
	new_miscfdlist.last_entry = 0;
	for(i=highest_fd;i>=0; i--)
	{
		if (!(cptr=local[i]))
			continue;
		if (IsListening(cptr) || IsLog(cptr))
		{
			new_miscfdlist.entry[++new_miscfdlist.last_entry] = i;
			cptr->fdlist = &new_miscfdlist;
			continue;
		}
		if (IsServer(cptr) || IsOper(cptr) ||
			DoingAuth(cptr) || DoingDNS(cptr) || !cptr->user)
		{
			new_fdlists[0].entry[++new_fdlists[0].last_entry] = i;
			cptr->fdlist = &new_fdlists[0];
			continue;
		}
		pri = NOW-cptr->user->last2;
		pri = (pri+1)/180;
		if (pri > 30)
			pri = 30;
		new_fdlists[pri].entry[++new_fdlists[pri].last_entry] = i;
		cptr->fdlist = &new_fdlists[pri];
	}

	return (NOW + FDLISTCHKFREQ + lifesux * FDLISTCHKFREQ);
}
