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

#ifndef __dich_conf_h__
#define __dich_conf_h__

#include "struct.h"

typedef struct ConfList aConfList;
typedef	struct ConfEntry aConfEntry;

struct ConfList
{
	unsigned int	length;
	aConfEntry	*conf_list;
};

struct ConfEntry
{
	char		*pattern;
	aConfItem	*conf;
	aConfEntry	*next;
};

extern	void		addto_conf_list();
extern	void		clear_conf_list();
extern	aConfItem	*find_matching_conf();
extern	void		l_addto_conf_list();
extern	aConfItem	*l_find_matching_conf();
extern	char		*host_field();
extern	char		*name_field();
extern	char		*rev_host_field();
extern	char		*rev_name_field();
extern	int		sortable();

#endif
