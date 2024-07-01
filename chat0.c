/*
 *      Chat -- a program for automatic session establishment (i.e. dial
 *              the phone and log in).
 *
 * Standard termination codes:
 *  0 - successful completion of the script
 *  1 - invalid argument, expect string too large, etc.
 *  2 - error on an I/O operation or fatal error condtion.
 *  3 - timeout waiting for a simple string.
 *  4 - the first string declared as "ABORT"
 *  5 - the second string declared as "ABORT"
 *  6 - ... and so on for successive ABORT strings.
 *
 *      This software is in the public domain.
 *
 *      Please send all bug reports, requests for information, etc. to:
 *
 *              Al Longyear (longyear@netcom.com)
 *              (I was the last person to change this code.)
 *
 *      Added -r "report file" switch & REPORT keyword.
 *              Robert Geer <bgeer@xmission.com>
 *
 *      Added -e "echo" switch & ECHO keyword
 *              Dick Streefland <dicks@tasking.nl>
 *
 *      The original author is:
 *
 *              Karl Fox <karl@MorningStar.Com>
 *              Morning Star Technologies, Inc.
 *              1760 Zollinger Road
 *              Columbus, OH  43221
 *              (614)451-1883
 *
 *      DOS port by ALM, tonilop@redestb.es
 */

#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dos.h>
#include <stdarg.h>

#include "syslog.h"

#ifndef STDIO
#include "chatasy.h"

/* The following definition is needed for marking unused irq vectors. The
 * restoring logic for these relies on vectors not having such a value
 * for restore them. As some original vectors can contain a 0 value, the
 * standar NULL provided with C is not well suited for this task.
 */
#define NILVEC ((void (interrupt *)())MK_FP(0xFFFF, 0xFFFF))

#define kbhit() (int)(unsigned char)bdos(0x0B, 0, 0)
#else   /* STDIO */
#include <io.h>
#endif  /* STDIO */

#define STR_LEN 1024

#ifndef SIGTYPE
#define SIGTYPE void
#endif

#undef __P
#define __P(x)  x

#ifndef STDIO
/* the following are in LOWLEVEL.ASM */
void set08handler __P((void interrupt (*)(), void far (*)(void)));
void interrupt int08handler __P(());

void interrupt (*orig_08_vector)() = NILVEC;

extern uint16 StktopC;

/* Some Borland C hacks */

extern unsigned _stklen = 8192;

int     comopen   = -1;
int     portirq   = 0;          /* COM port irq */
int     portbase  = 0;          /* COM port base */
int     portnum   = -1;         /* COM port number */
long    inspeed   = 0;          /* Input/Output speed requested */
long    baud_rate = 9600L;
#endif  /* STDIO */

/*************** Micro getopt() *********************************************/

#define OPTION(c,v)     (_O&2&&**v?*(*v)++:!c||_O&4?0:(!(_O&1)&& \
                                (--c,++v),_O=4,c&&**v=='-'&&v[0][1]?*++*v=='-'\
                                &&!v[0][1]?(--c,++v,0):(_O=2,*(*v)++):0))
#define OPTARG(c,v)     (_O&2?**v||(++v,--c)?(_O=1,--c,*v++): \
                                (_O=4,(char*)0):(char*)0)
#define OPTONLYARG(c,v) (_O&2&&**v?(_O=1,--c,*v++):(char*)0)
#define ARG(c,v)        (c?(--c,*v++):(char*)0)

static int _O = 0;              /* Internal state */

/*************** Micro getopt() *********************************************/

char *program_name;

#define MAX_ABORTS              50
#define MAX_REPORTS             50
#define DEFAULT_CHAT_TIMEOUT    45

int echo          = 0;
int verbose       = 0;
int Verbose       = 0;
int quiet         = 0;
int report        = 0;
int exit_code     = 0;
FILE* report_fp   = (FILE *) 0;
char *report_file = (char *) 0;
char *chat_file   = (char *) 0;
int timeout       = DEFAULT_CHAT_TIMEOUT;

char *abort_string[MAX_ABORTS], *fail_reason = (char *)0, fail_buffer[50];
int n_aborts = 0, abort_next = 0, timeout_next = 0, echo_next = 0;

char *report_string[MAX_REPORTS] ;
char  report_buffer[50] ;
int   n_reports = 0, report_next = 0, report_gathering = 0 ;

void   *dup_mem __P((void *b, size_t c));
void   *copy_of __P((char *s));
#ifndef STDIO
void    exit_fn1 __P((void));
int     write_serial __P((int, const char *, int));
int     read_serial __P((int, char *, int));
void far everytick __P((void));
void    asytimer __P((void));
int     setdevname __P((char *));
int     setspeed __P((char *));
int     setportbase __P((char *));
int     setportirq __P((char *));
#endif  /* STDIO */
void    usage __P((void));
void    logf __P((const char *str));
void    logflush __P((void));
void    fatal __P((const char *msg));
void    sysfatal __P((const char *msg));
SIGTYPE sigint __P((int signo));
void    init __P((void));
void    set_tty_parameters __P((void));
void    echo_stderr __P((int));
void    break_sequence __P((void));
void    terminate __P((int status));
char    *unquote_arg __P((char *arg));
void    do_file __P((char *chat_file));
int     get_string __P((register char *string));
int     put_string __P((register char *s));
int     write_char __P((int c));
int     put_char __P((int c));
int     get_char __P((void));
void    chat_send __P((register char *s));
char   *character __P((int c));
void    chat_expect __P((register char *s));
char   *clean __P((register char *s, int sending));
void    terminate __P((int status));
void    die __P((void));
char   *expect_strtok __P((char *, char *));

void
*dup_mem(b, c)
void *b;
size_t c;
    {
    void *ans = malloc(c);

    if (!ans)
        {
        fatal("memory error!");
        }

    memcpy(ans, b, c);
    return ans;
    }

void
*copy_of(s)
char *s;
    {
    return dup_mem(s, strlen (s) + 1);
    }

/*
 *      chat [ -v ] [ -t timeout ] [ -f chat-file ] [ -r report-file ] \
 *              [...[[expect[-say[-expect...]] say expect[-say[-expect]] ...]]]
 *
 *      Perform a UUCP-dialer-like chat script on stdin and stdout.
 */
#ifndef STDIO
void
exit_fn1()
    {
    if ( comopen >= 0 )
        {
        /* show COM handler info */
        if ( verbose > 1 )
            asy_info(comopen);

        /* set DTR & RTS low */
        asy_ioctl(comopen, PARAM_DOWN, 0, (int32)0);

        sleep(2);

        /* set DTR & RTS high */
        asy_ioctl(comopen, PARAM_UP, 0, (int32)0);

        sleep(1);

        /* send hangup string to modem */
        asy_write(comopen, (uint8 *)"+++ATH0\r", 8);

        sleep(1);
        /* release the COM handler */
        asy_stop(comopen, 0);
        comopen = -1;
        }

    /* restore timer tick original handler */
    if ( orig_08_vector != NILVEC )
        {
        setvect(0x08, orig_08_vector);
        orig_08_vector = NILVEC;
        }
    }

int
write_serial(comnum, src, len)
int comnum;
const char *src;
int len;
    {
    int i;
    clock_t maxtime = clock() + ((long)timeout * 182L / 10L);

    while ( clock() <= maxtime )
        {
        (void)kbhit();

        if ( (i = asy_txcheck(comnum)) < 0 )
            return -1;

        if ( i )
            continue;

        if ( (i = asy_write(comnum, (uint8 *)src, len)) == len )
            return len;
        else
            return -1;
        }

    errno = EINTR;

    return 0;
    }

int
read_serial(comnum, dest, len)
int comnum;
char *dest;
int len;
    {
    int i;
    clock_t maxtime = clock() + ((long)timeout * 182L / 10L);

    while ( clock() <= maxtime )
        {
         (void)kbhit();

        if ( (i = asy_rxcheck(comnum)) < 0 )
            return -1;

        if ( i < len )
            continue;

        if ( (i = asy_read(comnum, (uint8 *)dest, len)) == len )
            return len;
        else
            return -1;
        }

    errno = EINTR;

    return 0;
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
     * TX interrupt in case something bad happened.
     */
    asytimer();
}

/*
 * setdevname - Set the device name.
 */
int setdevname(cp)
char *cp;
{
    uint16 _seg *combiosp = (uint16 _seg *)0x0040;

    /*
     * Check if there is a device by this name.
     */
    if ( ! strnicmp(cp, "COM1", 4) ) {
        if ( (portbase = *(combiosp + (portnum = 0))) == 0 )
            goto noport;

        if ( portirq == 0 ) portirq = 4;
    }
    else if ( ! strnicmp(cp, "COM2", 4) ) {
        if ( (portbase = *(combiosp + (portnum = 1))) == 0 )
            goto noport;

        if ( portirq == 0 ) portirq = 3;
    }
    else if ( ! strnicmp(cp, "COM3", 4) ) {
        if ( (portbase = *(combiosp + (portnum = 2))) == 0 )
            goto noport;

        if ( portirq == 0 ) portirq = 4;
    }
    else if ( ! strnicmp(cp, "COM4", 4) ) {
        if ( (portbase = *(combiosp + (portnum = 3))) == 0 )
            goto noport;

        if ( portirq == 0 ) portirq = 3;
    }
    else {
        syslog(LOG_ERR, "Invalid COM device name %s.\n", cp);

        return 0;
    }

    return 1;

noport:
    syslog(LOG_ERR, "COM device %s not available.\n", cp);

    return -1;
}

/*
 * setspeed - Set the speed.
 */
int setspeed(arg)
char *arg;
{
    char *ptr;
    long spd;

    spd = (long)strtoul(arg, &ptr, 0);

    if ( ptr == arg || *ptr != 0 || spd == 0 )
        return 0;

    inspeed = spd;

    return 1;
}

/*
 * setportbase - Set the COM port base address.
 */
int setportbase(arg)
char *arg;
{
    char *ptr;
    long port;

    port = strtoul(arg, &ptr, 0);

    if ( ptr == arg || *ptr != 0 || port == 0 )
        return 0;

    portbase = (int)port;

    return 1;
}

/*
 * setportirq - Set the COM port irq.
 */
int setportirq(arg)
char *arg;
{
    char *ptr;
    long irq;

    irq = strtoul(arg, &ptr, 0);

    if ( ptr == arg || *ptr != 0 || irq == 0 )
        return 0;

    portirq = (int)irq;

    return 1;
}

#endif  /* STDIO */

int
main(argc, argv)
int argc;
char **argv;
    {
    int option;
    char *arg;

#ifndef STDIO
    /* Check DOS version. */
    if ( _osminor < 10 && _osmajor < 3 ) {
        /* Abort, DOS >= 3.10 required */
        fatal("DOS version >= 3.10 required");
    }

    /* set up our interrupt stack pointer */
    StktopC = ((uint16)_SP - 4096) & 0xFFFE;

    /* set up "call at exit time" routines */
    atexit(exit_fn1);
#endif

    program_name = "CHAT";
    tzset();

    while (option = OPTION(argc, argv))
        {
        switch (option)
            {
            case 'e':
                ++echo;
                break;

            case 'v':
                ++verbose;
                break;

            case 'V':
                ++Verbose;
                break;

            case 'f':
                if (arg = OPTARG(argc, argv))
                    {
                    chat_file = copy_of(arg);
                    }
                else
                    {
                    usage();
                    }
                break;

            case 't':
                if (arg = OPTARG(argc, argv))
                    {
                    timeout = atoi(arg);
                    }
                else
                    {
                    usage();
                    }
                break;

            case 'r':
                arg = OPTARG (argc, argv);
                if (arg)
                    {
                    if (report_fp != NULL)
                        {
                        fclose (report_fp);
                        }

                    report_file = copy_of(arg);
                    report_fp   = fopen(report_file, "a");

                    if (report_fp != NULL)
                        {
                        if (verbose)
                            {
                            fprintf(report_fp, "Opening \"%s\"...\n",
                                    report_file);
                            }

                        report = 1;
                        }
                    }
                break;

#ifndef STDIO
            case 'p':
                if (arg = OPTARG(argc, argv))
                    {
                    if ( setdevname(arg) < 1 )
                        exit(1);
                    }
                else
                    {
                    usage();
                    }
                break;

            case 'b':
                if (arg = OPTARG(argc, argv))
                    {
                    if ( setportbase(arg) < 1 )
                        usage();
                    }
                else
                    {
                    usage();
                    }
                break;

            case 'i':
                if (arg = OPTARG(argc, argv))
                    {
                    if ( setportirq(arg) < 1 )
                        usage();
                    }
                else
                    {
                    usage();
                    }
                break;

            case 's':
                if (arg = OPTARG(argc, argv))
                    {
                    if ( setspeed(arg) < 1 )
                        usage();
                    }
                else
                    {
                    usage();
                    }
                break;
#endif  /* STDIO */

            default:
                usage();
                break;
            }
      }
/*
 * Default the report file to the stderr location
 */
    if (report_fp == NULL)
        {
        report_fp = stderr;
        }

    init();

    if (chat_file != NULL)
        {
        arg = ARG(argc, argv);

        if (arg != NULL)
            {
            usage();
            }
        else
            {
            do_file(chat_file);
            }
        }
    else
        {
        while (arg = ARG(argc, argv))
            {
            chat_expect(unquote_arg(arg));

            if (arg = ARG(argc, argv))
                {
                chat_send(unquote_arg(arg));
                }
            }
        }
/*
 * Allow the last of the report string to be gathered before we terminate.
 */
    while (report_gathering)
        {
        int c;
        c = get_char();

        if (!iscntrl(c))
            {
            int rep_len = strlen(report_buffer);
            report_buffer[rep_len]     = c;
            report_buffer[rep_len + 1] = '\0';
            }
        else
            {
            report_gathering = 0;
            fprintf(report_fp, "chat:  %s\n", report_buffer);
            }
        }

    if ( comopen >= 0 ) {
        asy_stop(comopen, 1);
        comopen = -1;
    }

    terminate(0);
    }

/*
 * Remove surrounding quotes (" and ') from command line arguments.
 */
char *unquote_arg(arg)
char *arg;
    {
    char *sp, quote;

    sp = arg;

    while (*sp != '\0' && (*sp == ' ' || *sp == '\t'))
        ++sp;

    arg = sp;

    if (*sp == '"' || *sp == '\'')
        {
        quote = *sp++;
        arg = sp;

        while (*sp != quote)
            {
            if (*sp == '\0')
                {
                syslog(LOG_ERR, "unterminated quote in argument\n");
                terminate(1);
                }

            if (*sp++ == '\\')
                {
                if (*sp != '\0')
                    {
                    ++sp;
                    }
                }
            }

        if (*sp != '\0')
            {
            *sp = '\0';
            }
        }

    return arg;
    }

/*
 *  Process a chat script when read from a file.
 */
void do_file(chat_file)
char *chat_file;
    {
    int linect, len, sendflg;
    char *sp, *arg, quote;
    char buf [STR_LEN];
    FILE *cfp;

    cfp = fopen(chat_file, "r");

    if (cfp == NULL)
        {
        syslog(LOG_ERR, "%s -- open failed: %s", chat_file, strerror(errno));
        terminate(1);
        }

    linect = 0;
    sendflg = 0;

    while (fgets(buf, STR_LEN, cfp) != NULL)
        {
        sp = strchr(buf, '\n');

        if (sp)
            {
            *sp = '\0';
            }

        linect++;
        sp = buf;

        while (*sp != '\0')
            {
            if (*sp == ' ' || *sp == '\t')
                {
                ++sp;
                continue;
                }

            if (*sp == '"' || *sp == '\'')
                {
                quote = *sp++;
                arg = sp;

                while (*sp != quote)
                    {
                    if (*sp == '\0')
                        {
                        syslog(LOG_ERR, "unterminated quote (line %d)\n",
                              linect);
                        terminate(1);
                        }

                    if (*sp++ == '\\')
                        {
                        if (*sp != '\0')
                            {
                            ++sp;
                            }
                        }
                    }
                }
            else
                {
                arg = sp;

                while (*sp != '\0' && *sp != ' ' && *sp != '\t')
                    {
                    ++sp;
                    }
                }

            if (*sp != '\0')
                {
                *sp++ = '\0';
                }

            if (sendflg)
                {
                chat_send(arg);
                }
            else
                {
                chat_expect(arg);
                }

            sendflg = !sendflg;
            }
        }

    fclose(cfp);
    }

/*
 *      We got an error parsing the command line.
 */
void usage()
    {
    fprintf(stderr, "\
Usage: %s [options] {-f chat-file | chat-script}\n",
           program_name);
    fprintf(stderr, "\
Options: [-e] [-v] [-V] [-t timeout] [-r report-file]\n");
#ifndef STDIO
    fprintf(stderr, "\
         [-p COMn] [-b addr] [-i irq] [-s baud]\n");
#endif  /* STDIO */
    exit(1);
    }

char line[256];
char *p;

void logf(str)
const char *str;
    {
    p = line + strlen(line);
    strcat(p, str);

    if (str[strlen(str)-1] == '\n')
        {
        syslog(LOG_INFO, "%s", line);
        line[0] = 0;
        }
    }

void logflush()
    {
    if (line[0] != 0)
        {
        syslog(LOG_INFO, "%s", line);
        line[0] = 0;
        }
    }

/*
 *      Terminate with an error.
 */
void die()
    {
    terminate(1);
    }

/*
 *      Print an error message and terminate.
 */

void fatal(msg)
const char *msg;
    {
    syslog(LOG_ERR, "%s\n", msg);
    terminate(2);
    }

/*
 *      Print an error message along with the system error message and
 *      terminate.
 */

void sysfatal(msg)
const char *msg;
    {
    syslog(LOG_ERR, "%s: %s", msg, strerror(errno));
    terminate(2);
    }

SIGTYPE sigint(signo)
int signo;
    {
    fatal("received SIGINT, aborting");
    }

void init()
    {
    signal(SIGINT, sigint);
    set_tty_parameters();
    }

void set_tty_parameters()
    {
#ifndef STDIO
    /* establish COM handler */
    if ( portnum == -1 ) portnum = 0;
    if ( portbase == 0 ) portbase = 0x3F8;
    if ( portirq == 0 ) portirq = 4;
    if ( inspeed ) baud_rate = inspeed;
    if ( (comopen = asy_init(portnum, portbase, portirq, 1024, baud_rate, 1, 1, 0)) < 0 )
        {
        fatal("COM initialization error");
        }

     /* set DTR & RTS high */
     if ( asy_ioctl(comopen, PARAM_UP, 0, (int32)0) < 0 )
        {
        fatal("COM ioctl error");
        }

    /* install timer tick handler */
    orig_08_vector = getvect(0x08);
    set08handler(orig_08_vector, everytick);
    setvect(0x08, int08handler);

    if ( verbose > 1 )
        asy_info(comopen);
#endif
    }

void break_sequence()
    {
    }

void terminate(status)
int status;
    {
    echo_stderr(-1);

    if (report_file != (char *)0 && report_fp != (FILE *)NULL)
        {
        if (verbose)
            {
            fprintf(report_fp, "Closing \"%s\".\n", report_file);
            }

        fclose(report_fp);
        report_fp = (FILE *)NULL;
        }

    exit(status);
    }

/*
 *      'Clean up' this string.
 */
char *clean(s, sending)
register char *s;
int sending;
    {
    char temp[STR_LEN], cur_chr;
    register char *s1;
    int add_return = sending;
#define isoctal(chr) (((chr) >= '0') && ((chr) <= '7'))

    s1 = temp;

    while (*s)
        {
        cur_chr = *s++;

        if (cur_chr == '^')
            {
            cur_chr = *s++;

            if (cur_chr == '\0')
                {
                *s1++ = '^';
                break;
                }

            cur_chr &= 0x1F;

            if (cur_chr != 0)
                {
                *s1++ = cur_chr;
                }

            continue;
            }

        if (cur_chr != '\\')
            {
            *s1++ = cur_chr;
            continue;
            }

        cur_chr = *s++;

        if (cur_chr == '\0')
            {
            if (sending)
                {
                *s1++ = '\\';
                *s1++ = '\\';
                }
            break;
            }

        switch (cur_chr)
            {
        case 'b':
            *s1++ = '\b';
            break;

        case 'c':
            if (sending && *s == '\0')
                {
                add_return = 0;
                }
            else
                {
                *s1++ = cur_chr;
                }
            break;

        case '\\':
        case 'K':
        case 'p':
        case 'd':
            if (sending)
                {
                *s1++ = '\\';
                }

            *s1++ = cur_chr;
            break;

        case 'q':
            quiet = ! quiet;
            break;

        case 'r':
            *s1++ = '\r';
            break;

        case 'n':
            *s1++ = '\n';
            break;

        case 's':
            *s1++ = ' ';
            break;

        case 't':
            *s1++ = '\t';
            break;

        case 'N':
            if (sending)
                {
                *s1++ = '\\';
                *s1++ = '\0';
                }
            else
                {
                *s1++ = 'N';
                }
            break;

        default:
            if (isoctal (cur_chr))
                {
                cur_chr &= 0x07;

                if (isoctal (*s))
                    {
                    cur_chr <<= 3;
                    cur_chr |= *s++ - '0';

                    if (isoctal (*s))
                        {
                        cur_chr <<= 3;
                        cur_chr |= *s++ - '0';
                        }
                    }

                if (cur_chr != 0 || sending)
                    {
                    if (sending && (cur_chr == '\\' || cur_chr == 0))
                        {
                        *s1++ = '\\';
                        }

                    *s1++ = cur_chr;
                    }
                break;
                }

            if (sending)
                {
                *s1++ = '\\';
                }
            *s1++ = cur_chr;
            break;
            }
        }

    if (add_return)
        {
        *s1++ = '\r';
        }

    *s1++ = '\0'; /* guarantee closure */
    *s1++ = '\0'; /* terminate the string */

    return dup_mem (temp, (size_t) (s1 - temp)); /* may have embedded nuls */
    }

/*
 * A modified version of 'strtok'. This version skips \ sequences.
 */

char *expect_strtok(s, term)
char *s, *term;
    {
    static  char *str   = "";
    int     escape_flag = 0;
    char   *result;
/*
 * If a string was specified then do initial processing.
 */
    if (s)
        {
        str = s;
        }
/*
 * If this is the escape flag then reset it and ignore the character.
 */
    if (*str)
        {
        result = str;
        }
    else
        {
        result = (char *) 0;
        }

    while (*str)
        {
        if (escape_flag)
            {
            escape_flag = 0;
            ++str;
            continue;
            }

        if (*str == '\\')
            {
            ++str;
            escape_flag = 1;
            continue;
            }
/*
 * If this is not in the termination string, continue.
 */
        if (strchr (term, *str) == (char *) 0)
            {
            ++str;
            continue;
            }
/*
 * This is the terminator. Mark the end of the string and stop.
 */
        *str++ = '\0';
        break;
        }

    return (result);
    }

/*
 * Process the expect string
 */

void chat_expect(s)
char *s;
    {
    char *expect;
    char *reply;

    if (strcmp(s, "ABORT") == 0)
        {
        ++abort_next;
        return;
        }

    if (strcmp(s, "REPORT") == 0)
        {
        ++report_next;
        return;
        }

    if (strcmp(s, "TIMEOUT") == 0)
        {
        ++timeout_next;
        return;
        }

    if (strcmp(s, "ECHO") == 0)
        {
        ++echo_next;
        return;
        }
/*
 * Fetch the expect and reply string.
 */
    for (;;)
        {
        expect = expect_strtok(s, "-");
        s      = (char *) 0;

        if (expect == (char *) 0 || strcmp(expect, "''") == 0)
            {
            return;
            }

        reply = expect_strtok(s, "-");
/*
 * Handle the expect string. If successful then exit.
 */
        if (get_string(expect))
            {
            return;
            }
/*
 * If there is a sub-reply string then send it. Otherwise any condition
 * is terminal.
 */
        if (reply == (char *)0 || exit_code != 3)
            {
            break;
            }

        chat_send(reply);
        }
/*
 * The expectation did not occur. This is terminal.
 */
    if (fail_reason)
        {
        syslog(LOG_INFO, "Failed (%s)\n", fail_reason);
        }
    else
        {
        syslog(LOG_INFO, "Failed\n");
        }

    terminate(exit_code);
    }

/*
 * Translate the input character to the appropriate string for printing
 * the data.
 */

char *character(c)
int c;
    {
    static char string[10];
    char *meta;

    meta = (c & 0x80) ? "M-" : "";
    c &= 0x7F;

    if (c < 32)
        {
        sprintf(string, "%s^%c", meta, (int)c + '@');
        }
    else
        {
        if (c == 127)
            {
            sprintf(string, "%s^?", meta);
            }
        else
            {
            sprintf(string, "%s%c", meta, c);
            }
        }

    return (string);
    }

/*
 *  process the reply string
 */
void chat_send(s)
register char *s;
    {
    if (echo_next)
        {
        echo_next = 0;
        echo = (strcmp(s, "ON") == 0);
        return;
        }

    if (abort_next)
        {
        char *s1;

        abort_next = 0;

        if (n_aborts >= MAX_ABORTS)
            {
            fatal("Too many ABORT strings");
            }

        s1 = clean(s, 0);

        if (strlen(s1) > strlen(s)
            || strlen(s1) + 1 > sizeof(fail_buffer))
            {
            syslog(LOG_WARNING, "Illegal or too-long ABORT string ('%s')\n", s);
            die();
            }

        abort_string[n_aborts++] = s1;

        if (verbose)
            {
            logf("abort on (");

            for (s1 = s; *s1; ++s1)
                {
                logf(character(*s1));
                }

            logf(")\n");
            }
        return;
        }

    if (report_next)
        {
        char *s1;

        report_next = 0;
        if (n_reports >= MAX_REPORTS)
            {
            fatal("Too many REPORT strings");
            }

        s1 = clean(s, 0);

        if (strlen(s1) > strlen(s) || strlen(s1) > sizeof fail_buffer - 1)
            {
            syslog(LOG_WARNING, "Illegal or too-long REPORT string ('%s')\n", s);
            die();
            }

        report_string[n_reports++] = s1;

        if (verbose)
            {
            logf("report (");
            s1 = s;
            while (*s1)
                {
                logf(character(*s1));
                ++s1;
                }
            logf(")\n");
            }
        return;
        }

    if (timeout_next)
        {
        timeout_next = 0;
        timeout = atoi(s);

        if (timeout <= 0)
            {
            timeout = DEFAULT_CHAT_TIMEOUT;
            }

        if (verbose)
            {
            syslog(LOG_INFO, "timeout set to %d seconds\n", timeout);
            }

        return;
        }

    if (strcmp(s, "EOT") == 0)
        {
        s = "^D\\c";
        }
    else
        {
        if (strcmp(s, "BREAK") == 0)
            {
            s = "\\K\\c";
            }
        }

    if (!put_string(s))
        {
        syslog(LOG_INFO, "Failed\n");
        terminate(1);
        }
    }

int get_char()
    {
    int status;
    char c;

#ifdef STDIO
    status = read(0, &c, 1);
#else
    status = read_serial(comopen, &c, 1);
#endif

    switch (status)
        {
    case 1:
        return ((int)c & 0x7F);

    default:
        syslog(LOG_WARNING, "warning: read() on stdin returned %d\n",
               status);

    case -1:
        return (-1);
        }
    }

int put_char(c)
int c;
    {
    int status;
    char ch = c;

    delay(10);  /* inter-character typing delay (?) */

#ifdef STDIO
    status = write(1, &ch, 1);
#else
    status = write_serial(comopen, &ch, 1);
#endif

    switch (status)
        {
    case 1:
        return (0);

    default:
        syslog(LOG_WARNING, "warning: write() on stdout returned %d\n",
               status);

    case -1:
        return (-1);
        }
    }

int write_char(c)
int c;
    {
    if (put_char(c) < 0)
        {
        extern int errno;

        if (verbose)
            {
            if (errno == EINTR)
                {
                syslog(LOG_INFO, " -- write timed out\n");
                }
            else
                {
                syslog(LOG_INFO, " -- write failed: %s", strerror(errno));
                }
            }

        return (0);
        }

    return (1);
    }

int put_string(s)
register char *s;
    {
    s = clean(s, 1);

    if (verbose)
        {
        logf("send (");

        if (quiet)
            {
            logf("??????");
            }
        else
            {
            register char *s1 = s;

            for (s1 = s; *s1; ++s1)
                {
                logf(character(*s1));
                }
            }

        logf(")\n");
        }

    while (*s)
        {
        register char c = *s++;

        if (c != '\\')
            {
            if (!write_char(c))
                {
                return 0;
                }

            continue;
            }

        c = *s++;
        switch (c)
            {
        case 'd':
            sleep(1);
            break;

        case 'K':
            break_sequence();
            break;

        case 'p':
            delay(10);  /* 1/100th of a second (arg is microseconds) */
            break;

        default:
            if (!write_char(c))
                return 0;

            break;
            }
        }

    return (1);
    }

/*
 *      Echo a character to stderr.
 *      When called with -1, a '\n' character is generated when
 *      the cursor is not at the beginning of a line.
 */
void echo_stderr(n)
int n;
    {
        static int need_lf;
        char *s;

        switch (n)
            {
        case '\r':              /* ignore '\r' */
            break;

        case -1:
            if (need_lf == 0)
                break;
            /* fall through */
        case '\n':
            fprintf(stderr, "\n");
            need_lf = 0;
            break;

        default:
            s = character(n);
            fprintf(stderr, s);
            need_lf = 1;
            break;
            }
    }

/*
 *      'Wait for' this string to appear on this file descriptor.
 */
int get_string(string)
register char *string;
    {
    char temp[STR_LEN];
    int c, printed = 0, len, minlen;
    register char *s = temp, *end = s + STR_LEN;

    fail_reason = (char *)0;
    string = clean(string, 0);
    len = strlen(string);
    minlen = (len > sizeof(fail_buffer)? len: sizeof(fail_buffer)) - 1;

    if (verbose)
        {
        register char *s1;

        logf("expect (");

        for (s1 = string; *s1; ++s1)
            {
            logf(character(*s1));
            }

        logf(")\n");
        }

    if (len > STR_LEN)
        {
        syslog(LOG_INFO, "expect string is too long\n");
        exit_code = 1;
        return 0;
        }

    if (len == 0)
        {
        if (verbose)
            {
            syslog(LOG_INFO, "got it\n");
            }

        return (1);
        }

    while ( (c = get_char()) >= 0)
        {
        int n, abort_len, report_len;

        if (echo)
            {
            echo_stderr(c);
            }

        if (verbose)
            {
            if (c == '\n')
                {
                logf("\n");
                }
            else
                {
                logf(character(c));
                }
            }

        if (Verbose) {
           if (c == '\n')
               fputc('\n', stderr);
           else if (c != '\r')
               fprintf(stderr, "%s", character(c));
        }

        *s++ = c;

        if (!report_gathering)
            {
            for (n = 0; n < n_reports; ++n)
                {
                if ((report_string[n] != (char*) NULL) &&
                    s - temp >= (report_len = strlen(report_string[n])) &&
                    strncmp(s - report_len, report_string[n], report_len) == 0)
                    {
                    time_t time_now   = time ((time_t*) NULL);
                    struct tm* tm_now = localtime (&time_now);

                    strftime (report_buffer, 20, "%b %d %H:%M:%S ", tm_now);
                    strcat (report_buffer, report_string[n]);

                    report_string[n] = (char *) NULL;
                    report_gathering = 1;
                    break;
                    }
                }
            }
        else
            {
            if (!iscntrl(c))
                {
                int rep_len = strlen(report_buffer);
                report_buffer[rep_len]     = c;
                report_buffer[rep_len + 1] = '\0';
                }
            else
                {
                report_gathering = 0;
                fprintf(report_fp, "chat: %s\n", report_buffer);
                }
            }

        if (s - temp >= len &&
            c == string[len - 1] &&
            strncmp(s - len, string, len) == 0)
            {
            if (verbose)
                {
                logf(" -- got it\n");
                }

            return (1);
            }

        for (n = 0; n < n_aborts; ++n)
            {
            if (s - temp >= (abort_len = strlen(abort_string[n])) &&
                strncmp(s - abort_len, abort_string[n], abort_len) == 0)
                {
                if (verbose)
                    {
                    logf(" -- failed\n");
                    }
                else if (Verbose || echo)
                    {
                    echo_stderr('\n');
                    }

                exit_code = n + 4;
                strcpy(fail_reason = fail_buffer, abort_string[n]);
                return (0);
                }
            }

        if (s >= end)
            {
            strncpy(temp, s - minlen, minlen);
            s = temp + minlen;
            }
        }

    if (verbose && printed)
        {
        if (errno == EINTR)
            {
            logf(" -- read timed out\n");
            }
        else
            {
            logflush();
            syslog(LOG_INFO, " -- read failed: %s", strerror(errno));
            }
        }

    exit_code = 3;
    return (0);
    }

int syslog(int logid, const char *fmt, ...)
    {
    unsigned wlen;
    va_list marker;

    va_start(marker, fmt);
#ifndef STDIO
    wlen = vfprintf((logid == LOG_ERR ||
                     logid == LOG_WARNING ||
                     logid == LOG_NOTICE    ) ? stderr : stdout,
                    fmt, marker);
#else
    wlen = vfprintf(stderr, fmt, marker);
#endif
    va_end(marker);

    return wlen;
    }

#ifndef STDIO
#endif

