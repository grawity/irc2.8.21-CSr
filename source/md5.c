/************************************************************************
 *   IRC - Internet Relay Chat, ircd/md5.c 
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
 ************************************************************************/

#include "struct.h"
#include "md5.h"
#include "sys.h"
#include <sys/time.h>
#include <fcntl.h>

void	id_reseed PROTO((char *, int));
static	void md5_block PROTO((t_u32 *, t_u32 *, t_u32 *));

/*
** how unique ID generation works:
**
** . to get an ID, we do an MD5 step, and encode
**      66 bits of the result into something printable
**
** . to mix the seed with new random information, we MD5-hash the info
**      into the seed
**
** . on startup, mix some rather week data (current time, pid,
**	server name) to initialize the seed
**
*/

static	t_u32 seed[MD5_BLOCK_SIZE] = {
	0x67452301 , 0xefcdab89 , 0x98badcfe , 0x10325476 ,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

static	unsigned char *seed_char = (unsigned char *)seed;

static	t_u32 databuf_int[MD5_HASH_SIZE];
static	char *databuf = (char *)databuf_int;

static int ork[64] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 
	1, 6, 11, 0, 5, 10, 15, 4, 9, 14, 3, 8, 13, 2, 7, 12, 
 	5, 8, 11, 14, 1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2, 
 	0, 7, 14, 5, 12, 3, 10, 1, 8, 15, 6, 13, 4, 11, 2, 9
};

static int ors[64] = {
	7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 
	5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static	t_u32 t[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,

	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,

	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,

	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

/*
** Should not include any 'flag' characters like @ and %, or special chars
** like : * and #, but 8bit accented stuff is quite ok  -orabidoo
*/
static	char printable7[] = 
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[\\]{|}^ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõöøùúûüý";

static	char printable6[] = 
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[]{}";

void	id_init()
{
	int fd;

	struct tmpcrap {
	    struct timeval tv;
	    int pid;
	    int ppid;
	} st;

	if (sizeof(t_u32) != 4)
	    {
	    	fprintf(stderr, "Your ints are not 32 bits wide\n");
		fprintf(stderr, "Change the typedef for t_u32 in include/id.h\n");
		exit(1);
	    }

	gettimeofday(&st.tv, NULL);
	st.pid = getpid();
	st.ppid = getppid();
	id_reseed((char *)&st, sizeof(st));

#ifdef RPATH
	alarm(3);
	fd = open(RPATH, O_RDONLY);
	if (fd > 0)
	    {
	    	read(fd, databuf, 16);
		close(fd);
		alarm(0);
		md5_block(databuf_int, seed, seed);    	
	    }
	else
		alarm(0);
#endif
}

void	save_random()
{
	int fd;

#ifdef	RPATH
	alarm(3);
	fd = open(RPATH, O_WRONLY|O_CREAT|O_TRUNC);
	if (fd > 0)
	    {
		write(fd, seed_char, 16);
		fchmod(fd, 0600);
		close(fd);
	    }
	alarm(0);
#endif
}

static void	md5_block(in, out, x) 
	t_u32	*in, *out, *x;
{
	Reg1	t_u32	a, b, c, d;
	Reg2	i, j;

	a = in[0];
	b = in[1];
	c = in[2];
	d = in[3];
	for (i=0; i<4; i++)
	    {
	    	j = 4*i;
		a = b + rotl(a + F(b, c, d) + x[ork[j]] + t[j], ors[j]);
		d = a + rotl(d + F(a, b, c) + x[ork[j+1]] + t[j+1], ors[j+1]);
		c = d + rotl(c + F(d, a, b) + x[ork[j+2]] + t[j+2], ors[j+2]);
		b = c + rotl(b + F(c, d, a) + x[ork[j+3]] + t[j+3], ors[j+3]);
	    }
	for (i=0; i<4; i++)
	    {
	    	j = 4*i + 16;
		a = b + rotl(a + G(b, c, d) + x[ork[j]] + t[j], ors[j]);
		d = a + rotl(d + G(a, b, c) + x[ork[j+1]] + t[j+1], ors[j+1]);
		c = d + rotl(c + G(d, a, b) + x[ork[j+2]] + t[j+2], ors[j+2]);
		b = c + rotl(b + G(c, d, a) + x[ork[j+3]] + t[j+3], ors[j+3]);
	    }
	for (i=0; i<4; i++)
	    {
	    	j = 4*i + 32;
		a = b + rotl(a + H(b, c, d) + x[ork[j]] + t[j], ors[j]);
		d = a + rotl(d + H(a, b, c) + x[ork[j+1]] + t[j+1], ors[j+1]);
		c = d + rotl(c + H(d, a, b) + x[ork[j+2]] + t[j+2], ors[j+2]);
		b = c + rotl(b + H(c, d, a) + x[ork[j+3]] + t[j+3], ors[j+3]);
	    }
	for (i=0; i<4; i++)
	    {
	    	j = 4*i + 48;
		a = b + rotl(a + I(b, c, d) + x[ork[j]] + t[j], ors[j]);
		d = a + rotl(d + I(a, b, c) + x[ork[j+1]] + t[j+1], ors[j+1]);
		c = d + rotl(c + I(d, a, b) + x[ork[j+2]] + t[j+2], ors[j+2]);
		b = c + rotl(b + I(c, d, a) + x[ork[j+3]] + t[j+3], ors[j+3]);
	    }

	a += in[0];
	b += in[1];
	c += in[2];
	d += in[3];
	out[0] = a;
	out[1] = b;
	out[2] = c;
	out[3] = d;
}

void	id_reseed(in, len)
	char	*in;
	int	len;
{
	Reg1	i;

	for (i=0; i<len; i++)
	    	databuf[i%(4*MD5_HASH_SIZE)] += in[i];
	md5_block(databuf_int, seed, seed);    	
}

char	*id_get()
{
	static	char id[ID_GEN_LEN+1];
	Reg1	int i;

	md5_block(databuf_int, seed, seed);

	id[0] = '.';
	for (i=1; i<ID_GEN_LEN; i++)
		id[i] = printable7[seed_char[i] & 127];

	id[ID_GEN_LEN] = '\0';
	return id;
}

char	*cookie_get()
{
	static	char cookie[COOKIELEN+1];
	Reg1	int i;

	databuf_int[0]++;
	md5_block(databuf_int, seed, seed);

	for (i=0; i<COOKIELEN; i++)
		cookie[i] = printable6[seed_char[i] & 63];

	cookie[COOKIELEN] = '\0';
	return cookie;
}

/*
** calculates a channel password hash.  returns a static buffer!
*/
char	*hash_pw(pw, salt)
char	*pw, *salt;
{
	Reg1	int	i = 0, shift;
	Reg2	unsigned char *s;

	t_u32	mydata[MD5_BLOCK_SIZE];
	t_u32	outdata[MD5_HASH_SIZE];
	static	unsigned char	hash[CHPASSHASHLEN + 1];
	static	t_u32	hash_iv[MD5_HASH_SIZE] = {
		0 , 0x2564547 , 0x42424242 , 0x13717421
	};

	if (!salt)
	    {
	    	/* rand() sucks, but for this it really ought to be
		** enough -orabidoo
		*/
	    	hash[0] = printable6[(rand() >> 5) & 63];
	    	hash[1] = printable6[(rand() >> 5) & 63];
	    }
	else
	    {
	    	hash[0] = salt[0];
		hash[1] = salt[1];
	    }
	hash_iv[0] = (t_u32)hash[0] + ((t_u32)hash[1] << 8);

	for (i=0; i<MD5_BLOCK_SIZE; i++)
		mydata[i] = 0;

	/* read the data in, in a bytesex-independent manner */
	i = 0;
	shift = 0;
	s = (unsigned char *)pw;
	while (*s)
	    {
	    	mydata[i] |= ((*s++) << shift);
		shift += 8;
		if (shift > 24)
		    {
		    	shift = 0;
			if (++i >= MD5_BLOCK_SIZE)
				break;
		    }
	    }
	md5_block(hash_iv, outdata, mydata);
	for (i=0; i<MD5_BLOCK_SIZE; i++)
		mydata[i] = 0;
	md5_block(outdata, outdata, mydata);

	/* and get it back out */
	i = 0;
	shift = 0;
	s = hash + 2;
	while ((s - hash) < CHPASSHASHLEN)
	    {
	    	*s++ = printable7[(outdata[i] >> shift) & 127];
		shift += 8;
		if (shift > 24)
		    {
		    	shift = 0;
			if (++i >= MD5_HASH_SIZE)
				break;
		    }
	    }
	/* there's still room for the null */
	*s = '\0';
	return hash;
}

/*
** check a channel pass for validity.  note that this overwrites any 
** static buffer previously returned by hash_pw !
*/
int	match_pw(clear, chptr)
char	*clear;
aChannel *chptr;
{
	if (!*chptr->mode.pass)
		return -1;
	if (strcmp(clear, "none") == 0)
		return -1;
	if (chptr->mode.clear)
		return strcmp(chptr->mode.clear->pass, clear);

	return strcmp(hash_pw(clear, chptr->mode.pass), chptr->mode.pass);
}

