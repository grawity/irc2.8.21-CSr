
#define POLLREADFLAGS (POLLIN|POLLRDNORM)
#define POLLWRITEFLAGS (POLLOUT|POLLWRNORM)
#define SET_READ_EVENT( thisfd ){  CHECK_PFD( thisfd );\
                                   pfd->events |= POLLREADFLAGS;}
#define SET_WRITE_EVENT( thisfd ){ CHECK_PFD( thisfd );\
                                   pfd->events |= POLLWRITEFLAGS;}
#define CHECK_PFD( thisfd )                     \
        if ( pfd->fd != thisfd ) {              \
                pfd = &poll_fdarray[nbr_pfds++];\
                pfd->fd     = thisfd;           \
                pfd->events = 0;                \
        }


#ifdef DOG3

int     read_message(delay, listp)
time_t  delay;
fdlist  *listp;
#else
int     read_message(delay)
time_t  delay; /* Don't ever use ZERO here, unless you mean to poll and then
                * you have to have sleep/wait somewhere else in the code.--msa
                */
#endif
{
        Reg1    aClient *cptr;
        Reg2    int     nfds;
        struct  timeval wait;
#ifdef  pyr
        struct  timeval nowt;
        u_long  us;
        time_t  now;
#endif
        pollfd_t   poll_fdarray[MAXCONNECTIONS];
        pollfd_t * pfd     = poll_fdarray;
        pollfd_t * res_pfd = NULL;
        pollfd_t * udp_pfd = NULL;
	int nbr_pfds = 0;
        fd_set  read_set, write_set;
        time_t  delay2 = delay;
        u_long  usec = 0;
        int     res, length, fd;
        int     auth;
        register int i;
	
#ifdef DOG3
        aClient *authclnts[MAXCONNECTIONS];
        register int j;

        /* if it is called with NULL we check all active fd's */
        if (!listp)
        {
                listp = &default_fdlist;
                listp->last_entry = highest_fd+1; /* remember the 0th entry isnt used */
        }

#endif

#ifdef NPATH
        check_command(&delay, NULL);
#endif
#ifdef  pyr
        (void) gettimeofday(&nowt, NULL);
        now = nowt.tv_sec;
#endif

        for (res = 0;;)
        {
                nbr_pfds = 0;
                pfd      = poll_fdarray;
                pfd->fd  = -1;
                res_pfd  = NULL;
                udp_pfd  = NULL;
		auth = 0;

#ifdef DOG3
		for (i=listp->entry[j=1];j<=listp->last_entry;
                        i=listp->entry[++j])
#else
                for (i = highest_fd; i >= 0; i--)
#endif
                {
                        if (!(cptr = local[i]))
                                continue;
                        if (IsLog(cptr))
                                continue;
                        if (DoingAuth(cptr))
                        {
                                if (auth == 0)
                                        bzero( (char *)&authclnts,
                                                sizeof(authclnts) );
                                auth++;
                                Debug((DEBUG_NOTICE,"auth on %x %d", cptr, i));
                                PFD_SETR(cptr->authfd);
                                if (cptr->flags & FLAGS_WRAUTH)
                                        PFD_SETW(cptr->authfd);
                                authclnts[cptr->authfd] = cptr;
                        }
                        if (DoingDNS(cptr) || DoingAuth(cptr))
                                continue;
                        if (IsMe(cptr) && IsListening(cptr))
                        {
#define CONNECTFAST
#ifdef CONNECTFAST
                /* next line was 2, changing to 1 */
                /* if we dont have many clients just let em on */
                /* This is VERY bad if someone tries to send a lot
                   of clones to the server though, as mbuf's can't
                   be allocated quickly enough... - Comstud */
                                if (1==1)
#else
                                if (NOW > (cptr->lasttime + 2))
#endif
                                        PFD_SETR(i);
                                else if (delay2 > 2)
                                        delay2 = 2;
                        }
                        else if (!IsMe(cptr))
                        {
                                if (DBufLength(&cptr->recvQ) && delay2 > 2)
                                        delay2 = 1;
                                if (DBufLength(&cptr->recvQ) < 4088)
                                        PFD_SETR(i);
                        }

                        if (DBufLength(&cptr->sendQ) || IsConnecting(cptr))
#ifndef pyr
                                PFD_SETR(i);
#else
                        {
                                if (!(cptr->flags & FLAGS_BLOCKED))
                                        PFD_SETW(i);
                                else
                                        delay2 = 0, usec = 500000;
                        }
                        if (now - cptr->lw.tv_sec &&
                            nowt.tv_usec - cptr->lw.tv_usec < 0)
                                us = 1000000;
                        else
                                us = 0;
                        us += nowt.tv_usec;
                        if (us - cptr->lw.tv_usec > 500000)
                                cptr->flags &= ~FLAGS_BLOCKED;
#endif
                }

                if (udpfd >= 0)
		{
                        PFD_SETR(udpfd);
			udp_pfd = pfd;
		}
                if (resfd >= 0)
		{
                        PFD_SETR(resfd);
			res_pfd = pfd;
		}
                wait.tv_sec = MIN(delay2, delay);
                wait.tv_usec = usec;
		nfds = poll(poll_fdarray, nbr_pfds,
			wait.tv_sec*1000 + wait.tv_usec/1000);
                if (nfds == -1 && errno == EINTR)
                        return -1;
                else if (nfds >= 0)
                        break;
                report_error("select %s:%s", &me);
                res++;
                if (res > 5)
                        restart("too many select errors");
                sleep(10);
            }
        if (udp_pfd && (udp_pfd->revents & POLLREADFLAGS))
        {
                        polludp();
                        nfds--;
        }
        if (res_pfd && (res_pfd->revents & POLLREADFLAGS))
        {
                        do_dns_async();
                        nfds--;
        }

        for (pfd = poll_fdarray, i = 0;(nfds > 0) && (i < nbr_pfds);
                i++, pfd++)
	{
		if (!pfd->revents)
			continue;
		nbfds--;
		if ((pfd == udp_pfd) || (pfd == res_pfd))
			continue;
		fd = pfd->fd;			
		re = pfd->revents;
                if ((auth>0) && ((cptr=authclnts[fd]) != NULL) &&
                        (cptr->authfd == fd))
                {
                        auth--;
                        if (re & POLLREADFLAGS)
                                read_authports(cptr);
                        else if (re & POLLWRITEFLAGS)
                                send_authports(cptr);
                        continue;
                }
                if (!(cptr = local[fd]))
                        continue;

                if ((re & POLLREADFLAGS) && IsListening(cptr))
                {
                        cptr->lasttime = NOW;
                        /*
                        ** There may be many reasons for error return, but
                        ** in otherwise correctly working environment the
                        ** probable cause is running out of file descriptors
                        ** (EMFILE, ENFILE or others?). The man pages for
                        ** accept don't seem to list these as possible,
                        ** although it's obvious that it may happen here.
                        ** Thus no specific errors are tested at this
                        ** point, just assume that connections cannot
                        ** be accepted until some old is closed first.
                        */
                        if ((fd = accept(i, NULL, NULL)) < 0)
                            {
                                report_error("Cannot accept connections %s:%s",
                                                cptr);
                                break;
                            }
                        ircstp->is_ac++;
                        if (fd >= MAXCLIENTS)
                            {
                                ircstp->is_ref++;
                                sendto_ops("All connections in use. (%s)",
                                           get_client_name(cptr, TRUE));
                                (void)send(fd,
                                        "ERROR :All connections in use\r\n",
                                        32, 0);
                                (void)close(fd);
                                break;
                            }
                        /*
                         * Use of add_connection (which never fails :) meLazy
                         */
#ifdef  UNIXPORT
                        if (IsUnixSocket(cptr))
                                add_unixconnection(cptr, fd);
                        else
#endif
                                (void)add_connection(cptr, fd);
                        nextping = NOW;
                        if (!cptr->acpt)
                                cptr->acpt = &me;
                }
                if (IsMe(cptr))
                        continue;
                if (re & POLLWRITEFLAGS)
                {
                        int     write_err = 0;
                        /*
                        ** ...room for writing, empty some queue then...
                        */
                        if (IsConnecting(cptr))
                                  write_err = completed_connection(cptr);
                        if (!write_err)
                                  (void)send_queued(cptr);
                        if (IsDead(cptr) || write_err)
                        {
deadsocket:
                                (void)exit_client(cptr, cptr, &me,
                                             strerror(get_sockerr(cptr)));
                                continue;
                         }
                }
                length = 1;     /* for fall through case */
                if (!NoNewLine(cptr) || (re & POLLREADFLAGS))
                        length = read_packet(cptr, (re & POLLREADFLAGS));
#ifndef DOG3
                if (length > 0)
                        flush_connections(i);
#endif
		readcalls++;
		if (lenth == FLUSH_BUFFER)
			continue;
		if (IsDead(cptr))
			goto deadsocket;
		if (length > 0)
			continue;

                /*
                ** ...hmm, with non-blocking sockets we might get
                ** here from quite valid reasons, although.. why
                ** would select report "data available" when there
                ** wasn't... so, this must be an error anyway...  --msa
                ** actually, EOF occurs when read() returns 0 and
                ** in due course, select() returns that fd as ready
                ** for reading even though it ends up being an EOF. -avalon
                */
                Debug((DEBUG_ERROR, "READ ERROR: fd = %d %d %d",
                        i, errno, length));

                if (IsServer(cptr) || IsHandshake(cptr))
                {
                        if (length == 0)
                                 sendto_ops("Server %s closed the connection",
                                            get_client_name(cptr,FALSE));
                        else
                                 report_error("Lost connection to %s:%s",
                                              cptr);
                }
                (void)exit_client(cptr, cptr, &me, length >= 0 ?
                                          "EOF From client" :
                                          strerror(get_sockerr(cptr)));
            }
        return 0;
}

#endif /* USE_POLL */

