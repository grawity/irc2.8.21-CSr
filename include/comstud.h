#ifndef COMSTUD_H
#define COMSTUD_H

/* IDENTD_ONLY      - define this if you only want people running
                      identd to connect
              Note:   Non-identd people are allowed on by
                      not putting a '@' in an I: line...
                      For example:
                        I:204.122.*::*eskimo.com::10
                      let's people on from eskimo.com even
                      if they aren't running identd
*/

#define IDENTD_ONLY

/* QUOTE_KLINE      - define this if you want /QUOTE KLINE
*/

#define QUOTE_KLINE

/* NO_LOCAL_KLINE   - define this if you don't want little o:'s
                      using /QUOTE KLINE
*/

#define NO_LOCAL_KLINE

/* BAN_INFO         - define this if you want to see who did bans
                      and when they were done
*/

#define BAN_INFO

/* TOPIC_INFO       - define this if you want to see who did topics
                      and when they were done
*/

#define TOPIC_INFO

/* SIGNON_TIME      - define this if you want to see when i user
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

#define NO_RED_MODES

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

#define NO_MIXED_CASE

/* IGNORE_FIRST_CHAR - define this for NO_MIXED_CASE
                       if you wish to ignore the first character
*/

#define IGNORE_FIRST_CHAR

/* NO_SPECIAL      - define this if you want no "special" characters
                     in usernames.  A character is "special" if
                     it's not "a-z", "A-Z", "0-9", ".", "-", and
                     "_"
*/

#define NO_SPECIAL

/* REJECT_IPHONE   - define if you want to reject I-phoners
*/

#define REJECT_IPHONE

/* REJECT_BOTS     - define if you want to reject users from
                     connecting to the server with nicks
                     with 'bot', 'serv', or 'help' in them.
             Note:   you can specify an ip address to
                     ignore this
             Note:   this doesn't prevent anyone from
                     /nick HackBot once on IRC
*/

#define REJECT_BOTS

/* OPER_CAN_FLOOD1 - define this if you don't need flood protection
                     for all /opers including local (o:) ones
*/

#define OPER_CAN_FLOOD1

/* OPER_CAN_FLOOD2 - define this if you want only O: opers to be
                     able to flood
*/

#undef OPER_CAN_FLOOD2

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

#define  RESETIDLEONNOTICE


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

#define FNAME_CLONELOG "/home/irc/irc2.8.21+CSr3/logs/clones.log"


/* DEFAULT_IDLELIMIT  - if you have CHECK_IDLE defined above,
                        this value is the default # a client
                        can be idle before being kicked off of
                        IRC
                 Note:  This value can be changed on IRC with
                        /quote idle
*/

#define DEFAULT_IDLELIMIT  0

/* IDLE_IGNORE   - if you have IDLE_CHECK defined, you
                   can use this to specify an ip# that
                   will be allowed to idle
                   Wildcards are allowed
            Note:  If you want everyone to be checked use:
               #define IDLE_IGNORE ""
*/

#define IDLE_IGNORE "204.122.*"

/* BOT_IP_IGNORE - if you have CLONE_CHECK or REJECT_BOTS
                   defined, this can contain an ip# to ignore
                   Wildcards are allowed
            Note:  If you don't want to ignore an IP use:
               #define BOT_IP_IGNORE ""
*/

#define BOT_IP_IGNORE "204.122.*"

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

/* END OF FILE */




#endif /* COMSTUD_H */

