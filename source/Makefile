#************************************************************************
#*   IRC - Internet Relay Chat, ircd/Makefile
#*   Copyright (C) 1990 Jarkko Oikarinen
#*
#*   This program is free software; you can redistribute it and/or modify
#*   it under the terms of the GNU General Public License as published by
#*   the Free Software Foundation; either version 1, or (at your option)
#*   any later version.
#*
#*   This program is distributed in the hope that it will be useful,
#*   but WITHOUT ANY WARRANTY; without even the implied warranty of
#*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#*   GNU General Public License for more details.
#*
#*   You should have received a copy of the GNU General Public License
#*   along with this program; if not, write to the Free Software
#*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#*/

#
#  $Id: Makefile,v 1.1.1.1 1997/07/23 18:02:03 cbehrens Exp $
#

CC=gcc
RM=/bin/rm
CP=/bin/cp
TOUCH=touch
SHELL=/bin/sh
INSTALL=/usr/bin/install

# IRCDMODE given in top level Makefile, but added here to make sure
# compilation works if started in ircd subdirectory

IRCDMODE=771
INCLUDEDIR=../include

# RES - if you are missing the resolv library (man 5 resolv.conf), or you
# experience probems with ip# to hostname lookups for local machines or
# the server wont compile because _res is missing, uncomment RES.
# For those who know what these are, if you normally use the resolv+
# libraries and have setup /etc/resolv.conf, these are fromm resolv+ if they
# are not part of your system libraries.  In all cases you should try your
# system libraries before these.
#
#RES=res_init.o res_comp.o res_mkquery.o

OBJS= bsd.o dbuf.o packet.o send.o match.o parse.o support.o \
	channel.o class.o hash.o ircd.o list.o \
	res.o s_auth.o s_bsd.o s_conf.o s_debug.o s_err.o s_misc.o \
	s_numeric.o s_serv.o s_user.o whowas.o clone.o fdlist.o \
	dich_conf.o dog3.o gline.o $(RES)

SRC=$(OBJS:%.o=%.c)

MAKE = make 'CFLAGS=${CFLAGS}' 'CC=${CC}' 'IRCDLIBS=${IRCDLIBS}' \
	    'LDFLAGS=${LDFLAGS}' 'IRCDMODE=${IRCDMODE}'


all: ircd

ircd: $(OBJS) ../include/patchlevel.h
	$(SHELL) version.c.SH
	$(CC) $(CFLAGS) -c version.c
	$(CC) $(CFLAGS) $(OBJS) version.o $(LDFLAGS) $(IRCDLIBS) -o ircd
	chmod $(IRCDMODE) ircd

lint:
	lint $(LINTFLAGS) -I../include $(SRC) | egrep -v 'sendto_|debug'

install: all
	-if [ ! -d ${IRCDDIR} -a ! -f ${IRCDDIR} ] ; then \
		mkdir ${IRCDDIR}; \
	fi
	../bsdinstall -c -s -m ${IRCDMODE} ircd ${BINDIR}
	$(CP) ../doc/example.conf ${IRCDDIR}
	$(TOUCH) ${IRCDDIR}/ircd.motd
	$(RM) -f ${IRCDDIR}/ircd.m4
	$(TOUCH) ${IRCDDIR}/ircd.m4
	chmod +x buildm4
	./buildm4 ${IRCDDIR}

clean:
	$(RM) -f *.o *~ core ircd version.c \#* *.bak

depend:
	makedepend -I${INCLUDEDIR} ${SRC}

# Pfft

bsd.o: bsd.c ../include/common.h ../include/sys.h ../include/config.h\
	../include/comstud.h
	$(CC) $(CFLAGS) -c bsd.c

dbuf.o: dbuf.c ../include/config.h ../include/common.h ../include/dbuf.h\
	../include/comstud.h ../include/sys.h dbuf.c
	$(CC) $(CFLAGS) -c dbuf.c

packet.o: packet.c ../include/comstud.h ../include/struct.h\
	../include/config.h ../include/class.h ../include/dbuf.h\
	../include/common.h ../include/msg.h
	$(CC) $(CFLAGS) -c packet.c

parse.o: parse.c ../include/comstud.h ../include/struct.h\
	../include/config.h ../include/class.h ../include/dbuf.h\
	../include/common.h ../include/msg.h ../include/sys.h \
	../include/numeric.h
	$(CC) $(CFLAGS) -c parse.c

send.o: send.c ../include/struct.h ../include/config.h ../include/class.h\
	../include/comstud.h ../include/dbuf.h ../include/common.h
	$(CC) $(CFLAGS) -c send.c

match.o: match.c ../include/comstud.h ../include/config.h ../include/common.h\
	../include/sys.h
	$(CC) $(CFLAGS) -c match.c

support.o: support.c ../include/comstud.h ../include/config.h\
	../include/common.h ../include/sys.h support.c
	$(CC) $(CFLAGS) -c support.c

inet_addr.o: inet_addr.c ../include/comstud.h ../include/sys.h\
	../include/common.h
	$(CC) $(CFLAGS) -c inet_addr.c

channel.o: ../include/struct.h ../include/config.h ../include/dbuf.h \
	   ../include/numeric.h ../include/channel.h ../include/sys.h \
	   ../include/common.h ../include/comstud.h
	$(CC) $(CFLAGS) -c channel.c

class.o: ../include/struct.h ../include/class.h ../include/numeric.h \
	 ../include/common.h ../include/config.h ../include/sys.h \
	../include/comstud.h
	$(CC) $(CFLAGS) -c class.c

ircd.o: ircd.c ../include/struct.h ../include/config.h ../include/sys.h \
	../include/dbuf.h ../include/numeric.h ../include/common.h \
	../include/comstud.h
	$(CC) $(CFLAGS) -c ircd.c

list.o: list.c ../include/struct.h ../include/config.h ../include/dbuf.h \
	../include/sys.h ../include/common.h ../include/comstud.h
	$(CC) $(CFLAGS) -c list.c

res.o: res.c ../include/struct.h ../include/config.h ../include/res.h \
	   ../include/sys.h ../include/common.h ../include/comstud.h
	$(CC) $(CFLAGS) -c res.c

s_bsd.o: s_bsd.c ../include/struct.h ../include/config.h ../include/dbuf.h \
	 ../include/sys.h ../include/common.h ../include/comstud.h
	$(CC) $(CFLAGS) -c s_bsd.c

s_auth.o: s_auth.c ../include/struct.h ../include/config.h ../include/dbuf.h \
	  ../include/sys.h ../include/common.h ../include/comstud.h
	$(CC) $(CFLAGS) -c s_auth.c

s_conf.o: s_conf.c ../include/struct.h ../include/config.h ../include/sys.h \
	  ../include/common.h ../include/numeric.h  ../include/dbuf.h \
	  ../include/comstud.h ../include/dich_conf.h
	$(CC) $(CFLAGS) -c s_conf.c

s_debug.o: ../include/config.h ../include/struct.h ../include/common.h \
	  ../include/sys.h ../include/comstud.h s_debug.c 
	$(CC) $(CFLAGS) -c s_debug.c

s_err.o: ../include/config.h ../include/struct.h ../include/common.h \
	  ../include/sys.h ../include/numeric.h ../include/msg.h \
	  ../include/comstud.h s_err.c
	$(CC) $(CFLAGS) -c s_err.c

s_misc.o: s_misc.c ../include/struct.h ../include/config.h ../include/dbuf.h \
	  ../include/common.h ../include/sys.h ../include/numeric.h \
	  ../include/comstud.h
	$(CC) $(CFLAGS) -c s_misc.c

s_user.o: s_user.c ../include/struct.h ../include/config.h ../include/sys.h \
	  ../include/common.h ../include/dbuf.h ../include/channel.h \
	  ../include/msg.h ../include/numeric.h ../include/comstud.h
	$(CC) $(CFLAGS) -c s_user.c

s_serv.o: s_serv.c ../include/struct.h ../include/config.h ../include/sys.h \
	  ../include/common.h ../include/dbuf.h ../include/channel.h \
	  ../include/msg.h ../include/numeric.h ../include/comstud.h
	$(CC) $(CFLAGS) -c s_serv.c

s_numeric.o: s_numeric.c ../include/config.h ../include/sys.h \
	  ../include/common.h ../include/struct.h ../include/dbuf.h \
	  ../include/numeric.h ../include/comstud.h
	$(CC) $(CFLAGS) -c s_numeric.c

whowas.o: whowas.c ../include/struct.h ../include/config.h \
	../include/sys.h ../include/common.h ../include/dbuf.h \
	../include/numeric.h ../include/comstud.h
	$(CC) $(CFLAGS) -c whowas.c

hash.o: hash.c ../include/struct.h ../include/sys.h ../include/hash.h \
	../include/common.h ../include/config.h ../include/comstud.h
	$(CC) $(CFLAGS) -c hash.c

dich_conf.o: dich_conf.c ../include/dich_conf.h ../include/struct.h \
	../include/common.h ../include/comstud.h ../include/sys.h
	$(CC) $(CFLAGS) -c dich_conf.c

clone.o: clone.c ../include/comstud.h ../include/config.h\
	../include/struct.h
	$(CC) $(CFLAGS) -c clone.c

fdlist.o: fdlist.c ../include/comstud.h ../include/config.h\
	../include/struct.h
	$(CC) $(CFLAGS) -c fdlist.c

dog3.o: dog3.c ../include/comstud.h ../include/config.h\
	../include/struct.h
	$(CC) $(CFLAGS) -c dog3.c

gline.o: gline.c ../include/comstud.h ../include/config.h\
	../include/struct.h ../include/gline.h
	$(CC) $(CFLAGS) -c gline.c

#dough_malloc.o: dough_malloc.c ../include/comstud.h ../include/config.h\
#	../include/struct.h ../include/dough_malloc.h
#	$(CC) $(CFLAGS) -c dough_malloc.c

