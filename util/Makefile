#************************************************************************
#*   IRC - Internet Relay Chat, util/Makefile (was ircd/crypt/Makefile)
#*   Copyright (C) 1991 Darren Reed
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

SHELL=/bin/sh

MAKE = make 'CFLAGS=${CFLAGS}' 'CC=${CC}' 'IRCDLIBS=${IRCDLIBS}' \
	'LDFLAGS=${LDFLAGS}' 'IRCDMODE=${IRCDMODE}'

all: mkpasswd chkconf
crypt: install

mkpasswd: mkpasswd.c
	$(CC) $(CFLAGS) mkpasswd.c $(LDFLAGS) $(IRCDLIBS) -o mkpasswd

chkconf:
	@if [ -f ../include/setup.h ]; then \
		if [ ! -f match.o ]; then \
			echo "Building match.c";\
			$(CC) -I../include $(CFLAGS) -c match.c;\
		fi;\
		echo "Creating chkconf";\
		$(CC) -I../include $(CFLAGS) chkconf.c match.o $(LDFLAGS) $(IRCDLIBS) -o chkconf; \
	else \
		echo "Need to run configure in previous directory first"; \
	fi

install:
	@echo "Install them yourself!"

clean:
	/bin/rm -f mkpasswd chkconf match.o chkconf.o mkpasswd.o
