/************************************************************************
 *   IRC - Internet Relay Chat, include/hash.h
 *   Copyright (C) 1991 Darren Reed
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

#ifndef	__hash_include__
#define __hash_include__

typedef	struct	hashentry {
	int	hits;
	int	links;
	void	*list;
	} aHashEntry;

#define	DEFAULT_HASHSIZE	16333   /* prime number */
#define	DEFAULT_CHANNELHASHSIZE	7019	/* prime number */

extern	int	HASHSIZE;
extern	int	CHANNELHASHSIZE;

#endif	/* __hash_include__ */
