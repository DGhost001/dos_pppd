/*
 * fsm.h - {Link, IP} Control Protocol Finite State Machine definitions.
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
 * $Id: fsm.h,v 1.5 1995/05/19 03:17:35 paulus Exp $
 */

/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

/*
 * Packet header = Code, id, length.
 */
#define HEADERLEN       (sizeof (u_char) + sizeof (u_char) + sizeof (u_short))


/*
 *  CP (LCP, IPCP, etc.) codes.
 */
#define CONFREQ         1       /* Configuration Request */
#define CONFACK         2       /* Configuration Ack */
#define CONFNAK         3       /* Configuration Nak */
#define CONFREJ         4       /* Configuration Reject */
#define TERMREQ         5       /* Termination Request */
#define TERMACK         6       /* Termination Ack */
#define CODEREJ         7       /* Code Reject */


/*
 * Each FSM is described by a fsm_callbacks and a fsm structure.
 */
struct fsm;
typedef struct fsm_callbacks {
    void (*resetci)(struct fsm*);          /* Reset our Configuration Information */
    int  (*cilen)(struct fsm*);            /* Length of our Configuration Information */
    void (*addci)(struct fsm*, u_char*, int*);            /* Add our Configuration Information */
    int  (*ackci)(struct fsm*, u_char*, int);            /* ACK our Configuration Information */
    int  (*nakci)(struct fsm*, u_char*, int);            /* NAK our Configuration Information */
    int  (*rejci)(struct fsm*, u_char*, int);            /* Reject our Configuration Information */
    int  (*reqci)(struct fsm*,u_char*, int*, int);            /* Request peer's Configuration Information */
    void (*up)(struct fsm*);               /* Called when fsm reaches OPENED state */
    void (*down)(struct fsm*);             /* Called when fsm leaves OPENED state */
    void (*starting)(struct fsm*);         /* Called when we want the lower layer */
    void (*finished)(struct fsm*);         /* Called when we don't want the lower layer */
    void (*protreject)(void);       /* Called when Protocol-Reject received */
    void (*retransmit)(struct fsm*);       /* Retransmission is necessary */
    int  (*extcode)(struct fsm*, int, int, u_char*, int);          /* Called when unknown code received */
    char *proto_name;           /* String name for protocol (for messages) */
} fsm_callbacks;


typedef struct fsm {
    int unit;                   /* Interface unit number */
    int protocol;               /* Data Link Layer Protocol field value */
    int state;                  /* State */
    int flags;                  /* Contains option bits */
    u_char id;                  /* Current id */
    u_char reqid;               /* Current request id */
    u_char seen_ack;            /* Have received valid Ack/Nak/Rej to Req */
    int timeouttime;            /* Timeout time in milliseconds */
    int maxconfreqtransmits;    /* Maximum Configure-Request transmissions */
    int retransmits;            /* Number of retransmissions left */
    int maxtermtransmits;       /* Maximum Terminate-Request transmissions */
    int nakloops;               /* Number of nak loops since last ack */
    int maxnakloops;            /* Maximum number of nak loops tolerated */
    fsm_callbacks *callbacks;   /* Callback routines */
} fsm;


/*
 * Link states.
 */
#define INITIAL         0       /* Down, hasn't been opened */
#define STARTING        1       /* Down, been opened */
#define CLOSED          2       /* Up, hasn't been opened */
#define STOPPED         3       /* Open, waiting for down event */
#define CLOSING         4       /* Terminating the connection, not open */
#define STOPPING        5       /* Terminating, but open */
#define REQSENT         6       /* We've sent a Config Request */
#define ACKRCVD         7       /* We've received a Config Ack */
#define ACKSENT         8       /* We've sent a Config Ack */
#define OPENED          9       /* Connection available */

/*
 * Flags - indicate options controlling FSM operation
 */
#define OPT_PASSIVE     1       /* Don't die if we don't get a response */
#define OPT_RESTART     2       /* Treat 2nd OPEN as DOWN, UP */
#define OPT_SILENT      4       /* Wait for peer to speak first */
#define OPT_DISABLE     8       /* LCP proto reject this protocol */

/*
 * Timeouts.
 */
#define DEFTIMEOUT      3       /* Timeout time in seconds */
#define DEFMAXTERMREQS  2       /* Maximum Terminate-Request transmissions */
#define DEFMAXCONFREQS  10      /* Maximum Configure-Request transmissions */
#define DEFMAXNAKLOOPS  5       /* Maximum number of nak loops */


/*
 * Prototypes
 */
void fsm_init(fsm *);

#ifndef IPX_CHANGE
void fsm_lowerup(fsm *);
#else
void fsm_lowerup(fsm *, int);
#endif /* IPX_CHANGE */

void fsm_lowerdown(fsm *);
void fsm_open(fsm *);
void fsm_close(fsm *);
void fsm_input(fsm *, u_char *, int);
void fsm_protreject(fsm *);
void fsm_sdata(fsm *, u_char, u_char, u_char *, int);


/*
 * Variables
 */
extern int peer_mru[];          /* currently negotiated peer MRU (per unit) */
