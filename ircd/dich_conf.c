/************************************************************************
 *   IRC - Internet Relay Chat, ircd/dich_conf.c
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

#include <string.h>
#include "common.h"
#include "struct.h"

#undef STDLIBH
#include "sys.h"

#include "h.h"
#include "dich_conf.h"

#ifndef SYSV
#define memmove(x,y,N) bcopy(y,x,N)
#endif

void report_conf_links(sptr, List, numeric, c)
aClient *sptr;
aConfList *List;
int	numeric;
char	c;
{
	register aConfItem *tmp;
	register int current;
	char *host, *pass, *name;
	static	char	null[] = "<NULL>";
	int port;

	if (!List || !List->conf_list)
		return;

	for (current = 0; current < List->length; current++)
	{
		aConfEntry *ptr = &List->conf_list[current];

		for(;ptr;ptr=ptr->next)
		{
			if (ptr->sub)
				report_conf_links(sptr, ptr->sub, numeric, c);
			if (!(tmp=ptr->conf))
				continue;
			host = BadPtr(tmp->host) ? null : tmp->host;
			pass = BadPtr(tmp->passwd) ? null : tmp->passwd;
			name = BadPtr(tmp->name) ? null : tmp->name;
			port = (int)tmp->port;
			if (tmp->status == CONF_KILL)
				sendto_one(sptr, rpl_str(numeric), me.name,
					sptr->name, c, host, pass,
					name, port, get_conf_class(tmp));
			else
				sendto_one(sptr, rpl_str(numeric), me.name,
					sptr->name, c, host, name, port,
					get_conf_class(tmp));
		}
	} 
}

void reverse(s1, s2)
char *s1;
char *s2;
{
	char *start_point;

	start_point = s2;

	while(*s2)
		s2++;
	s2--;
	while(s2 >= start_point)
		*s1++ = *s2--;
	*s1 = '\0';
}

/*
 * This function decides whether a string may be used with ordered lists.
 * It returns 1 if the string can be stored as is in an ordered list.
 * -1 means the string has to be reversed. A string that can't be put in
 * an ordered list yields 0 and should be manipulated with the linear
 * functions : l_addto_conf_list() and l_find_matching_conf(). -Sol
 */
int
sortable(my_string)
char	*my_string;
{
	char	*p;
	char	*rev;

	if (!my_string)
		return 0;	/* NULL patterns aren't allowed in ordered
				   lists (will have to use linear functions)
				   -Sol */

	if (strchr(my_string, '?'))
		return 0;	/* reject strings with '?' as non-sortable
				   whoever uses '?' patterns anyway ? -Sol */

	for (p=my_string;*p && (*p == '*');p++);
	if (!*p)
		return 0;	/* only wildcards, not good -Sol */

	for (;*p && *p != '*';p++);
	if (!*p)
		return -1; /* string of the form *word : needs reversal -Sol */

	rev = (char *) MyMalloc(strlen(my_string)+1);
	reverse(rev, my_string);

	for (p=rev;*p && (*p == '*');p++);
	for (;*p && *p != '*';p++);
	if (!*p)
	{
		MyFree(rev);
		return 1; /* string of the form word* -Sol */
	}

	MyFree(rev);
        return (0);
}

/*
 * The following functions extract from the conf-lines the strings
 * that we need. They HAVE to allocate space to save the string.
 * That space will be freed by clear_conf_list(). -Sol
 */
char		*
host_field(my_conf)
aConfItem	*my_conf;
{
	char	*tmp;
	char	*ptr;

	if (!my_conf || !my_conf->host)
		return NULL;

	if ((ptr = strchr(my_conf->host, '@')) != NULL)
		ptr++;
	else
		ptr = my_conf->host;
	tmp = (char *) MyMalloc(strlen(ptr)+1);
	strcpy(tmp, ptr);

	return tmp;
}

char		*
name_field(my_conf)
aConfItem	*my_conf;
{
	char	*tmp;
	char	*ptr;

	if (!my_conf || !my_conf->name)
		return NULL;

	if ((ptr = strchr(my_conf->name, '@')) != NULL)
		ptr++;
	else
		ptr = my_conf->host;
	tmp = (char *) MyMalloc(strlen(ptr)+1);
	strcpy(tmp, ptr);

	return tmp;
}

char		*
rev_host_field(my_conf)
aConfItem	*my_conf;
{
	char	*tmp;
	char	*ptr;

	if (!my_conf || !my_conf->host)
		return NULL;

	if ((ptr = strchr(my_conf->host, '@')) != NULL)
		ptr++;
	else
		ptr = my_conf->host;
	tmp = (char *) MyMalloc(strlen(ptr)+1);
	reverse(tmp, ptr);

	return tmp;
}

char		*
rev_name_field(my_conf)
aConfItem	*my_conf;
{
	char	*tmp;
	char	*ptr;

	if (!my_conf || !my_conf->name)
		return NULL;

	if ((ptr = strchr(my_conf->name, '@')) != NULL)
		ptr++;
	else
		ptr = my_conf->host;
	tmp = (char *) MyMalloc(strlen(ptr)+1);
	strcpy(tmp, ptr);
	reverse(tmp);

	return tmp;
}

/*
 * In order not to realloc memory space each time we add an entry, we do it
 * in blocks of 100 ConfEntry structures. Returns 1 if we reallocated. -Sol
 */
static int
grow_list(my_list)
aConfList	*my_list;
{
	unsigned int	length = my_list->length;
	aConfEntry	*base = my_list->conf_list;

	if ((length % 5) == 0)	/* Re-alloc only after 5 entries -Sol */
	{
		/* List is going to grow : allocate 5 more entries
		   Choose 5 to keep the list as small as possible -Sol */
		aConfEntry	*new;

		new = (aConfEntry *) MyMalloc((length+100)*sizeof(aConfEntry));
		memcpy(new, base, length*sizeof(aConfEntry));
		if (base)
		{
			memcpy(new, base, length*sizeof(aConfEntry));
			MyFree(base);
		}
		my_list->conf_list = base = new;

		return 1;
	}

	return 0;
}

/*
 * This function inserts an entry at the correct location (lexicographicaly
 * speakin) in the configuration list.
 * If the new entry matches entries already in the list, we replace them
 * with the new entry and chain them as a list in its "sub" field.
 * If the field chosen as a criteria is already in the list, we chain the
 * new entry. -Sol
 */
void
addto_conf_list(my_list, my_conf, cmp_field)
aConfList	*my_list;
aConfItem	*my_conf;
char		*(*cmp_field)();
{
	char			*field = cmp_field(my_conf);
	unsigned int		length = my_list->length;
	aConfEntry		*base = my_list->conf_list;
	register unsigned int	lower = 0;
	register unsigned int	upper = length-1;
	register unsigned int	compare = (length-1)/2;
	register int		pos;

	if (!field)
		return;	/* Reject NULL patterns -Sol */

	if (!length)
	{
		/* List is still empty : add the entry and exit -Sol */
		if (grow_list(my_list))
			base = my_list->conf_list;
		base->pattern = field;
		base->conf = my_conf;
		base->next = NULL;
		base->sub = NULL;
		my_list->length++;
		return;
	}

	while (lower != upper)
	{
		pos = strcasecmp(field, base[compare].pattern);
		if ((pos == 0) || !matches(field, base[compare].pattern) ||
		    !matches(base[compare].pattern, field))
		{
			lower = upper = compare;
			break;
		}
		else
			if (pos < 0)
				upper = compare;
			else
				if (lower == compare)
					lower = compare+1;
				else
					lower = compare;
		compare = (lower+upper)/2;
	}

	pos = strcasecmp(field, base[compare].pattern);

	if (!pos)
	{
		aConfEntry	*new;

		MyFree(field);
		new = (aConfEntry *) MyMalloc(sizeof(aConfEntry));
		new->pattern = NULL;
		new->conf = my_conf;
		new->next = base[compare].next;
		new->sub = NULL;
		base[compare].next = new;
	}
	else
	{
		if (!matches(base[compare].pattern, field))
		{
			if (!base[compare].sub)
			{
				base[compare].sub = (aConfList *)
					MyMalloc(sizeof(aConfList));
				memset(base[compare].sub, 0, sizeof(aConfList));
			}
			addto_conf_list(base[compare].sub, my_conf, cmp_field);
			return;
		}
		if (!matches(field, base[compare].pattern))
		{
			unsigned int	bottom, top;
			aConfList	*new;

			/* Look for entries to be moved to sublist. -Sol */
			bottom = compare;
			while ((bottom > 0) &&
			       !matches(field, base[bottom-1].pattern))
				bottom--;
			top = compare;
			while ((top < length-1) &&
			       !matches(field, base[top+1].pattern))
				top++;

			/* Create sublist -Sol */
			new = (aConfList *) MyMalloc(sizeof(aConfList));
			new->length = top-bottom+1;
			new->conf_list = (aConfEntry *)
				MyMalloc((new->length/5+1)*5*
					sizeof(aConfEntry));
			memcpy(new->conf_list, &base[bottom],
				new->length*sizeof(aConfEntry));
			/* Pack entries -Sol */
			memmove(&base[bottom+1], &base[top+1],
				(length-top-1)*sizeof(aConfEntry));
			/* Don't worry if we are using more memory than
			   necessary : it will be adjusted next time we
			   realloc. -Sol */
			base[bottom].pattern = field;
			base[bottom].conf = my_conf;
			base[bottom].next = NULL;
			base[bottom].sub = new;
			my_list->length -= (top-bottom);
			return;
		}
		if (grow_list(my_list))
			base = my_list->conf_list;
		if (pos > 0)
		{
			/* Add after -Sol */
			memmove(&base[compare+2], &base[compare+1],
				(length-1-compare)*sizeof(aConfEntry));
			base[compare+1].pattern = field;
			base[compare+1].conf = my_conf;
			base[compare+1].next = NULL;
			base[compare+1].sub = NULL;
		}
		else
		{
			/* Add before -Sol */
			memmove(&base[compare+1], &base[compare],
				(length-compare)*sizeof(aConfEntry));
				base[compare].pattern = field;
			base[compare].conf = my_conf;
			base[compare].next = NULL;
			base[compare].sub = NULL;
		}
		my_list->length++;
	}
}

/*
 * As the name says : this clears a configuration list. -Sol
 */
void
clear_conf_list(my_list)
aConfList	*my_list;
{
	if (!my_list)
		return;

	if (my_list->conf_list)
	{
		int	current;

		/* Loop through all patterns to free conf-entries
		   with the same pattern -Sol */
                for (current = 0; current < my_list->length; current++)
                {
                        aConfEntry      *ptr = &my_list->conf_list[current];

                        if (ptr->sub)
                        {
                                clear_conf_list(ptr->sub);
                                MyFree(ptr->sub);
                        }
			if (ptr->conf)
				free_conf(ptr->conf);
                        while (ptr->next)
                        {
                                aConfEntry      *tmp = ptr->next->next;

                                if (ptr->next->pattern)
                                        MyFree(ptr->next->pattern);
				if (ptr->next->sub)
				{
					clear_conf_list(ptr->next->sub);
					MyFree(ptr->next->sub);
				}
				if (ptr->next->conf)
					free_conf(ptr->next->conf);
                               	MyFree(ptr->next);
                                ptr->next = tmp;
                        }
                        if (ptr->pattern)
                                MyFree(ptr->pattern);
                }
                MyFree(my_list->conf_list);
	}

	my_list->length = 0;
	my_list->conf_list = NULL;
}

/*
 * This function returns the first matching configuration line if both
 * my_list and tomatch aren't NULL. Otherwise, it returns the next one.
 * It yields NULL if there are no (more) matches. -Sol
 */
aConfItem	*
find_matching_conf(my_list, tomatch)
aConfList	*my_list;
char		*tomatch;
{
	static	aConfEntry	*matching;
	static	aConfEntry	*offset;
	aConfEntry		*base;
	unsigned int		lower, upper, compare;
	static	char		*name;

	if (my_list && tomatch)
	{
		matching = NULL;

		if (!my_list->length)
			return NULL;

		base = my_list->conf_list;
		lower = 0;
		upper = my_list->length-1;
		compare = (lower+upper)/2;
		name = tomatch;

		while (lower != upper)
		{
			int	pos;

			if (!matches(base[compare].pattern, name))
			{
				matching = offset = &base[compare];
				return base[compare].conf;
			}

			pos = strcasecmp(name, base[compare].pattern);

			if (pos < 0)
				upper = compare;
			else
				if (lower == compare)
					lower = compare+1;
				else
					lower = compare;
			compare = (lower+upper)/2;
		}

		if (!matches(base[compare].pattern, name))
		{
			matching = offset = &base[compare];
			return base[compare].conf;
		}

		return NULL;
	}
	else
		if (!matching)
			return NULL;
		else
		{
			if (offset->next)
			{
				offset = offset->next;
				return offset->conf;
			}
			if (!matching->sub)
			{
				matching = NULL;
				return NULL;
			}
			return find_matching_conf(matching->sub, name);
		}
}

/*
 * This simply adds a configuration line at the end of the list.
 * Of course, it makes no sense to use addto_conf_list() and
 * l_addto_conf_list() on the same list unless you want to garble it. -Sol
 */
void
l_addto_conf_list(my_list, my_conf, cmp_field)
aConfList	*my_list;
aConfItem	*my_conf;
char		*(*cmp_field)();
{
	char		*field = cmp_field(my_conf);
	unsigned int	length = my_list->length;
	aConfEntry	*base;

	(void) grow_list(my_list);
	base = my_list->conf_list;

	base[length].pattern = field;
	base[length].conf = my_conf;
	base[length].next = NULL;
	base[length].sub = NULL;
	my_list->length++;
}

/*
 * This looks for the first match in the list if my_list and tomatch are both
 * not NULL, otherwise it will return the next match.
 * NULL means there are no (more) matches.
 * This should be used only on lists created with l_addto_conf_list() since
 * it won't test the "next" attribute of aConfEntry, therefore missing entries
 * if there were chained entries. -Sol
 */
aConfItem	*
l_find_matching_conf(my_list, tomatch)
aConfList	*my_list;
char		*tomatch;
{
	static	aConfEntry	*base;
	static	unsigned int	current;
	static	unsigned int	length;
	static	char		*name;

	if (my_list && tomatch)
		if (!my_list->length)
		{
			current = -1;
			return NULL;
		}

	if (!my_list || !tomatch)
	{
		if (current == -1)
			return NULL;
		current++;
	}
	else
	{
		base = my_list->conf_list;
		current = 0;
		length = my_list->length;
		name = tomatch;
	}

	while (current < length)
	{
		if (!base[current].pattern)
			return base[current].conf;	/* Let's say NULL
							   matches all -Sol */
		else
			if (!matches(base[current].pattern, name))
				return base[current].conf;
		current++;
	}

	current = -1;
	return NULL;
}
