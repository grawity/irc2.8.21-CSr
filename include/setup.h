#ifndef __setup_include__
#define __setup_include__
#define	PARAMH
#define	UNISTDH
#define	STRINGH
#define	STRINGSH
#define	STDLIBH
#undef	STDDEFH
#define	SYSSYSLOGH
#undef	NOINDEX
#undef	NEED_STRERROR
#define	NEED_STRTOKEN
#undef	NEED_STRTOK
#undef	NEED_INET_ADDR
#undef	NEED_INET_NTOA
#undef	NEED_INET_NETOF
#define	GETTIMEOFDAY
#undef	LRAND48
#define	MALLOCH	<malloc.h>
#define	NBLOCK_POSIX
#define	POSIX_SIGNALS
#undef	TIMES_2
#define	GETRUSAGE_2
#endif
