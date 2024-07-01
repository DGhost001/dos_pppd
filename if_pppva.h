/*      $Id: if_pppvar.h,v 1.2 1995/06/12 11:36:51 paulus Exp $ */
/*
 * if_pppvar.h - private structures and declarations for PPP.
 *
 * Copyright (c) 1994 The Australian National University.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  This software is provided without any
 * warranty, express or implied. The Australian National University
 * makes no representations about the suitability of this software for
 * any purpose.
 *
 * IN NO EVENT SHALL THE AUSTRALIAN NATIONAL UNIVERSITY BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
 * THE AUSTRALIAN NATIONAL UNIVERSITY HAVE BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * THE AUSTRALIAN NATIONAL UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUSTRALIAN NATIONAL UNIVERSITY HAS NO
 * OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
 * OR MODIFICATIONS.
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
 */

/*
 *  ==FILEVERSION 4==
 *
 *  NOTE TO MAINTAINERS:
 *     If you modify this file at all, increment the number above.
 *     if_pppvar.h is shipped with a PPP distribution as well as with the kernel;
 *     if everyone increases the FILEVERSION number above, then scripts
 *     can do the right thing when deciding whether to install a new if_pppvar.h
 *     file.  Don't change the format of that line otherwise, so the
 *     installation script can recognize it.
 */

/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

/*
 * Supported network protocols.  These values are used for
 * indexing sc_npmode.
 */

#ifndef _IF_PPPVA_H_
#define _IF_PPPVA_H_

#define NP_IP   0               /* Internet Protocol */
#define NUM_NP  1               /* Number of NPs. */

/*
 * Buffers for the PPP process have the following structure
 */

#define RBUFSIZE  2048          /* MUST be a power of 2 and be <= 4095 */

struct ppp_buffer {
  int                   size;           /* Size of the buffer area      */
  int                   count;          /* Count of characters in bufr  */
  int                   head;           /* index to head of list        */
  int                   tail;           /* index to tail of list        */
  int                   locked;         /* Buffer is being sent         */
  int                   type;           /* Type of the buffer           */
                                        /* =0, device read buffer       */
                                        /* =1, device write buffer      */
                                        /* =2, daemon write buffer      */
                                        /* =3, daemon read buffer       */
  unsigned short        fcs;            /* Frame Check Sequence (CRC)   */
  unsigned char         filler[4];      /* Extra space if needed        */
};

/* Given a pointer to the ppp_buffer then return base address of buffer */
#define BUF_BASE(buf) ((u_char *)(&buf[1]))

/*
 * Structure describing each ppp unit.
 */

struct ppp {
    int             magic;              /* magic value for structure    */
    int             line;               /* PPP channel number   */

  /* Bitmapped flag fields. */
    char            inuse;              /* are we allocated?            */
    char            escape;             /* 0x20 if prev char was PPP_ESC*/
    char            toss;               /* toss this frame              */
    unsigned long   flags;              /* miscellany                   */

    ext_accm        xmit_async_map;     /* 1 bit means that given control
                                           character is quoted on output*/
    unsigned long   recv_async_map;     /* 1 bit means that given control
                                           character is ignored on input*/
    unsigned char   chflagsmap[256];    /* for optimized async map access */

    int             mtu;                /* maximum xmit frame size      */
    int             mru;                /* maximum receive frame size   */

  /* Information about the current tty data */
    int             comid;              /* COM port id */

  /* Interface to the network layer */

  /* VJ Header compression data */
#ifdef ALLOWVJ
    struct slcompress *slcomp;          /* for header compression       */
    struct ppp_buffer *cbuf;            /* VJ compression buffer        */
#endif  /* ALLOWVJ */

    unsigned long   last_xmit;          /* time of last transmission    */

  /* These are pointers to the malloc()ed frame buffers.
     These buffers are used while processing a packet.  If a packet
     has to hang around for the user process to read it, it lingers in
     the user buffers below. */

    struct ppp_buffer *tbuf;            /* Transmission buffer          */
    struct ppp_buffer *rbuf;            /* Receiving buffer             */

  /* Queues for select() functionality */

  /* Statistic information */
    struct pppstat        stats;        /* statistic information        */
    struct ppp_idle       ddinfo;       /* demand dial information      */

  /* PPP compression protocol information */
#ifdef ALLOWCCP
    unsigned long sc_bytessent;         /* count of octets sent */
    unsigned long sc_bytesrcvd;         /* count of octets received */
    enum    NPmode sc_npmode[NUM_NP];   /* what to do with each NP */
    struct  compressor *sc_xcomp;       /* transmit compressor */
    void    *sc_xc_state;               /* transmit compressor state */
    struct  compressor *sc_rcomp;       /* receive decompressor */
    void    *sc_rc_state;               /* receive decompressor state */
#endif  /* ALLOWCCP */
};

#endif  /* _IF_PPPVA_H_ */

