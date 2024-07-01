/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

#include <sys/types.h>
#include "types.h"
#include "syslog.h"

#include "pppd.h"

#ifdef DEBUGMAIN
#include <stdlib.h>
#include <string.h>
/* for not including mem.h (laziness) */
unsigned coreleft(void);
#endif

#include <ctype.h>
#include <stdarg.h>
#include <dir.h>
#include <dos.h>
#include <errno.h>

#include "magic.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"

#ifdef IPX_CHANGE
#include "ipxcp.h"
#endif /* IPX_CHANGE */

#include "upap.h"
#include "chap.h"
#include "ccp.h"
#include "pathname.h"
#include "patchlev.h"
#include "if_ppp.h"
#include "if_pppva.h"
#include "asy.h"

struct  callout {
    u_int32_t       c_time;         /* time at which to call routine */
    caddr_t         c_arg;          /* argument to routine */
    void            (*c_func)();    /* routine */
    struct callout  *c_next;
};

/* Stolen from UNDOCDOS.H (Schulman et. al.) */
typedef struct {                /* Memory Control Block entry   */
    uint8 type;                 /* 'M'=in chain; 'Z'=at end     */
    uint16 owner;               /* PSP of the owner             */
    uint16 size;                /* in 16-byte paragraphs        */
    uint8 unused[3];
    uint8 dos4[8];              /* filename, if DOS4+           */
} MCB, far * LPMCB;

/* Stolen from PSP.H (Geoff Chappel) */
typedef struct {
    uint8 Int20h[2];
    uint16 spCeiling;
    uint8 cReserved04h;
    uint8 CPMCall[5];
    void (far *lpInt22h)();
    void (interrupt far *lpInt23h)();
    void (interrupt far *lpInt24h)();
    uint16 spParentPSP;
    uint8 cDefaultHandleTable[20];
    uint16 spEnvironment;
    void far *lpStack;
    uint16 wNumberOfHandles;
    uint8 far *lpHandleTable;
    void far *lpReserved38h;
    uint8 cReserved3Ch;
    uint8 cAppendFlag;
    uint8 cReserved3Eh[2];
    uint16 wDOSVersion;
    uint8 cReserved42h[0x50 - 0x42];
    uint8 Int21hRetf[3];
    uint8 cReserved53h[2];
    uint8 ExtendedFCB1[7];
    uint8 FCB1[0x10];
    uint8 FCB2[0x14];
    uint8 cCommandLineLength;
    uint8 CommandLine[0x100 - 0x81];
} PSP, far *LPPSP;

/* The following definition is needed for marking unused irq vectors. The
 * restoring logic for these relies on vectors not having such a value
 * for restore them. As some original vectors can contain a 0 value, the
 * standar NULL provided with C is not well suited for this task.
 */
#define NILVEC ((void (interrupt *)())MK_FP(0xFFFF, 0xFFFF))

/*********************************/

/* External funcs */

int parse_args(int, char **);
int options_from_file(const char *, int);
int options_from_user(void);
int options_for_tty(int);
void link_terminated(int unit);
void asytimer(void);
int isifup(int);
/* the following is in CRITICAL.ASM */
void interrupt crithandler();
/* the following are in LOWLEVEL.ASM */
void set08handler (void interrupt (*)(), void far (*)(void));
void interrupt int08handler();
void interrupt pktinthandler();
/* in PKTDRVR.C */
int verify_packet_int(int);

/* Prototypes */

void asyrxschedule(int, int);
void calltimeout(void);
void log_packet(u_char *, int, char *);
void format_packet(u_char *, int, void (*)(void *, char *, ...), void *);
void pr_log(void *arg, char *fmt, ...);
void print_string(char *, int, void (*)(void *, char *, ...), void *);

static void far everytick(void);
static void interrupt dummy_int1B();
static int runconnector(char *, char *);

/**************************************/

/* Some Borland C hacks */

extern unsigned _stklen = 2304;
extern unsigned _heaplen = 11264;

unsigned _nfile = 20;   /* to avoid the inclusion of C runtime file I/O */

/* Interface vars */

char *progname;                 /* Name of this program */
int ifunit = -1;                /* Interface unit number */
volatile int phase;             /* Where the link is at (state) */
volatile int kill_link;         /* Flag for killing the link */
volatile int hungup;            /* Flag indicates terminal has been hung up */
long baud_rate = 9600L;
int going_resident = 0;
int comopen = -1;

u_char outpacket_buf[PPP_MRU+PPP_HDRLEN]; /* buffer for outgoing packet */

/* Private vars */

static struct callout *callout = NULL;  /* Callout list */

/* Counter incremented every tick (18.2 per second). The flag indicates
 * that update was done.
 */
extern volatile u_int32_t jiffies;
extern volatile uint8 jifflag;

/*
 * PPP Data Link Layer "protocol" table.
 * One entry per supported protocol.
 */
static struct protent {
    u_short protocol;
    void (*init)();
    void (*input)();
    void (*protrej)();
    int  (*printpkt)();
    void (*datainput)();
    char *name;
} prottbl[] = {
    { PPP_LCP, lcp_init, lcp_input, lcp_protrej, lcp_printpkt, NULL, "LCP" },
    { PPP_IPCP, ipcp_init, ipcp_input, ipcp_protrej, ipcp_printpkt, NULL, "IPCP" },
    { PPP_PAP, upap_init, upap_input, upap_protrej, upap_printpkt, NULL, "PAP" },
#ifdef ALLOWCHAP
    { PPP_CHAP, ChapInit, ChapInput, ChapProtocolReject, ChapPrintPkt, NULL, "CHAP" },
#endif  /* ALLOWCHAP */
#ifdef IPX_CHANGE
    { PPP_IPXCP, ipxcp_init, ipxcp_input, ipxcp_protrej, ipxcp_printpkt, NULL, "IPXCP" },
#endif  /* IPX_CHANGE */
#ifdef ALLOWCCP
    { PPP_CCP, ccp_init, ccp_input, ccp_protrej, ccp_printpkt, ccp_datainput, "CCP" },
#endif  /* ALLOWCCP */
};

#define N_PROTO (sizeof(prottbl) / sizeof(prottbl[0]))

static int orgbrk = -1;
static int disable_syslog = 0;
static void interrupt (*orig_08_vector)() = NILVEC;
static void interrupt (*orig_1B_vector)() = NILVEC;
static void interrupt (*orig_23_vector)() = NILVEC;
static void interrupt (*orig_24_vector)() = NILVEC;
static void interrupt (*orig_pkt_vector)() = NILVEC;

/**************************************/

/*
 * Returns number of timer ticks since program started.
 */

u_int32_t getjiffies(void)
{
    u_int32_t long r;
    int state;

    state = dirps(); r = jiffies; restore(state);

    return r;
}


/*
 * Function called from the timer tick handler in LOWLEVEL.ASM. The far
 * calling convention is required. The original timer handler is called
 * before entering here, also registers are saved and interrupts are
 * enabled. The stack is switched to our own interrupt stack, the function
 * is protected against reentering by a semaphore managed by the real
 * handler in LOWLEVEL.ASM.
 */

void far everytick(void)
{
    /*
     * Call asynch periodic processing, mostly for retriggering the
     * TX interrupt in case something bad happened. Also checks the
     * receiver fifos and triggers ppp_tty_receive() in case is not
     * already running.
     */
    asytimer();

    if ( ! going_resident )
        return;

    /* detect loss of carrier */
    if ( modem && ! get_rlsd_asy(comopen) ) {
        MAINDEBUG((LOG_ERR, "main: COM%d, carrier lost.\n", comopen + 1));
        hungup = 1;
        /* carrier lost, terminate ppp link */
        lcp_lowerdown(ifunit);  /* serial link is no longer available */
        link_terminated(ifunit);
    }

    /* process timeouts */
    calltimeout();
}


/*
 * C runtime sleep() replacement.
 */

void sleep(unsigned seconds)
{
    u_int32_t lim;

    lim = getjiffies();
    lim += (seconds * HZ);

    while ( getjiffies() < lim ) {
        jifflag = 1;
        while ( jifflag );
    }
}


/*
 * BIOS CTRL+C handler.
 */

void interrupt dummy_int1B()
{
    kill_link = 1;
}


/*
 * DOS CTRL+C handler.
 */

void interrupt brkhandler()
{
    /* do nothing */
}


/*
 * This code comes from the DOS internals book by Geoff Chappel. I adapted
 * it somewhat for BorlandC.
 */

#define pspaddr ((LPPSP)MK_FP(_psp, NULL))

void close_DOS_handles(void)
{
    /*  This function closes all file handles.  Note that there is no
        way to determine at DOS function level the number of open
        handles or even a useful upper bound.  Ordinarily, programs
        written in Microsoft C have a maximum of 20 handles, but the
        user is provided with instructions for increasing this by
        modifying the C run-time module CRT0DAT.

        Each process has its own handle table, the address and size of
        which may be found by inspecting the PSP.  The table consists of
        single-byte indexes into the System File Tables.  The entry FFh
        indicates that no open file is associated with the handle.  The
        Microsoft C library function _dos_close is called to close each
        open handle.  */

    int handle;
    uint8 far *tableptr;

    handle = (pspaddr->wNumberOfHandles);

    if ( handle ) {
        handle--;
        tableptr = pspaddr->lpHandleTable + handle;

        do {
            if ( *tableptr != 0xFF )
                _dos_close(handle);

            tableptr--;
            handle--;
        } while ( handle >= 0 );
    }
}


unsigned free_DOS_environment(void)
{
    /*  This function calls DOS to release the block of memory beginning
        with the program's environment.  It returns a DOS error code if
        the environment exists but cannot be released;  0, otherwise.

        The environment is located by consulting the program's PSP, the
        segment address of which is recorded by the C run-time and made
        available through the variable _psp.  Memory is released by
        calling the Microsoft C library function _dos_freemem ().  */

    unsigned envseg;
    unsigned errorcode;

    envseg = pspaddr->spEnvironment;

    if ( envseg ) {
        errorcode = _dos_freemem(envseg);

        if ( errorcode == 0 )
            pspaddr->spEnvironment = 0;
        else
            return (errorcode);
    }

    return (0);
}


/*
 * This functions checks if the vectors used for timer tick and packet
 * driver interface still owned by us.
 */
int main_chkirqvecs(void)
{
    if ( (orig_pkt_vector != NILVEC && FP_SEG(getvect(pktint)) != _CS)
         ||
         (orig_08_vector != NILVEC && FP_SEG(getvect(0x08)) != _CS) )
        return 0;

    return 1;
}


/*
 * Function installed via atexit() for cleanup before exiting.
 */

void exit_fn1(void)
{
    /* skip PPP shutdown stuff if we are going resident */
    if ( ! going_resident ) {
        syslog(LOG_NOTICE,
               "PPP link is down, driver not installed.\n");

        /* shutdown the ppp handler */
        if ( ifunit != -1 ) {
            if ( comopen != -1 )
                asy_set_ppp(comopen, -1);

            ppp_dev_close(ifunit);
            ifunit = -1;
        }

        /* shutdown the serial link & modem */
        if ( comopen != -1 ) {
            /* show COM handler info */
            if ( debug )
                asy_info(comopen);

            /* set DTR & RTS low */
            if ( asy_ioctl(comopen, PARAM_DOWN, 0, (int32)0) < 0 ) {
                MAINDEBUG((LOG_ERR, "main: COM%d PARAM_DOWN ioctl error.\n", comopen + 1));
            }

            sleep(2);

            /* set DTR & RTS high */
            if ( asy_ioctl(comopen, PARAM_UP, 0, (int32)0) < 0 ) {
                MAINDEBUG((LOG_ERR, "main: COM%d PARAM_UP ioctl error.\n", comopen + 1));
            }

            sleep(1);

            /* send hangup string to modem */
            if ( asy_write(comopen, (u_char *)"+++ATH0\r", 8) < 0 ) {
                MAINDEBUG((LOG_ERR, "main: COM%d\n asy_write() error.\n", comopen + 1));
            }

            sleep(1);
            /* release the COM handler */
            asy_stop(comopen);
            comopen = -1;
        }

        /* restore packet driver original handler */
        if ( orig_pkt_vector != NILVEC ) {
            setvect(pktint, orig_pkt_vector);
            orig_pkt_vector = NILVEC;
        }

        /* restore timer tick original handler */
        if ( orig_08_vector != NILVEC ) {
            setvect(0x08, orig_08_vector);
            orig_08_vector = NILVEC;
        }
    }
    else {
        syslog(LOG_NOTICE,
               "Installed packet driver handler at vector 0x%.2X.\n", pktint);
        /* we are going resident, close all dos file handles associated
           with our process, per Geoff Chappel (DOS internals) */
        close_DOS_handles();
        /* also free our environment */
        free_DOS_environment();
    }

    /* The following code restores DOS critical handler and DOS break
       handler. Geoff Chappel (DOS internals) suggest that you don't
       want to do this, as DOS can fail or be interrupted during the
       going resident stage. Anyway DOS itself will restore these
       vectors with the ones saved in the program PSP, but I'm unsure
       about this behavior. I make the cleanup now, and hope all goes
       fine. */

    /* restore critical error original handler */
    if ( orig_24_vector != NILVEC ) {
        setvect(0x24, orig_24_vector);
        orig_24_vector = NILVEC;
    }

    /* restore BIOS CTRL+C original handler */
    if ( orig_1B_vector != NILVEC ) {
        setvect(0x1B, orig_1B_vector);
        orig_1B_vector = NILVEC;
    }

    /* restore DOS CTRL+C original handler */
    if ( orig_23_vector != NILVEC ) {
        setvect(0x23, orig_23_vector);
        orig_23_vector = NILVEC;
    }

    /* restore DOS BREAK flag if needed */
    if ( orgbrk != -1 ) {
        setcbrk(orgbrk);
        orgbrk = -1;
    }

    /* disable messages output when staying resident */
    if ( going_resident )
        disable_syslog = 1;
}


/*
 * Program entry point :-)
 */

int main(int argc, char *argv[])
{
    int i;
    unsigned pkeep;
    char *p, pppdpath[MAXPATH], optspath[MAXPATH];
#if 0
    int ch, extended;
#endif

    extern uint16 StktopC;

    /* Check DOS version. */
    if ( _osminor < 10 && _osmajor < 3 ) {
        /* Abort, DOS >= 3.10 required */
        syslog(LOG_ERR, "DOS version >= 3.10 required.\n");

        return -1;
    }

    /* set up our interrupt stack pointer */
    StktopC = ((uint16)_SP - 1280) & 0xFFFE;

    /* set up "call at exit time" routines */
    atexit(exit_fn1);

    /* set up DOS CTRL+C mode (BREAK OFF) */
    orgbrk = getcbrk();
    setcbrk(0);

    /* install DOS CTRL+C handler */
    orig_23_vector = getvect(0x23);
    setvect(0x23, brkhandler);

    /* install BIOS CTRL+C handler */
    orig_1B_vector = getvect(0x1B);
    setvect(0x1B, dummy_int1B);

    /* install critical error handler */
    orig_24_vector = getvect(0x24);
    setvect(0x24, crithandler);

    /* install timer tick handler */
    orig_08_vector = getvect(0x08);
    set08handler(orig_08_vector, everytick);
    setvect(0x08, int08handler);

    progname = *argv;
    strcpy(optspath, progname);

    if ( (p = strrchr(optspath, '\\')) != NULL )
        *(p + 1) = '\0';

    strcpy(pppdpath, optspath);
    strcat(optspath, _PATH_SYSOPTIONS);
    strcpy(hostname, "bogushost");

#if 0
    pkeep = (uint16)_SS - _psp + ((uint16)_SP >> 4) + 16U;

    {
        MCB progMCB;

        progMCB = *(LPMCB)MK_FP(_psp - 1, 0);
        MAINDEBUG((LOG_NOTICE,
            "MCB: type %c, owner 0x%.4x, size %lu, unused %.2x-%.2x-%.2x,"
            " dos4 %-8.8s\n",
            progMCB.type, progMCB.owner,
            ((uint32)progMCB.size) << 4,
            progMCB.unused[0], progMCB.unused[1], progMCB.unused[2],
            progMCB.dos4));
    }
#else
    pkeep = ((LPMCB)MK_FP(_psp - 1, 0))->size;
#endif

    /* set up the ppp interface, abort if the open fails */
    if ( (ifunit = ppp_dev_open()) < 0 ) {
        return -1;
    }

    /*
     * Initialize to the standard option set, then parse, in order,
     * the system options file, the user's options file, the command
     * line arguments and the tty options file.
     */
    for ( i = 0 ; i < N_PROTO ; i++ )
        (*prottbl[i].init)(ifunit);

    if ( ! options_from_file(optspath, 0) ||
         ! options_from_user() ||
         ! parse_args(argc-1, argv+1) )
        die(1);

    /* set up the lowlevel (kernel) PPP debugging level */
    ppp_set_debug(ifunit, kdebugflag);

    /*
     * Initialize magic number package.
     */
    magic_init();

#if 0
    /* initialize authentication data, provisional */
#ifdef GERARD
    strcpy(user, "pppuser");
    strcpy(passwd, "gerard");
#else
    strcpy(user, "tonilop@redestb");
    strcpy(passwd, "adv47418");
#endif

    /* initialize asyncmap negotiation, provisional */
    lcp_wantoptions[ifunit].asyncmap |= 0x000A0000LU; /* escape ^S and ^Q */
    lcp_wantoptions[ifunit].neg_asyncmap = 1;
#endif

    /* establish COM handler */
    if ( portnum == -1 ) portnum = 0;
    if ( ! options_for_tty(portnum + 1) ) die(1);
    if ( portbase == 0 ) portbase = 0x3F8;
    if ( portirq == 0 ) portirq = 4;
    if ( inspeed ) baud_rate = inspeed;
    if ( asy_init(portnum, portbase, portirq, (PPP_MTU * 2) + 24, baud_rate, crtscts, modem, 0) < 0 ) {
        MAINDEBUG((LOG_ERR, "main: COM%d initialization error.\n", comopen + 1));

        return -1;
    }

    /* set up the ppp interface serial device */
    ppp_set_tty(ifunit, comopen = portnum);

    /* set DTR & RTS high */
    if ( asy_ioctl(comopen, PARAM_UP, 0, (int32)0) < 0 ) {
        MAINDEBUG((LOG_ERR, "main: COM%d PARAM_UP ioctl error.\n", comopen + 1));

        return -1;
    }

    /* try to find an available packet driver interrupt vector */
    if ( (pktint = verify_packet_int(pktint)) == 0 ) {
        /* none found, complain and exits */
        syslog(LOG_ERR, "No free packet driver vector found.\n");

        return -1;
    }

    orig_pkt_vector = getvect(pktint);
    setvect(pktint, pktinthandler);

    /* show resident size and space left in the near heap */
    MAINDEBUG((LOG_NOTICE, "Resident size: %lu\n", (uint32)pkeep << 4));
    MAINDEBUG((LOG_NOTICE, "coreleft: %u\n", coreleft()));

    /* show COM handler info */
    if ( debug )
        asy_info(comopen);

    /* run connector program (if specified) */
    if ( connector && runconnector(pppdpath, connector) ) {
        free(connector);
        MAINDEBUG((LOG_ERR, "main: connection script failed.\n"));

        return -1;
    }

    free(connector);

    /* abort if no carrier present */
    if ( modem && ! get_rlsd_asy(comopen) ) {
        MAINDEBUG((LOG_ERR, "main: COM%d, carrier lost.\n", comopen + 1));

        return -1;
    }

    /*
     * Start opening the connection, and wait for
     * incoming events (reply, timeout, etc.).
     */
    hungup = 0;
    kill_link = 0;
    MAINDEBUG((LOG_NOTICE, "main: connect ppp%d <--> COM%d.\n", ifunit, comopen + 1));
    lcp_lowerup(ifunit);
    lcp_open(ifunit);   /* Start protocol */
    asy_set_ppp(comopen, ifunit);

    for ( phase = PHASE_ESTABLISH ; phase != PHASE_DEAD ; ) {
#if 0
        asyrxschedule(comopen, ifunit);
#endif
        /* detect loss of carrier */
        if ( modem && ! get_rlsd_asy(comopen) ) {
            MAINDEBUG((LOG_ERR, "main: COM%d, carrier lost.\n", comopen + 1));
            hungup = 1;
            /* carrier lost, terminate ppp link */
            lcp_lowerdown(ifunit);      /* serial link is no longer available */
            link_terminated(ifunit);
        }

        /* someone wants the ppp link down */
        if ( kill_link ) {
            lcp_close(ifunit);
            kill_link = 0;
        }

        /* process timeouts */
        calltimeout();

        /* if IP network phase is OK, prepare for terminate as resident */
        if ( phase == PHASE_NETWORK && isifup(ifunit) ) {
            debug = 0;
            ppp_set_debug(ifunit, kdebugflag = 0);
            going_resident = 1;
            _dos_keep(0, pkeep);
        }
#if 0
        /* some debug stuff and keyboard control */
        if ( kbhit() ) {
            extended = FALSE;

            if ( ! (ch = getch()) ) {
                ch = getch();
                extended = TRUE;
            }

            if ( extended ) {
                switch ( ch ) {
                    case 59:    /* F1 */
                       asy_info(comopen);
                    break;

                    case 117:   /* CTRL+FIN */
                        kill_link = 1;
                    break;
                }
            }
        }
#endif  /* 0 */
    }

    return 0;
}


/*
 * This function runs an external program for connection chores. It will
 * modify the 'command' string, that is in the heap created by strdup().
 * Note we are calling an internal Borlandc lib function, documented only
 * in the library source.
 */

int _spawn(char *, char *, char *);

static int tryspawn(char *ppath, char *pname, char *pext, char *pcmdline)
{
    int pathlen;
    char progpath[MAXPATH];

    if ( ppath ) {
        strcpy(progpath, ppath);
        pathlen = strlen(progpath);
    }
    else {
        progpath[0] = '\0';
        pathlen = 0;
    }

    if ( (pathlen += strlen(pname)) >= MAXPATH )
        return -1;

    strcat(progpath, pname);

    if ( pext ) {
        if ( (pathlen += strlen(pext)) >= MAXPATH )
            return -1;

        strcat(progpath, pext);
    }

    errno = 0;

    return _spawn(progpath, pcmdline, (char *)0);
}

static int runconnector(char *path, char *command)
{
    int rc, i, memstrat, memstrat_changed = 0;
    union REGS regs;
    char *p, *pargs, *pname, cmdline[130];

    /* disect program name (possibily with full path) from arguments */
    pname = command;
    while ( isspace(*pname) ) ++pname;
    pargs = pname;
    while ( !isspace(*pargs) ) ++pargs;
    *pargs++ = '\0';
    while ( isspace(*pargs) ) ++pargs;

    /* form a DOS style command line */
    cmdline[0] = min(strlen(pargs), 127);
    strncpy(&cmdline[1], pargs, 127);
    cmdline[127] = '\0';
    strcat(&cmdline[1], "\r");

    /* under DOS >= 5, change the memory allocation strategy for
       trying low memory first. It happens to be some trouble with
       CHAT.EXE execution if there is an upper memory block long
       enough for the program image but not for the required memory
       to run it. The problem only arises if you used LOADHIGH for
       running PPPD. */
    if ( _osmajor >= 5 ) {
        regs.x.ax = 0x5800;
        intdos(&regs, &regs);
        memstrat = regs.x.ax;
        memstrat_changed = 1;
        regs.x.ax = 0x5801;
        regs.x.bx = 0x0000;     /* first fit, try low memory first */
        intdos(&regs, &regs);
    }

    /* the following loop makes two attempts to run the program, the
       first with the name specified in connector, the second with
       the path where PPPD.EXE lives appended */
    p = NULL;
    i = 1;
    do {
        /* first attempt, program name as is */
        if ( (rc = tryspawn(p, pname, NULL, cmdline)) >= 0 ||
             errno != ENOENT )
            break;

        /* now with .EXE appended */
        if ( (rc = tryspawn(p, pname, ".exe", cmdline)) >= 0 ||
             errno != ENOENT )
            break;

        /* now with .COM appended */
        if ( (rc = tryspawn(p, pname, ".com", cmdline)) >= 0
             || errno != ENOENT )
            break;
    } while ( p = path, i-- );

    /* restore memory strategy if needed */
    if ( memstrat_changed ) {
        regs.x.ax = 0x5801;
        regs.x.bx = memstrat;
        intdos(&regs, &regs);
    }

    return rc;
}


/*
 * Function called from tty receiver ISR when there is data available.
 */

#ifdef LOWLEVELASY

void asyrxschedule(int comdev, int unit)
{
    /* process incoming serial data */
    while ( asy_rxcheck(comdev) > 0 && ppp_tty_receive(unit) >= 0 )
        ;
}

#else

void asyrxschedule(int comdev, int unit)
{
    int i;
    static uint8 rcvbuf[128];

    /* process incoming serial data */
    while ( (i = asy_rxcheck(comdev)) > 0 ) {
        i = min(i, sizeof(rcvbuf));

        if ( asy_read(comdev, rcvbuf, i) <= 0 ) {
            MAINDEBUG((LOG_ERR, "rxmain: asy_read() failed.\n"));
        }
        else {
            if ( ppp_tty_receive(unit, rcvbuf, i) < 0 )
                break;
        }
    }
}

#endif  /* LOWLEVELASY */

/*
 * Function called when no network packet arrives.
 */

int rcv_proto_unknown(struct ppp *ppp, u_short protocol, u_char *p, int len)
{
    int i;

    if ( debug /*&& (debugflags & DBG_INPACKET)*/ )
        log_packet(p - PPP_HDRLEN, len + PPP_HDRLEN, "rcvd ");
    /*
     * Toss all non-LCP packets unless LCP is OPEN.
     */
    if ( protocol != PPP_LCP && lcp_fsm[ppp->line].state != OPENED ) {
        MAINDEBUG((LOG_INFO, "rcv_proto_unknown: Received non-LCP packet when LCP not open.\n"));

        return 0;
    }
    /*
     * Upcall the proper protocol input routine.
     */
    for ( i = 0 ; i < sizeof(prottbl) / sizeof(struct protent) ; i++ ) {
        if ( prottbl[i].protocol == protocol ) {
            (*prottbl[i].input)(ppp->line, p, len);

            return 1;
        }

        if ( protocol == (prottbl[i].protocol & ~0x8000)
            && prottbl[i].datainput != NULL ) {
            (*prottbl[i].datainput)(ppp->line, p, len);

            return 1;
        }
    }

    MAINDEBUG((LOG_WARNING, "rcv_proto_unknown: unknown protocol (0x%x) received.\n", protocol));
    lcp_sprotrej(ppp->line, p - PPP_HDRLEN, len + PPP_HDRLEN);

    return 0;
}


/*
 * untimeout - Unschedule a timeout.
 */

void untimeout(void (*func)(), caddr_t arg)
{
    struct callout **copp, *freep;

    MAINDEBUG((LOG_DEBUG, "Untimeout %x:%x.\n", func, arg));

    /*
     * Find first matching timeout and remove it from the list.
     */
    for ( copp = &callout ; (freep = *copp) != NULL ; copp = &freep->c_next ) {
        if ( freep->c_func == func && freep->c_arg == arg ) {
            *copp = freep->c_next;
            (void)free((char *) freep);
            break;
        }
    }
}


/*
 * timeout - Schedule a timeout.
 *
 * Note that this timeout takes the number of seconds, NOT hz (as in
 * the kernel).
 */

void timeout(void (*func)(), caddr_t arg, u_int32_t timev)
{
    struct callout *newp, *p, **pp;
    u_int32_t timenow;

    MAINDEBUG((LOG_DEBUG, "Timeout %x:%x in %lu seconds.\n", func, arg, timev));

    /*
     * Allocate timeout.
     */
    if ( (newp = (struct callout *)malloc(sizeof(struct callout))) == NULL ) {
        MAINDEBUG((LOG_ERR, "Out of memory in timeout()!\n"));
        die(1);
    }

    newp->c_arg = arg;
    newp->c_func = func;
    timenow = getjiffies();
    newp->c_time = timenow + (timev * HZ);

    /*
     * Find correct place and link it in.
     */
    for ( pp = &callout ; (p = *pp) != NULL ; pp = &p->c_next ) {
        if ( newp->c_time < p->c_time )
            break;
    }

    newp->c_next = p;
    *pp = newp;
}


/*
 * calltimeout - Call any timeout routines which are now due.
 */

void calltimeout(void)
{
    struct callout *p;
    u_int32_t timenow;

    extern uint8 jifflag;

    if ( jifflag )
        return;

    timenow = getjiffies();
    jifflag = 1;

    while ( callout != NULL ) {
        p = callout;

        if ( ! (p->c_time < timenow) )
            break;              /* no, it's not time yet */

        callout = p->c_next;
        MAINDEBUG((LOG_DEBUG, "calltimeout, calling %x:%x.\n", p->c_func, p->c_arg));
        (*p->c_func)(p->c_arg);
        free((char *)p);
    }
}


/*
 * output - Output PPP packet.
 */

void output(int unit, unsigned char *p, int len)
{
#ifdef DEBUGMAIN
    if ( debug )
        log_packet(p, len, "sent ");
#endif  /* DEBUGMAIN */

    if ( ppp_tty_write(unit, p, len) <= 0 ) {
        MAINDEBUG((LOG_ERR, "output: call to ppp_tty_write() failed.\n"));
    }
}


/*
 * log_packet - format a packet and log it.
 */
#ifdef DEBUGMAIN

static char line[256];  /* line to be logged accumulated here */
static char *linep;

void log_packet(u_char *p, int len, char *prefix)
{
    strcpy(line, prefix);
    linep = line + strlen(line);

    format_packet(p, len, pr_log, NULL);

    if ( linep != line )
        syslog(LOG_DEBUG, "%s\n", line);
}


/*
 * format_packet - make a readable representation of a packet,
 * calling 'printer(arg, format, ...)' to output it.
 */

void format_packet(u_char *p, int len, void (*printer)(void *, char *, ...), void *arg)
{
    int i, n;
    u_short proto;
    u_char x;

    if ( len >= PPP_HDRLEN && p[0] == PPP_ALLSTATIONS && p[1] == PPP_UI ) {
        p += 2;
        GETSHORT(proto, p);
        len -= PPP_HDRLEN;

        for ( i = 0 ; i < N_PROTO ; ++i )
            if ( proto == prottbl[i].protocol )
                break;

        if ( i < N_PROTO ) {
            printer(arg, "[%s", prottbl[i].name);
            n = (*prottbl[i].printpkt)(p, len, printer, arg);
            printer(arg, "]");
            p += n;
            len -= n;
        }
        else {
            printer(arg, "[proto=0x%x]", proto);
        }
    }

    for ( ; len > 0 ; --len ) {
        GETCHAR(x, p);
        printer(arg, " %.2x", x);
    }
}


void pr_log(void *arg, char *fmt, ...)
{
    int n;
    va_list pvar;
    char pr_buf[256];

    va_start(pvar, fmt);
    vsprintf(pr_buf, fmt, pvar);
    va_end(pvar);

    n = strlen(pr_buf);

    if ( linep + n + 1 > line + sizeof(line) ) {
        syslog(LOG_DEBUG, "%s", line);
        linep = line;
    }

    strcpy(linep, pr_buf);
    linep += n;
}


/*
 * print_string - print a readable representation of a string using
 * printer.
 */

void print_string(char *p, int len, void (*printer)(void *, char *, ...), void *arg)
{
    int c;

    printer(arg, "\"");

    for ( ; len > 0 ; --len ) {
        c = *p++;

        if ( ' ' <= c && c <= '~' )
            printer(arg, "%c", c);
        else
            printer(arg, "\\%.3o", c);
    }

    printer(arg, "\"");
}

#else   /* DEBUGMAIN */

#pragma argsused
void log_packet(u_char *p, int len, char *prefix)
{
}


#pragma argsused
void format_packet(u_char *p, int len, void (*printer)(void *, char *, ...), void *arg)
{
}


#pragma argsused
void pr_log(void *arg, char *fmt, ...)
{
}


#pragma argsused
void print_string(char *p, int len, void (*printer)(void *, char *, ...), void *arg)
{
}

#endif  /* DEBUGMAIN */

int syslog(int logid, const char *fmt, ...)
{
    unsigned len, wlen;
    va_list marker;
    char buf[256];

    va_start(marker, fmt);
    len = vsprintf(buf, fmt, marker);
    va_end(marker);

    if ( buf[len - 1] == '\n' && buf[len - 2] != '\r' ) {
        buf[len - 1] = '\r';
        buf[len++] = '\n';
    }

    if ( ! disable_syslog )
        _dos_write(( logid == LOG_ERR || logid == LOG_NOTICE) ? 2 : 1,
                   (void far *)buf, len, &wlen);
    else
        wlen = len;

    return wlen;
}


/*
 * Return an apropiate netmask based in the local ip, gateway ip (remote)
 * and the options netmask. This is slightly different under Linux, as it
 * takes aditional steps for ORing the netmasks of other devices that lies
 * on the same subnet as the PPP interface. Under DOS there is no such
 * things, so I take the aproach that Frank Moltzan explains in the docs
 * that come with his PPPPKT06 driver.
 */
u_int32_t GetMask(u_int32_t addr, u_int32_t gateway)
{
    u_int32_t mask;

    addr = ntohl(addr);
    gateway = ntohl(gateway);

    if ( IN_CLASSA(addr) ) {    /* determine network mask for address class */
        mask = IN_CLASSA_NET;
    }
    else {
        if ( IN_CLASSB(addr) ) {
            mask = IN_CLASSB_NET;
        }
        else {
            mask = IN_CLASSC_NET;
        }
    }

    /* class D nets are disallowed by bad_ip_adrs */

    /* do a consistency check on the netmask, this is needed because
       we are a packet driver that can emulate ethernet devices and
       the TCP/IP routing must believe that it can reach the gateway. */
    while ( (addr & mask) != (gateway & mask) ) {
        /* what this does is to find the netmask that satisfies
           the condition -> (ip & netmask) == (gateway & netmask) */
        mask <<= 1;
    }

    return (htonl(mask) | netmask);
}


/*
 * demuxprotrej - Demultiplex a Protocol-Reject.
 */

void demuxprotrej(int unit, int protocol)
{
    int i;

    /*
     * Upcall the proper Protocol-Reject routine.
     */
    for ( i = 0 ; i < sizeof(prottbl) / sizeof(struct protent) ; i++ ) {
        if ( prottbl[i].protocol == protocol ) {
            (*prottbl[i].protrej)(unit);
            return;
        }
    }

    MAINDEBUG((LOG_WARNING, "demuxprotrej: Unrecognized Protocol-Reject for protocol 0x%x", protocol));
}


/*
 * quit - Clean up state and exit (with an error indication).
 */

void quit(void)
{
    die(1);
}


/*
 * die - like quit, except we can specify an exit status.
 */

void die(int status)
{
    exit(status);
}


/*
 * novm - log an error message saying we ran out of memory, and die.
 */

void novm(char *msg)
{
    syslog(LOG_ERR, "Memory exhausted allocating %s.\n", msg);
    die(1);
}


u_int32_t inet_addr(const char *s)
{
    u_int32_t n;
    int i;
    u_short t;

    n = 0;

    if ( s == NULL )
        return -1L;

    /* Skip any leading stuff (e.g., spaces, '[') */
    while ( *s != '\0' && !isdigit(*s) )
        s++;

    if ( *s == '\0' )
        return -1L;

    for ( i = 24 ; i >= 0 ; i -= 8 ) {
        if ( (t = (u_short)strtoul(s, (char **)0, 10)) > 255 )
            return -1L;

        n |= (u_int32_t)t << i;

        if ( ! i )
            return htonl(n);

        if ( (s = strchr(s, '.')) == NULL )
            return -1L;

        s++;
    }

    return -1L;
}


/*
 * Placeholder to avoid the linking of C lib file I/O, a Borland C hack :-)
 */
void _setupio(void)
{
}

