#include <stdio.h>
#include <string.h>
#include "struct.h"
#include "common.h"
#include "sys.h"

/* the return value is the size of the string */
int irc_sprintf(outp,formp,in0p,in1p,in2p,in3p,in4p,in5p,in6p,in7p,in8p,in9p,
	       in10p)
     char *outp;
     char *formp;
     char *in0p,*in1p,*in2p,*in3p,*in4p,*in5p,*in6p,*in7p,*in8p,*in9p,*in10p;
{
  /* rp for Reading, wp for Writing, fp for the Format string */
  char *inp[11]; /* we could hack this if we know the format of the stack */
  register char *rp,*fp,*wp;
  register char f;
  register int i=0;

  inp[0]=in0p; inp[1]=in1p; inp[2]=in2p; inp[3]=in3p; inp[4]=in4p; 
  inp[5]=in5p; inp[6]=in6p; inp[7]=in7p; inp[8]=in8p; inp[9]=in9p; 
  inp[10]=in10p; 

  fp = formp;
  wp = outp;
  
  rp = inp[i]; /* start with the first input string */

  /* just scan the format string and puke out whatever is necessary
     along the way... */

  while (f = *(fp++)) {
    
    if (f!= '%') *(wp++) = f;
    else
      switch (*(fp++)) {
	register char g;

      case 's': /* put the most common case at the top */
	while (g = *(rp++)) *(wp++) = g; 	/* copy a string */
	rp = inp[++i];                  /* get the next parameter */
	break;
      case 'd':
	{
	  register int myint,quotient;
	  register int write=0;
	  myint = (int)rp;
	  
	  if (myint > 999 || myint < 0) goto barf;
	  if(quotient=myint/100) {
	    *(wp++) = (char) (quotient + (int) '0');
	    myint %=100;
	    *(wp++) = (char) (myint/10 + (int) '0');
	  }
	  else {
	    myint %=100;
	    if (quotient = myint/10)
	      *(wp++) = (char) (quotient + (int)'0');
	  }
	  myint %=10;
	  *(wp++) = (char) ((myint) + (int) '0');

	  rp = inp[++i];
	}
	break;
      case 'u':
	{
	  register unsigned int myuint;
	  myuint = (unsigned int)rp;
	  
	  if (myuint < 100 || myuint > 999) goto barf;
	  
	  *(wp++) = (char) ((myuint / 100) + (unsigned int) '0');
	  myuint %=100;
	  *(wp++) = (char) ((myuint / 10) + (unsigned int) '0');
	  myuint %=10;
	  *(wp++) = (char) ((myuint) + (unsigned int) '0');

	  rp = inp[++i];
	}
	break;
      case '%':
	*(wp++) = '%';
	break;
      default:
	/* oh shit */
	goto barf;
	break;
      }
  }
  *wp = '\0'; /* leaves wp pointing to the terminating NULL in the string */
  return strlen(outp);
 barf:
  sprintf(outp,formp,in0p,in1p,in2p,in3p,in4p,in5p,in6p,in7p,in8p,
	  in9p,in10p);
  return strlen(outp);
}
