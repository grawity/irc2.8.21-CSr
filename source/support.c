/************************************************************************
 *   IRC - Internet Relay Chat, common/support.c
 *   Copyright (C) 1990, 1991 Armin Gruner
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

#ifndef lint
static  char rcsid[] = "@(#)$Id: support.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"


extern	void	outofmemory();

char *mstrcpy(dest, src)
char **dest;
char *src;
{
	if ((*dest = (char *) MyMalloc(strlen(src) + 1)) == NULL)
		return NULL;
	return (char *) strcpy(*dest, src);
}

char *loadfile(filename)
char *filename;
{
	int fd, num;
	struct stat Sb;
	char *buff;

	fd = open(filename, O_RDONLY);
	if (fd == -1)
		return NULL;
	if (fstat(fd, &Sb) == -1)
	{
		close(fd);
		return NULL;
	}
	buff = (char *)MyMalloc(Sb.st_size+1);
	num = read(fd, buff, Sb.st_size);
	close(fd);
	if (num != Sb.st_size)
	{
		MyFree(buff);
		return NULL;
	}
	buff[num] = (char) 0;
	return buff;
}

char *get_token(src, token_sep)
char **src;
char *token_sep;
{
	char *tok;

	if (!(src && *src && **src))
		return NULL;

	while(**src && strchr(token_sep, **src))
		(*src)++;

	if(**src)
		tok = *src;
	else
		return NULL;

	*src = strpbrk(*src, token_sep);
	if (*src)
	{
		**src = '\0';
		(*src)++;
		while(**src && strchr(token_sep, **src))
			(*src)++;
	}
	else
		*src = "";
	return tok;
}

#ifdef NEED_STRTOKEN
/*
** 	strtoken.c --  	walk through a string of tokens, using a set
**			of separators
**			argv 9/90
**
**	$Id: support.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $
*/

char *strtoken(save, str, fs)
char **save;
char *str, *fs;
{
	char *pos = *save;	/* keep last position across calls */
	register char *tmp;

	if (str)
		pos = str;		/* new string scan */

	while (pos && *pos && index(fs, *pos) != NULL)
		pos++; 		 	/* skip leading separators */

	if (!pos || !*pos)
		return (pos = *save = NULL); 	/* string contains only sep's */

	tmp = pos; 			/* now, keep position of the token */

	while (*pos && index(fs, *pos) == NULL)
		pos++; 			/* skip content of the token */

	if (*pos)
		*pos++ = '\0';		/* remove first sep after the token */
	else
		pos = NULL;		/* end of string */

	*save = pos;
	return(tmp);
}
#endif /* NEED_STRTOKEN */

#ifdef NEED_STRERROR
/*
**	strerror - return an appropriate system error string to a given errno
**
**		   argv 11/90
**	$Id: support.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $
*/

char *strerror(err_no)
int err_no;
{
	static	char	buff[40];
	char	*errp;

	errp = (err_no > sys_nerr ? (char *)NULL : sys_errlist[err_no]);

	if (errp == (char *)NULL)
	{
		errp = buff;
		(void) sprintf(errp, "Unknown Error %d", err_no);
	}
	return errp;
}

#endif /* NEED_STRERROR */

/*
**	inetntoa  --	changed name to remove collision possibility and
**			so behaviour is gaurunteed to take a pointer arg.
**			-avalon 23/11/92
**	inet_ntoa --	returned the dotted notation of a given
**			internet number (some ULTRIX don't have this)
**			argv 11/90).
**	inet_ntoa --	its broken on some Ultrix/Dynix too. -avalon
**	$Id: support.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $
*/

char	*inetntoa(in)
char	*in;
{
	static	char	buf[16];
	register	u_char	*s = (u_char *)in;
	register	int	a,b,c,d;

	a = (int)*s++;
	b = (int)*s++;
	c = (int)*s++;
	d = (int)*s++;
	(void) sprintf(buf, "%d.%d.%d.%d", a,b,c,d );

	return buf;
}

#ifdef NEED_INET_NETOF
/*
**	inet_netof --	return the net portion of an internet number
**			argv 11/90
**	$Id: support.c,v 1.1.1.1 1997/07/23 18:02:05 cbehrens Exp $
**
*/

int inet_netof(in)
struct in_addr in;
{
    int addr = in.s_net;

    if (addr & 0x80 == 0)
	return ((int) in.s_net);

    if (addr & 0x40 == 0)
	return ((int) in.s_net * 256 + in.s_host);

    return ((int) in.s_net * 256 + in.s_host * 256 + in.s_lh);
}
#endif /* NEED_INET_NETOF */


#if defined(DEBUGMODE)
void	dumpcore(msg, p1, p2, p3, p4, p5, p6, p7, p8, p9)
char	*msg, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9;
{
	static	time_t	lastd = 0;
	static	int	dumps = 0;
	char	corename[12];
	time_t	now;
	int	p;

	now = time(NULL);

	if (!lastd)
		lastd = now;
	else if (now - lastd < 60 && dumps > 2)
		(void)s_die();
	if (now - lastd > 60)
	    {
		lastd = now;
		dumps = 1;
	    }
	else
		dumps++;
	p = getpid();
	if (fork()>0) {
		kill(p, 3);
		kill(p, 9);
	}
	write_pidfile();
	(void)sprintf(corename, "core.%d", p);
	(void)rename("core", corename);
	Debug((DEBUG_FATAL, "Dumped core : core.%d", p));
	sendto_ops("Dumped core : core.%d", p);
	Debug((DEBUG_FATAL, msg, p1, p2, p3, p4, p5, p6, p7, p8, p9));
	sendto_ops(msg, p1, p2, p3, p4, p5, p6, p7, p8, p9);
		(void)s_die();
}

static	char	*marray[20000];
static	int	mindex = 0;

#define	SZ_EX	(sizeof(char *) + sizeof(size_t) + 4)
#define	SZ_CHST	(sizeof(char *) + sizeof(size_t))
#define	SZ_CH	(sizeof(char *))
#define	SZ_ST	(sizeof(size_t))

char	*MyMalloc(x)
size_t	x;
{
	register int	i;
	register char	**s;
	char	*ret;

	ret = (char *)malloc(x + (size_t)SZ_EX);

	if (!ret)
		outofmemory();
	bzero(ret, (int)x + SZ_EX);
	bcopy((char *)&ret, ret, SZ_CH);
	bcopy((char *)&x, ret + SZ_ST, SZ_ST);
	bcopy("VAVA", ret + SZ_CHST + (int)x, 4);
	Debug((DEBUG_MALLOC, "MyMalloc(%ld) = %#x", x, ret+8));
	for(i = 0, s = marray; *s && i < mindex; i++, s++)
		;
 	if (i < 20000)
	    {
		*s = ret;
		if (i == mindex)
			mindex++;
	    }
	return ret + SZ_CHST;
    }

char    *MyRealloc(x, y)
char	*x;
size_t	y;
    {
	register int	l;
	register char	**s;
	char	*ret, *cp;
	size_t	i;
	int	k;

	x -= SZ_CHST;
	bcopy(x, (char *)&cp, SZ_CH);
	bcopy(x + SZ_CH, (char *)&i, SZ_ST);
	bcopy(x + (int)i + SZ_CHST, (char *)&k, 4);
	if (bcmp((char *)&k, "VAVA", 4) || (x != cp))
		dumpcore("MyRealloc %#x %d %d %#x %#x", x, y, i, cp, k);
	ret = (char *)realloc(x, y + (size_t)SZ_EX);

	if (!ret)
		outofmemory();
	bcopy((char *)&ret, ret, SZ_CH);
	bcopy((char *)&y, ret + SZ_CH, SZ_ST);
	bcopy("VAVA", ret + SZ_CHST + (int)y, 4);
	Debug((DEBUG_NOTICE, "MyRealloc(%#x,%ld) = %#x", x, y, ret + SZ_CHST));
	for(l = 0, s = marray; *s != x && l < mindex; l++, s++)
		;
 	if (l < mindex)
		*s = NULL;
	else if (l == mindex)
		Debug((DEBUG_MALLOC, "%#x !found", x));
	for(l = 0, s = marray; *s && l < mindex; l++,s++)
		;
 	if (l < 20000)
	    {
		*s = ret;
		if (l == mindex)
			mindex++;
	    }
	return ret + SZ_CHST;
    }

void	MyFree(x)
char	*x;
{
	size_t	i;
	char	*j;
	u_char	k[4];
	register int	l;
	register char	**s;

	if (!x)
		return;
	x -= SZ_CHST;

	bcopy(x, (char *)&j, SZ_CH);
	bcopy(x + SZ_CH, (char *)&i, SZ_ST);
	bcopy(x + SZ_CHST + (int)i, (char *)k, 4);

	if (bcmp((char *)k, "VAVA", 4) || (j != x))
		dumpcore("MyFree %#x %ld %#x %#x", x, i, j,
			 (k[3]<<24) | (k[2]<<16) | (k[1]<<8) | k[0]);

#undef	free
	(void)free(x);
#define	free(x)	MyFree(x)
	Debug((DEBUG_MALLOC, "MyFree(%#x)",x + SZ_CHST));

	for (l = 0, s = marray; *s != x && l < mindex; l++, s++)
		;
	if (l < mindex)
		*s = NULL;
	else if (l == mindex)
		Debug((DEBUG_MALLOC, "%#x !found", x));
}

#else

#ifndef DOUGH_MALLOC

void	MyFree(x)
void *x;
{
	if (((char *)x) != NULL)
		free(x);
}

char	*MyMalloc(x)
size_t	x;
{
	char *ret = (char *)malloc(x);

	if (!ret)
		outofmemory();
	return	ret;
}

char	*MyRealloc(x, y)
char	*x;
size_t	y;
{
	char *ret = (char *)realloc(x, y);

	if (!ret)
		outofmemory();
	return ret;
}

#endif

#endif


/*
** read a string terminated by \r or \n in from a fd
**
** Created: Sat Dec 12 06:29:58 EST 1992 by avalon
** Returns:
**	0 - EOF
**	-1 - error on read
**     >0 - number of bytes returned (<=num)
** After opening a fd, it is necessary to init dgets() by calling it as
**	dgets(x,y,0);
** to mark the buffer as being empty.
*/
int	dgets(fd, buf, num)
int	fd, num;
char	*buf;
{
	static	char	dgbuf[8192];
	static	char	*head = dgbuf, *tail = dgbuf;
	register char	*s, *t;
	register int	n, nr;

	/*
	** Sanity checks.
	*/
	if (head == tail)
		*head = '\0';
	if (!num)
	    {
		head = tail = dgbuf;
		*head = '\0';
		return 0;
	    }
	if (num > sizeof(dgbuf) - 1)
		num = sizeof(dgbuf) - 1;
dgetsagain:
	if (head > dgbuf)
	    {
		for (nr = tail - head, s = head, t = dgbuf; nr > 0; nr--)
			*t++ = *s++;
		tail = t;
		head = dgbuf;
	    }
	/*
	** check input buffer for EOL and if present return string.
	*/
	if (head < tail &&
	    ((s = index(head, '\n')) || (s = index(head, '\r'))) && s < tail)
	    {
		n = MIN(s - head + 1, num);	/* at least 1 byte */
dgetsreturnbuf:
		bcopy(head, buf, n);
		head += n;
		if (head == tail)
			head = tail = dgbuf;
		return n;
	    }

	if (tail - head >= num)		/* dgets buf is big enough */
	    {
		n = num;
		goto dgetsreturnbuf;
	    }

	n = sizeof(dgbuf) - (tail - dgbuf) - 1;
	nr = read(fd, tail, n);
	if (nr == -1)
	    {
		head = tail = dgbuf;
		return -1;
	    }
	if (!nr)
	    {
		if (tail > head)
		    {
			n = MIN(tail - head, num);
			goto dgetsreturnbuf;
		    }
		head = tail = dgbuf;
		return 0;
	    }
	tail += nr;
	*tail = '\0';
	for (t = head; (s = index(t, '\n')); )
	    {
		if ((s > head) && (s > dgbuf))
		    {
			t = s-1;
			for (nr = 0; *t == '\\'; nr++)
				t--;
			if (nr & 1)
			    {
				t = s+1;
				s--;
				nr = tail - t;
				while (nr--)
					*s++ = *t++;
				tail -= 2;
				*tail = '\0';
			    }
			else
				s++;
		    }
		else
			s++;
		t = s;
	    }
	*tail = '\0';
	goto dgetsagain;
}

#ifdef NEED_BCMP
int bcmp(s1, s2, n)
const void *s1;
const void *s2;
size_t n;
{
	register char *t1 = (char *)s1, *t2 = (char *)s2;
	while(n && (*(t1++) == *(t2++)))
		n--;
	return n ? 1 : 0;
}
#endif

#ifdef NEED_BZERO
void bzero(s, n)
void *s;
size_t n;
{
	register char *t = s;

	while(n--)
		*(t++) = (char) 0;
}
#endif

/*
 * By Mika
 */
int	irc_sprintf(outp, formp,
		    i0, i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i11)
char	*outp;
char	*formp;
char	*i0, *i1, *i2, *i3, *i4, *i5, *i6, *i7, *i8, *i9, *i10, *i11;
{
	/* rp for Reading, wp for Writing, fp for the Format string */
	/* we could hack this if we know the format of the stack */
	char	*inp[12];
	register	char	*rp, *fp, *wp, **pp = inp;
	register	char	f;
	register	long	myi;
	int	i;

	inp[0] = i0;
	inp[1] = i1;
	inp[2] = i2;
	inp[3] = i3;
	inp[4] = i4;
	inp[5] = i5;
	inp[6] = i6;
	inp[7] = i7;
	inp[8] = i8;
	inp[9] = i9;
	inp[10] = i10;
	inp[11] = i11;

	/*
	 * just scan the format string and puke out whatever is necessary
	 * along the way...
	 */

	for (i = 0, wp = outp, fp = formp; (f = *fp++); )
		if (f != '%')
			*wp++ = f;
		else
			switch (*fp++)
			{
 			/* put the most common case at the top */
			/* copy a string */
			case 's':
				for (rp = *pp++; (*wp++ = *rp++); )
					;
				--wp;
				/* get the next parameter */
				break;
			/*
			 * reject range for params to this mean that the
			 * param must be within 100-999 and this +ve int
			 */
			case 'd':
			case 'u':
				myi = (long)*pp++;
				if ((myi < 100) || (myi > 999))
				{
					(void)sprintf(outp, formp, i0, i1, i2,
						      i3, i4, i5, i6, i7, i8,
						      i9, i10, i11);
					return strlen(outp);
				}
				*wp++ = (char)(myi / 100 + (int) '0');
				myi %= 100;
				*wp++ = (char)(myi / 10 + (int) '0');
				myi %= 10;
				*wp++ = (char)(myi + (int) '0');
				break;
			case 'c':
				*wp++ = (char)(long)*pp++;
				break;
			case '%':
				*wp++ = '%';
				break;
			default :
				(void)sprintf(outp, formp, i0, i1, i2, i3, i4,
					      i5, i6, i7, i8, i9, i10, i11);
				return strlen(outp);
			}
	*wp = '\0';
	return wp - outp;
}
