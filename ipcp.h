/*
 * ipcp.h - IP Control Protocol definitions.
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
 * $Id: ipcp.h,v 1.5 1994/09/21 06:47:37 paulus Exp $
 */

/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

/*
 * Options.
 */
#define CI_ADDRS        1       /* IP Addresses */
#define CI_COMPRESSTYPE 2       /* Compression Type */
#define CI_ADDR         3

#define CI_MS_DNS1      129     /* Primary DNS value */
#define CI_MS_DNS2      131     /* Secondary DNS value */

#define MAX_STATES 16           /* from slcompress.h */

#define IPCP_VJMODE_OLD 1       /* "old" mode (option # = 0x0037) */
#define IPCP_VJMODE_RFC1172 2   /* "old-rfc"mode (option # = 0x002d) */
#define IPCP_VJMODE_RFC1332 3   /* "new-rfc"mode (option # = 0x002d, */
                                /*  maxslot and slot number compression) */

#define IPCP_VJ_COMP 0x002d     /* current value for VJ compression option*/
#define IPCP_VJ_COMP_OLD 0x0037 /* "old" (i.e, broken) value for VJ */
                                /* compression option*/

typedef struct ipcp_options {
    unsigned neg_addr : 1;      /* Negotiate IP Address? */
    unsigned old_addrs : 1;     /* Use old (IP-Addresses) option? */
    unsigned req_addr : 1;      /* Ask peer to send IP address? */
    unsigned default_route : 1; /* Assign default route through interface? */
    unsigned proxy_arp : 1;     /* Make proxy ARP entry for peer? */
    unsigned neg_vj : 1;        /* Van Jacobson Compression? */
    unsigned old_vj : 1;        /* use old (short) form of VJ option? */
    unsigned accept_local : 1;  /* accept peer's value for ouraddr */
    unsigned accept_remote : 1; /* accept peer's value for hisaddr */
    u_short vj_protocol;        /* protocol value to use in VJ option */
    u_char maxslotindex, cflag; /* values for RFC1332 VJ compression neg. */
    u_int32_t ouraddr, hisaddr; /* Addresses in NETWORK BYTE ORDER */
#ifdef USE_MS_DNS
    u_int32_t dnsaddr[2];       /* Primary and secondary DNS entries */
#endif
} ipcp_options;

extern fsm ipcp_fsm[];
extern ipcp_options ipcp_wantoptions[];
extern ipcp_options ipcp_gotoptions[];
extern ipcp_options ipcp_allowoptions[];
extern ipcp_options ipcp_hisoptions[];
extern u_int32_t namsrvaddr[][2];

void ipcp_init(int);
void ipcp_open(int);
void ipcp_close(int);
void ipcp_lowerup(int);
void ipcp_lowerdown(int);
void ipcp_input(int, u_char *, int);
void ipcp_protrej(int);
int  ipcp_printpkt(u_char *, int, void (*)(void *, char *, ...), void *);
