/************************************************************************
 *   IRC - Internet Relay Chat, include/dich_conf.h
 *   Copyright (C) 1995 Philippe Levan
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
 * The dich_conf.h and dich_conf.c were written to provide a generic interface
 * for configuration lines matching.
 * I decided to write it after I read Roy's K: line tree patch.
 * the underlying ideas are the following :
 * . get rid of the left/right branches by using a dichotomy on an ordered
 *   list
 * . arrange patterns matching one another in a tree so that if we find a
 *   possible match, all other possible matches are "below"
 * These routines are meant for fast matching. There is no notion of "best"
 * of "first" (meaning the order in which the lines are read) match.
 * Therefore, the following functions are fit for K: lines matching but not
 * I: lines matching (as sad as it may be).
 * Other kinds of configuration lines aren't as time consuming or just aren't
 * use for matching so it's irrelevant to try and use these functions for
 * anything else. However, I still made them as generic as possible, just
 * in case.
 *
 * -Soleil (Philippe Levan)
 *
 */

/*
**  $Id: dich_conf.h,v 1.1.1.1 1997/07/23 18:02:02 cbehrens Exp $
*/

#ifndef DICH_CONF_H
#define DICH_CONF_H

#include "struct.h"

typedef struct ConfList
{
	unsigned int	length;
	unsigned int	size;
	struct ConfEntry *conf_list;
} aConfList;

typedef struct DichConf
{
	struct ConfList	Forward;
	struct ConfList	Backward;
	struct ConfList	Unsorted;
} aDichConf;

typedef struct ConfEntry
{
	char			*pattern;
	time_t			expires;
	struct ConfItem		*conf;
	struct ConfEntry	*next;
	struct ConfList		*sub;
} aConfEntry;

extern	void		memory_dichconf();
extern	void		flush_dichconf();
extern	void		mark_dichconf();
extern	void		init_dichconf();
extern	void		clear_dichconf();
extern	void		addto_dichconf();
extern	void		report_dichconf_links();
extern	aConfItem	*find_dichconf(/*
				aDichConf *dconf,
				char *username,
				char *host;
				char *ip,
				int best,
				char *reply */);

#endif /* DICH_CONF_H */
