---
$Id: README.CS,v 1.7 1998/02/21 02:50:07 cbehrens Exp $
---

This is the README for irc2.8.21+CSr30.5.  Basically, this is just a list
of changes...more information can be found in include/comstud.h.

Any questions or comments can be directed to cbehrens@concentric.net
The newest versions of +CS ircds can be found at:
     ftp://ftp.lump.net/pub/cbehrens/ircd

---

NOTE: Please change values in comstud.h to your liking...
      The default values are probably less than ideal for you! 

New in CSr31pl2:

1) Added FLOOD_WAIT and FLOOD_MAX #defines to comstud.h to rate limit
    remote /motd, /info, /stats, etc commands.


New in CSr31pl1:

1) Small linux fix for those version that have poll() where there is
    no poll.h, but there is sys/poll.h
2) ->ipstring not being \0 terminated.
3) m_kline() fix to check for ->user != NULL


New in CSr31:

1) res fixes.
2) NO_PRIORITY define.  People running <= Solaris 2.5.1 will want to
   #define this.  It seems there are 2 problems with using any
   priority code on Solaris (in hybrid and digi too).  The 2 problems
   are
     a) People get "one way lagged"  It seems that the server won't
          read from clients even though they have sent data.
          My guess is that poll() does not return that an fd is 
          ready to be read if you ignore it for a few seconds.
          It will be eventually read, sometimes 30 minutes later.
     b) netstat -m shows enormous amounts of streams data in use..
          probably because of a).
   Turning the priority code off solves both of these problems.
   Next +CS release should totally remove the need for priority
   code.  If I can do it, it will be available in /hybrid also.
3) Enhanced inet_ntoa, from Thomas Helvey <tomh@inxpress.net>
   Actually, this just pretty much clutters support.c...and that
   function doesn't use much CPU to begin with, but oh well..
4) s_misc.c - get_client_name used 2 times in same send2ops...can't do
      this as it returns ptr to static buffer.
5) Fixed wallops not propagating?

New in CSr30.5:

1) Bug fix for those who use select() instead of poll().  highfd in
    s_bsd.c not being set correctly.
2) Typo in CSr30 for BSD/OS 3.0 fixed, plus other code removed
3) SHOW_NICKNAMES wasn't completed...fixed.


New in CSr30:

1) Fixed problem of bind() failing before connect() for identd.
2) Fixed a change that I missed in r29 dealing with the '$' prefix in I:
3) Changed the appearance of /stats ?
4) Added /stats p to see what opers are on the server.
5) fixed stupid FASTCONNECT define in s_bsd.c
6) Added timestamps to /quote klines (Murray)
7) Problem where SERVER message gets dropped fixed.  Fix actually came
   from krys, in 2.9.3b14.
8) Fixed a weird bug in sendto_common_chan*()...only seemed to crash
   when -O was used...but should crash no matter what.
9) Auto-killing ghosted users that have invalid servernames...(I thought this
   made it into CSr28...but I guess it didn't.)
10) Changed some of the Idle and Clone rejection stuff...Should reduce
    memory and CPU (although it might not be enough to be noticable)
11) Totally new priority system, replaces dog3's.
12) Improvements to m_whois, should help CPU.
13) Improvements to get_client_ping, etc, should help CPU.
14) Treed I: line support (just for Rogue_F)
15) Removed AWAY from /whowas
16) Nick propogation bug fixed (fix is also in TSora3, but is not really
      a TS bug)
17) Added TSora3
18) Even if a client matches an I: with an ip, it lets the client resolve
19) Fixed a TS bug in s_user.c which is not fixed in TSora3
20) configure script built by autoconf.  configure.in heavily ripped from
      2.9.x, but modified for +CS
21) s_serv.c buffer overflow fix
22) res.c buffer overflow fix
23) Added s_serv.c changes from Dianora for converting bogus server names
      and stats requests to printable chars.
24) weird channel.c code replaced
25) added dich_conf stuff to /stats z
26) made /stats z oper-only, since it has to do a lot of computing.
27) OPERWALL command...this is the new command for passing wallops
      done by opers.  Too bad folks...I'm no longer passing WALLOPS
      done by non-servers...upgrade.
28) umode +z is for oper wallops (+w is only for server wallops)
29) EXT command...currently unused...but is there for future phasing
      in of things.  EXT command passes everything to all servers.
30) umode +n (nick change notices)...I don't like this one, but I guess
      the information is available in /trace anyway.
31) Glines, /quote GLINE
32) #ifdef HAVE_FD_ALLOC for BSD/OS 3.0 and those OS's that have
     dynamicly sized fd_set
33) ->ipstr added to client struct for local connetions.  Replaced
     few inet_ntoa's with ->ipstr
34) Changed IP# Mismatch messages to show ip string instead of hex value.
35) Y: limit is now global, and not a per-I: limit.  Ie, if you have
    an I: for *.com and *.edu that are both in class '1' and class '1' is
    limited to 20 clients...then *.com + *.edu will not go over 20.  The
    old way lets 20 *.com and 20 *.edu clients on.  If you want that, you
    can make 2 different Y: lines or #define OLD_Y_LIMIT in comstud.h
36) various strncat()s and strncpy()s fixed in case.
37) timezone fix for date() from Rodder.
38) resolving problem on startup fixed?
39) _local_ kills don't have flood protection.


New in CSr29:

1) Pfft. A bug I made in r28 doesn't crash on Solaris, when it should.
   No more crashes on startup I hope
2) Changed the name of a bunch of defines because "UMODE" conflicts with
   a define in IRIX system headers.  Just for you, Xney..
3) Added what people are calling "virtual host."  The ip# that ircd
   will bind to doesn't have anything to do with the servername in
   the M: line anymore.   The ip# to bind to will be the unused passwd
   field of the M: line.  For example:
   M:irc.concentric.net:206.173.136.204:Concentric Network Corp:6667
                        ^^^^^^^^^^^^^^^
                        ip to bind to
   If this field is left blank, or is a *, INADDR_ANY is used...which
   will bind to all ips on that machine.

New in CSr28:

1) duh. '#' won't work in I: lines for passing identd (See #1 in CSr27
   below).  This has been changed to '$'.
2) Fixed a memory leak in DICHCONF/Kline stuff.
3) /whowas code totally rewritten.  This code is better than Avalon's
   ever was.  You can now raise NICKNAMEHISTORYLENGTH in config.h to
   a value that makes sense with today's number of clients.  Without
   rewriting, you could expect server lockup, mostly during splits.
   These changes should lower CPU usage quite a bit, especially during
   splits.
4) /who code tweaked.  IsMember() = CPU hog...IsMember() has been changed
   and m_who does less find_user_link()s now...much less.  This is
   a big reducer of CPU usage.  Also another "bug" was removed from
   m_who that can possibly cause server lockups.  The possibility still
   remains, though...not sure how to improve it further without totally
   breaking /who.
5) This goes with #4, but...
   /who nickname was optimized tremendously.  However, this breaks /who
   if you do /who username and username happens to match a nickname.
   Oh well...too bad.
6) Hash table values for clients and channels raised
7) Various res.c changes that are in 2.9.x.  I'm hoping these will fix
   my coredumps, but I highly doubt it.
8) Maybe other things...I dunno...


New in CSr27:

1) There are noticable differences in I: lines now.
   There are 4 special characters that you can stick in front
   of a hostname or ip# that do special things.
   These characers are:
   -  This will never put a ~ for a user not running identd
   +  This will force people matching this I: to require identd
   !  This means to only allow 1 connection per ip# in this I:
   #  This means to allow a client to connect even if they don't
      have identd.  (This is only useful if your server is compiled
      for IDENTD_ONLY)
   Examples:
 a) I:x::!*@*ppp*::class  will only allow X connections per ip# for people
      who have *ppp* in their hostname where X is given in the Y: line.
      (See below in CSr25 about Y: lines).
    If there is no ! and you have a limit in your Y: line, then it matches
      full user@host instead of just host.

 b) I:x::-*@*.cris.com::class will never show a ~, even though they may
      not be running identd.  (This is equivilent to the old way of not
      specifying a @ in the I: line).

2) Rewrote parts of read_message() for the select() loop.  There used to
   be 3 loops, which have now been condensed to 1.
3) Rewrote poll() usage in read_message() so that poll() will finally work
   on Solaris and you can get > 1024 fds w/o using 99% CPU!
4) Added extra charaters to deny in usernames for NO_SPECIAL.  Also fixed
   the fact that it wasn't checking identd replies, too.
5) Rewrote parts of check_pings() to try and make it more optimized..
6) #define RESTRICT_STATSK --
   This restricts users from getting the full listing
   of /stats K which they really don't need.  If the user is on another
   server, /stats k <yourserver> will just tell them if they are banned or
   not.  If they are, it shows them the bans.  If the user is on your
   server, they may: /stats k * <nick or user@host> and it'll tell them
   if that nick or user@host is banned.  And yes, opers can still get
   the full stats k output.  Maybe this is bad, maybe not, but I did it.
   So, opers can /stats k, or they can /stats k * <nick or user@host>
7) #define PASS_KLINES --
   Hrm, well, this will pass /quote kline's to our downlinks/uplinks.
   This is not very useful right now, as it sends to ALL servers connected
   to you.  Note that all servers cept +CS servers (currently) will
   ignore it.  See #8:
8) #define ALLOW_KLINES_FROM_SERVERS --
   Define this if you wish to parse the klines that are sent from #7.
   Note, you have to select which servers to accept from. See #9:
9) #define ALLOWED_KLINES --
   Define this to the servers you wish to allow klines from.  Ie,
   #define ALLOWED_KLINES "*.cris.com *.netcom.com"
   This only allows k-lines to be accepted from those servers!
10) Changed some values in config.h dealing with auto-reconnecting
    to servers.  Servers are trying to reconnect too quickly and
    are causing a lot of problems.  It's good to wait about 30 seconds
    before trying to reconnect.
11) Added a /quote HTM which tries to tell you our current rate of
    in coming data...not sure if it's totally accurate, but it's
    interesting..
12) Added /quote ADDKLINES for those using dated klines files...
    It allows you to /quote addklines 961017
13) Eliminated mass messaging by disallowing more than 10 nicknames
    for PRIVMSG nick1,nick2,...nick10.  It notices /oper's if a person
    tries more than 20, which means he's probably trying to abuse.
    Some people are going to be pissed, because it breaks their
    chanop-wall scripts, but screw them.  Privmsg nick1,nick2 should be
    disallowed to happen at the same time (without penalty anyway).
    (Maybe it should just lag the users really bad?)
14) Added a lot of typecasting to s_bsd.c, which at least eliminates
    all compiler warnings on Solaris, which should be okay for other
    OS's (?)
15) Fixed the problem of rehashing with a new P: line on Solaris.
    You can now add ports as you wish without a problem.
16) Fixed the accept: Protocol error on Solaris.  Solaris's accept()
    returns EPROTO when a connection dies before or as it's accept()'d.
17) Fixed some other getsockopt() errors that would happen occasionally...
    (at least on Solaris)
18) Added more stats to /stats ? (to be more like Taner's)
19) Added RPL_ENDOFTRACE to be like /digi and +th
20) Added more informative messages to the user for Invalid username
    rejections and bot rejections.
21) Fixed a problem with early releases of CSr25 where it was trying to
    kill people who were already killed (m_kill).  Don't ask =)
22) Now canonizing blah,blah,blah in /topic, /list and, /whowas.
23) Upped the NICKNAMEHISTORYLENGTH size in config.h to 15000.  This uses
    around 3 megs of RAM.  If you don't have that to spare, lower this.
    Currently, there is usually around 10000 out of 15000 used.
24) When a KILL line takes effect, kick the user off with the reason,
    instead of just "K-lined".
25) Cleaned up a lot of the code that figures out the MAX_BUFFER size
    for reads and writes.
26) Upped listen(, 5) to listen(, 1024)
27) Fixed a place where fds weren't being removed from the fdlists when
    a client was exiting.
28) Removed the source ports for clients from /stats L

CSr26 wasn't released.

New in CSr25:

1) Fixed a bug in a patch that DWildstar gave me in CSr21.
   No more spewing of "Would have caused fake directions" for my
   uplinks.  Yay!
2) Fixed BETTER_MOTD where the motd would not be present until
   you /rehash'd the server...and sometimes when /rehash'ing it
   wouldn't reload.
3) Fixed LIMIT_UH stuff.  Actually, it's changed a bit.  The limit #
   is set via the connection frequency field of the class (Y: line)
   For example, I:*.slavery.com::*.slavery.com::1
   And          Y:1:180:1:15:1000000
                       ^^^
   That will allow only 1 unique u@h from *.slavery.com
   This way you can put "2" or so for the class that you use for
   open *@* I: lines if you want to allow them 2 connections...etc.
4) You can now #define USE_POLL on SunOS to use SunOS's poll()

5) Created a new usermode +d, and moved "Identd response differs"
   messages there
6) Created a new usermode +b, and moved the Bot/Invalid username
   notices to it.
7) Created a new usermode +l, and moved UH_LIMIT reject notices there
8) Got rid of the +r usermode, which is in 2.9 servers.

New in CSr24:

1) BUFFERED_LOGS were not logging at all...this is fixed.
2) Patched match.c to fix coredumps
3) Attempted to add poll(), using basically all of the code from 2.9
   This is for Solaris folks only, of course.  There is a #define in
   config.h for "USE_POLL" after the section where you #define SOL20
4) Added #define LIMIT_UH.  You can now limit your server to only allow
   a certain # of connections from a u@h...For example, LIMIT_UH 1 will
   only allow 1 u@h per client.  If another person with the same u@h tries
   to connect, they will be rejected.  While on IRC, you can /QUOTE LIMITUH #
5) Added a notice to opers when a person tries to "evade" identd.
   I had this in one of my 2.8.16 ircds, but when ds2 added it to his, it
   reminded me that I had forgotten to carry it over to my 2.8.21 series...
   Thanks, buddy =) Sorry, I am going to stick with using identd replies
   for clients' usernames.

New in CSr23:

1) Changed the timing of the NO_NICK_FLOODS stuff.  If you change your
   nick 4 times within 15 seconds, you can't change your nick for 30 seconds.
2) Commented out the flushing of dns cache when /rehashing.  There seems
   to be a problem on some OS's where the cache isn't cleared and ircd
   coredumps soon after the rehash() function ends.
3) Changed read_motd() to work on all systems.  On AIX the last line
   of the motd was being read 2 times.
4) Fixed a small bug where 'motd' wasn't init'd correctly when the
   server would start.

New in CSr22:

1) Fixed REJECT_BOTS stuff.  This was broken starting with r20 or so.
   The actual problem was just a missing closing comment (*/) in comstud.h
2) Fixed K: lines that I broke in ircd.c in r21.  I had an /* instead of */
   which basically commented out all the K: checking.
3) Fixed the mode #chan +k bug.
4) Added BETTER_MOTD which keeps the ircd.motd in RAM which saves a lot
   of disk access on high-access client servers
   If you change the /motd, just /rehash to "reload" it.
5) Added BUFFERED_LOGS which doesn't write to users.log and clones.log
   each time a client exits or is rejected for cloning.  This should
   save some disk access as well, tho using a bit more RAM :-/
6) Disabled channel TS stuff once again.


New in CSr21:

1) Readded IDLE_CHECK, using ds2's idea of not allowing them to reconnect
   after being kicked off.
2) Added a NO_NICK_FLOODS define, disallowing users to change nicks after
   3 times in 60 seconds.
3) Added DWildstar's patches for "Non-chanop" messages..
4) Fixed bugs in s_user.c dealing with TS (found by Taner)
5) Hopefully fixed "Invis count off" notices...
6) Reenabled channel TS stuff.
7) Tried to fix a bug where the server sends "Odd Server Stuff" to clients

New in CSr20:

1) Removed an #ifdef from dbuf.c which causes some problems on some OS's
2) Fixed, and commented out a function in dich_conf() that isn't used
3) Fixed EAGAIN problem on connect() on DG/UX, thanks to
      Greg Moeller <gkm@moe.magic.mb.ca>
   For DGUX, add #define DGUX to config.h or comstud.h
4) Added orabidoo's TS patch to stop wrong deop's.
5) Patched TS to show deoppings to come from local server, not remote
      asamonte@calpoly.edu helped me with this one...
6) m_lusers (/LUSERS) now uses pre-computed info.  This should save
   hella CPU on heavily cliented servers
   PS, my channel counting counts secret channels...the old version
   of ircd did not...who knows why, but you can get the real # of
   channels via /stats z anyway.
7) Optimized the linear list for K: lines in DICH_CONF (the non-sortable ones)
8) Removed DICH_CONF & DOUGH_HASH defines...made them part of stock +CS
   Also, did the same thing for DBUF_TAIL
9) Applied DOUGH_HASH routines to channel hashing too...
10) Kline comment is now displayed to other real ops when the k-line is done
11) Moved rest of nick collisions into +f
12) Moved "Too many", "Unauth" and so forth messages into +u 

New in CSr19:

1) Oops, fixed how klining with no reason will core the server =(

New in CSr18:

1) Fix from Philippe for dich_conf K: line matching and so forth...
2) Added B: lines to desigate hosts/ip#'s to ignore for bot/clone checking
   if defined...Thanks to Philippe for this as well

Note: BOT_IP_IGNORE is no longer a #define, you must use B: lines
      (the same way as K: and I: lines to specify a host/ip or so forth
       to make pass thru the bot/clone check...)

3) Fixed DEBUGMODE and DOUGH_HASH not being able to be defined together (?)
4) Remove IDLE checking for disconnecting IDLE users...this patch was
   basically a waste, since most clients auto-reconnect...maybe I'll do
   something that won't let the clients reconnect...we'll see...
5) Removed the #define's for removing flood control for opers...if you want
   this, you will have to hack it in yourself...I don't want to catch any
   crap for it =)
6) Minimal performance improvement, lowering the # of calls to time()
7) Performance improvements, removing K: lines from the main linear
   "conf" list, when using DICH_CONF...before, it still added them to
   the "conf" list, and also kept them in separate DICH lists...This
   improves the speed in looking up I:/Y: lines and so forth...so when
   clients connect, less CPU is used.
8) Added #define for E-lines.  These lines are for hosts that should
   be allowed to connect, even if it matches a K-line...for instance,
   K-line all of *netcom.com, E-line *cbehrens@*netcom.com to allow it
   to connect  (This is only valuable to open I: servers mostly)
9) When using QUOTE KLINE with DICH_CONF, the K-line is added to ircd.conf
   and added in memory internally without rereading the whole ircd.conf
10) Fixed various other things with QUOTE KLINE...
11) Fixed timezone bug
12) Added #define RESTRICT, so that you can #define whether or not
    you want to disable /list, and /lusers when in dog3's HTM

New in CSr17:

1) Updates for +TSora

New in CSr16:

1) TSora - A timestamping protocal which disallows "op hacking" and nick
        colliding.
        Thanks to Roger Espel Llima <espel@clipper.ens.fr>

2) Better kline checking - This should be faster, and use less CPU
        Thanks to Philippe Levan <levan@epix.net>

3) Fixed /rehash not working when the server is in dog3's High Traffic Mode

4) Added nicknames to users.log

5) Upped values in dog3.h, config.h, and hash.c for larger sendqs and a
   bigger hash table for clients.


