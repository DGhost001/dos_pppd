/*      $Id: if_ppp.h,v 1.3 1995/06/12 11:36:50 paulus Exp $    */

/*
 * if_ppp.h - Point-to-Point Protocol definitions.
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
 */

/*
 *  ==FILEVERSION 6==
 *
 *  NOTE TO MAINTAINERS:
 *     If you modify this file at all, increment the number above.
 *     if_ppp.h is shipped with a PPP distribution as well as with the kernel;
 *     if everyone increases the FILEVERSION number above, then scripts
 *     can do the right thing when deciding whether to install a new if_ppp.h
 *     file.  Don't change the format of that line otherwise, so the
 *     installation script can recognize it.
 */

/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

#ifndef _IF_PPP_H_
#define _IF_PPP_H_

#include "ppp_defs.h"

/*
 * Packet sizes
 */

#define PPP_MTU         1500    /* Default MTU (size of Info field) */
#define PPP_MAXMRU      1500    /* Largest MRU we allow */
#define PPP_VERSION     "2.2.0"
#define PPP_MAGIC       0x5002  /* Magic value for the ppp structure */
#define PROTO_IPX       0x002b  /* protocol numbers */
#define PROTO_DNA_RT    0x0027  /* DNA Routing */

/*
 * Bit definitions for flags.
 */

#define SC_COMP_PROT    0x00000001LU    /* protocol compression (output) */
#define SC_COMP_AC      0x00000002LU    /* header compression (output) */
#define SC_COMP_TCP     0x00000004LU    /* TCP (VJ) compression (output) */
#define SC_NO_TCP_CCID  0x00000008LU    /* disable VJ connection-id comp. */
#define SC_REJ_COMP_AC  0x00000010LU    /* reject adrs/ctrl comp. on input */
#define SC_REJ_COMP_TCP 0x00000020LU    /* reject TCP (VJ) comp. on input */
#define SC_CCP_OPEN     0x00000040LU    /* Look at CCP packets */
#define SC_CCP_UP       0x00000080LU    /* May send/recv compressed packets */
#define SC_ENABLE_IP    0x00000100LU    /* IP packets may be exchanged */
#define SC_COMP_RUN     0x00001000LU    /* compressor has been inited */
#define SC_DECOMP_RUN   0x00002000LU    /* decompressor has been inited */
#define SC_DEBUG        0x00010000LU    /* enable debug messages */
#define SC_LOG_INPKT    0x00020000LU    /* log contents of good pkts recvd */
#define SC_LOG_OUTPKT   0x00040000LU    /* log contents of pkts sent */
#define SC_LOG_RAWIN    0x00080000LU    /* log all chars received */
#define SC_LOG_FLUSH    0x00100000LU    /* log all chars flushed */
#define SC_MASK         0x0fE0ffffLU    /* bits that user can change */

/* state bits */

#define SC_ESCAPED      0x80000000LU    /* saw a PPP_ESCAPE */
#define SC_FLUSH        0x40000000LU    /* flush input until next PPP_FLAG */
#define SC_VJ_RESET     0x20000000LU    /* Need to reset the VJ decompressor */
#define SC_XMIT_BUSY    0x10000000LU    /* ppp_write_wakeup is active */
#define SC_RCV_ODDP     0x08000000LU    /* have rcvd char with odd parity */
#define SC_RCV_EVNP     0x04000000LU    /* have rcvd char with even parity */
#define SC_RCV_B7_1     0x02000000LU    /* have rcvd char with bit 7 = 1 */
#define SC_RCV_B7_0     0x01000000LU    /* have rcvd char with bit 7 = 0 */
#define SC_DC_FERROR    0x00800000LU    /* fatal decomp error detected */
#define SC_DC_ERROR     0x00400000LU    /* non-fatal decomp error detected */

/*
 * Ioctl definitions.
 */

/* Structure describing a CCP configuration option, for PPPIOCSCOMPRESS */

/* statistic information */

/*
 * Ioctl definitions.
 */

 /*
  * Prototypes
  */

void ppp_send_config(int, int, u_int32_t, int, int);
void ppp_recv_config(int, int, u_int32_t, int, int);
void ppp_set_xaccm(int, ext_accm);
void ppp_set_debug(int, int);
int ppp_set_tty(int, int);
int ppp_dev_open(void);
void ppp_dev_close(int);
#ifndef LOWLEVELASY
int ppp_tty_receive(int, const u_char *, int);
#else   /* LOWLEVELASY */
int ppp_tty_receive(int);
#endif  /* LOWLEVELASY */
int ppp_tty_write(int, const u_char *, int);
int ppp_dev_xmit(int, const u_char far *, int);

#endif /* _IF_PPP_H_ */
