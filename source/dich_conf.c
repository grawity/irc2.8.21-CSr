/************************************************************************
 *   IRC - Internet Relay Chat, ircd/dich_conf.c
 *   Copyright (C) 1995-1996 Philippe Levan
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

#ifndef lint
static  char rcsid[] = "@(#)$Id: dich_conf.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $";
#endif

#include "struct.h"
#include "sys.h"
#include "h.h"
#include "dich_conf.h"
#include "common.h"


/*
 * This function is used for /stats commands if you use dich_conf.
 * Thank you to Chris Behrens for this addition -Sol
 */
static void _dich_report_conf(sptr, List, numeric, c, muser, mhost, num)
aClient		*sptr;
aConfList	*List;
int		numeric;
char		c;
char		*muser;
char		*mhost;
int		*num;
{
	register aConfItem	*tmp;
	register int		current;
	char			*host, *pass, *name;
	static char		null[] = "<NULL>";
	char			string[10];
	int			port;

	if (!List || !List->conf_list)
		return;

	for (current = 0; current < List->length; current++)
	{
		aConfEntry	*ptr = &List->conf_list[current];

		for(; ptr; ptr = ptr->next)
		{
			if (ptr->sub)
				_dich_report_conf(sptr, ptr->sub, numeric, c,
					muser, mhost, num);
			if (!(tmp = ptr->conf))
				continue;
			host = BadPtr(tmp->host) ? null : tmp->host;
			pass = BadPtr(tmp->passwd) ? null : tmp->passwd;
			name = BadPtr(tmp->name) ? null : tmp->name;
			port = (int)tmp->port;
			*string = (char) 0;
			if (IsNoTilde(tmp))
				strcat(string, "-");
			if (IsLimitIp(tmp))
				strcat(string, "!");
			if (IsNeedIdentd(tmp))
				strcat(string, "+");
			if (IsPassIdentd(tmp))
				strcat(string, "$");
			if (IsNoMatchIp(tmp))
				strcat(string, "%");
			if (muser)
			{
				if ((!strcmp(muser, "*") ||
					!match(name,muser)) &&
					!match(host,mhost))
					;
				else
					continue;
			}					
			if (num)
				(*num)++;
			if (tmp->status & CONF_ILLEGAL)
				continue;
			if (tmp->expires && (tmp->expires < NOW))
				tmp->status |= CONF_ILLEGAL;
			if (tmp->status & CONF_KILL)
				sendto_one(sptr, rpl_str(numeric), me.name,
					sptr->name, c, host, pass,
					name, port, get_conf_class(tmp));
			else if (tmp->status & CONF_CLIENT)
				sendto_one(sptr, rpl_str(numeric), me.name,
					sptr->name, c, string, host,
					name, port, get_conf_class(tmp));
			else if (tmp->status & CONF_GLINE)
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

/*
 * The following function reverses string s2 into string s1 (space for
 * s1 must have been pre-allocated.
 * Thanks to Diane Bruce. -Sol
 */
static void _dich_reverse(s1, s2)
char *s1;
char *s2;
{
	char	*start_point;

	start_point = s2;

	while(*s2)
		s2++;
	s2--;
	while(s2 >= start_point)
		*s1++ = *s2--;
	*s1 = '\0';
}

#if OLD_SORTABLE

/*
 * This function decides whether a string may be used with ordered lists.
 * It returns 1 if the string can be stored as is in an ordered list.
 * -1 means the string has to be reversed. A string that can't be put in
 * an ordered list yields 0 and should be manipulated with the linear
 * functions : _dich_l_addto_list() and _dich_l_find_conf(). -Sol
 */
static int _dich_sortable(my_string)
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

	for (p = my_string; *p == '*'; p++);
	if (!*p)
		return 0;	/* only wildcards, not good -Sol */

	for (; *p && *p != '*'; p++);
	if (!*p)
		return -1; /* string of the form *word : needs reversal -Sol */

	rev = (char *) MyMalloc(strlen(my_string)+1);
	_dich_reverse(rev, my_string);

	for (p = rev; *p == '*'; p++);
	for (; *p && *p != '*'; p++);
	if (!*p)
	{
		MyFree(rev);
		return 1; /* string of the form word* -Sol */
	}

	MyFree(rev);
        return (0);
}
#else

/*
 * This function decides whether a string may be used with ordered lists.
 * It returns 1 if the string can be stored as is in an ordered list.
 * -1 means the string has to be reversed. A string that can't be put in
 * an ordered list yields 0 and should be manipulated with the linear
 * functions : l_addto_conf_list() and l_find_matching_conf(). -Sol
 *
 * a little bit rewritten, and yes. I tested it. it is faster.
 *      - Dianora
 */

static int _dich_sortable(my_string)
char *my_string;
{
	int state=0;
	char *p;

	if (!my_string)
		return 0;	/* NULL patterns aren't allowed in ordered
				   lists (will have to use linear functions)
						-Sol
				*/

	if (strchr(my_string, '?'))
		return 0;	/* reject strings with '?' as non-sortable
				   whoever uses '?' patterns anyway ?
						-Sol
				*/
	for(p=my_string;;)
	{
		switch(state)
		{
			case 0:
				if (*p == '*')
					state = 1;
				else
					state = 4;
				break;
			case 1:
				if (!*p)
					/* only wildcards, not good -Sol */
					return 0;
				else if (*p != '*')
					state = 2;
				break;
			case 2:
				if (!*p)
					return -1;
				else if (*p == '*')
					return 0;
				break;

			case 4:
				if (*p == '*')
					state = 5;
				else if (!*p)
					return 1; 
				break;
			case 5:
				if (!*p)
					return 1;
				else if (*p != '*')
					return 0;
				break;
			default:
				state = 0;
			break;
		}
		p++;
	}
	/* NOT REACHED */
}

#endif


/*
 * The following functions extract from the conf-lines the strings
 * that we need. They HAVE to allocate space to save the string.
 * That space will be freed by _dich_clear_list(). -Sol
 */
static char *_dich_host_field(my_conf)
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

static char *_dich_rev_host_field(my_conf)
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
	_dich_reverse(tmp, ptr);

	return tmp;
}

/*
 * In order not to realloc memory space each time we add an entry, we do it
 * in blocks of 100 ConfEntry structures. Returns 1 if we reallocated. -Sol
 */
static int _dich_grow_list(my_list)
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
		my_list->size = length+100;
		if (base)
		{
			bcopy(base, new, length*sizeof(aConfEntry));
			MyFree(base);
		}
		my_list->conf_list = new;

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
static void _dich_addto_conf(my_list, my_conf, cmp_field)
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
		if (_dich_grow_list(my_list))
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
		if ((pos == 0) || !match(field, base[compare].pattern) ||
		    !match(base[compare].pattern, field))
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
		if (!match(base[compare].pattern, field))
		{
			if (!base[compare].sub)
			{
				base[compare].sub = (aConfList *)
					MyMalloc(sizeof(aConfList));
				bzero(base[compare].sub, sizeof(aConfList));
			}
			_dich_addto_conf(base[compare].sub, my_conf, cmp_field);
			MyFree(field);
			return;
		}
		if (!match(field, base[compare].pattern))
		{
			unsigned int	bottom, top;
			aConfList	*new;

			/* Look for entries to be moved to sublist. -Sol */
			bottom = compare;
			while ((bottom > 0) &&
			       !match(field, base[bottom-1].pattern))
				bottom--;
			top = compare;
			while ((top < length-1) &&
			       !match(field, base[top+1].pattern))
				top++;

			/* Create sublist -Sol */
			new = (aConfList *) MyMalloc(sizeof(aConfList));
			new->length = top-bottom+1;
			new->size = (new->length/5+1)*5;
			new->conf_list = (aConfEntry *)
				MyMalloc(new->size * sizeof(aConfEntry));
			bcopy(&base[bottom], new->conf_list,
				new->length*sizeof(aConfEntry));
			/* Pack entries -Sol */
			bcopy(&base[top+1], &base[bottom+1],
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
		if (_dich_grow_list(my_list))
			base = my_list->conf_list;
		if (pos > 0)
		{
			/* Add after -Sol */
			bcopy(&base[compare+1], &base[compare+2],
				(length-1-compare)*sizeof(aConfEntry));
			base[compare+1].pattern = field;
			base[compare+1].conf = my_conf;
			base[compare+1].next = NULL;
			base[compare+1].sub = NULL;
		}
		else
		{
			/* Add before -Sol */
			bcopy(&base[compare], &base[compare+1],
				(length-compare)*sizeof(aConfEntry));
				base[compare].pattern = field;
			base[compare].conf = my_conf;
			base[compare].next = NULL;
			base[compare].sub = NULL;
		}
		my_list->length++;
	}
}

/* Rebuilds the dichconf, removing ILLEGAL confs, freeing if not in use */
static void _dich_flush_conf(my_list)
aConfList	*my_list;
{
	aConfEntry *tmp, **tmp2, *tmp3;
	aConfEntry **base;
	aConfList newlist;
	int	current;
	int	*i;

	if (!my_list)
		return;
	newlist.length = 0;
	newlist.conf_list = NULL;

	if (my_list->conf_list)
	{
		/* Loop through all patterns to free conf-entries
		   with the same pattern -Sol */
		_dich_grow_list(&newlist);
		base = &(newlist.conf_list);
		(*base)->sub = NULL;
		(*base)->conf = NULL;
		(*base)->next = NULL;
		i = &(newlist.length);
		*i = 0;
		for (current = 0; current < my_list->length; current++)
		{
			tmp = &my_list->conf_list[current];

			if (tmp->sub)
			{
				_dich_flush_conf(tmp->sub);
				if (tmp->sub->length == 0)
				{
					if (tmp->sub->conf_list)
						MyFree(tmp->sub->conf_list);
					MyFree(tmp->sub);
					tmp->sub = NULL;
				}
			}
			for(tmp2 = &(tmp->next);*tmp2;tmp2 = &(*tmp2)->next )
			{
				if (!((*tmp2)->conf) ||
				((*tmp2)->conf->status & CONF_ILLEGAL) ||
				((*tmp2)->conf->expires &&
					((*tmp2)->conf->expires < NOW)))
				{
					tmp3 = *tmp2;
					*tmp2 = tmp3->next;
					if (tmp3->conf && !tmp3->conf->clients)
						free_conf(tmp3->conf);
					MyFree(tmp3);
					if (!(*tmp2))
						break;
				}
			}
			if (!tmp->conf || (tmp->conf->status & CONF_ILLEGAL) ||
				(tmp->conf->expires &&
					(tmp->conf->expires < NOW)))
			{
				if (tmp->next)
				{
					tmp->next->pattern = tmp->pattern;
					tmp->next->sub = tmp->sub;
					if (_dich_grow_list(&newlist))
						base = &(newlist.conf_list);
					bcopy(tmp->next,
						&((*base)[*i]),
						sizeof(aConfEntry));
					(*i)++;
				}
				else if (tmp->sub)
				{
					aConfEntry *temp;

					newlist.size=tmp->sub->length+(*i)+100;
					temp = (aConfEntry *)MyMalloc(
					sizeof(aConfEntry)*newlist.size);
					bcopy(*base, temp,
						sizeof(aConfEntry)*(*i));
					MyFree((*base));
					*base = temp;
					bcopy(tmp->sub->conf_list,
						&((*base)[*i]),
						sizeof(aConfEntry)*
						tmp->sub->length);
					(*i) += tmp->sub->length;
					MyFree(tmp->sub->conf_list);
					MyFree(tmp->sub);
				}
				if (tmp->conf && !tmp->conf->clients)
					free_conf(tmp->conf);
				continue;
			}
			if (_dich_grow_list(&newlist))
				base = &(newlist.conf_list);
			bcopy(tmp, &((*base)[*i]), sizeof(aConfEntry));
			(*i)++;
		}
		MyFree(my_list->conf_list);
		if (newlist.length == 0)
		{
			my_list->conf_list = NULL;
			MyFree(newlist.conf_list);
		}
		else
			my_list->conf_list = newlist.conf_list;
		my_list->length = newlist.length;
		my_list->size = newlist.size;
	}
}

static void _dich_memory_conf(my_list, num, mem)
aConfList	*my_list;
int		*num;
unsigned long	*mem;
{
	aConfEntry *tmp;
	int	current;

	if (!my_list)
		return;

	if (my_list->conf_list)
	{

		/* Loop through all patterns to free conf-entries
		   with the same pattern -Sol */
		(*mem) += sizeof(aConfEntry)*my_list->size;
		for (current = 0; current < my_list->length; current++)
		{
			tmp = &my_list->conf_list[current];
			if (tmp->pattern)
				(*mem) += strlen(tmp->pattern);
			(*num)++;
			if (tmp->sub)
			{
				(*mem) += sizeof(aConfList);
				_dich_memory_conf(tmp->sub, num, mem);
			}
			if (tmp->conf)
			{
				(*mem) += sizeof(aConfItem);
				(*mem) += tmp->conf->host ?
					strlen(tmp->conf->host)+1 : 0;
				(*mem) += tmp->conf->passwd ?
					strlen(tmp->conf->passwd)+1 : 0;
				(*mem) += tmp->conf->name ?
					strlen(tmp->conf->name)+1 : 0;
			}
			tmp = tmp->next;
			while (tmp)
			{
				(*num)++;
				(*mem) += sizeof(aConfEntry);
				if (tmp->conf)
				{
					(*mem) += sizeof(aConfItem);
					(*mem) += tmp->conf->host ?
						strlen(tmp->conf->host)+1 : 0;
					(*mem) += tmp->conf->passwd ?
						strlen(tmp->conf->passwd)+1 : 0;
					(*mem) += tmp->conf->name ?
						strlen(tmp->conf->name)+1 : 0;
				}
				tmp = tmp->next;
			}
		}
	}
}

static void _dich_mark_conf(my_list, flag)
aConfList	*my_list;
int		flag;
{
	aConfEntry *tmp;
	int	current;

	if (!my_list)
		return;

	if (my_list->conf_list)
	{

		/* Loop through all patterns to free conf-entries
		   with the same pattern -Sol */
		for (current = 0; current < my_list->length; current++)
		{
			tmp = &my_list->conf_list[current];

			if (tmp->sub)
				_dich_mark_conf(tmp->sub, flag);
			if (tmp->conf)
				tmp->conf->status |= flag;
			tmp = tmp->next;
			while (tmp)
			{
				if (tmp->conf)
					tmp->conf->status |= flag;
				tmp = tmp->next;
			}
		}
	}
}

/*
 * As the name says : this clears a configuration list. -Sol
 */
static void _dich_clear_list(my_list, clear_conf)
aConfList	*my_list;
int		clear_conf;
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
			aConfEntry	*ptr = &my_list->conf_list[current];

			if (ptr->sub)
			{
				_dich_clear_list(ptr->sub, clear_conf);
				MyFree(ptr->sub);
			}
			if (clear_conf && ptr->conf)
				free_conf(ptr->conf);
			while (ptr->next)
			{
				aConfEntry	*tmp = ptr->next->next;

				if (ptr->next->pattern)
					MyFree(ptr->next->pattern);
				if (clear_conf && ptr->next->conf)
					free_conf(ptr->next->conf);
				/* No need to check ptr->next->sub because
				   a next never has a sub. -Sol */
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
static aConfItem *_dich_find_conf(my_list, tomatch, num_matched)
aConfList	*my_list;
char		*tomatch;
int		*num_matched;
{
	static	aConfEntry	*matching;
	static	aConfEntry	*offset;
	static	int		nummatched = 0;
	aConfEntry		*base;
	unsigned int		lower, upper, compare;
	static	char		*name;
	int			temp;

	if (num_matched)
		*num_matched = 0;
	if (my_list && tomatch)
	{
		matching = NULL;

		if (!my_list->length)
		{
			nummatched = 0;
			return NULL;
		}

		base = my_list->conf_list;
		lower = 0;
		upper = my_list->length-1;
		compare = (lower+upper)/2;
		name = tomatch;

		while (lower != upper)
		{
			int	pos;

			if (temp=matches(base[compare].pattern, name))
			{
				matching = offset = &base[compare];
				nummatched = temp;
				if (num_matched)
					*num_matched = temp;
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

		if (temp = matches(base[compare].pattern, name))
		{
			matching = offset = &base[compare];
			nummatched = temp;
			if (num_matched)
				*num_matched = temp;
			return base[compare].conf;
		}
		nummatched = 0;
		return NULL;
	}
	else
		if (!matching)
		{
			nummatched = 0;
			return NULL;
		}
		else
		{
			if (offset->next)
			{
				offset = offset->next;
				if (num_matched)
					*num_matched = nummatched;
				return offset->conf;
			}
			if (!matching->sub)
			{
				matching = NULL;
				nummatched = 0;
				return NULL;
			}
			return _dich_find_conf(matching->sub, name,
				num_matched);
		}
}

/*
 * Builds a list for non-sortable entries.
 * It doesn't respect at all the semantics of the date structure but only
 * uses it as a storage.
 * Rewritten by Chris Behrens. -Sol
 */
static void _dich_l_addto_conf(my_list, my_conf, cmp_field)
aConfList	*my_list;
aConfItem	*my_conf;
char		*(*cmp_field)();
{
	char		*field = cmp_field(my_conf);
	aConfEntry	*base;

	base = (aConfEntry *) MyMalloc(sizeof(aConfEntry));

	base->pattern = field;
	base->conf = my_conf;
	base->next = my_list->conf_list;
	base->sub = NULL;
	my_list->conf_list = base;
	if (!my_list->length)
		my_list->length++;
}

/*
 * This looks for the first match in the list if my_list and tomatch are both
 * not NULL, otherwise it will return the next match.
 * NULL means there are no (more) matches.
 * This will always ignore sub fields, hence should be used with lists built
 * by _dich_l_addto_conf() only.
 * Modified to be used with Chris Behrens's version of _dich_l_addto_conf().
 * -Sol
 */
static aConfItem *_dich_l_find_conf(my_list, tomatch, num_matched)
aConfList	*my_list;
char		*tomatch;
int		*num_matched;
{
	static	aConfEntry	*base = NULL;
	static	char		*name = NULL;
	int			num = 0;

	if (num_matched)
		*num_matched = 0;

	if (my_list && tomatch)
	{
		if (!my_list->length)
		{
			base = NULL;
			return NULL;
		}
		base = my_list->conf_list;
		name = tomatch;
	}
	else if (!base || !name)
	{
		base = NULL;
		return NULL;
	}
	else
		base = base->next;

        while (base && !(num=matches(base->pattern, name)))
                base = base->next;
	if (num_matched)
		*num_matched = num;

        return base ? base->conf : NULL;
}

aConfItem *find_dichconf(dconf, username, host, ip, best, reply)
aDichConf *dconf;
char *username;
char *host;
char *ip;
int best;
char *reply;
{
	char *rev = NULL;
	aConfList *list;
	aConfItem *tmp, *saveconf = NULL;
	int num1 = 0, num2 = 0, savenum = 0;
	static char nothing[] = "";

	if (!username)
		username = nothing;
	if (!ip || !*ip || strlen(ip) > (size_t) HOSTLEN)
		goto check_host;

	rev = (char *) MyMalloc(strlen(ip)+1);
	_dich_reverse(rev, ip);

	/* Start with hostnames of the form "*word" (most frequent) -Sol */
	list = &(dconf->Backward);
	while ((tmp = _dich_find_conf(list, rev, &num1)) != NULL)
	{
		list = NULL;
		if (tmp->expires && (tmp->expires < NOW))
			continue;
		if (IsNoMatchIp(tmp))
			continue;
		if (best)
		{
			if (!tmp->name)
			{
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
				continue;
			}
			if ((num2 = matches(tmp->name, username)) != 0)
			{
				num1 += num2;
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
			}
			continue;
		}
		if (!tmp->name || !match(tmp->name, username))
		{
			if (!(tmp->status & CONF_KILL))
				goto matched;
			if (BadPtr(tmp->passwd))
				goto matched;
			if (is_comment(tmp->passwd))
				goto matched;
			if (check_time_interval(tmp->passwd, reply))
				goto matched;
		}
        }
        /* Try hostnames of the form "word*" -Sol */
	list = &(dconf->Forward);
        while ((tmp = _dich_find_conf(list, ip, &num1)) != NULL)
        {
		list = NULL;
		if (tmp->expires && (tmp->expires < NOW))
			continue;
		if (IsNoMatchIp(tmp))
			continue;
		if (best)
		{
			if (!tmp->name)
			{
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
				continue;
			}
			if ((num2 = matches(tmp->name, username)) != 0)
			{
				num1 += num2;
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
			}
			continue;
		}
		if (!tmp->name || !match(tmp->name, username))
		{
			if (!(tmp->status & CONF_KILL))
				goto matched;
			if (BadPtr(tmp->passwd))
				goto matched;
			if (is_comment(tmp->passwd))
				goto matched;
			if (check_time_interval(tmp->passwd, reply))
				goto matched;
		}
	}

        list = &(dconf->Unsorted);
        while ((tmp = _dich_l_find_conf(list, ip, &num1)) != NULL)
        {
		list = NULL;
		if (tmp->expires && (tmp->expires < NOW))
			continue;
		if (IsNoMatchIp(tmp))
			continue;
		if (best)
		{
			if (!tmp->name)
			{
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
				continue;
			}
			if ((num2 = matches(tmp->name, username)) != 0)
			{
				num1 += num2;
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
			}
			continue;
		}
		if (!tmp->name || !match(tmp->name, username))
		{
			if (!(tmp->status & CONF_KILL))
				goto matched;
			if (BadPtr(tmp->passwd))
				goto matched;
			if (is_comment(tmp->passwd))
				goto matched;
			if (check_time_interval(tmp->passwd, reply))
				goto matched;
		}
	}
	if (!best && tmp)
		goto matched;
	MyFree(rev);
	if (!host || !*host || !strcmp(host, ip))
	{
		if (best && savenum)
			return saveconf;
		return NULL;
	}
check_host:
	if (!host)
		return NULL;
	if (strlen(host)  > (size_t) HOSTLEN ||
		(username ? strlen(username) : 0) > (size_t) HOSTLEN)
		return NULL;

	rev = (char *) MyMalloc(strlen(host)+1);
	_dich_reverse(rev, host);

	/* Start with hostnames of the form "*word" (most frequent) -Sol */
	list = &(dconf->Backward);
	while ((tmp = _dich_find_conf(list, rev, &num1)) != NULL)
	{
		list = NULL;
		if (tmp->expires && (tmp->expires < NOW))
			continue;
		if (best)
		{
			if (!tmp->name)
			{
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
				continue;
			}
			if ((num2 = matches(tmp->name, username)) != 0)
			{
				num1 += num2;
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
			}
			continue;
		}
		if (!tmp->name || !match(tmp->name, username))
		{
			if (!(tmp->status & CONF_KILL))
				goto matched;
			if (BadPtr(tmp->passwd))
				goto matched;
			if (is_comment(tmp->passwd))
				goto matched;
			if (check_time_interval(tmp->passwd, reply))
				goto matched;
		}
        }
        /* Try hostnames of the form "word*" -Sol */
	list = &(dconf->Forward);
        while ((tmp = _dich_find_conf(list, host, &num1)) != NULL)
        {
		list = NULL;
		if (tmp->expires && (tmp->expires < NOW))
			continue;
		if (best)
		{
			if (!tmp->name)
			{
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
				continue;
			}
			if ((num2 = matches(tmp->name, username)) != 0)
			{
				num1 += num2;
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
			}
			continue;
		}
		if (!tmp->name || !match(tmp->name, username))
		{
			if (!(tmp->status & CONF_KILL))
				goto matched;
			if (BadPtr(tmp->passwd))
				goto matched;
			if (is_comment(tmp->passwd))
				goto matched;
			if (check_time_interval(tmp->passwd, reply))
				goto matched;
		}
        }

        list = &(dconf->Unsorted);
        while ((tmp = _dich_l_find_conf(list, host, &num1)) != NULL)
        {
		list = NULL;
		if (tmp->expires && (tmp->expires < NOW))
			continue;
		if (best)
		{
			if (!tmp->name)
			{
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
				continue;
			}
			if ((num2 = matches(tmp->name, username)) != 0)
			{
				num1 += num2;
				if (num1 > savenum)
				{
					savenum = num1;
					saveconf = tmp;
				}
			}
			continue;
		}
		if (!tmp->name || !match(tmp->name, username))
		{
			if (!(tmp->status & CONF_KILL))
				goto matched;
			if (BadPtr(tmp->passwd))
				goto matched;
			if (is_comment(tmp->passwd))
				goto matched;
			if (check_time_interval(tmp->passwd, reply))
				goto matched;
		}
	}
	if (best)
	{
		MyFree(rev);
		if (savenum)
			return saveconf;
		return NULL;
	}
matched:
	MyFree(rev);
	if (tmp)
		return tmp;
	return NULL;
}

void init_dichconf(dconf)
aDichConf **dconf;
{
	if (!dconf)
		return;
	if (!*dconf)
	{
		*dconf = (aDichConf *)MyMalloc(sizeof(aDichConf));
	}
	(*dconf)->Forward.length = 0;
	(*dconf)->Forward.conf_list = NULL;
	(*dconf)->Backward.length = 0;
	(*dconf)->Backward.conf_list = NULL;
	(*dconf)->Unsorted.length = 0;
	(*dconf)->Unsorted.conf_list = NULL;
}

void report_dichconf_links(sptr, List, numeric, c, muser, mhost, num)
aClient		*sptr;
aDichConf	*List;
int		numeric;
char		c;
char		*muser;
char		*mhost;
int		*num;
{
	if (!List)
		return;
	_dich_report_conf(sptr, &(List->Forward), numeric, c, muser,
		mhost, num);
	_dich_report_conf(sptr, &(List->Backward), numeric, c, muser,
		mhost, num);
	_dich_report_conf(sptr, &(List->Unsorted), numeric, c, muser,
		mhost, num);
}

void clear_dichconf(List, num)
aDichConf	*List;
int		num;
{
	if (!List)
		return;
	_dich_clear_list(&(List->Forward), num);
	_dich_clear_list(&(List->Backward), num);
	_dich_clear_list(&(List->Unsorted), num);
}

void addto_dichconf(List, aconf)
aDichConf *List;
aConfItem *aconf;
{
	char    *host = _dich_host_field(aconf);

	if (!List || !aconf)
		return;
	switch (_dich_sortable(host))
	{
		case 0 :
			_dich_l_addto_conf(&(List->Unsorted), aconf,
				_dich_host_field);
			break;
		case 1 :
			_dich_addto_conf(&(List->Forward), aconf,
				_dich_host_field);
			break;
		case -1 :
			_dich_addto_conf(&(List->Backward), aconf,
				_dich_rev_host_field);
			break;
	}
	MyFree(host);
}

void mark_dichconf(List, flag)
aDichConf *List;
int flag;
{
	if (!List)
		return;
	_dich_mark_conf(&(List->Unsorted), flag);
	_dich_mark_conf(&(List->Forward), flag);
	_dich_mark_conf(&(List->Backward), flag);
}

void flush_dichconf(List)
aDichConf *List;
{
	if (!List)
		return;
	_dich_flush_conf(&(List->Unsorted));
	_dich_flush_conf(&(List->Forward));
	_dich_flush_conf(&(List->Backward));
}

void memory_dichconf(List, num, mem)
aDichConf *List;
int *num;
unsigned long *mem;
{
	if (!num || !mem || !List)
		return;
	*num = 0;
	*mem = sizeof(aDichConf);
	_dich_memory_conf(&(List->Unsorted), num, mem);
	_dich_memory_conf(&(List->Forward), num, mem);
	_dich_memory_conf(&(List->Backward), num, mem);
}

