/*  PPP for Linux
 *
 *  Michael Callahan <callahan@maths.ox.ac.uk>
 *  Al Longyear <longyear@netcom.com>
 *
 *  Dynamic PPP devices by Jim Freeman <jfree@caldera.com>.
 *  ppp_tty_receive ``noisy-raise-bug'' fixed by Ove Ewerlid <ewerlid@syscon.uu.se>
 *
 *  ==FILEVERSION 8==
 *
 *  NOTE TO MAINTAINERS:
 *     If you modify this file at all, increment the number above.
 *     ppp.c is shipped with a PPP distribution as well as with the kernel;
 *     if everyone increases the FILEVERSION number above, then scripts
 *     can do the right thing when deciding whether to install a new ppp.c
 *     file.  Don't change the format of that line otherwise, so the
 *     installation script can recognize it.
 */

/*
   Sources:

   slip.c

   RFC1331: The Point-to-Point Protocol (PPP) for the Transmission of
   Multi-protocol Datagrams over Point-to-Point Links

   RFC1332: IPCP

   ppp-2.0

   Flags for this module (any combination is acceptable for testing.):

   OPTIMIZE_FLAG_TIME - Number of jiffies to force sending of leading flag
                        character. This is normally set to ((HZ * 3) / 2).
                        This is 1.5 seconds. If zero then the leading
                        flag is always sent.

   CHECK_CHARACTERS   - Enable the checking on all received characters for
                        8 data bits, no parity. This adds a small amount of
                        processing for each received character.
*/

/* $Id: ppp.c,v 1.5 1995/06/12 11:36:53 paulus Exp $
 * Added dynamic allocation of channels to eliminate
 *   compiled-in limits on the number of channels.
 *
 * Dynamic channel allocation code Copyright 1995 Caldera, Inc.,
 *   released under the GNU General Public License Version 2.
 */

/* DOS port by ALM, tonilop@redestb.es */

#include <stdlib.h>
#include <mem.h>
#include <dos.h>

#include "asy.h"
#ifdef LOWLEVELASY
#include "n8250.h"
extern struct asy Asy[ASY_MAX];
#endif  /* LOWLEVELASY */
#include "ppp_defs.h"
#include "if_pppva.h"
#include "if_ppp.h"
#include "if_ether.h"
#include "syslog.h"
#include "pppd.h"
#include "fmem.h" /* For the far memory functions */

#define OPTIMIZE_FLAG_TIME  ((HZ * 3)/2)
/*
#define CHECK_CHARACTERS    1
#define PPP_COMPRESS        1
*/

#ifndef PPP_MAX_DEV
#define PPP_MAX_DEV 1
#endif

#ifdef ALLOWCCP
#undef   PACKETPTR
#define  PACKETPTR 1
#include "ppp-comp.h"
#undef   PACKETPTR

#define bsd_decompress  (*ppp->sc_rcomp->decompress)
#define bsd_compress    (*ppp->sc_xcomp->compress)

static int ppp_register_compressor(struct compressor *cp);
static void ppp_unregister_compressor(struct compressor *cp);
#endif  /* ALLOWCCP */

#ifndef PPP_IPX
#define PPP_IPX 0x2b  /* IPX protocol over PPP */
#endif

#ifndef PPP_LQR
#define PPP_LQR 0xc025  /* Link Quality Reporting Protocol */
#endif

/*
 * External functions
 */
char *ip_ntoa(u_int32_t);
int handle_ip_packet(int, const u_char *, int);

/*
 * Local functions
 */
static struct ppp_buffer *ppp_alloc_buf(int, int);
static void ppp_free_buf(struct ppp_buffer *);
static int ppp_rcv_rx(struct ppp *, u_short proto, u_char *, int);
static int rcv_proto_ip(struct ppp *, u_short, u_char *, int);
#ifdef ALLOWIPX
static int rcv_proto_ipx(struct ppp *, u_short, u_char *, int);
#endif  /* ALLOWIPX */
#ifdef ALLOWVJ
static int rcv_proto_vjc_comp(struct ppp *, u_short, u_char *, int);
static int rcv_proto_vjc_uncomp(struct ppp *, u_short, u_char *, int);
#endif  /* ALLOWVJ */
#ifdef ALLOWLQR
static int rcv_proto_lqr(struct ppp *, u_short, u_char *, int);
#endif  /* ALLOWLQR */

static void ppp_doframe_lower(struct ppp *, u_char *, int);
static int ppp_doframe(struct ppp *);

static void ppp_stuff_char(struct ppp *, struct ppp_buffer *, u_char);
static int ppp_dev_xmit_lower(struct ppp *, struct ppp_buffer *, const u_char *, int, int);
static int ppp_dev_xmit_frame(struct ppp *, struct ppp_buffer *, const u_char *, int);
#ifdef ALLOWLQR
static u_char *store_long(u_char *, u_int32_t);
#endif  /* ALLOWLQR */
static int send_revise_frame(struct ppp *, const u_char *, int);
static int ppp_dev_xmit_ip1(struct ppp *, u_char *, int);
static int ppp_dev_xmit_ip(struct ppp *, const u_char far *, int);

#ifdef DEBUGPPP
static void ppp_print_hex(u_char *, const u_char *, int);
static void ppp_print_char(u_char *, const u_char *, int);
static void ppp_print_buffer(const char *, const u_char *, int);
#endif  /* DEBUGPPP */

#ifdef ALLOWCCP
static struct compressor *find_compressor(int);
extern int ppp_bsd_compressor_init(void);
static void ppp_proto_ccp(struct ppp *, u_char *, int, int);
static int rcv_proto_ccp(struct ppp *, u_short, u_char *, int);
#endif  /* ALLOWCCP */

#define INS_CHAR(pbuf, c) (BUF_BASE(pbuf)[(pbuf)->count++] = (u_char)(c))

#ifndef OPTIMIZE_FLAG_TIME
#define OPTIMIZE_FLAG_TIME  0
#endif

static int flag_time = OPTIMIZE_FLAG_TIME;

/*
#define IN_XMAP(ppp, c)  (ppp->xmit_async_map[(c) >> 5] & (1LU << ((c) & 0x1F)))
#define IN_RMAP(ppp, c)  ((((u_short)(u_char)(c)) < 0x20) && ppp->recv_async_map & (1LU << (c)))
*/
/*
#define IN_XMAP(ppp, c)  (((u_short *)&(ppp->xmit_async_map))[(c) >> 4] & (1 << ((c) & 0x0F)))
#define IN_RMAP(ppp, c)  ((((u_short)(u_char)(c)) < 0x20) && ((u_short *)&(ppp->recv_async_map))[(c) >> 4] & (1 << ((c) & 0x0F)))
*/
#define RMAPTEST 1
#define XMAPTEST 2
#define PARITEST 4
#define IN_XMAP(ppp, c)  ((ppp)->chflagsmap[(u_char)(c)] & XMAPTEST)
#define IN_RMAP(ppp, c)  ((((u_short)(u_char)(c)) < 0x20) && ((ppp)->chflagsmap[(u_char)(c)] & RMAPTEST))

struct ppp_hdr {
    u_char address;
    u_char control;
    u_char protocol[2];
};

#define PPP_HARD_HDR_LEN    (sizeof(struct ppp_hdr))

/*
 * Buffer types
 */
#define BUFFER_TYPE_DEV_RD  0  /* ppp read buffer       */
#define BUFFER_TYPE_TTY_WR  1  /* tty write buffer      */
#define BUFFER_TYPE_DEV_WR  2  /* ppp write buffer      */
#define BUFFER_TYPE_TTY_RD  3  /* tty read buffer       */
#define BUFFER_TYPE_VJ      4  /* vj compression buffer */

static char szVersion[] = PPP_VERSION;
static struct ppp *ppptbl[PPP_MAX_DEV] = { NULL };
static int num_opened = 0;

#define PPPTTY(x)       (ppptbl[(x)]->comid)
#define PPPDEV(x)       (ppptbl[(x)])

typedef int (* pfn_proto)(struct ppp *, u_short, u_char *, int);

typedef struct ppp_proto_struct {
    int         proto;
    pfn_proto   func;
} ppp_proto_type;

static ppp_proto_type proto_list[] = {
    { PPP_IP,         rcv_proto_ip         },
#ifdef ALLOWIPX
    { PPP_IPX,        rcv_proto_ipx        },
#endif  /* ALLOWIPX */
#ifdef ALLOWVJ
    { PPP_VJC_COMP,   rcv_proto_vjc_comp   },
    { PPP_VJC_UNCOMP, rcv_proto_vjc_uncomp },
#endif  /* ALLOWVJ */
#ifdef ALLOWLQR
    { PPP_LQR,        rcv_proto_lqr        },
#endif  /* ALLOWLQR */
#ifdef ALLOWCCP
    { PPP_CCP,        rcv_proto_ccp        },
#endif  /* ALLOWCCP */
    { 0,              rcv_proto_unknown    }  /* !!! MUST BE LAST !!! */
};

/*
 * Values for FCS calculations.
 */
#define PPP_INITFCS 0xffff  /* Initial FCS value */
#define PPP_GOODFCS 0xf0b8  /* Good final FCS value */
/*
#define PPP_FCS(fcs, c) (((fcs) >> 8) ^ ppp_crc16_table[((fcs) ^ (c)) & 0xff])
*/
#define PPP_FCS(fcs, c) ((((u_char *)&(fcs))[1]) ^ ppp_crc16_table[((fcs) ^ (c)) & 0xff])

u_short ppp_crc16_table[256] =
{
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

#ifdef CHECK_CHARACTERS
static u_int32_t paritytab[8] =
{
    0x96696996LU, 0x69969669LU, 0x69969669LU, 0x96696996LU,
    0x69969669LU, 0x96696996LU, 0x96696996LU, 0x69969669LU
};
#endif

static void set_bits_in_map(u_char *map, int bmask, int nbits, ext_accm accm)
{
    int i;

    for ( i = 0 ; i < nbits ; i++ ) {
        if ( ((u_short *)accm)[(i) >> 4] & (1 << ((i) & 0x0F)) )
            map[i] |= bmask;
        else
            map[i] &= ~bmask;
    }
}

/*
 * ppp_send_config - configure the transmit characteristics of
 * the ppp interface.
 */

void ppp_send_config(int unit, int mtu, u_int32_t asyncmap, int pcomp, int accomp)
{
    struct ppp *ppp;
    u_int32_t x;

    extern int peermruval[NUM_PPP];

    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "ppp_send_config: invalid unit.\n"));

        return;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "ppp_send_config: unit not opened.\n"));

        return;
    }

    ppp->mtu = min(mtu, PPP_MRU);
    peermruval[unit] = ppp->mtu;

    ppp->xmit_async_map[0] = asyncmap;
    set_bits_in_map(ppp->chflagsmap, XMAPTEST, 32, ppp->xmit_async_map);
    PPPDEBUG((LOG_DEBUG, "ppp_send_config: set xmit asyncmap %lx.\n", asyncmap));

    x = ppp->flags;
    x = pcomp ? x | SC_COMP_PROT : x & ~SC_COMP_PROT;
    x = accomp ? x | SC_COMP_AC : x & ~SC_COMP_AC;
    ppp->flags = x;
    PPPDEBUG((LOG_DEBUG, "ppp_send_config: set flags %lx, mtu %d.\n", x, mtu));
}

/*
 * ppp_recv_config - configure the receive-side characteristics of
 * the ppp interface.
 */
#pragma argsused
void ppp_recv_config(int unit, int mru, u_int32_t asyncmap, int pcomp, int accomp)
{
    struct ppp *ppp;
    u_int32_t x;

    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "ppp_recv_config: invalid unit.\n"));

        return;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "ppp_recv_config: unit not opened.\n"));

        return;
    }

    ppp->mru = min(mru, PPP_MTU);

    ppp->recv_async_map = asyncmap;
    set_bits_in_map(ppp->chflagsmap, RMAPTEST, 32, &asyncmap);
    PPPDEBUG((LOG_DEBUG, "ppp_recv_config: set recv asyncmap %lx.\n", asyncmap));

    x = ppp->flags;
    x = accomp ? x & ~SC_REJ_COMP_AC : x | SC_REJ_COMP_AC;
    ppp->flags = x;
    PPPDEBUG((LOG_DEBUG, "ppp_recv_config: set flags %lx, mru %d.\n", x, mru));
}

/*
 * ppp_set_xaccm - set the extended transmit ACCM for the interface.
 */

void ppp_set_xaccm(int unit, ext_accm accm)
{
    struct ppp *ppp;
    u_int32_t taccm[8];

    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "ppp_set_xaccm: invalid unit.\n"));

        return;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "ppp_set_xaccm: unit not opened.\n"));

        return;
    }

    memcpy(taccm, accm, sizeof(ppp->xmit_async_map));
    taccm[1]  =  0x00000000LU;
    taccm[2] &= ~0x40000000LU;
    taccm[3] |=  0x60000000LU;

    if ( (taccm[2] & taccm[3]) != 0 ||
         (taccm[4] & taccm[5]) != 0 ||
         (taccm[6] & taccm[7]) != 0    ) {
        PPPDEBUG((LOG_DEBUG, "ppp_set_xaccm: invalid value ?.\n"));
    }
    else {
        memcpy(ppp->xmit_async_map, taccm, sizeof(ppp->xmit_async_map));
        set_bits_in_map(ppp->chflagsmap, XMAPTEST, 256, taccm);
        PPPDEBUG((LOG_DEBUG,
                  "ppp_set_xaccm: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx.\n",
                  taccm[7], taccm[6], taccm[5], taccm[4], taccm[3], taccm[2], taccm[1], taccm[0]));
    }
}

/*
 * Set the debug level for the PPP interface. The debug_level argument is
 * taken as a bit combination of the following values:
 * 1, basic debug messages.
 * 2, log packets before/after escape encoding.
 * 4, log raw tty I/O packets.
 */

void ppp_set_debug(int unit, int debug_level)
{
#ifdef DEBUGPPP
    struct ppp *ppp;

    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "ppp_set_debug: invalid unit.\n"));

        return;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "ppp_set_debug: unit not opened.\n"));

        return;
    }

    if ( debug_level & 1 )
        ppp->flags |= SC_DEBUG;
    else
        ppp->flags &= ~SC_DEBUG;

    if ( debug_level & 2 )
        ppp->flags |= (SC_LOG_OUTPKT | SC_LOG_INPKT);
    else
        ppp->flags &= ~(SC_LOG_OUTPKT | SC_LOG_INPKT);

    if ( debug_level & 4 )
        ppp->flags |= (SC_LOG_FLUSH | SC_LOG_RAWIN);
    else
        ppp->flags &= ~(SC_LOG_FLUSH | SC_LOG_RAWIN);
#endif  /* DEBUGPPP */
}

/*
 * Set the serial interface for a ppp unit.
 */

int ppp_set_tty(int unit, int ttyid)
{
    struct ppp *ppp;

    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "ppp_set_tty: invalid unit.\n"));

        return -1;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "ppp_set_tty: unit not opened.\n"));

        return -1;
    }

    ppp->comid = ttyid;

    return ttyid;
}

/*
 * Open a PPP device and returns pointer to ppp struct
 */

int ppp_dev_open(void)
{
    int unit;
    struct ppp *ppp;

    for ( unit = 0 ; unit < PPP_MAX_DEV ; unit++ )
        if ( PPPDEV(unit) == NULL )
            break;

    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "ppp_dev_open: no more units available.\n"));

        return -1;
    }

    if ( (ppp = malloc(sizeof(struct ppp))) == NULL ) {
        MAINDEBUG((LOG_ERR, "ppp_dev_open: out of memory.\n"));

        return -1;
    }

    ppp->magic = PPP_MAGIC;
    ppp->line = unit;
    ppp->inuse = 1;
    ppp->comid = -1;
    ppp->toss = 0xE0;
    ppp->escape = 0;

    ppp->flags = 0;
    ppp->mtu = PPP_MRU;
    ppp->mru = PPP_MTU;

    memset(ppp->xmit_async_map, 0, sizeof(ppp->xmit_async_map));
    memset(ppp->chflagsmap, 0, sizeof(ppp->chflagsmap));
    ppp->xmit_async_map[0] = 0xffffffffLU;
    ppp->xmit_async_map[3] = 0x60000000LU;
    ppp->recv_async_map = 0x00000000LU;
    set_bits_in_map(ppp->chflagsmap, RMAPTEST, 32, &(ppp->recv_async_map));
    set_bits_in_map(ppp->chflagsmap, XMAPTEST, 256, ppp->xmit_async_map);
#ifdef CHECK_CHARACTERS
    set_bits_in_map(ppp->chflagsmap, PARITEST, 256, paritytab);
#endif  /* CHECK_CHARACTERS */

    ppp->rbuf = NULL;
    ppp->tbuf = NULL;
#ifdef ALLOWVJ
    ppp->cbuf = NULL;
    ppp->slcomp = NULL;
#endif  /* ALLOWVJ */

#ifdef ALLOWCCP
    /* PPP compression data */
    ppp->sc_xc_state =
    ppp->sc_rc_state = NULL;
#endif  /* ALLOWCCP */

#ifdef ALLOWVJ
/*
 * Allocate space for the default VJ header compression slots
 */
    ppp->slcomp = slhc_init(16, 16);

    if ( ppp->slcomp == NULL ) {
        MAINDEBUG((LOG_ERR, "ppp_dev_open: slhc_init() failed.\n"));
        free(ppp);

        return -1;
    }
#endif  /* ALLOWVJ */

    ppp->tbuf = ppp_alloc_buf((PPP_MRU * 2) + 24, BUFFER_TYPE_TTY_WR);

    if ( ppp->tbuf == NULL ) {
        MAINDEBUG((LOG_ERR, "ppp_dev_open: out of memory.\n"));
        free(ppp);

        return -1;
    }

    ppp->rbuf = ppp_alloc_buf(ppp->mru + 84, BUFFER_TYPE_DEV_RD);

    if ( ppp->rbuf == NULL ) {
        MAINDEBUG((LOG_ERR, "ppp_dev_open: out of memory.\n"));
        free(ppp->tbuf);
        free(ppp);

        return -1;
    }

    ppp->rbuf->size -= 80;  /* reserve space for vj header expansion */

#ifdef ALLOWVJ
    ppp->cbuf = ppp_alloc_buf(ppp->mru + PPP_HARD_HDR_LEN, BUFFER_TYPE_VJ);

    if ( ppp->cbuf == NULL ) {
        MAINDEBUG((LOG_ERR, "ppp_dev_open: out of memory.\n"));
        free(ppp->rbuf);
        free(ppp->tbuf);
        free(ppp);

        return -1;
    }
#endif  /* ALLOWVJ */

    ppp->flags &= ~SC_XMIT_BUSY;
    /* clear statistics */
    memset(&ppp->stats, '\0', sizeof(struct pppstat));
    /* Reset the demand dial information */
    ppp->ddinfo.xmit_idle =               /* time since last NP packet sent */
    ppp->ddinfo.recv_idle = getjiffies(); /* time since last NP packet received */
    ppp->last_xmit = getjiffies() - flag_time;
    PPPDEV(unit) = ppp;
    PPPDEBUG((LOG_DEBUG, "ppp_dev_open: channel ppp%d open.\n", unit));
    ++num_opened;

    return unit;
}

/*
 * Close a PPP device and free all structures and buffers
 */

void ppp_dev_close(int unit)
{
    struct ppp *ppp;

    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "ppp_dev_close: invalid unit.\n"));

        return;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "ppp_dev_close: unit not opened.\n"));

        return;
    }

    ppp_free_buf(ppp->rbuf);
    ppp_free_buf(ppp->tbuf);

#ifdef ALLOWVJ
    if ( ppp->slcomp ) {
        slhc_free(ppp->slcomp);
    }

    ppp_free_buf(ppp->cbuf);
#endif  /* ALLOWVJ */

    free(ppp);
    PPPDEV(unit) = NULL;
    PPPDEBUG((LOG_DEBUG, "ppp_dev_close: channel ppp%d closed.\n", unit));
    --num_opened;
}

/*
 * Routine to allocate a buffer for later use by the driver.
 */

static struct ppp_buffer *ppp_alloc_buf(int size, int type)
{
    struct ppp_buffer *buf;

    buf = (struct ppp_buffer *)malloc(size + sizeof(struct ppp_buffer));

    if ( buf != NULL ) {
        buf->size = size - 1; /* Mask for the buffer size */
        buf->type = type;
        buf->locked = 0;
        buf->count = 0;
        buf->head = 0;
        buf->tail = 0;
        buf->fcs = PPP_INITFCS;
    }

    return (buf);
}

/*
 * Routine to release the allocated buffer.
 */

static void ppp_free_buf(struct ppp_buffer *ptr)
{
    if ( ptr != NULL )
        free(ptr);
}

/*************************************************************
 * TTY INPUT
 *    The following functions handle input that arrives from
 *    the TTY.  It recognizes PPP frames and either hands them
 *    to the network layer or queues them for delivery to a
 *    user process reading this TTY.
 *************************************************************/

/*
 * Callback function when data is available at the tty driver.
 */

#ifndef LOWLEVELASY
int ppp_tty_receive(int unit, const u_char *data, int count)
{
    u_char chr;
    struct ppp *ppp;
    struct ppp_buffer *buf = NULL;
/*
 * Verify the table pointer.
 */
    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "ppp_tty_receive: invalid unit.\n"));

        return -1;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "ppp_tty_receive: unit not opened.\n"));

        return -1;
    }
/*
 * Fetch the pointer to the buffer. Be careful about race conditions.
 */
    if ( (buf = ppp->rbuf) == NULL ) {
        PPPDEBUG((LOG_ERR, "ppp_tty_receive: unallocated receive buffer.\n"));

        return -1;
    }
/*
 * Print the buffer if desired
 */
#ifdef DEBUGPPP
    if ( ppp->flags & SC_LOG_RAWIN )
        ppp_print_buffer("ppp_tty_receive:", data, count);
#endif  /* DEBUGPPP */
/*
 * Collect the character.
 */
    while ( count-- > 0 ) {
        chr = *data++;
/*
 * Set the flags for 8 data bits and no parity.
 *
 * Actually, it sets the flags for d7 being 0/1 and parity being even/odd
 * so that the normal processing would have all flags set at the end of the
 * session. A missing flag bit would denote an error condition.
 */
#ifdef CHECK_CHARACTERS
        if ( chr & 0x80 )
            ppp->flags |= SC_RCV_B7_1;
        else
            ppp->flags |= SC_RCV_B7_0;
/*
        if ( paritytab[chr >> 5] & (1LU << (chr & 0x1F)) )
*/
/*
        if ( ((u_short *)&paritytab)[chr >> 4] & (1 << (chr & 0x0F)) )
*/
        if ( ppp->chflagsmap[(u_char)chr] & PARITEST )
            ppp->flags |= SC_RCV_ODDP;
        else
            ppp->flags |= SC_RCV_EVNP;
#endif
/*
 * Branch on the character. Process the escape character. The sequence ESC ESC
 * is defined to be ESC.
 */
        switch ( chr ) {
            case PPP_ESCAPE: /* PPP_ESCAPE: invert bit in next character */
                ppp->escape = PPP_TRANS;
            break;
/*
 * FLAG. This is the end of the block. If the block terminated by ESC FLAG,
 * then the block is to be ignored. In addition, characters before the very
 * first FLAG are also tossed by this procedure.
 */
            case PPP_FLAG:  /* PPP_FLAG: end of frame */
                ppp->stats.ppp_ibytes += buf->count;

                if ( ppp->escape )
                    ppp->toss |= 0x80;
/*
 * Process frames which are not to be ignored. If the processing failed,
 * then clean up the VJ tables.
 */
                if ( (ppp->toss & 0x80) != 0 || ppp_doframe(ppp) == 0 ) {
#ifdef ALLOWVJ
                    slhc_toss(ppp->slcomp);
#else
                    ;
#endif  /* ALLOWVJ */
                }
/*
 * Reset all indicators for the new frame to follow.
 */
                buf->count = 0;
                buf->fcs = PPP_INITFCS;
                ppp->escape = 0;
                ppp->toss = 0;
            break;
/*
 * All other characters in the data come here. If the character is in the
 * receive mask then ignore the character.
 */
            default:
                if ( IN_RMAP(ppp, chr) )
                    break;
/*
 * Adjust the character and if the frame is to be discarded then simply
 * ignore the character until the ending FLAG is received.
 */
                chr ^= ppp->escape;
                ppp->escape = 0;

                if ( ppp->toss != 0 )
                    break;
/*
 * If the count sent is within reason then store the character, bump the
 * count, and update the FCS for the character.
 */
                if ( buf->count < buf->size ) {
                    INS_CHAR(buf, chr);
                    buf->fcs = PPP_FCS(buf->fcs, chr);
                    break;
                }
/*
 * The peer sent too much data. Set the flags to discard the current frame
 * and wait for the re-synchronization FLAG to be sent.
 */
                ppp->stats.ppp_ierrors++;
                ppp->toss |= 0xC0;
            break;
        }
    }

    return buf->count;
}
#else   /* LOWLEVELASY */
int ppp_tty_receive(int unit)
{
    u_char chr;
    struct fifo *fp;
    struct ppp_buffer *buf;
    struct ppp *ppp;
#ifdef DEBUGPPP
    int i = 0;
    static uint8 rcvbuf[128];
#endif  /* DEBUGPPP */
/*
 * Verify the table pointer.
 */
    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "ppp_tty_receive: invalid unit.\n"));

        return -1;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "ppp_tty_receive: unit not opened.\n"));

        return -1;
    }
/*
 * Fetch the pointer to the buffer. Be careful about race conditions.
 */
    if ( (buf = ppp->rbuf) == NULL ) {
        PPPDEBUG((LOG_ERR, "ppp_tty_receive: unallocated receive buffer.\n"));

        return -1;
    }

    fp = &Asy[ppp->comid].fifo;
/*
 * Collect the character.
 */
#ifdef DEBUGPPP
    while ( i < sizeof(rcvbuf) && fp->cnt > 0 ) {
        rcvbuf[i++] = chr = *fp->rp++;
#else   /* DEBUGPPP */
    while ( fp->cnt > 0 ) {
        chr = *fp->rp++;
#endif  /* DEBUGPPP */

        if ( fp->rp >= fp->ep )
            fp->rp = fp->buf;

        fp->cnt--;
/*
 * Set the flags for 8 data bits and no parity.
 *
 * Actually, it sets the flags for d7 being 0/1 and parity being even/odd
 * so that the normal processing would have all flags set at the end of the
 * session. A missing flag bit would denote an error condition.
 */
#ifdef CHECK_CHARACTERS
        if ( chr & 0x80 )
            ppp->flags |= SC_RCV_B7_1;
        else
            ppp->flags |= SC_RCV_B7_0;
/*
        if ( paritytab[chr >> 5] & (1LU << (chr & 0x1F)) )
*/
/*
        if ( ((u_short *)&paritytab)[chr >> 4] & (1 << (chr & 0x0F)) )
*/
        if ( ppp->chflagsmap[(u_char)chr] & PARITEST )
            ppp->flags |= SC_RCV_ODDP;
        else
            ppp->flags |= SC_RCV_EVNP;
#endif
/*
 * Branch on the character. Process the escape character. The sequence ESC ESC
 * is defined to be ESC.
 */
        switch ( chr ) {
            case PPP_ESCAPE: /* PPP_ESCAPE: invert bit in next character */
                ppp->escape = PPP_TRANS;
            break;
/*
 * FLAG. This is the end of the block. If the block terminated by ESC FLAG,
 * then the block is to be ignored. In addition, characters before the very
 * first FLAG are also tossed by this procedure.
 */
            case PPP_FLAG:  /* PPP_FLAG: end of frame */
#ifdef DEBUGPPP
                /*
                 * Print the buffer if desired and if end of frame.
                 */
                if ( (ppp->flags & SC_LOG_RAWIN) && buf->count && i ) {
                    ppp_print_buffer("ppp_tty_receive:", rcvbuf, i);
                    i = 0; /* prevent further printing at function exit */
                }
#endif  /* DEBUGPPP */

                ppp->stats.ppp_ibytes += buf->count;

                if ( ppp->escape )
                    ppp->toss |= 0x80;
/*
 * Process frames which are not to be ignored. If the processing failed,
 * then clean up the VJ tables.
 */
                if ( (ppp->toss & 0x80) != 0 || ppp_doframe(ppp) == 0 ) {
#ifdef ALLOWVJ
                    slhc_toss(ppp->slcomp);
#else
                    ;
#endif  /* ALLOWVJ */
                }
/*
 * Reset all indicators for the new frame to follow.
 */
                buf->count = 0;
                buf->fcs = PPP_INITFCS;
                ppp->escape = 0;
                ppp->toss = 0;
            break;
/*
 * All other characters in the data come here. If the character is in the
 * receive mask then ignore the character.
 */
            default:
                if ( IN_RMAP(ppp, chr) )
                    break;
/*
 * Adjust the character and if the frame is to be discarded then simply
 * ignore the character until the ending FLAG is received.
 */
                chr ^= ppp->escape;
                ppp->escape = 0;

                if ( ppp->toss != 0 )
                    break;
/*
 * If the count sent is within reason then store the character, bump the
 * count, and update the FCS for the character.
 */
                if ( buf->count < buf->size ) {
                    INS_CHAR(buf, chr);
                    buf->fcs = PPP_FCS(buf->fcs, chr);
                    break;
                }
/*
 * The peer sent too much data. Set the flags to discard the current frame
 * and wait for the re-synchronization FLAG to be sent.
 */
                ppp->stats.ppp_ierrors++;
                ppp->toss |= 0xC0;
            break;
        }
    }
#ifdef DEBUGPPP
/*
 * Print the buffer if desired
 */
    if ( i && (ppp->flags & SC_LOG_RAWIN) )
        ppp_print_buffer("ppp_tty_receive:", rcvbuf, i);
#endif  /* DEBUGPPP */

    return buf->count;
}
#endif  /* LOWLEVELASY */

/*
 * Put the input frame into the networking system for the indicated protocol
 */

static int ppp_rcv_rx(struct ppp *ppp, u_short proto, u_char *data, int count)
{
    ppp->ddinfo.recv_idle = getjiffies();

    if ( proto == htons(ETH_P_IP) )
        return handle_ip_packet(ppp->line, (const u_char *)data, count);

    return 0;
}

/*
 * Process the receipt of an IP frame
 */
#pragma argsused
static int rcv_proto_ip(struct ppp *ppp, u_short proto, u_char *data, int count)
{
    if ( ppp->flags & SC_ENABLE_IP ) {
        if ( count > 0 ) {
            PPPDEBUG((LOG_DEBUG, "rcv_proto_ip: passing %d bytes up, flags = %lx.\n", count, ppp->flags));
            return ppp_rcv_rx(ppp, htons(ETH_P_IP), data, count);
        }
    }
    else {
        PPPDEBUG((LOG_DEBUG, "rcv_proto_ip: dropping IP packet, interface ppp%d is down.\n", ppp->line));
    }

    return 0;
}

#ifdef ALLOWLQR
/*
 * Handle a LQR packet.
 *
 * The LQR packet is passed along to the pppd process just like any
 * other PPP frame. The difference is that some processing needs to be
 * performed to append the current data to the end of the frame.
 */

static int rcv_proto_lqr(struct ppp *ppp, u_short proto, u_char *data, int len)
{
#if 0   /* until support is in the pppd process don't corrupt the reject. */
    u_char *p;

    if ( len > 8 ) {
        if ( len < 48 )
            memset(&data[len], '\0', 48 - len);
/*
 * Fill in the fields from the driver data
 */
        p = &data[48];
        p = store_long(p, ++ppp->stats.ppp_ilqrs);
        p = store_long(p, ppp->stats.ppp_ipackets);
        p = store_long(p, ppp->stats.ppp_discards);
        p = store_long (p, ppp->stats.ppp_ierrors);
        p = store_long(p, ppp->stats.ppp_ioctects + len);

        len = 68;
    }
#endif
/*
 * Pass the frame to the pppd daemon.
 */
    return rcv_proto_unknown(ppp, proto, data, len);
}
#endif  /* ALLOWLQR */

/* on entry, a received frame is in ppp->rbuf.bufr
   check it and dispose as appropriate */

static void ppp_doframe_lower(struct ppp *ppp, u_char *data, int count)
{
    u_short proto = PPP_PROTOCOL(data);
    ppp_proto_type *proto_ptr;
/*
 * Ignore empty frames
 */
    if ( count <= 4 )
        return;
/*
 * Count the frame and print it
 */
    ++ppp->stats.ppp_ipackets;

#ifdef DEBUGPPP
    if ( ppp->flags & SC_LOG_INPKT )
        ppp_print_buffer("ppp_doframe_lower:", data, count);
#endif  /* DEBUGPPP */

/*
 * Find the procedure to handle this protocol. The last one is marked
 * as a protocol 0 which is the 'catch-all' to feed it to the pppd daemon.
 */
    proto_ptr = proto_list;

    while ( proto_ptr->proto != 0 && proto_ptr->proto != proto )
        ++proto_ptr;
/*
 * Update the appropriate statistic counter.
 */
    if ( (*proto_ptr->func)(ppp, proto, &data[PPP_HARD_HDR_LEN], count - PPP_HARD_HDR_LEN) )
        ppp->stats.ppp_ioctects += count;
    else
        ++ppp->stats.ppp_discards;
}

/* on entry, a received frame is in ppp->rbuf.bufr
   check it and dispose as appropriate */

static int ppp_doframe(struct ppp *ppp)
{
    u_char *data = BUF_BASE(ppp->rbuf);
    int count = ppp->rbuf->count;
    int addr, ctrl, proto;
    int new_count;
    u_char *new_data;
/*
 * If there is a pending error from the receiver then log it and discard
 * the damaged frame.
 */
    if ( ppp->toss ) {
        PPPDEBUG((LOG_DEBUG, "ppp_doframe: tossing frame, reason = %d\n", ppp->toss));

        ppp->stats.ppp_ierrors++;

        return 0;
    }
/*
 * An empty frame is ignored. This occurs if the FLAG sequence precedes and
 * follows each frame.
 */
    if ( count == 0 )
        return 1;
/*
 * Generate an error if the frame is too small.
 */
    if ( count < PPP_HARD_HDR_LEN ) {
        PPPDEBUG((LOG_DEBUG, "ppp_doframe: got runt ppp frame, %d chars\n", count));
#ifdef ALLOWVJ
        slhc_toss(ppp->slcomp);
#endif  /* ALLOWVJ */
        ppp->stats.ppp_ierrors++;

        return 1;
    }
/*
 * Generate an error if the frame is too large.
 */
    if ( count > (ppp->mru + 4) ) {
        PPPDEBUG((LOG_DEBUG, "ppp_doframe: ppp frame larger than our mru, %d chars\n", count));
#ifdef ALLOWVJ
        slhc_toss(ppp->slcomp);
#endif  /* ALLOWVJ */
        ppp->stats.ppp_ierrors++;

        return 1;
    }
/*
 * Verify the CRC of the frame and discard the CRC characters from the
 * end of the buffer.
 */
    if ( ppp->rbuf->fcs != PPP_GOODFCS ) {
        PPPDEBUG((LOG_DEBUG, "ppp_doframe: frame with bad fcs, excess = %x\n", ppp->rbuf->fcs ^ PPP_GOODFCS));
        ppp->stats.ppp_ierrors++;

        return 0;
    }

    count -= 2;     /* ignore the fcs characters */
/*
 * Ignore the leading ADDRESS and CONTROL fields in the frame.
 */
    addr = PPP_ALLSTATIONS;
    ctrl = PPP_UI;

    if ( (data[0] == PPP_ALLSTATIONS) && (data[1] == PPP_UI) ) {
        data  += 2;
        count -= 2;
    }
/*
 * Obtain the protocol from the frame
 */
    proto = (u_short)*data++;

    if ( (proto & 1) == 0 ) {
        proto = (proto << 8) | (u_short)*data++;
        --count;
    }
/*
 * Rewrite the header with the full information. This may encroach upon
 * the 'filler' area in the buffer header. This is the purpose for the
 * filler.
 */
    *(--data) = proto;
    *(--data) = proto >> 8;
    *(--data) = ctrl;
    *(--data) = addr;
    count += 3;
/*
 * Process the active decompressor.
 */
#ifdef ALLOWCCP
    if ( (ppp->sc_rc_state != (void *)0) &&
         (ppp->flags & SC_DECOMP_RUN) &&
         ((ppp->flags & (SC_DC_FERROR | SC_DC_ERROR)) == 0) ) {
        if ( proto == PPP_COMP ) {
/*
 * If the frame is compressed then decompress it.
 */
            new_data = malloc(ppp->mru + 4);

            if ( new_data == NULL ) {
                PPPDEBUG((LOG_DEBUG, "ppp_doframe: no memory\n"));

                slhc_toss(ppp->slcomp);
                (*ppp->sc_rcomp->incomp)(ppp->sc_rc_state, data, count);

                return 1;
            }
/*
 * Decompress the frame
 */
            new_count = bsd_decompress(ppp->sc_rc_state, data, count, new_data, ppp->mru + 4);

            switch ( new_count ) {
                default:
                    ppp_doframe_lower(ppp, new_data, new_count);
                    free(new_data);

                    return 1;

                case DECOMP_OK:
                break;

                case DECOMP_ERROR:
                    ppp->flags |= SC_DC_ERROR;
                break;

                case DECOMP_FATALERROR:
                    ppp->flags |= SC_DC_FERROR;
                break;
            }
/*
 * Log the error condition and discard the frame.
 */
            PPPDEBUG((LOG_DEBUG, "ppp_doframe: decompress err %d\n", new_count));

            free(new_data);
            slhc_toss(ppp->slcomp);

            return 1;
        }
/*
 * The frame is not special. Pass it through the compressor without
 * actually compressing the data
 */
        (*ppp->sc_rcomp->incomp)(ppp->sc_rc_state, data, count);
    }
#endif  /* ALLOWCCP */
/*
 * Process the uncompressed frame.
 */
    ppp_doframe_lower(ppp, data, count);

    return 1;
}

/* stuff a character into the transmit buffer, using PPP's way of escaping
   special characters.
   also, update fcs to take account of new character */

static void ppp_stuff_char(struct ppp *ppp, struct ppp_buffer *buf, u_char chr)
{
/*
 * The buffer should not be full.
 */
    if ( ppp->flags & SC_DEBUG ) {
        if ( (buf->count < 0) || (buf->count > 3000) ) {
            PPPDEBUG((LOG_DEBUG, "ppp_stuff_char: %x %d\n", (u_short)buf->count, (u_short)chr));
        }
    }
/*
 * Update the FCS and if the character needs to be escaped, do it.
 */
    buf->fcs = PPP_FCS(buf->fcs, chr);

    if ( IN_XMAP(ppp, chr) ) {
        chr ^= PPP_TRANS;
        INS_CHAR(buf, PPP_ESCAPE);
    }
/*
 * Add the character to the buffer.
 */
    INS_CHAR(buf, chr);
}

/*
 * Procedure to encode the data with the proper escapement and send the
 * data to the remote system.
 */

static int ppp_dev_xmit_lower(struct ppp *ppp, struct ppp_buffer *buf, const u_char *data, int count, int non_ip)
{
    u_short write_fcs;
    int address, control;
    int proto;
    int r;

    extern u_short timercount;

/*
 * Wait for the TX buffer to become available.
 */
    if ( ppp->comid < 0 ) {
        PPPDEBUG((LOG_DEBUG, "ppp_dev_xmit_lower: invalid comid.\n"));

        return 1;
    }

    timercount = HZ * 10;       /* 10 second timeout */

    while ( (r = asy_txcheck(ppp->comid)) != 0 ) {
        if ( r < 0 ) {
            PPPDEBUG((LOG_DEBUG, "ppp_dev_xmit_lower: asy_txcheck() error.\n"));

            return 1;
        }

        if ( timercount == 0 ) {
            PPPDEBUG((LOG_DEBUG, "ppp_dev_xmit_lower: asy_txcheck() timeout.\n"));

            return 1;
        }
    }
/*
 * Insert the leading FLAG character
 */
    buf->count = 0;

    if ( non_ip || flag_time == 0 )
        INS_CHAR(buf, PPP_FLAG);
    else {
        if ( (getjiffies() - ppp->last_xmit) > flag_time )
            INS_CHAR(buf, PPP_FLAG);
    }

    ppp->last_xmit = getjiffies();
    buf->fcs = PPP_INITFCS;
/*
 * Emit the address/control information if needed
 */
    address = PPP_ADDRESS(data);
    control = PPP_CONTROL(data);
    proto = PPP_PROTOCOL(data);

    if ( address != PPP_ALLSTATIONS ||
         control != PPP_UI          ||
         (ppp->flags & SC_COMP_AC) == 0 ) {
        ppp_stuff_char(ppp, buf, address);
        ppp_stuff_char(ppp, buf, control);
    }
/*
 * Emit the protocol (compressed if possible)
 */
    if ( (ppp->flags & SC_COMP_PROT) == 0 || (proto & 0xFF00) )
        ppp_stuff_char(ppp, buf, proto >> 8);

    ppp_stuff_char(ppp, buf, proto);
/*
 * Insert the data
 */
    data += 4;
    count -= 4;

    while ( count-- > 0 )
        ppp_stuff_char(ppp, buf, *data++);
/*
 * Add the trailing CRC and the final flag character
 */
    write_fcs = buf->fcs ^ 0xFFFF;
    ppp_stuff_char(ppp, buf, write_fcs);
    ppp_stuff_char(ppp, buf, write_fcs >> 8);

    PPPDEBUG((LOG_DEBUG, "ppp_dev_xmit_lower: fcs is 0x%x\n", write_fcs));
/*
 * Add the trailing flag character
 */
    INS_CHAR(buf, PPP_FLAG);
/*
 * Print the buffer
 */
#ifdef DEBUGPPP
    if ( ppp->flags & SC_LOG_FLUSH )
        ppp_print_buffer("ppp_dev_xmit_lower:", BUF_BASE(buf), buf->count);
    else {
        PPPDEBUG((LOG_DEBUG, "ppp_dev_xmit_lower: writing %d chars\n", buf->count));
    }
#endif  /* DEBUGPPP */
/*
 * Send the block to the tty driver.
 */
    ppp->stats.ppp_obytes += buf->count;

    if ( asy_write(ppp->comid, BUF_BASE(buf), buf->count) < 0 ) {
        PPPDEBUG((LOG_DEBUG, "ppp_dev_xmit_lower: asy_write() error.\n"));

        return 1;
    }

    return 0;
}

/*
 * Send an frame to the remote with the proper bsd compression.
 *
 * Return 0 if frame was queued for transmission.
 *        1 if frame must be re-queued for later driver support.
 */

static int ppp_dev_xmit_frame(struct ppp *ppp, struct ppp_buffer *buf, const u_char *data, int count)
{
    int proto;
    int address, control;
    u_char *new_data;
    int new_count, retcode;
/*
 * Print the buffer
 */
#ifdef DEBUGPPP
    if ( ppp->flags & SC_LOG_OUTPKT )
        ppp_print_buffer("ppp_dev_xmit_frame:", data, count);
#endif  /* DEBUGPPP */

/*
 * Determine if the frame may be compressed. Attempt to compress the
 * frame if possible.
 */
    proto = PPP_PROTOCOL(data);
#ifdef ALLOWCCP
    address = PPP_ADDRESS(data);
    control = PPP_CONTROL(data);

    if ( ((ppp->flags & SC_COMP_RUN) != 0) &&
         (ppp->sc_xc_state != (void *)0) &&
         (address == PPP_ALLSTATIONS) &&
         (control == PPP_UI) &&
         (proto != PPP_LCP) &&
         (proto != PPP_CCP) ) {
        new_data = malloc(count);

        if ( new_data == NULL ) {
            PPPDEBUG((LOG_DEBUG, "ppp_dev_xmit_frame: no memory\n"));

            return 1;
        }

        new_count = bsd_compress(ppp->sc_xc_state, data, new_data, count, count);

        if ( new_count > 0 ) {
            ++ppp->stats.ppp_opackets;
            ppp->stats.ppp_ooctects += new_count;

            retcode = ppp_dev_xmit_lower(ppp, buf, new_data, new_count, 0);
            free(new_data);

            return retcode;
        }
/*
 * The frame could not be compressed.
 */
        free(new_data);
    }
#endif  /* ALLOWCCP */
/*
 * The frame may not be compressed. Update the statistics before the
 * count field is destroyed. The frame will be transmitted.
 */
    ++ppp->stats.ppp_opackets;
    ppp->stats.ppp_ooctects += count;
/*
 * Go to the escape encoding
 */
    return ppp_dev_xmit_lower(ppp, buf, data, count, (proto & 0xFF00) != 0);
}

#ifdef ALLOWLQR
/* local function to store a value into the LQR frame */
static u_char *store_long(u_char *p, u_int32_t value)
{
    *p++ = ((u_char *)&value)[3];
    *p++ = ((u_char *)&value)[2];
    *p++ = ((u_char *)&value)[1];
    *p++ = ((u_char *)&value)[0];

    return p;
}
#endif  /* ALLOWLQR */

/*
 * Revise the tty frame for specific protocols.
 */
#pragma argsused
static int send_revise_frame(struct ppp *ppp, const u_char *data, int len)
{
    u_char *p;

    switch ( PPP_PROTOCOL(data) ) {
#ifdef ALLOWLQR
/*
 * Update the LQR frame with the current MIB information. This saves having
 * the daemon read old MIB data from the driver.
 */
        case PPP_LQR:
            len = 48;   /* total size of this frame */
            p = (u_char *)&data[40];    /* Point to last two items. */
            p = store_long(p, ppp->stats.ppp_opackets + 1);
            p = store_long(p, ppp->stats.ppp_ooctects + len);
        break;
#endif  /* ALLOWLQR */
#ifdef ALLOWCCP
/*
 * Outbound compression frames
 */
        case PPP_CCP:
            ppp_proto_ccp(ppp, data + PPP_HARD_HDR_LEN, len  - PPP_HARD_HDR_LEN, 0);
        break;
#endif  /* ALLOWCCP */
/*
 * All other frame types
 */
        default:
        break;
    }

    return len;
}

/*
 * write a frame with NR chars from BUF to TTY
 * we have to put the FCS field on ourselves
 */

int ppp_tty_write(int unit, const u_char *data, int count)
{
    struct ppp *ppp;
/*
 * Verify the pointer to the PPP data and that the tty is still in PPP mode.
 */
    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "ppp_tty_write: invalid unit.\n"));

        return -1;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "ppp_tty_write: unit not opened.\n"));

        return -1;
    }
/*
 * Ensure that the caller does not wish to send too much.
 */
    if ( count > ppp->mtu ) {
        PPPDEBUG((LOG_DEBUG, "ppp_tty_write: truncating user packet from %u to mtu %d\n", count, ppp->mtu));

        count = ppp->mtu;
    }
/*
 * Change the LQR frame
 */
    count = send_revise_frame(ppp, data, count);
/*
 * Send the data
 */
    ppp_dev_xmit_frame(ppp, ppp->tbuf, data, count);

    return (int)count;
}

/*
 * Send an IP frame to the remote with vj header compression.
 *
 * Return 0 if frame was queued for transmission.
 *        1 if frame must be re-queued for later driver support.
 */

static int ppp_dev_xmit_ip1(struct ppp *ppp, u_char *data, int len)
{
    int proto = PPP_IP;
    struct ppp_hdr *hdr;
/*
 * Ensure that the PPP device is still up
 */
    if ( ! (ppp->flags & SC_ENABLE_IP) ) {
        PPPDEBUG((LOG_DEBUG, "ppp_dev_xmit_ip1: packet sent when interface ppp%d is down for IP.\n", ppp->line));

        return 1;
    }
/*
 * Print the frame being sent
 */
#ifdef DEBUGPPP
    if ( ppp->flags & SC_LOG_OUTPKT )
        ppp_print_buffer("ppp_dev_xmit_ip1:", data, len);
#endif  /* DEBUGPPP */

/*
 * At this point, the buffer will be transmitted. There is no other exit.
 *
 * Try to compress the header.
 */
#ifdef ALLOWVJ
    if ( ppp->flags & SC_COMP_TCP ) {
        len = slhc_compress(ppp->slcomp, data, len, BUF_BASE(ppp->cbuf) + PPP_HARD_HDR_LEN, &data, (ppp->flags & SC_NO_TCP_CCID) == 0);

        if ( data[0] & SL_TYPE_COMPRESSED_TCP ) {
            proto = PPP_VJC_COMP;
            data[0] ^= SL_TYPE_COMPRESSED_TCP;
        } else {
            if ( data[0] >= SL_TYPE_UNCOMPRESSED_TCP )
                proto = PPP_VJC_UNCOMP;
            data[0] = (data[0] & 0x0f) | 0x40;
        }
    }
#endif  /* ALLOWVJ */
/*
 * Send the frame
 */
    len += PPP_HARD_HDR_LEN;
    hdr = &((struct ppp_hdr *)data)[-1];

    hdr->address = PPP_ALLSTATIONS;
    hdr->control = PPP_UI;
    hdr->protocol[0] = 0;
    hdr->protocol[1] = proto;

    return ppp_dev_xmit_frame(ppp, ppp->tbuf, (const u_char *)hdr, len);
}

/*
 * This is just an interum solution until the 1.3 kernel's networking is
 * available. The 1.2 kernel has problems with device headers before the
 * buffers.
 *
 * This routine should be deleted, and the ppp_dev_xmit_ip1 routine called
 * by this name.
 */

static int ppp_dev_xmit_ip(struct ppp *ppp, const u_char far *data, int len)
{
    struct ppp_hdr *hdr;

    static u_char ip_pkt_buf[PPP_MRU+PPP_HDRLEN];
/*
 * Ensure that the caller does not wish to send too much.
 */
    if ( len > ppp->mtu ) {
        PPPDEBUG((LOG_DEBUG, "ppp_dev_xmit_ip: truncating user packet from %u to mtu %d\n", len, ppp->mtu));

        len = ppp->mtu;
    }

    hdr = (struct ppp_hdr *)ip_pkt_buf;
    _fmemcpy(&hdr[1], data, len);
    return ppp_dev_xmit_ip1(ppp, (u_char *)&hdr[1], len);
}

/*
 * Send a frame to the remote.
 */

int ppp_dev_xmit(int unit, const u_char far *data, int len)
{
    int answer;
    struct ppp *ppp;
/*
 * Validate the pointer to the PPP structure
 */
    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "ppp_dev_xmit: invalid unit.\n"));

        return -1;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "ppp_dev_xmit: unit not opened.\n"));

        return -1;
    }
/*
 * just a little sanity check.
 */
    if ( data == NULL || len == 0 ) {
        PPPDEBUG((LOG_ERR, "ppp_dev_xmit: null packet.\n"));

        return 0;
    }

    PPPDEBUG((LOG_DEBUG, "ppp_dev_xmit [ppp%d]: data %p.\n", unit, data));
/*
 * Send an IP frame.
 */
    answer = ppp_dev_xmit_ip(ppp, data, len);
/*
 * This is the end of the transmission.
 */
    if ( answer == 0 ) {
        ppp->ddinfo.xmit_idle = getjiffies();
    }

    return answer;
}

/*
 * sifup - Config the interface up and enable IP packets to pass.
 */

int sifup(int unit)
{
    struct ppp *ppp;
/*
 * Validate the pointer to the PPP structure
 */
    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "sifup: invalid unit.\n"));

        return 0;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "sifup: unit not opened.\n"));

        return 0;
    }

    ppp->flags |= SC_ENABLE_IP;
    MAINDEBUG((LOG_NOTICE, "sifup: IP interface active.\n"));

    return 1;
}

/*
 * isifup - Returns true if the interface is up and IP is enabled.
 */

int isifup(int unit)
{
    struct ppp *ppp;
/*
 * Validate the pointer to the PPP structure
 */
    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "isifup: invalid unit.\n"));

        return 0;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "isifup: unit not opened.\n"));

        return 0;
    }

    if ( ppp->flags & SC_ENABLE_IP )
        return 1;

    return 0;
}

/*
 * sifdown - Config the interface down and disable IP packets to pass.
 */

int sifdown(int unit)
{
    struct ppp *ppp;
/*
 * Validate the pointer to the PPP structure
 */
    if ( unit >= PPP_MAX_DEV ) {
        MAINDEBUG((LOG_ERR, "sifdown: invalid unit.\n"));

        return 0;
    }

    if ( (ppp = PPPDEV(unit)) == NULL || ppp->magic != PPP_MAGIC ) {
        MAINDEBUG((LOG_ERR, "sifdown: unit not opened.\n"));

        return 0;
    }

    ppp->flags &= ~SC_ENABLE_IP;
    MAINDEBUG((LOG_NOTICE, "sifdown: IP interface inactive.\n"));

    return 1;
}

int cifaddr()
{
    return 1;
}

int cifdefaultroute()
{
    return 1;
}

int cifproxyarp()
{
    return 1;
}

int sifdefaultroute()
{
    return 1;
}

int sifproxyarp()
{
    return 1;
}

/*
 * sifaddr - Config the interface IP addresses and netmask.
 */

u_int32_t locipval[NUM_PPP];
u_int32_t remipval[NUM_PPP];
u_int32_t netmaskval[NUM_PPP];

int sifaddr(int unit, u_int32_t our_adr, u_int32_t his_adr, u_int32_t net_mask)
{
    extern char locipstr[NUM_PPP][16];
    extern char remipstr[NUM_PPP][16];
    extern char netmaskstr[NUM_PPP][16];

    locipval[unit]   = our_adr;
    remipval[unit]   = his_adr;
    netmaskval[unit] = net_mask;

#ifndef DEBUGPPP
    strcpy(locipstr[unit], ip_ntoa(our_adr));
    strcpy(remipstr[unit], ip_ntoa(his_adr));
    strcpy(netmaskstr[unit], ip_ntoa(net_mask));
#else   /* DEBUGPPP */
    syslog(LOG_DEBUG, "sifaddr ppp%d: local %s,", unit,
           strcpy(locipstr[unit], ip_ntoa(our_adr)));
    syslog(LOG_DEBUG, " remote %s,",
           strcpy(remipstr[unit], ip_ntoa(his_adr)));
    syslog(LOG_DEBUG, " netmask %s.\n",
           strcpy(netmaskstr[unit], ip_ntoa(net_mask)));
#endif  /* DEBUGPPP */

    return 1;
}

int sifvjcomp()
{
    return 1;
}

#ifdef DEBUGPPP
/*************************************************************
 * UTILITIES
 *    Miscellany called by various functions above.
 *************************************************************/

/*
 * Utility procedures to print a buffer in hex/ascii
 */

static void ppp_print_hex(u_char *out, const u_char *in, int count)
{
    u_char next_ch;
    static char hex[] = "0123456789ABCDEF";

    while ( count-- > 0 ) {
        next_ch = *in++;
        *out++ = hex[(next_ch >> 4) & 0x0F];
        *out++ = hex[next_ch & 0x0F];
        ++out;
    }
}

static void ppp_print_char(u_char *out, const u_char *in, int count)
{
    u_char next_ch;

    while ( count-- > 0 ) {
        next_ch = *in++;

        if ( next_ch < 0x20 || next_ch > 0x7e )
            *out++ = '.';
        else {
            *out++ = next_ch;
            if ( next_ch == '%' )   /* printk/syslogd has a bug !! */
                *out++ = '%';
        }
    }

    *out = '\0';
}

static void ppp_print_buffer(const char *name, const u_char *buf, int count)
{
    u_char line[44];

    if ( name != NULL )
        syslog(LOG_DEBUG, "ppp: %s count = %d\n", name, count);

    while ( count > 8 ) {
        memset(line, 32, 44);
        ppp_print_hex(line, buf, 8);
        ppp_print_char(&line[8 * 3 + 1], buf, 8);
        syslog(LOG_DEBUG, "%s\n", line);
        count -= 8;
        buf += 8;
    }

    if ( count > 0 ) {
        memset(line, 32, 44);
        ppp_print_hex(line, buf, count);
        ppp_print_char(&line[8 * 3 + 1], buf, count);
        syslog(LOG_DEBUG, "%s\n", line);
    }
}
#endif  /* DEBUGPPP */

