/* OS- and machine-dependent stuff for the 8250 asynch chip on a IBM-PC
 * Copyright 1991 Phil Karn, KA9Q
 *
 * 16550A support plus some statistics added mah@hpuviea.at 15/7/89
 *
 * CTS hardware flow control from dkstevens@ucdavis,
 * additional stats from delaroca@oac.ucla.edu added by karn 4/17/90
 * Feb '91      RLSD line control reorganized by Bill_Simpson@um.cc.umich.edu
 * Sep '91      All control signals reorganized by Bill Simpson
 * Apr '92      Control signals redone again by Phil Karn
 */

/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

#include <dos.h>
#include "syslog.h"
#include "global.h"
#include "n8250.h"
#include "asy.h"
#include "asyimprt.h"
#include "pppd.h"

/* In lowlevel.asm */
INTERRUPT asy0vec(void);
INTERRUPT asy1vec(void);
INTERRUPT asy2vec(void);
INTERRUPT asy3vec(void);

/* External functions.
*/
void asyrxschedule(int comdev, int unit);

/* Global functions.
 */
INTERRUPT (far *(asyint)(int dev))();

/* Local functions.
 */
#ifndef IOMACROS
static void setbit(unsigned port, char bits);
static void clrbit(unsigned port, char bits);
static void writebit(unsigned port, char mask, int val);
#endif  /* IOMACROS */
static int setirq(unsigned irq, INTERRUPT (*handler)());
static INTERRUPT (*getirq(unsigned int irq))();
static int maskoff(unsigned irq);
static int maskon(unsigned irq);
static int getmask(unsigned irq);
static int asyrxint(struct asy *asyp);
static void asytxint(struct asy *asyp);
static void asymsint(struct asy *asyp);
static int rlsdstat(struct asy *ap);

#ifndef LOWLEVELASY
static struct asy Asy[ASY_MAX];
#else   /* LOWLEVELASY */
struct asy Asy[ASY_MAX];
#endif  /* LOWLEVELASY */

/* ASY interrupt handlers
 */
static INTERRUPT (*Handle[ASY_MAX])() = {
    asy0vec,asy1vec,asy2vec,asy3vec
};

/* Flag that indicates IRQs higher than 8 are in use.
 */
int Isat = 0;

/* Set bit(s) in I/O port.
 */
#ifdef IOMACROS
#define setbit(p,b) outportb((p), inportb((p)) | (b))
#else
static void setbit(unsigned port, char bits)
{
    outportb(port, inportb(port) | bits);
}
#endif


/* Clear bit(s) in I/O port.
 */
#ifdef IOMACROS
#define clrbit(p,b) outportb((p), inportb((p)) & ~(b))
#else
static void clrbit(unsigned port, char bits)
{
    outportb(port, inportb(port) & ~bits);
}
#endif


/* Set or clear selected bits(s) in I/O port.
 */
#ifdef IOMACROS
#define writebit(p,m,v) (void)((v) ? outportb((p), inportb((p)) | (m)) : outportb((p), inportb((p)) & ~(m)))
#else
static void writebit(unsigned port, char mask, int val)
{
    char x;

    x = inportb(port);

    if ( val )
        x |= mask;
    else
        x &= ~mask;

    outportb(port,x);
}
#endif


/* Install hardware interrupt handler.
 * Takes IRQ numbers from 0-7 (0-15 on AT) and maps to actual 8086/286
 * vectors. Note that bus line IRQ2 maps to IRQ9 on the AT.
 */
static int setirq(unsigned irq, INTERRUPT (*handler)())
{
    /* Set interrupt vector */
    if ( irq < 8 ) {
        setvect(8 + irq, handler);
    }
    else if ( irq < 16 ) {
        Isat = 1;
        setvect(0x70 + irq - 8, handler);
    }
    else {
        return -1;
    }

    return 0;
}


/* Return pointer to hardware interrupt handler.
 * Takes IRQ numbers from 0-7 (0-15 on AT) and maps to actual 8086/286
 * vectors.
 */
static INTERRUPT (*getirq(unsigned int irq))()
{
    /* Set interrupt vector */
    if ( irq < 8 ) {
        return getvect(8 + irq);
    }
    else if ( irq < 16 ) {
        return getvect(0x70 + irq - 8);
    }
    else {
        return NULL;
    }
}


/* Disable hardware interrupt.
 */
static int maskoff(unsigned irq)
{
    if ( irq < 8 ) {
        setbit(0x21, (char)(1 << irq));
    }
    else if ( irq < 16 ) {
        irq -= 8;
        setbit(0xa1, (char)(1 << irq));
    }
    else {
        return -1;
    }

    return 0;
}


/* Enable hardware interrupt */
static int maskon(unsigned irq)
{
    if ( irq < 8 ) {
        clrbit(0x21, (char)(1 << irq));
    }
    else if ( irq < 16 ) {
        irq -= 8;
        clrbit(0xa1, (char)(1 << irq));
    }
    else {
        return -1;
    }

    return 0;
}


/* Return 1 if specified interrupt is enabled, 0 if not, -1 if invalid.
 */
static int getmask(unsigned irq)
{
    if ( irq < 8 ) {
        return (inportb(0x21) & (1 << irq)) ? 0 : 1;
    }
    else if ( irq < 16 ) {
        irq -= 8;
        return (inportb(0xa1) & (1 << irq)) ? 0 : 1;
    }
    else {
        return -1;
    }
}


/* Initialize asynch port "dev".
 */
int asy_init(int dev, int base, int irq, uint16 bufsize,
             long speed, int cts, int rlsd, int chain)
{
    struct fifo *fp;
    struct asy *ap;
    int i_state;

    if ( dev < 0 || dev >= ASY_MAX )
        return -1;

    ap = &Asy[dev];
    /* Set up receiver FIFO */
    fp = &ap->fifo;

    if ( (fp->buf = malloc(bufsize)) == NULL ) {
        return -1;
    }

    fp->bufsize = bufsize;
    fp->ep = &fp->buf[fp->bufsize];
    fp->wp = fp->rp = fp->buf;
    fp->cnt = 0;
#ifdef DEBUGTTY
    fp->hiwat = 0;
    fp->overrun = 0;
#endif  /* DEBUGTTY */

    ap->pppunit = -1;
    ap->pppsem = 0;
    ap->addr = base;
    ap->vec = irq;
    ap->chain = chain;

    /* Purge the receive data buffer */
    (void)inportb(base + RBR);

    i_state = dirps();

    /* Save original interrupt vector, mask state, control bits */
    if ( ap->vec != -1 ) {
        ap->save.vec = getirq(ap->vec);
        ap->save.mask = getmask(ap->vec);
    }

    ap->save.lcr = inportb(base + LCR);
    ap->save.ier = inportb(base + IER);
    ap->save.mcr = inportb(base + MCR);
    ap->msr = ap->save.msr = inportb(base + MSR);
    ap->carrier = ((ap->msr & MSR_RLSD) != 0);
    ap->save.iir = inportb(base + IIR);

    /* save speed bytes */
    setbit(base + LCR,LCR_DLAB);
    ap->save.divl = inportb(base + DLL);
    ap->save.divh = inportb(base + DLM);
    clrbit(base + LCR, LCR_DLAB);

    /* save modem control flags */
    ap->cts = (cts == 1);
    ap->rlsd = rlsd;

    /* Set interrupt vector to SIO handler */
    if ( ap->vec != -1 )
        setirq(ap->vec, Handle[dev]);

    /* Set line control register: 8 bits, no parity */
    outportb(base + LCR, LCR_8BITS);

    /* determine if 16550A, turn on FIFO mode and clear RX and TX FIFOs */
    outportb(base + FCR, FIFO_ENABLE);

    /* According to National ap note AN-493, the FIFO in the 16550 chip
     * is broken and must not be used. To determine if this is a 16550A
     * (which has a good FIFO implementation) check that both bits 7
     * and 6 of the IIR are 1 after setting the fifo enable bit. If
     * not, don't try to use the FIFO.
     */
    if ( (inportb(base + IIR) & IIR_FIFO_ENABLED) == IIR_FIFO_ENABLED ) {
        ap->is_16550a = TRUE;
        outportb(base + FCR, FIFO_SETUP);
    }
    else {
        /* Chip is not a 16550A. In case it's a 16550 (which has a
         * broken FIFO), turn off the FIFO bit.
         */
        outportb(base + FCR, 0);
        ap->is_16550a = FALSE;
    }

    /* Turn on receive interrupts and optionally modem interrupts;
     * leave transmit interrupts off until we actually send data.
     */
    if ( ap->rlsd || ap->cts )
        outportb(base + IER, IER_MS|IER_DAV);
    else
        outportb(base + IER, IER_DAV);

    /* Turn on 8250 master interrupt enable (connected to OUT2) */
    setbit(base + MCR, MCR_OUT2);

    /* Enable interrupt */
    if ( ap->vec != -1 )
        maskon(ap->vec);

#ifdef DEBUGTTY
    ap->fifotimeouts =
    ap->rxints       =
    ap->rxchar       =
    ap->txints       =
    ap->overrun      =
    ap->txchar       =
    ap->rxhiwat      =
    ap->msint_count  =
    ap->txto         =
    ap->cdchanges    = 0;
#endif  /* DEBUGTTY */

    asy_speed(dev, speed);

    restore(i_state);

    return dev;
}

/* Deinitialize asynch port "dev".
 * Restore COM state and frees used buffers.
 */
int asy_stop(int dev)
{
    struct asy *ap;
    unsigned base;
    int i_state;

    if ( dev < 0 || dev >= ASY_MAX )
        return -1;

    ap = &Asy[dev];
    base = ap->addr;

    i_state = dirps();

    (void)inportb(base + RBR); /* Purge the receive data buffer */

    if ( ap->is_16550a ) {
        /* Purge hardware FIFOs and disable if we weren't already
         * in FIFO mode when we entered. Apparently some
         * other comm programs can't handle 16550s in
         * FIFO mode; they expect 16450 compatibility mode.
         */
        outportb(base + FCR, FIFO_SETUP);

        if ( (ap->save.iir & IIR_FIFO_ENABLED) != IIR_FIFO_ENABLED )
            outportb(base + FCR, 0);
    }

    /* Restore original interrupt vector and 8259 mask state */
    if ( ap->vec != -1 ) {
        setirq(ap->vec, ap->save.vec);

        if( ap->save.mask )
            maskon(ap->vec);
        else
            maskoff(ap->vec);
    }

    /* release receiver buffer */
    free(ap->fifo.buf);
    ap->fifo.buf = NULL;

    /* Restore speed regs */
    setbit(base + LCR, LCR_DLAB);
    outportb(base + DLL, ap->save.divl);   /* Low byte */
    outportb(base + DLM, ap->save.divh);   /* Hi byte */
    clrbit(base + LCR, LCR_DLAB);

    /* Restore control regs */
    outportb(base + LCR, ap->save.lcr);
    outportb(base + IER, ap->save.ier);
    outportb(base + MCR, ap->save.mcr);

    restore(i_state);

    return 0;
}


/* Set asynch line speed.
 */
int asy_speed(int dev, long bps)
{
    unsigned base;
    long divisor;
    struct asy *asyp;
    int i_state;

    if ( bps <= 0 || dev < 0 || dev >= ASY_MAX )
        return -1;

    asyp = &Asy[dev];

    if ( bps == 0 )
        return -1;

    asyp->speed = bps;

    base = asyp->addr;
    divisor = BAUDCLK / bps;

    i_state = dirps();

    /* Purge the receive data buffer */
    (void)inportb(base + RBR);

    if ( asyp->is_16550a )        /* clear tx+rx fifos */
        outportb(base + FCR, FIFO_SETUP);

    /* Turn on divisor latch access bit */
    setbit(base + LCR, LCR_DLAB);

    /* Load the two bytes of the register */
    outportb(base + DLL, divisor);     /* Low byte */
    outportb(base + DLM, divisor >> 8);    /* Hi byte */

    /* Turn off divisor latch access bit */
    clrbit(base + LCR, LCR_DLAB);

    restore(i_state);

    return 0;
}


/* Set asynch line PPP device number, returns -1 if error.
 */
int asy_set_ppp(int dev, int unit)
{
    if ( dev < 0 || dev >= ASY_MAX ) {
        return -1;
    }

    return Asy[dev].pppunit = unit;
}


/* Asynchronous line I/O control.
 */
int32 asy_ioctl(int dev, int cmd, int set, int32 val)
{
    struct asy *ap = &Asy[dev];
    uint16 base = ap->addr;

    if ( dev < 0 || dev >= ASY_MAX )
        return (int32)(-1);

    switch ( cmd ) {
        case PARAM_SPEED:
            if ( set ) {
                asy_speed(dev, val);
            }

            return (int32)(ap->speed);

        case PARAM_DTR:
            if ( set ) {
                writebit(base + MCR, MCR_DTR, (int)val);
            }

            return (inportb(base + MCR) & MCR_DTR) ? (int32)(TRUE) : (int32)(FALSE);

        case PARAM_RTS:
            if ( set ) {
                writebit(base + MCR, MCR_RTS, (int)val);
            }

            return (inportb(base + MCR) & MCR_RTS) ? (int32)(TRUE) : (int32)(FALSE);

        case PARAM_DOWN:
            clrbit(base + MCR, MCR_RTS);
            clrbit(base + MCR, MCR_DTR);

            return (int32)(FALSE);

        case PARAM_UP:
            setbit(base + MCR, MCR_RTS);
            setbit(base + MCR, MCR_DTR);

            return (int32)(TRUE);
    }

    return (int32)(-1);
}


/* Send a buffer on the serial transmitter without waiting for completion.
 * Returns -1 if invalid device, 0 if buffer busy, else returns cnt.
 * BEWARE !!!, this functions transmits the buffer by interrupts and does
 * not do any local copy. It means that the buffer passed in can't be a
 * local varibale, as it can go out of scope before the contents get
 * actually transmitted.
 */
int asy_write(int dev, uint8 *buf, uint16 cnt)
{
    unsigned base;
    struct dma *dp;
    struct asy *asyp;

    if ( dev < 0 || dev >= ASY_MAX )
        return -1;

    asyp = &Asy[dev];
    base = asyp->addr;
    dp = &asyp->dma;

    if ( dp->busy )
        return 0;  /* Already busy */

    if ( cnt ) {
        dp->data = buf;
        dp->cnt = cnt;
        dp->busy = 1;

        /* If CTS flow control is disabled or CTS is true,
         * enable transmit interrupts here so we'll take an immediate
         * interrupt to get things going. Otherwise let the
         * modem control interrupt enable transmit interrupts
         * when CTS comes up. If we do turn on TxE,
         * "kick start" the transmitter interrupt routine, in case just
         * setting the interrupt enable bit doesn't cause an interrupt
         */

        if ( ! asyp->cts || (asyp->msr & MSR_CTS) ) {
            setbit(base + IER,IER_TxE);
            asytxint(asyp);
        }
    }

    return cnt;
}


/* Returns number of chars in tx output buffer for dev or -1 if error.
 */
int asy_txcheck(int dev)
{
    if ( dev < 0 || dev >= ASY_MAX ) {
        return -1;
    }

    return ((struct dma *)&(Asy[dev].dma))->cnt;
}


/* Read data from asynch line without blocking.
 * Returns number of bytes read, up to 'cnt' max.
 * It will return 0 if no data available, -1 if invalid device.
 */
int asy_read(int dev, uint8 *buf, uint16 cnt)
{
    struct fifo *fp;
    int i_state, i;

    if ( cnt == 0 )
        return 0;

    if ( dev < 0 || dev >= ASY_MAX ) {
        return -1;
    }

    fp = &Asy[dev].fifo;

    /* Atomic read of and subtract from fp->cnt */
    i_state = dirps();

    if ( fp->cnt != 0 ) {
        if ( cnt > fp->cnt )
            cnt = fp->cnt;  /* Limit to data on hand */

        fp->cnt -= cnt;
    }
    else
        cnt = 0;

    restore(i_state);

    i = cnt;

    while ( i-- != 0 ) {
        /* This can be optimized later if necessary */
        *buf++ = *fp->rp++;

        if ( fp->rp >= fp->ep )
            fp->rp = fp->buf;
    }

    return cnt;
}


/* Returns number of chars in rx input buffer for dev
 * or -1 if invalid device.
 */
int asy_rxcheck(int dev)
{
    if ( dev < 0 || dev >= ASY_MAX ) {
        return -1;
    }

    return ((struct fifo *)&(Asy[dev].fifo))->cnt;
}


/* Blocking read one character from asynch line.
 * Returns character or -1 if aborting.
 */
int get_asy(int dev)
{
    uint8 c;
    int tmp;

    for (;;) {
        if ( (tmp = asy_read(dev, &c, 1)) == 1 )
            return c;
        else if ( tmp < 0 )
            return tmp;
    }
}


/* Interrupt handler for 8250 asynch chip (called from lowlevel.asm).
 * Common interrupt handler code for 8250/16550 port.
 */
INTERRUPT (far *(asyint)(int dev))()
{
    struct asy *asyp;
    uint16 base;
    uint8 iir;

    asyp = &Asy[dev];
    base = asyp->addr;

    while ( ((iir = inportb(base + IIR)) & IIR_IP) == 0 ) {
        switch ( iir & IIR_ID_MASK ) {
            case IIR_RDA:   /* Receiver interrupt */
                asyrxint(asyp);
            break;

            case IIR_THRE:  /* Transmit interrupt */
                asytxint(asyp);
            break;

            case IIR_MSTAT: /* Modem status change */
                asymsint(asyp);
#ifdef DEBUGTTY
                asyp->msint_count++;
#endif  /* DEBUGTTY */
            break;
        }

#ifdef DEBUGTTY
        /* should happen at end of a single packet */
        if ( iir & IIR_FIFO_TIMEOUT )
            asyp->fifotimeouts++;
#endif  /* DEBUGTTY */
    }

    if ( asyp->fifo.cnt && asyp->pppunit >= 0 && ! asyp->pppsem ) {
        ++(asyp->pppsem);
        enable();
        asyrxschedule(dev, asyp->pppunit);
        disable();
        --(asyp->pppsem);
    }

    return asyp->chain ? asyp->save.vec : NULL;
}


/* Process 8250 receiver interrupts */
static int asyrxint(struct asy *asyp)
{
    struct fifo *fp;
    unsigned base;
    uint8 c, lsr;
#ifdef DEBUGTTY
    int cnt = 0;

    asyp->rxints++;
#endif  /* DEBUGTTY */
    base = asyp->addr;
    fp = &asyp->fifo;

    for (;;) {
        lsr = inportb(base + LSR);

#ifdef DEBUGTTY
        if ( lsr & LSR_OE )
            asyp->overrun++;
#endif  /* DEBUGTTY */

        if ( lsr & LSR_DR ) {
            c = inportb(base + RBR);
#ifdef DEBUGTTY
            asyp->rxchar++;
#endif  /* DEBUGTTY */

            /* If buffer is full, we have no choice but
             * to drop the character
             */
            if ( fp->cnt != fp->bufsize ) {
                *fp->wp++ = c;

                if ( fp->wp >= fp->ep )
                    /* Wrap around */
                    fp->wp = fp->buf;

                fp->cnt++;

#ifdef DEBUGTTY
                if ( fp->cnt > fp->hiwat )
                    fp->hiwat = fp->cnt;

                cnt++;
#endif  /* DEBUGTTY */
            }
#ifdef DEBUGTTY
            else
                fp->overrun++;
#endif  /* DEBUGTTY */
        }
        else
            break;
    }

#ifdef DEBUGTTY
    if ( cnt > asyp->rxhiwat )
        asyp->rxhiwat = cnt;

    return cnt;
#else   /* DEBUGTTY */
    return 0;
#endif  /* DEBUGTTY */
}


/* Handle 8250 transmitter interrupts */
static void asytxint(struct asy *asyp)
{
    struct dma *dp;
    unsigned base;
    int count;

    base = asyp->addr;
    dp = &asyp->dma;
#ifdef DEBUGTTY
    asyp->txints++;
#endif  /* DEBUGTTY */

    if ( !dp->busy || (asyp->cts && !(asyp->msr & MSR_CTS)) ) {
        /* These events "shouldn't happen". Either the
         * transmitter is idle, in which case the transmit
         * interrupts should have been disabled, or flow control
         * is enabled but CTS is low, and interrupts should also
         * have been disabled.
         */
        clrbit(base + IER, IER_TxE);
        return; /* Nothing to send */
    }

    if ( ! (inportb(base + LSR) & LSR_THRE) )
        return; /* Not really ready */

    /* If it's a 16550A, load up to 16 chars into the tx hw fifo
     * at once. With an 8250, it can be one char at most.
     */
    if ( asyp->is_16550a ) {
        count = min(dp->cnt, OUTPUT_FIFO_SIZE);

        /* 16550A: LSR_THRE will drop after the first char loaded
         * so we can't look at this bit to determine if the hw fifo is
         * full. There seems to be no way to determine if the tx fifo
         * is full (any clues?). So we should never get here while the
         * fifo isn't empty yet.
         */
#ifdef DEBUGTTY
        asyp->txchar += count;
#endif  /* DEBUGTTY */
        dp->cnt -= count;

        while ( count-- != 0 )
            outportb(base + THR, *dp->data++);
    }
    else {    /* 8250 */
        do {
#ifdef DEBUGTTY
            asyp->txchar++;
#endif  /* DEBUGTTY */
            outportb(base + THR, *dp->data++);
        } while ( --dp->cnt != 0 && (inportb(base + LSR) & LSR_THRE) );
    }

    if ( dp->cnt == 0 ) {
        dp->busy = 0;
        /* Disable further transmit interrupts */
        clrbit(base + IER, IER_TxE);
    }
}


/* Handle 8250 modem status change interrupt */
static void asymsint(struct asy *asyp)
{
    unsigned base = asyp->addr;

    asyp->msr = inportb(base + MSR);

    if ( asyp->cts && (asyp->msr & MSR_DCTS) ) {
        /* CTS has changed and we care */
        if ( asyp->msr & MSR_CTS ) {
            /* CTS went up */
            if ( asyp->dma.busy ) {
                /* enable transmit interrupts and kick */
                setbit(base + IER, IER_TxE);
                asytxint(asyp);
            }
        } else {
            /* CTS now dropped, disable Transmit interrupts */
            clrbit(base + IER, IER_TxE);
        }
    }

    if ( asyp->rlsd && (asyp->msr & MSR_DRLSD) ) {
        /* RLSD just changed and we care, signal it */
        asyp->carrier = ((asyp->msr & MSR_RLSD) != 0);
#ifdef DEBUGTTY
        /* Keep count */
        asyp->cdchanges++;
#endif  /* DEBUGTTY */
    }
}


static int rlsdstat(struct asy *ap)
{
    if ( ap->rlsd )
        return ap->carrier;
    else if ( ap->cts )
        return ((ap->msr & MSR_RLSD) != 0);
    else
        return ((inportb(ap->addr + MSR) & MSR_RLSD) != 0);
}


/* Get the RLSD signal status.
 */
int get_rlsd_asy(int dev)
{
    if ( dev < 0 || dev >= ASY_MAX ) {
        return -1;
    }

    return rlsdstat(&Asy[dev]);
}


/* Wait for a signal that the RLSD modem status has changed.
 */
int wait_rlsd_asy(int dev, int new_rlsd)
{
    struct asy *ap;

    if ( dev < 0 || dev >= ASY_MAX ) {
        return -1;
    }

    ap = &Asy[dev];

    for (;;) {
        /* Wait for state change to requested value */
        if ( new_rlsd && rlsdstat(ap) )
            return 1;

        if ( !new_rlsd && !rlsdstat(ap) )
            return 0;
    }
}


/* Poll the asynch input queues; called on every clock tick.
 * This helps limit the interrupt ring buffer occupancy when long
 * packets are being received.
 */
void asytimer(void)
{
    struct asy *asyp;
    int i, i_state;

    for ( asyp = Asy, i = 0 ; i < ASY_MAX ; asyp++, i++ ) {
        if ( asyp->fifo.buf == NULL )
            continue;

        if ( asyp->dma.busy
             && (inportb(asyp->addr + LSR) & LSR_THRE)
             && (!asyp->cts || (asyp->msr & MSR_CTS)) ) {
#ifdef DEBUGTTY
            asyp->txto++;
#endif  /* DEBUGTTY */
            i_state = dirps();
            asytxint(asyp);
            restore(i_state);
        }
    }

    for ( asyp = Asy, i = 0 ; i < ASY_MAX ; asyp++, i++ ) {
        if ( asyp->fifo.buf == NULL )
            continue;

        i_state = dirps();
        if ( asyp->fifo.cnt && asyp->pppunit >= 0 && ! asyp->pppsem ) {
            ++(asyp->pppsem);
            restore(i_state);
            asyrxschedule(i, asyp->pppunit);
            i_state = dirps();
            --(asyp->pppsem);
        }
        restore(i_state);
    }
}


/* Check if the interrupt vectors in use for COM ports still owned by us.
 */
int asy_chkirqvecs(void)
{
    struct asy *asyp;
    int i;

    for ( asyp = Asy, i = 0 ; i < ASY_MAX ; asyp++, i++ ) {
        if ( asyp->fifo.buf == NULL )
            continue;

        if ( FP_SEG(getirq(asyp->vec)) != _CS )
            return 0;
    }

    return 1;
}


/* Far functions meant to be called by programs that use the private
 * interface functions in PKTDRVR.C, CHAT for example. These functions
 * uses whatever com id is defined in the external variable 'comopen',
 * which is set up in DOSMAIN.C.
 */


/* A simple wrapper for asy_rxcheck()
 */
int far _loadds asyf_rxcheck(void)
{
    return asy_rxcheck(comopen);
}


/* A simple wrapper for asy_txcheck()
 */
int far _loadds asyf_txcheck(void)
{
    return asy_txcheck(comopen);
}


/* This function calls asy_read() with a 1 count and checks the result.
 * If 1 is returned from the call, then a valid byte was found. A 0 value
 * return means that nothing is in the rx buffer, we return -1 in this
 * case. A less than 0 value returned means some error occured, subtract
 * 1 to it and return the resulting value.
 */
int far _loadds asyf_getc(void)
{
    int r;
    /* DON'T CHANGE THIS !!!, this variable must lie in the
       Data segment for a proper addressing from asy_read() later. */
    static uint8 chr;

    if ( (r = asy_read(comopen, &chr, 1)) == 1 )
        return (int)chr;
    else if ( r == 0 )
        return -1;

    return (r - 1);
}

/* First we check if the tx buffer is busy, if so we return a 0 to the
 * caller. Note that we make a copy of the byte to a static buffer, cause
 * asy_write() must not be used with local buffers in the stack. We return
 * the asy_write() result to the caller.
 */
int far _loadds asyf_putc(uint8 chr)
{
    /* DON'T CHANGE THIS !!!, this variable must lie in the
       Data segment for a proper addressing from asy_write() later. */
    static uint8 chrbuf;

    if ( ((struct dma *)&(Asy[comopen].dma))->busy )
        return 0;

    chrbuf = chr;

    return asy_write(comopen, &chrbuf, 1);
}


const ASY_HOOKS asy_exportinfo = {
    sizeof(asy_exportinfo),
    asyf_rxcheck,
    asyf_txcheck,
    asyf_getc,
    asyf_putc
};


/* Prints info about asynch port "dev".
 */
#ifndef DEBUGTTY
#pragma argsused
int asy_info(int dev)
{
    return 0;
}
#else   /* DEBUGTTY */
int asy_info(int dev)
{
    struct asy *asyp;
    int mcr, msr;

    if ( dev < 0 || dev >= ASY_MAX )
        return -1;

    asyp = &Asy[dev];

    syslog(LOG_NOTICE, "COM%d %X %u:", dev + 1, asyp->addr, asyp->vec);

    if ( asyp->is_16550a )
        syslog(LOG_NOTICE, " [NS16550A]");

    if ( asyp->cts )
        syslog(LOG_NOTICE, " [cts flow control]");

    if ( asyp->rlsd )
        syslog(LOG_NOTICE, " [rlsd line control]");

    syslog(LOG_NOTICE, " %lu bps\n", asyp->speed);

    mcr = inportb(asyp->addr + MCR);

    if ( asyp->cts || asyp->rlsd )
        msr = asyp->msr;
    else
        msr = inportb(asyp->addr + MSR);

    syslog(LOG_NOTICE, " MC: int %lu  DTR %s  RTS %s  CTS %s  DSR %s  RI %s  CD %s\n",
            asyp->msint_count,
            (mcr & MCR_DTR) ? "On" : "Off",
            (mcr & MCR_RTS) ? "On" : "Off",
            (msr & MSR_CTS) ? "On" : "Off",
            (msr & MSR_DSR) ? "On" : "Off",
            (msr & MSR_RI) ? "On" : "Off",
            (msr & MSR_RLSD) ? "On" : "Off");

    syslog(LOG_NOTICE, " RX: int %lu  chars %lu  hw over %lu  hw hi %lu",
            asyp->rxints, asyp->rxchar, asyp->overrun, asyp->rxhiwat);

    asyp->rxhiwat = 0;

    if(asyp->is_16550a)
        syslog(LOG_NOTICE, "  fifo TO %lu", asyp->fifotimeouts);

    syslog(LOG_NOTICE, "  sw over %lu  sw hi %u\n",
            asyp->fifo.overrun, asyp->fifo.hiwat);

    asyp->fifo.hiwat = 0;

    syslog(LOG_NOTICE, " TX: int %lu  chars %lu  THRE TO %lu  %s\n",
            asyp->txints,asyp->txchar, asyp->txto,
            asyp->dma.busy ? " BUSY" : "");

    return 0;
}
#endif  /* DEBUGTTY */
