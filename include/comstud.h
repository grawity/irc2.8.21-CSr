#ifndef COMSTUD_H
#define COMSTUD_H

/*
**  $Id: comstud.h,v 1.3 1997/07/29 19:49:01 cbehrens Exp $
*/


/* NO_PRIORITY     - Turns off the priority system.  Solaris still seems
                     to have a problem with delaying read()ing an fd until
                     a later time even though the fd is ready to be read.
                     I've never had a report of lag problems on other
                     OSes...but here is the define.
*/

#undef NO_PRIORITY



/* OLD_Y_LIMIT     - CSr30 now makes Y: limits the GLOBAL limit.
                     See README.CS.  If you prefer the old way (WHY?),
                     define this.
*/

#undef OLD_Y_LIMIT


/* G_LINES         - Define this if you want to listen to GLines
                     See README.CS
*/

#define G_LINES


/* FNAME_GLINELOG  - Logfile for glines
*/

#define FNAME_GLINELOG "logs/gline.log"


/* I_LINES         - Treed I: lines.  Yes, that's right.  Some of
                     you might have to redo your I: lines if you wish
                     to use this.  This code will not help those with
                     very few I: lines.

                     The format in ircd.conf is:
                     I:user@host:password:user@host:port:class
                     This will add 2 entries, unless they are both
                     identical.  Ip numbers are supported on either
                     side.  Also note, if for "user@host" you have "X"
                     or "NOMATCH" or "NOMATCHME", that entry will be
                     ignored.

                     (The format of I:'s may change in the future to be
                     exactly like K:'s)
*/

#undef I_LINES


/* SHOW_NICKCHANGES - I'm not a real fan of this, but it shows
                      *LOCAL* nick changes in umode +n.  This
                      information can be obtained from /trace anyway.
*/

#undef SHOW_NICKCHANGES


/* SHOW_HEADERS
		   - Taner's code for seeing what the server is doing
		     when you connect to it.
*/

#undef SHOW_HEADERS
 

/* D_LINES
		   - Define this for .conf lines that basically
                     ignore a site.  If they try to connect to your
                     server, the connection will be closed immediately.
                     Note: You must specify ip#'s...and usernames aren't
                     allowed (won't be matched).
*/

#define D_LINES

/* FNAME_DLINE_LOG
                   - Define this to a filename to log ip#'s that have
                     been rejected from connecting to the server...
*/

#define FNAME_DLINE_LOG "./logs/dlines.log"


/* RESTRICT_STATSK
                   - Define this if you want to restrict /stats k so
                     that it will only show if a certain spec is banned.
                     Ie, /stats k will tell you if you are banned or not.
                     /stats k <server or *> <nick/u@h> tells you if that
                     person is banned.
*/

#define RESTRICT_STATSK


/* NO_REDUNDANT_KLINES
                   - This will check the kline that you try and put in
                     via /quote kline and will not allow it if it matches
                     a kline that's already in memory.
*/

#define NO_REDUNDANT_KLINES


/* PUT_KLINES_IN_IRCD_CONF
                   - Starting in CSr25, klines can now be stored in a file
                     separated from ircd.conf.  The file name is chosen
                     via KPATH in config.h.  If QUOTE_KLINE is defined,
                     by default, when you /quote kline, it'll put these
                     new K: lines in the kline.conf file instead of ircd.conf
                     This can be used as a way to separate the millions
                     of K: lines that you may have.
*** If you do not like the idea of separate files, define this ***
*/

#undef PUT_KLINES_IN_IRCD_CONF


/* SEPARATE_QUOTE_KLINES_BY_DATE
		   - If PUT_KLINES_IN_IRCD_CONF is #undefined, you may
                     #define this to put /quote klines into separate files
                     by date.  Ie, if KPATH is "kline.conf", and today is
                     09/09/96, it'll put today's quote klines in:
                     kline.conf.960909
             Note:   These files with dates will NOT be loaded when
                     you start ircd...and if you /rehash all the quote klines
                     for the day are lost, until you manually move the ones
                     from kline.conf.960909 into kline.conf

          Purpose:   Sometimes you may not want your quote klines to last
                     more than a day, or you may want to look thru the
                     kline.conf.960909 file before you move them into
                     kline.conf.
*/

#define SEPARATE_QUOTE_KLINES_BY_DATE


/* LIMIT_UH        - Use this if you want to use the connect frequency
		     field in the Y: lines to limit that class to a
                     certain # of same u@h's.
                     For example, you can limit class 1 so that only 1
                     connection per u@h is allowed (by setting the confreq
                     field to 1)  This should cut down on clones and multiple
		     clients.

*/

#define LIMIT_UH


/* BUFFERED_LOGS   - define this to reduce disk IO when writing users.log
                     and clones.log
*/

#define BUFFERED_LOGS


/* BETTER_MOTD      - define this to keep the MOTD in ram to reduce disk
                      IO.  /REHASH to reload the MOTD.
*/

#define BETTER_MOTD


/* NO_NICK_FLOODS   - define this to limit local users to 3 nick changes
                      in 60 seconds
*/

#define NO_NICK_FLOODS


/* RESTRICT         - define this if using dog3 stuff, and wish to
                      disallow /LIST and other CPU intensive commands
                      when in HIGH TRAFFIC MODE
*/

#undef RESTRICT


/* B_LINES          - Define this if you wish to ignore ip#'s or hosts
                      from being tested for bots or clonebots...
                      B:*eskimo.com::* will not check for bots or
                      clones from eskimo.com domain.
*/

#define B_LINES


/* E_LINES          - Define this if you wish to have lines that bypass
                      K: line checking...ie for example:
                      You want to K-line all of netcom.com except for
                      *cbehrens@*netcom.com, use:
                      K:*netcom.com::*
                      E:*netcom.com::*cbehrens
*/

#define E_LINES 


/* MAXBUFFERS       - make receive socket buffers the maximum
                      size they can be...up to 64K
NOTE: send buffers will remain at 8K as bad things will happen if
they are increased!
*/

#define MAXBUFFERS

#ifndef MAXBUFFERS

/* This is the receive buffer size to use if MAXBUFFER isn't used: */

#define READBUFSIZE 32767

#endif

/* DBUF_INIT        - define this if you want to pre-allocate
                      4 megs of dbufs...this should help in
                      the long run according to dog3 =)
                      1000 = 1000*4kb = 4 megs
                      just #undef if you think it's unneeded =)
*/

/* #define DBUF_INIT 1000 */
#undef DBUF_INIT


/* IDENTD_ONLY      - define this if you only want people running
                      identd to connect
              Note:   Non-identd people are allowed on by
                      not putting a '@' in an I: line...
                      For example:
                        I:204.122.*::*eskimo.com::10
                      let's people on from eskimo.com even
                      if they aren't running identd
*/

#undef IDENTD_ONLY


/* QUOTE_KLINE      - define this if you want /QUOTE KLINE
*/

#define QUOTE_KLINE

/* NO_LOCAL_KLINE   - define this if you don't want little o:'s
                      using /QUOTE KLINE
*/

#undef NO_LOCAL_KLINE

/* DOG3             - define this if you want dog3's lifesux stuff.
                      Checks the amt of data coming in...and if it
                      is high, clients are checked a little less often
*/
#define DEFAULT_LOADRECV	110
#define DEFAULT_LOADCFREQ	5

#define DOG3


/* USE_UH           - define this if you want to use n!u@h
                      for BAN_INFO
*/

#define USE_UH


/* BAN_INFO         - define this if you want to see who did bans
                      and when they were done
*/

#define BAN_INFO


/* TOPIC_INFO       - define this if you want to see who did topics
                      and when they were done
*/

#define TOPIC_INFO


/* SIGNON_TIME      - define this if you want to see when a user
                      signed into irc when doing /whois
*/

#define SIGNON_TIME


/* HIGHEST_CONNECTION - define this if you want to keep track
                        of your max connections
*/

#define HIGHEST_CONNECTION


/* NO_RED_MODES    - define this if you don't want redundant modes
                     i.e., if someone is opped they can't be opped
                     /mode +ooo nick nick nick results in
                     /mode * +o nick
*/

#undef NO_RED_MODES


/* IP_BAN_ALL      - define this if you want a really cool ban
                     system
                     What this does is if you ban an ip# like:
                     /mode * +b *!*@129.186.*  then obviously
                     those ip#'s are banned, but *!*@*iastate.edu
                     is also banned with this patch.  This is
                     nice is some machines from a site don't
                     resolve for some reason.  This way you can
                     just ban the ip#'s and even if they resolve
                     they'll be banned from channels
              Note:  This doesn't work in reverse.  If
                     you ban *!*@*iastate.edu, *!*@129.186.* is
                     not banned
*/

#define IP_BAN_ALL


/* NO_MIXED_CASE   - define this if you wish to reject usernames
                     like: FuckYou which don't have all one case
*/


#undef NO_MIXED_CASE


/* STRICT_USERNAMES
                   - Define this if you only want letters, numbers,
                     -, and _ to be allowed in usernames...
*/

#define STRICT_USERNAMES


/* IGNORE_FIRST_CHAR - define this for NO_MIXED_CASE
                       if you wish to ignore the first character
*/

#undef IGNORE_FIRST_CHAR


/* NO_SPECIAL      - define this if you want no "special" characters
                     in usernames.  A character is "special" if
                     it's not "a-z", "A-Z", "0-9", ".", "-", and
                     "_"
*/

#define NO_SPECIAL


/* REJECT_BOTS     - Performs minimal checking to see if a client
                     which is trying to connect is a bot...
                     If it is, it will be rejected from connecting.
                     See BOTS_NOTICE
*/

#define REJECT_BOTS


/* BOTS_NOTICE     - Performs minimal checking to see if a client
                     which is trying to connect is a bot...
                     If it is, it will send a notice to opers who
                     are usermode +r about a possible bot connecting
                     (or being rejected if REJECT_BOTS is defined).
*/

#define BOTS_NOTICE


/* STATS_NOTICE    - send a notice to /opers on the server when
                     someone does /stats requests
                     (Non-useful...just used to see who's spying ;)
*/

#define STATS_NOTICE


/* FAILED_OPER_NOTICE - send a notice to all opers when someone
                        tries to /oper and uses an incorrect pw
*/

#define FAILED_OPER_NOTICE


/* FNAME_FAILED_OPER - define this as a filename of a logfile
                       if you wish to log when someone tries
                       to /oper and uses an incorrect pw
               Note:   The filename must exist before logging
                       will take place.
               Note:   #undef FNAME_FAILED_OPER if you don't
                       want them logged
*/

#define FNAME_FAILED_OPER "./logs/failed.log"


/* CLIENT_NOTICES - define this if you wish to see client connecting
                    and exiting notices via /umode +c
*/

#define CLIENT_NOTICES


/* DONT_SEND_FAKES - define this if you don't want Fake: notices
                     sent to users...there are tons of fakes all
                     the time and it takes a lot of CPU to send
                     them to all +s or +f people
*/

#define DONT_SEND_FAKES


/* FK_USERMODES  - define this if you want +f and +k usermodes
                   +f would then show fakes and serverkills
                           (fakes if DONT_SEND_FAKES is not defined)
                   +k would show operkills
                   +s would not contain those anymore and would
                      just contain normal/error notices
	    Note:  the purpose of this is so that you can easily
                   do /umode -f+k to ignore serverkills and fakes
                   and easily see oper kills still

*/

#define FK_USERMODES


/* RESETIDLEONNOTICE - define this if idletimes should
                       be /reset on notice as well as /msg
*/

#undef  RESETIDLEONNOTICE


/* USERNAMES_IN_TRACE - define this if you want to see usernames
                        in /trace
             Note:  usernames will be seen in other commands too
                    most likely
             Note:  also, all sites that don't have ident running
                    will show a ~ in front of their username in
                    this version even if you don't have a '@' in
                    your I: lines.
*/

#define USERNAMES_IN_TRACE


/* IDLE_CHECK     - define this if you wish to have an idle checker
                    built into the server
             Note:  Idletime is not reset on msgs to invalid nicks
                    or channels
             Note:  Idletime is not reset on msgs to self

 *** *** *** Note:  A user will not be able to reconnect for 60 seconds
                    until after they are knocked off.  If they try to connect
                    before then, 60 more seconds are added to each attempt.
 *** *** *** Note:  E: line u@h's are exempt from idle checking.

*/

#define IDLE_CHECK


/* KLINE_CHECK    - this is how often (in seconds) that K: lines
                    should be checked.  Every fifteen minutes is
                    a good number (900 seconds).  This reduces
                    CPU.
            Note:   K-lines are still checked on connect and
                    on /rehash
*/

#define KLINE_CHECK 900


/* CLONE_CHECK    - define this if you wish to enable clonebot
                    checking
                    The way it works is that it checks how fast
                    people connect from the same machine
                    Once a certain amount of clients connects from
                    the same machine in a small amount of time,
                    that machine is refused to connect for a minute
                    or so  (All numbers can be defined below)
*/

#define CLONE_CHECK                    


/* FNAME_CLONELOG - define this if you have CLONE_CHECK defined
                    and you wish to log clones
*/

#define FNAME_CLONELOG "./logs/clones.log"


/* DEFAULT_IDLELIMIT  - if you have CHECK_IDLE defined above,
                        this value is the default # a client
                        can be idle before being kicked off of
                        IRC
                 Note:  This value can be changed on IRC with
                        /quote idle
*/

#define DEFAULT_IDLELIMIT  0

/*  THE REST OF THIS STUFF IS TO CONFIGURE CLONE CHECKING */

/*
Note: good numbers to use are 5 bots joining with no more than
      4 seconds inbetween the connections
      I.e., NUM_CLONES = 6, CLONE_RESET = 10
*/

/* NUM_CLONES - protection is enabled after this many users
                are counted from a site within
                CLONE_RESET (see below) amount of seconds!
*/

#define NUM_CLONES 5


/* CLONE_RESET - after this # of seconds of no connects from
                 a machine, the # of clones (NUM_CLONES) is reset
                 to 0
*/

#define CLONE_RESET 4


/* KILL_CLONES - define this if you wish to have previous clients
                 from the machine in question to be exited
         Note:   You can define a limited # of seconds that they
                 have been irc to kill them....see CLONE_TIME
*/

#undef  KILL_CLONES


/* CLONE_TIME - If KILL_CLONES is defined, this is the # of
                seconds ago the clones had to be loaded before
                they are killed.  Grrr,  I mean for example:
                if CLONE_TIME == 30, then all connects that
                are 30 seconds old or less from the clone site
                are exited (not really killed, but exited)
*/

#define CLONE_TIME 30

/***********************************************************************/
/*             DO NOT CHANGE ANYTHING AFTER THIS POINT                 */
/***********************************************************************/

#if !defined(CLONE_CHECK) && !defined(REJECT_BOTS) && !defined(BOTS_NOTICE)
#ifdef B_LINES
#undef B_LINES
#endif
#endif

#ifdef G_LINES
#define GLINE_CONFFILE "gline.conf"
#endif

#endif /* COMSTUD_H */

