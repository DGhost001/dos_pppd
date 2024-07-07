/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

#include <stdlib.h>
#include <mem.h>
#include <dos.h>

#include "pktdrvr.h"
#include "asyimprt.h"
#include "if_ppp.h"
#include "pppd.h"
#include "fmem.h" /* far memory functions */

#define MAX_HANDLE      10      /* maximum number of handles */
#define MAX_P_LEN       8       /* maximum type length */
#define EADDR_LEN       6       /* Ethernet address length */
#define MAX_ADDR_LEN    16      /* maximum number of bytes in our address. */
#define GIANT           1514    /* largest legal size packet, no fcs */
#define MAX_MULTICAST   8       /* maximum number of multicast addresses. */

/* this structure agrees with the pushing order in LOWLEVEL.ASM. If you
 * ever change it remember to change this too.
 */

typedef struct {
    u_short dx, cx, bx, ax;
} WORDR;

typedef struct {
    u_char dl, dh, cl, ch, bl, bh, al, ah;
} BYTER;

typedef union {
    WORDR w;
    BYTER b;
} DUALR;

typedef struct {
    u_short es, ds, di, si, bp;
/*  u_short dx, cx, bx, ax; */
    DUALR   dr;
    u_short ip, cs, flags;
} INTERRUPT_REGS;

typedef struct {
    u_char    in_use;
    u_char    packet_type[MAX_P_LEN];
    u_short   packet_type_len;
    int (far *receiver)();
    u_char    receiver_sig[8];
    u_char    class;
} PER_HANDLE;

typedef struct {
    u_char  major_rev;  /* Revision of Packet Driver spec */
    u_char  minor_rev;  /*  this driver conforms to. */
    u_char  length;     /* Length of structure in bytes */
    u_char  addr_len;   /* Length of a MAC-layer address */
    u_short mtu;        /* MTU, including MAC headers */
    u_short multicast_aval; /* Buffer size for multicast addr */
    u_short rcv_bufs;   /* (# of back-to-back MTU rcvs) - 1 */
    u_short xmt_bufs;   /* (# of successive xmits) - 1 */
    u_short int_num;    /* Interrupt # to hook for post-EOI
                           processing, 0 == none */
} PARAM;

/***************************/

static u_char driver_class[]  = { CL_SERIAL_LINE,0,0,0 };
static u_char driver_type[]   = { 0,0,0,0 };
static char driver_name[]     = "PPPD220F";
static u_char driver_function = 1; /* only basic funtionality present */
static u_short majver         = 11;

static u_short address_len = EADDR_LEN;
static u_char rom_address[MAX_ADDR_LEN];
static u_char my_address[MAX_ADDR_LEN];

static u_char ip_type[] = { 0x08, 0x00 };
static u_char arp_type[] = { 0x08, 0x06 };

static PER_HANDLE handles[MAX_HANDLE];

static u_char valid_pi_vects[] = {
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x68, 0x69,
    0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x78, 0x79, 0x79,
    0x7A, 0x7B, 0x7C, 0x7D, 0x7E
};

static u_char pktsig[] = "PKT DRVR";

static PARAM parameter_list = {
    1,                          /* major rev of packet driver */
    9,                          /* minor rev of packet driver */
    14,                         /* length of parameter list */
    EADDR_LEN,                  /* length of MAC-layer address */
    GIANT,                      /* MTU, including MAC headers */
    MAX_MULTICAST * EADDR_LEN,  /* buffer size of multicast addrs */
    0,                          /* (# of back-to-back MTU rcvs) - 1 */
    0,                          /* (# of successive xmits) - 1 */
    0                           /* Interrupt # to hook for post-EOI */
                                /* processing, 0 == none */
};

static PER_HANDLE *termin_handle = NULL;

/* external functions defined in LOWLEVEL.ASM for making the upcall */
u_char far *get_buffer(PER_HANDLE *, u_short, int (far *)());
void copy_done(PER_HANDLE *, u_short, u_char far *, int (far *)());

/***************************/

/*
 * Helper function for verifying that a handle is OK.
 */
static u_char verify_handle(PER_HANDLE *phandle)
{
    if ( (u_char *)phandle < (u_char *)handles ||
         (u_char *)phandle >= ((u_char *)handles + sizeof(handles)) ||
         phandle->in_use != 1 )
        return BAD_HANDLE;

    return NO_ERROR;
}

/***************************/

/*
 * Helper function for counting number of active handles.
 */
static u_short count_handles(void)
{
    u_short cnt;
    PER_HANDLE *phandle;

    for ( cnt = 0, phandle = handles ;
          (u_char *)phandle < ((u_char *)handles + sizeof(handles)) ;
          phandle++ )
        cnt += phandle->in_use;

    return cnt;
}

/***************************/

/*
 * Helper function for locating the handler for a given packet type.
 */
static PER_HANDLE *recv_locate(u_char class, u_char *type)
{
    PER_HANDLE *phandle;

    for ( phandle = handles ;
          (u_char *)phandle < ((u_char *)handles + sizeof(handles)) ;
          phandle++ ) {
        /* if handle not in use, loop again */
        if ( phandle->in_use == 0 )
            continue;

        /* if they don't have a receiver, loop again */
        if ( phandle->receiver == NULL )
            continue;

        /* per request by the UK people, we match on IEEE 802.3 classes,
           then types. For all others, we match on type, then class.  This
           lets their software work without breaking BLUEBOOK type length=0
           clients. */
        if ( phandle->class == CL_IEEE8023 && class != CL_IEEE8023 )
            continue;

        /* if this handle packet type len is 0, they want them all. The
           statament after this 'if' will return this handle as a match */
        if ( phandle->packet_type_len != 0 ) {
            /* if the packet doesn't have this handle class, don't bother */
            if ( phandle->class != class )
                continue;

            /* compare types, in case of match the statament after this 'if'
               will return the actual handle as a match */
            if ( memcmp(phandle->packet_type,
                        type,
                        phandle->packet_type_len) != 0 )
                continue;
        }

        return phandle;
    }

    /* none found, return NULL handle */
    return NULL;
}

/***************************/

/*
 * Function called by ppp_rcv_rx() in PPP.C when IP packet arrives.
 */
int handle_ip_packet(int unit, const u_char *data, int count)
{
    PER_HANDLE *phandle;
    u_char far *dptr;

    /* if the packet doesn't come from the currently active ppp interface,
       drop it gracefully (?) */
    if ( unit != ifunit )
        /* returning 0 means packet wasn't processed */
        return 0;

    /* locate a receiver for IP packets (or any packet) */
    if ( (phandle = recv_locate(driver_class[0], ip_type)) == NULL )
        return 0;

    /* receiver found, request for a buffer */
    if ( (dptr = get_buffer(phandle, count, phandle->receiver)) == NULL )
        /* no buffer available */
        return 0;

    /* transfer the data */
    _fmemcpy(dptr, data, count);

    /* signal copy done */
    copy_done(phandle, count, dptr, phandle->receiver);

    return 1;
}

/***************************/

/*
 * This function handles the ACCESS TYPE command for the packet driver.
 */
static u_char f_access_type(INTERRUPT_REGS far *r)
{
    u_char *pclass;
    PER_HANDLE *phandle, *free_handle;
    u_short count;

    /* search our class list for a match until end of list */
    for ( pclass = driver_class ; *pclass != 0 ; pclass++ ) {
        /* try again if classes doesn't match */
        if ( r->dr.b.al != *pclass )
            continue;

        /* if they don't want a generic type and the one they want is
           not our type, fail */
        if ( r->dr.w.bx != 0xFFFF &&
             (int)driver_type[0] != r->dr.w.bx )
            return NO_TYPE;

        /* if they don't want a generic number and the one they want is
           not our number, fail */
        if ( r->dr.b.dl != 0 && r->dr.b.dl != 1 )
            return NO_NUMBER;

        /* if the requested type length is too long, can't be ours */
        if ( r->dr.w.cx > MAX_P_LEN )
            return BAD_TYPE;

        /* now we do two things: look for an open handle, and check the
           existing handles to see if they're replicating a packet type. */
        for ( free_handle = NULL, phandle = handles ;
              (u_char *)phandle < ((u_char *)handles + sizeof(handles)) ;
              phandle++ ) {
            /* if this handle is in use and the class match the one
               they want and the type match the one they want, fail */
            if ( phandle->in_use ) {
                /* loop again if this handle class is not the one they want */
                if ( r->dr.b.al != phandle->class )
                    continue;

                /* get the minimum of their length and our length. As
                   currently implemented, only ony receiver gets the
                   packets, so we have to ensure that the shortest
                   prefix is unique */
                count = min(r->dr.w.cx, phandle->packet_type_len);

                /* pass-all TYPE? (zero TYPE length) */
                if ( count == 0 ) {
                    /* put pass-all last */
                    phandle += (MAX_HANDLE - 1);
                }
                else {
                    /* if TYPE fields match, fail */
                    if ( _fmemcmp(phandle->packet_type,
                                  MK_FP(r->ds, r->si),
                                  count ) == 0 )
                        return TYPE_INUSE;

                    /* goo look at the next one */
                    continue;
                }
            }

            /* remember a free handle if didn't found yet */
            if ( free_handle == NULL )
                free_handle = phandle;
        }

        /* did we find a free handle ? */
        if ( free_handle == NULL ) {
            /* no - return error */
            return NO_SPACE;
        }

        /* remember that we're using it */
        phandle = free_handle;
        phandle->in_use = 1;
        /* remember the receiver type */
        phandle->receiver = MK_FP(r->es, r->di);

        /* remember their type */
        if ( r->dr.w.cx != 0 ) {
            _fmemcpy(phandle->packet_type, MK_FP(r->ds, r->si), r->dr.w.cx);

            /* check if this acces type comes from TERMIN.COM */
            if ( r->dr.w.cx == sizeof(u_short) && 
                 (u_short *)(phandle->packet_type)[0] == 0 ) {
                /* yes, remember the handle for freeing the DOS memory block
                   occupied by the program in the call to f_release_access() */
                termin_handle = phandle;
            }
            else {
                termin_handle = NULL;
            }
        }

        /* remember their type length */
        phandle->packet_type_len = r->dr.w.cx;
        /* copy the first 8 bytes to the receiver signature (Windows hack) */
        _fmemcpy(phandle->receiver_sig, phandle->receiver, 8);
        /* remember the class */
        phandle->class = r->dr.b.al;
        /* return the handle to them */
        r->dr.w.ax = (u_short)phandle;

        return NO_ERROR;
    }

    return NO_CLASS;
}

/***************************/

/*
 * This function handles the RELEASE TYPE command for the packet driver.
 */
static u_char f_release_type(INTERRUPT_REGS far *r)
{
    u_char retcode;
    PER_HANDLE *phandle = (PER_HANDLE *)(r->dr.w.bx);

    /* ensure that their handle is real */
    if ( (retcode = verify_handle(phandle)) != NO_ERROR ) {
        /* no, fail. But first release the DOS memory block if this
           release attempt comes from TERMIN.COM */
        if ( termin_handle && termin_handle == phandle )
            freemem(_psp);

        return retcode;
    }

    /* mark this handle as being unused */
    phandle->in_use = 0;

    return NO_ERROR;
}

/***************************/

/*
 * This function handles the SEND PACKET command for the packet driver.
 */
static u_char f_send_pkt(INTERRUPT_REGS far *r)
{
    if ( ppp_dev_xmit(ifunit, MK_FP(r->ds, r->si), r->dr.w.cx) != 0 )
        return CANT_SEND;

    return NO_ERROR;
}

/***************************/

/*
 * This function handles the GET ADDRESS command for the packet driver.
 */
static u_char f_get_address(INTERRUPT_REGS far *r)
{
    /* fail if there is no enough room for our address */
    if ( r->dr.w.cx < address_len )
        return NO_SPACE;

    /* tell them how long our address is */
    r->dr.w.cx = address_len;
    /* copy our address into their area */
    _fmemcpy(MK_FP(r->es, r->di), my_address, address_len);

    return NO_ERROR;
}

/***************************/

/*
 * This function handles the TERMINATE command for the packet driver.
 */
static u_char f_terminate(INTERRUPT_REGS far *r)
{
    u_char retcode;
    PER_HANDLE *phandle = (PER_HANDLE *)(r->dr.w.bx);

    /* external funcs and vars used for the termination process */
    int asy_chkirqvecs(void);
    int main_chkirqvecs(void);
    void exit_fn1(void);
    void lcp_close(int);

    /* ensure that their handle is real */
    if ( (retcode = verify_handle(phandle)) != NO_ERROR )
        /* no, fail */
        return retcode;

    /* mark handle as free */
    phandle->in_use = 0;

    /* all handles gone ? */
    if ( count_handles() )
        /* no, can't exit completely */
        return CANT_TERMINATE;

    /* irq vectors still ours ? */
    if ( ! asy_chkirqvecs() || ! main_chkirqvecs() )
        /* no, can't exit completely */
        return CANT_TERMINATE;

    /* if the link is currently open, close it and wait for termination */
    if ( phase != PHASE_DEAD ) {
        lcp_close(ifunit);
        while ( phase != PHASE_DEAD );
    }

    /* reset modem and deinstall irq vectors */
    going_resident = 0;
    exit_fn1();

    return NO_ERROR;
}

/***************************/

/*
 * This function handles the RESET INTERFACE command for the packet driver.
 * A dumb NO OP at the moment, only makes handle verification and then
 * succeeds.
 */
static u_char f_reset_interface(INTERRUPT_REGS far *r)
{
    u_char retcode;
    PER_HANDLE *phandle = (PER_HANDLE *)(r->dr.w.bx);

    /* ensure that their handle is real */
    if ( (retcode = verify_handle(phandle)) != NO_ERROR )
        /* no, fail */
        return retcode;

    return NO_ERROR;
}

/***************************/

/*
 * This function is called by the packet driver interrupt service handler
 * located in LOWLEVEL.ASM. It receives a far * pointing to the caller
 * registers in the callers stack. Interrupts are enabled on entry and the
 * stack is set up to our own stack for extra safety.
 */
void pktdriver(INTERRUPT_REGS far *r)
{
    u_char retcode;

    /* start clearing carry flag */
    r->flags &= ~CARRY_FLAG;

    switch ( r->dr.b.ah ) {
        case DRIVER_INFO:
            /* verify correct calling convention */
            if ( r->dr.b.al == 0xFF ) {
                r->dr.b.ch = driver_class[0];
                r->dr.w.bx = majver;
                r->dr.w.dx = (int)driver_type[0];
                r->dr.b.cl = 0; /* number 0 */
                r->ds      = _DS;
                r->si      = (u_short)&driver_name;
                r->dr.b.al = driver_function;
            }
            else {
                /* wrong calling convention, return error */
                r->flags |= CARRY_FLAG;
            }
        break;

        case ACCESS_TYPE:
            if ( (retcode = f_access_type(r)) != NO_ERROR ) {
                r->dr.b.dh = retcode;
                r->flags   |= CARRY_FLAG;
            }
        break;

        case RELEASE_TYPE:
            if ( (retcode = f_release_type(r)) != NO_ERROR ) {
                r->dr.b.dh = retcode;
                r->flags   |= CARRY_FLAG;
            }
        break;

        case SEND_PKT:
            if ( (retcode = f_send_pkt(r)) != NO_ERROR ) {
                r->dr.b.dh = retcode;
                r->flags   |= CARRY_FLAG;
            }
        break;

        case GET_ADDRESS:
            if ( (retcode = f_get_address(r)) != NO_ERROR ) {
                r->dr.b.dh = retcode;
                r->flags   |= CARRY_FLAG;
            }
        break;

        case TERMINATE:
            if ( (retcode = f_terminate(r)) != NO_ERROR ) {
                r->dr.b.dh = retcode;
                r->flags   |= CARRY_FLAG;
            }
        break;

        case RESET_INTERFACE:
            if ( (retcode = f_reset_interface(r)) != NO_ERROR ) {
                r->dr.b.dh = retcode;
                r->flags   |= CARRY_FLAG;
            }
        break;

        /* User extension, provide the caller with the addresses of
           serial functions that can be used to access the currently
           open COM port. At the moment only CHAT knows about it.
           The caller pass the struct size in BX, and receives a far
           pointer to the struct in DS:SI (if struct size is OK). */
        case GET_ASYFUNCS:
            /* Make sure the caller uses the correct calling convention */
            if ( r->dr.w.bx != sizeof(ASY_HOOKS) ) {
                r->ds = r->si = 0;
                r->dr.b.dh = BAD_COMMAND;
                r->flags   |= CARRY_FLAG;
            }

            r->ds = FP_SEG((void far *)&asy_exportinfo);
            r->si = FP_OFF((void far *)&asy_exportinfo);
        break;

        default:
            r->dr.b.dh = BAD_COMMAND;
            r->flags   |= CARRY_FLAG;
        break;
    }
}

/***************************/

/*
 * This routine checks for a free packet driver interrupt vector. If you
 * pass in 0, it does an automatic search and returns the first free one
 * in the allowed range. If you pass a specific number, it will check if
 * it is a free one.
 */
int verify_packet_int(int pinum)
{
    u_char *pvalid, *pvalidend;
    u_char far *psig;

    /* they look for a specific vector, checkit */
    if ( pinum ) {
        /* see if it is one of the valid vector numbers for packet drivers */
        if ( (pvalid = memchr(valid_pi_vects,
                              pinum,
                              sizeof(valid_pi_vects))) == NULL )
            /* no, return error indication */
            return 0;

        /* limit the following scan to one vector */
        pvalidend = pvalid + 1;
    }
    /* they are looking for the first free one,
       init the following scan acordling */
    else {
        pvalid = valid_pi_vects;
        pvalidend = ((u_char *)valid_pi_vects + sizeof(valid_pi_vects));
    }

    /* do a free vector search */
    for ( ; pvalid < pvalidend ; pvalid++ ) {
        /* check if there is an installed packet driver there */
        if ( (psig = (u_char far *)getvect(*pvalid)) == NULL ||
             _fmemcmp(psig + 3, pktsig, sizeof(pktsig) - 1) != 0 )
            /* no, it is available */
            return *pvalid;
    }

    /* no free one was found, return error indication */
    return 0;
}

