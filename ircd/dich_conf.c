/*
 * The dich_conf.h and dich_conf.c were written to provide a generic interface
 * for configuration lines matching.
 * I decided to write it after I read Roy's K: line tree patch and was guided
 * by the following question : why use a tree ?
 * The idea of the tree is to have a left branch and a right branch, with
 * "higher" or "lower" entries on one side or the other : that's the same as
 * dichotomizing, which only requires an ordered list.
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
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "dich_conf.h"

#ifndef SYSV
#define memmove(x,y,N) bcopy(y,x,N)
#endif

/*
 * The following 2 functions are, in part, borrowed from Roy's K: line tree
 * patch -Sol
 */
void
reverse(s)
    char s[];
{
        /* Reverse a string, from K&R C 2nd ed.    -roy */

        int c, i, j;
        for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
                c = s[i];
                s[i] = s[j];
                s[j] = c;
        }
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

	for (p=my_string;*p && (*p == '*' || *p == '?');p++);
	if (!*p)
		return 0;	/* only wildcards, not good -Sol */

	for (;*p && *p != '*' && *p != '?';p++);
	if (*p == 0)
		return -1; /* string of the form *word : needs reversal -Sol */

	rev = (char *) MyMalloc(strlen(my_string)+1);
	strcpy(rev, my_string);

	for (p=rev;*p && (*p == '*' || *p == '?');p++);
	for (;*p && *p != '*' && *p != '?';p++);
	if (*p == 0)
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
	strcpy(tmp, ptr);
	reverse(tmp);

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

	if ((length % 100) == 0)
	{
		/* List is going to grow : allocate 100 more entries -Sol */
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
		my_list->length++;
		return;
	}

	while (lower != upper)
	{
		pos = strcmp(field, base[compare].pattern);
		if (pos == 0)
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

	pos = strcmp(field, base[compare].pattern);

	if (!pos)
	{
		aConfEntry	*new;

		MyFree(field);
		new = (aConfEntry *) MyMalloc(sizeof(aConfEntry));
		new->pattern = NULL;
		new->conf = my_conf;
		new->next = base[compare].next;
		base[compare].next = new;
	}
	else
	{
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
		}
		else
		{
			/* Add before -Sol */
			memmove(&base[compare+1], &base[compare],
				(length-compare)*sizeof(aConfEntry));
				base[compare].pattern = field;
			base[compare].conf = my_conf;
			base[compare].next = NULL;
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
			aConfEntry	*ptr = &my_list->conf_list[current];

			while (ptr->next)
			{
				aConfEntry	*tmp = ptr->next->next;

				if (ptr->next->pattern)
					MyFree(ptr->next->pattern);
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
	static	aConfEntry	*base;
	static	aConfEntry	*offset;
	static	unsigned int	lower, upper, compare;
	static	char		*name;
	int			dont_match = 0;

	if (my_list && tomatch)
		if (!my_list->length)
		{
			lower = upper = compare = -1;
			return NULL;
		}

	if (!my_list || !tomatch)
	{
		if (lower == -1)
			return NULL;

		if (offset && offset->next)
		{
			offset = offset->next;
			return offset->conf;
		}

		dont_match = 1;
	}
	else
	{
		base = my_list->conf_list;
		lower = 0;
		upper = my_list->length-1;
		compare = (lower+upper)/2;
		name = tomatch;
		offset = NULL;
	}

	while (lower != upper)
	{
		int	pos;

		if (!dont_match)
		{
			if (!matches(base[compare].pattern, name))
			{
				offset = &base[compare];
				return base[compare].conf;
			}
		}
		else
			dont_match = 0;

		pos = strcmp(name, base[compare].pattern);

		if (pos < 0)
			upper = compare;
		else
			if (lower == compare)
				lower = compare+1;
			else
				lower = compare;
		compare = (lower+upper)/2;
	}

	if (!dont_match)
	{
		if (!matches(base[compare].pattern, name))
		{
			offset = &base[compare];
			return base[compare].conf;
		}
	}

	lower = upper = compare = -1;
	return NULL;
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
