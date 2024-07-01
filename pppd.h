/*
 * pppd.h - PPP daemon global declarations.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: pppd.h,v 1.8 1995/04/26 06:46:31 paulus Exp $
 */

/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

/*
 * TODO:
 */

#ifndef __PPPD_H__
#define __PPPD_H__

#include <sys/types.h>          /* for u_int32_t, if defined */
#include "types.h"
#include "ppp_defs.h"
#include "if_pppva.h"

#define NUM_PPP 1               /* One PPP interface supported (per process) */

/*
 * Limits.
 */

#define MAXWORDLEN      80      /* max length of word in file (incl null) */
#define MAXARGS         1       /* max # args to a command */
#define MAXNAMELEN      80      /* max length of hostname or name for auth */
#define MAXSECRETLEN    80      /* max length of password or secret */

/*
 * Global variables.
 */

extern volatile int hungup;     /* Physical layer has disconnected */
extern int      ifunit;         /* Interface unit number */
extern char     ifname[];       /* Interface name */
extern int      fd;             /* Serial device file descriptor */
extern char     hostname[];     /* Our hostname */
extern u_char   outpacket_buf[]; /* Buffer for outgoing packets */
extern volatile int phase;      /* Current state of link - see values below */
extern long     baud_rate;      /* Current link speed in bits/sec */
extern char     *progname;      /* Name of this program */
extern int      going_resident; /* Flag set when going TSR */
extern int      comopen;        /* Currently active COM port */

/*
 * Variables set by command-line options.
 */

extern int      debug;          /* Debug flag */
extern int      kdebugflag;     /* Tell kernel to print debug messages */
extern int      crtscts;        /* Use hardware flow control */
extern int      modem;          /* Use modem control lines */
extern long     inspeed;        /* Input/Output speed requested */
extern u_int32_t netmask;       /* IP netmask to set on interface */
extern char     user[];         /* Username for PAP */
extern char     passwd[];       /* Password for PAP */
extern int      auth_required;  /* Peer is required to authenticate */
extern int      lcp_echo_interval; /* Interval between LCP echo-requests */
extern int      lcp_echo_fails; /* Tolerance to unanswered echo-requests */
extern char     hostname[];     /* Our hostname */
extern char     our_name[];     /* Our name for authentication purposes */
extern char     remote_name[];  /* Peer's name for authentication */
extern int      portirq;        /* COM port irq */
extern int      portbase;       /* COM port base */
extern int      portnum;        /* COM port number */
extern int      pktint;         /* packet driver irq number */
extern char     *connector;     /* Script to establish physical link */

/*
 * Values for phase.
 */
#define PHASE_DEAD              0
#define PHASE_ESTABLISH         1
#define PHASE_AUTHENTICATE      2
#define PHASE_NETWORK           3
#define PHASE_TERMINATE         4

/*
 * Prototypes.
 */
u_int32_t getjiffies(void);
                        /* process no network packet */
int rcv_proto_unknown(struct ppp *, u_short, u_char *, int);

void quit(void);        /* Cleanup and exit */
void die(int);
void novm(char *msg);
                        /* Look-alike of kernel's timeout() */
void timeout(void (*)(), caddr_t, u_int32_t);
                        /* Look-alike of kernel's untimeout() */
void untimeout(void (*)(), caddr_t);
                        /* Output a PPP packet */
void output(int, u_char *, int);
                        /* Demultiplex a Protocol-Reject */
void demuxprotrej(int, int);
                        /* Check peer-supplied username/password */
int  check_passwd(int, char *, int, char *, int, char **, int *);
                        /* get "secret" for chap */
int  get_secret(int, char *, char *, char *, int *, int);
                        /* get netmask for address */
u_int32_t GetMask(u_int32_t, u_int32_t);

/*
 * Inline versions of get/put char/short/long.
 * Pointer is advanced; we assume that both arguments
 * are lvalues and will already be in registers.
 * cp MUST be u_char *.
 *
 * ALM, I'm trying to optimize this for the 16 bit DOS version.
 *
 */
#if 0
#define GETCHAR(c, cp) { \
    (c) = *(cp)++; \
}

#define PUTCHAR(c, cp) { \
    *(cp)++ = (u_char)(c); \
}

#define GETSHORT(s, cp) { \
    (s) = *(cp)++ << 8; \
    (s) |= *(cp)++; \
}

#define PUTSHORT(s, cp) { \
    *(cp)++ = (u_char)((s) >> 8); \
    *(cp)++ = (u_char)(s); \
}

#define GETLONG(l, cp) { \
    (l) = *(cp)++ << 8; \
    (l) |= *(cp)++; (l) <<= 8; \
    (l) |= *(cp)++; (l) <<= 8; \
    (l) |= *(cp)++; \
}

#define PUTLONG(l, cp) { \
    *(cp)++ = (u_char)((l) >> 24); \
    *(cp)++ = (u_char)((l) >> 16); \
    *(cp)++ = (u_char)((l) >> 8); \
    *(cp)++ = (u_char)(l); \
}
#else
#define GETCHAR(c, cp) { \
    (c) = *(cp)++; \
}

#define PUTCHAR(c, cp) { \
    *(cp)++ = (u_char)(c); \
}

#define GETSHORT(s, cp) { \
    (s) = ntohs(*((u_short *)(cp))++); \
}

#define PUTSHORT(s, cp) { \
    *((u_short *)(cp))++ = htons((s)); \
}

#define GETLONG(l, cp) { \
    (l) = ntohl(*((u_int32_t *)(cp))++); \
}

#define PUTLONG(l, cp) { \
    *((u_int32_t *)(cp))++ = htonl((l)); \
}
#endif

#define INCPTR(n, cp)   ((cp) += (n))
#define DECPTR(n, cp)   ((cp) -= (n))

#define kbhit()         (int)(unsigned char)bdos(0x0B, 0, 0)
#define getch()         (int)(unsigned char)bdos(0x08, 0, 0)

#undef  FALSE
#define FALSE   0
#undef  TRUE
#define TRUE    1

/*
 * System dependent definitions for user-level 4.3BSD UNIX implementation.
 */

#define DEMUXPROTREJ(u, p)      demuxprotrej(u, p)

#define TIMEOUT(r, f, t)        timeout((r), (f), (t))
#define UNTIMEOUT(r, f)         untimeout((r), (f))

#define BCOPY(s, d, l)          memcpy(d, s, l)
#define BZERO(s, n)             memset(s, 0, n)
#define EXIT(u)                 quit()

/*
 * MAKEHEADER - Add Header fields to a packet.
 */
#define MAKEHEADER(p, t) { \
    PUTCHAR(PPP_ALLSTATIONS, p); \
    PUTCHAR(PPP_UI, p); \
    PUTSHORT(t, p); \
}

#define HZ 18   /* en realidad 18.2 ticks por segundo, pero redondeo */

#ifdef DEBUGALL
#define DEBUGMAIN       1
#define DEBUGFSM        1
#define DEBUGLCP        1
#define DEBUGIPCP       1
#define DEBUGIPXCP      1
#define DEBUGUPAP       1
#define DEBUGCHAP       1
#define DEBUGPPP        1
#define DEBUGAUTH       1
#endif

#ifdef DEBUGMAIN
#define MAINDEBUG(x)    if ( debug ) syslog x
#else
#define MAINDEBUG(x)
#endif

#ifdef DEBUGFSM
#define FSMDEBUG(x)     if ( debug ) syslog x
#else
#define FSMDEBUG(x)
#endif

#ifdef DEBUGLCP
#define LCPDEBUG(x)     if ( debug ) syslog x
#else
#define LCPDEBUG(x)
#endif

#ifdef DEBUGIPCP
#define IPCPDEBUG(x)    if ( debug ) syslog x
#else
#define IPCPDEBUG(x)
#endif

#ifdef DEBUGIPXCP
#define IPXCPDEBUG(x)   if ( debug ) syslog x
#else
#define IPXCPDEBUG(x)
#endif

#ifdef DEBUGUPAP
#define UPAPDEBUG(x)    if ( debug ) syslog x
#else
#define UPAPDEBUG(x)
#endif

#ifdef DEBUGCHAP
#define CHAPDEBUG(x)    if ( debug ) syslog x
#else
#define CHAPDEBUG(x)
#endif

#ifdef DEBUGPPP
#define PPPDEBUG(x)     if ( ppp->flags & SC_DEBUG ) syslog x
#else
#define PPPDEBUG(x)
#endif

#ifdef DEBUGAUTH
#define AUTHDEBUG(x)    if ( debug ) syslog x
#else
#define AUTHDEBUG(x)
#endif

#if defined(DEBUGUPAP) || defined(DEBUGCHAP)
#define PRINTMSG(m, l)  { m[l] = '\0'; syslog(LOG_NOTICE, "Remote message: %s\n", m); }
#else
#define PRINTMSG(m, l)
#endif

#ifndef MIN
#define MIN(a, b)       ((a) < (b)? (a): (b))
#endif
#ifndef MAX
#define MAX(a, b)       ((a) > (b)? (a): (b))
#endif

#endif /* __PPP_H__ */
