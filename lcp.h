/*
 * lcp.h - Link Control Protocol definitions.
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
 * $Id: lcp.h,v 1.8 1995/06/12 11:22:47 paulus Exp $
 */

/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

/*
 * Options.
 */
#define CI_MRU          1       /* Maximum Receive Unit */
#define CI_ASYNCMAP     2       /* Async Control Character Map */
#define CI_AUTHTYPE     3       /* Authentication Type */
#define CI_QUALITY      4       /* Quality Protocol */
#define CI_MAGICNUMBER  5       /* Magic Number */
#define CI_PCOMPRESSION 7       /* Protocol Field Compression */
#define CI_ACCOMPRESSION 8      /* Address/Control Field Compression */

/*
 * LCP-specific packet types.
 */
#define PROTREJ         8       /* Protocol Reject */
#define ECHOREQ         9       /* Echo Request */
#define ECHOREP         10      /* Echo Reply */
#define DISCREQ         11      /* Discard Request */

/*
 * The state of options is described by an lcp_options structure.
 */
typedef struct lcp_options {
    unsigned passive : 1;           /* Don't die if we don't get a response */
    unsigned silent : 1;            /* Wait for the other end to start first */
    unsigned restart : 1;           /* Restart vs. exit after close */
    unsigned neg_mru : 1;           /* Negotiate the MRU? */
    unsigned neg_asyncmap : 1;      /* Negotiate the async map? */
    unsigned neg_upap : 1;          /* Ask for UPAP authentication? */
    unsigned neg_chap : 1;          /* Ask for CHAP authentication? */
    unsigned neg_magicnumber : 1;   /* Ask for magic number? */
    unsigned neg_pcompression : 1;  /* HDLC Protocol Field Compression? */
    unsigned neg_accompression : 1; /* HDLC Address/Control Field Compression? */
    unsigned neg_lqr : 1;           /* Negotiate use of Link Quality Reports */
    u_short mru;                    /* Value of MRU */
    u_char chap_mdtype;             /* which MD type (hashing algorithm) */
    u_int32_t asyncmap;             /* Value of async map */
    u_int32_t magicnumber;
    int numloops;                   /* Number of loops during magic number neg. */
    u_int32_t lqr_period;           /* Reporting period for LQR 1/100ths second */
} lcp_options;

extern fsm lcp_fsm[];
extern lcp_options lcp_wantoptions[];
extern lcp_options lcp_gotoptions[];
extern lcp_options lcp_allowoptions[];
extern lcp_options lcp_hisoptions[];
extern u_int32_t xmit_accm[][8];

#define DEFMRU  1500            /* Try for this */
#define MINMRU  128             /* No MRUs below this */
#define MAXMRU  1500            /* Normally limit MRU to this */

void lcp_init(int);
void lcp_open(int);
void lcp_close(int);
void lcp_lowerup(int);
void lcp_lowerdown(int);
void lcp_input(int, u_char *, int);
void lcp_protrej(int);
void lcp_sprotrej(int, u_char *, int);
int  lcp_printpkt(u_char *, int, void (*)(void *, char *, ...), void *);

/* Default number of times we receive our magic number from the peer
   before deciding the link is looped-back. */
#define DEFLOOPBACKFAIL 5
