#include <stdio.h>
#include <string.h>
#include "struct.h"
#include "common.h"
#include "sys.h"

/*
 * By Mika
 */
int	irc_sprintf(outp, formp, i0, i1, i2, i3, i4, i5, i6, i7, i8, i9, i10)
char	*outp;
char	*formp;
char	*i0, *i1, *i2, *i3, *i4, *i5, *i6, *i7, *i8, *i9, *i10;
{
	/* rp for Reading, wp for Writing, fp for the Format string */
	/* we could hack this if we know the format of the stack */
	char	*inp[11];
	Reg1	char	*rp, *fp, *wp, **pp = inp;
	Reg1	char	f;
	Reg1	int	myi;
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
				myi = (int)*pp++;
				if ((myi < 100) || (myi > 999))
				    {
					(void)sprintf(outp, formp, i0, i1, i2,
						      i3, i4, i5, i6, i7, i8,
						      i9, i10);
					return -1;
				    }

				*wp++ = (char)(myi / 100 + (int) '0');
				myi %= 100;
				*wp++ = (char)(myi / 10 + (int) '0');
				myi %= 10;
				*wp++ = (char)(myi + (int) '0');
				break;
			case 'c':
				*wp++ = (char)*pp++;
				break;
			case '%':
				*wp++ = '%';
				break;
			default :
				(void)sprintf(outp, formp, i0, i1, i2, i3, i4,
					      i5, i6, i7, i8, i9, i10);
				return -1;
			}
	*wp = '\0';
	return wp - outp;
}

