NOTE: Please change values in comstud.h and dog3.h to your liking...
      The default values are probably less than ideal for you! 

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


