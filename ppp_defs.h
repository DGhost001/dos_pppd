/*      $Id: ppp_defs.h,v 1.2 1994/09/21 01:31:06 paulus Exp $  */

/*
 * ppp_defs.h - PPP definitions.
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
 */

/*
 *  ==FILEVERSION 6==
 *
 *  NOTE TO MAINTAINERS:
 *     If you modify this file at all, increment the number above.
 *     ppp_defs.h is shipped with a PPP distribution as well as with the kernel;
 *     if everyone increases the FILEVERSION number above, then scripts
 *     can do the right thing when deciding whether to install a new ppp_defs.h
 *     file.  Don't change the format of that line otherwise, so the
 *     installation script can recognize it.
 */

/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

#ifndef _PPP_DEFS_H_
#define _PPP_DEFS_H_

/*
 * The basic PPP frame.
 */
#define PPP_HDRLEN      4       /* octets for standard ppp header */
#define PPP_FCSLEN      2       /* octets for FCS */
#define PPP_MRU         1500    /* default MRU = max length of info field */

#define PPP_ADDRESS(p)  (((u_char *)(p))[0])
#define PPP_CONTROL(p)  (((u_char *)(p))[1])
#define PPP_PROTOCOL(p) ((((u_char *)(p))[2] << 8) + ((u_char *)(p))[3])

/*
 * Significant octet values.
 */
#define PPP_ALLSTATIONS 0xff    /* All-Stations broadcast address */
#define PPP_UI          0x03    /* Unnumbered Information */
#define PPP_FLAG        0x7e    /* Flag Sequence */
#define PPP_ESCAPE      0x7d    /* Asynchronous Control Escape */
#define PPP_TRANS       0x20    /* Asynchronous transparency modifier */

/*
 * Protocol field values.
 */
#define PPP_IP          0x21    /* Internet Protocol */
#define PPP_IPX         0x2b    /* IPX protocol */
#define PPP_VJC_COMP    0x2d    /* VJ compressed TCP */
#define PPP_VJC_UNCOMP  0x2f    /* VJ uncompressed TCP */
#define PPP_COMP        0xfd    /* compressed packet */
#define PPP_IPCP        0x8021  /* IP Control Protocol */
#define PPP_IPXCP       0x802b  /* IPX Control Protocol */
#define PPP_CCP         0x80fd  /* Compression Control Protocol */
#define PPP_LCP         0xc021  /* Link Control Protocol */
#define PPP_PAP         0xc023  /* Password Authentication Protocol */
#define PPP_LQR         0xc025  /* Link Quality Report protocol */
#define PPP_CHAP        0xc223  /* Cryptographic Handshake Auth. Protocol */

#include <sys/types.h>   /* This defines u_int32_t */
#include "types.h"

/*
 * Extended asyncmap - allows any character to be escaped.
 */
typedef u_int32_t ext_accm[8];

/*
 * What to do with network protocol (NP) packets.
 */
enum NPmode {
    NPMODE_PASS,                /* pass the packet through */
    NPMODE_DROP,                /* silently drop the packet */
    NPMODE_ERROR,               /* return an error */
    NPMODE_QUEUE                /* save it up for later. */
};

/*
 * Statistics for LQRP and pppstats
 */
struct pppstat  {
    u_int32_t        ppp_discards;   /* # frames discarded */
    u_int32_t        ppp_ibytes;     /* bytes received */
    u_int32_t        ppp_ioctects;   /* bytes received not in error */
    u_int32_t        ppp_ipackets;   /* packets received */
    u_int32_t        ppp_ierrors;    /* receive errors */
#ifdef ALLOWLQR
    u_int32_t        ppp_ilqrs;      /* # LQR frames received */
#endif  /* ALLOWLQR */
    u_int32_t        ppp_obytes;     /* raw bytes sent */
    u_int32_t        ppp_ooctects;   /* frame bytes sent */
    u_int32_t        ppp_opackets;   /* packets sent */
    u_int32_t        ppp_oerrors;    /* transmit errors */
#ifdef ALLOWLQR
    u_int32_t        ppp_olqrs;      /* # LQR frames sent */
#endif  /* ALLOWLQR */
};

#ifdef ALLOWVJ
struct vjstat {
    u_int32_t        vjs_packets;    /* outbound packets */
    u_int32_t        vjs_compressed; /* outbound compressed packets */
    u_int32_t        vjs_searches;   /* searches for connection state */
    u_int32_t        vjs_misses;     /* times couldn't find conn. state */
    u_int32_t        vjs_uncompressedin; /* inbound uncompressed packets */
    u_int32_t        vjs_compressedin;   /* inbound compressed packets */
    u_int32_t        vjs_errorin;    /* inbound unknown type packets */
    u_int32_t        vjs_tossed;     /* inbound packets tossed because of error */
};
#endif  /* ALLOWVJ */

#ifdef ALLOWCCP
struct compstat {
    u_int32_t        unc_bytes;      /* total uncompressed bytes */
    u_int32_t        unc_packets;    /* total uncompressed packets */
    u_int32_t        comp_bytes;     /* compressed bytes */
    u_int32_t        comp_packets;   /* compressed packets */
    u_int32_t        inc_bytes;      /* incompressible bytes */
    u_int32_t        inc_packets;    /* incompressible packets */
    /* the compression ratio is defined as in_count / bytes_out */
    u_int32_t        in_count;       /* Bytes received */
    u_int32_t        bytes_out;      /* Bytes transmitted */
    double           ratio;          /* not computed in kernel. */
};
#endif  /* ALLOWCCP */

struct ppp_stats {
    struct pppstat p;      /* basic PPP statistics */
#ifdef ALLOWVJ
    struct vjstat  vj;     /* VJ header compression statistics */
#endif  /* ALLOWVJ */
};

#ifdef ALLOWCCP
struct ppp_comp_stats {
    struct compstat     c;      /* packet compression statistics */
    struct compstat     d;      /* packet decompression statistics */
};
#endif  /* ALLOWCCP */

/*
 * The following structure records the time in seconds since
 * the last NP packet was sent or received.
 */
struct ppp_idle {
    unsigned long xmit_idle;    /* time since last NP packet sent */
    unsigned long recv_idle;    /* time since last NP packet received */
};

#endif /* _PPP_DEFS_H_ */

