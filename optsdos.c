/*
 * options.c - handles option processing for PPP.
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

/* DOS port by ALM, tonilop@redestb.es */

#include <sys/types.h>
#include "types.h"
#include <string.h>
#include <stdlib.h>
#include <dir.h>
#include <fcntl.h>
#include <dos.h>
#include <errno.h>
#include <ctype.h>
#include <io.h>

#include "pppd.h"
#include "pathname.h"
#include "patchlev.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "upap.h"

#include "syslog.h"

#ifdef IPX_CHANGE
int     ip_enabled = 1;         /* Enable IPCP and IP protocol */
int     ipx_enabled = 1;        /* Enable IPXCP and IPX protocol */
#endif /* IPX_CHANGE */

int     debug = 0;              /* Debug flag */
int     kdebugflag = 0;         /* Tell kernel to print debug messages */
int     crtscts = 0;            /* Use hardware flow control */
int     modem = 1;              /* Use modem control lines */
long    inspeed = 0;            /* Input/Output speed requested */
u_int32_t netmask = 0;          /* IP netmask to set on interface */
char    user[MAXNAMELEN];       /* Username for PAP */
char    passwd[MAXSECRETLEN];   /* Password for PAP */
int     auth_required = 0;      /* Peer is required to authenticate */
int     lcp_echo_interval = 0;  /* Interval between LCP echo-requests */
int     lcp_echo_fails = 0;     /* Tolerance to unanswered echo-requests */
char    hostname[MAXNAMELEN];   /* Our hostname */
char    our_name[MAXNAMELEN];   /* Our name for authentication purposes */
char    remote_name[MAXNAMELEN]; /* Peer's name for authentication */
int     portirq = 0;            /* COM port irq */
int     portbase = 0;           /* COM port base */
int     portnum = -1;           /* COM port number */
int     pktint = 0;             /* packet driver irq number */
char    *connector = NULL;      /* Script to establish physical link */

/*
 * External funcs
 */
char *ip_ntoa(u_int32_t ipaddr);
u_int32_t inet_addr(const char *s);
int bad_ip_adrs(u_int32_t addr);
int getword(int, char *, int *, const char *);

/*
 * Prototypes
 */
#ifdef DEBUGMAIN
static int setdebug(char **);
static int setkdebug(char **);
#endif  /* DEBUGMAIN */
static int noopt(char **);
static int setipaddr(char *arg);

#ifdef ALLOWVJ
static int setnovj(void);
static int setnovjccomp(void);
static int setvjslots(char **);
#endif  /* ALLOWVJ */

static int nopap(char **);
static int setupapfile(char **);

#ifdef ALLOWCHAP
static int nochap(void);
static int reqchap(void);
#endif  /* ALLOWCHAP */

static int setspeed(char *);
static int noaccomp(char **);
static int noasyncmap(char **);
static int noipaddr(char **);
static int nomagicnumber(char **);
static int setasyncmap(char **);
static int setescape(char **);
static int setmru(char **);
static int setmtu(char **);
static int nomru(char **);
static int nopcomp(char **);
static int setconnector(char **);
static int setdomain(char **);
static int setnetmask(char **);
static int setcrtscts(char **);
static int setnocrtscts(char **);
static int setxonxoff(char **);
static int setmodem(char **);
static int setlocal(char **);

#ifdef ALLOWCHAP
static int setname(char **);
#endif  /* ALLOWCHAP */

static int setuser(char **);
static int setpasswd(char **);
static int setremote(char **);
static int readfile(char **);
static int sethostname(char **);
static int setlcptimeout(char **);
static int setlcpterm(char **);
static int setlcpconf(char **);
static int setlcpfails(char **);
static int setipcptimeout(char **);
static int setipcpterm(char **);
static int setipcpconf(char **);
static int setipcpfails(char **);
static int setpaptimeout(char **);
static int setpapreqs(char **);
static int setpapreqtime(char **);

#ifdef ALLOWCHAP
static int setchaptimeout(char **);
static int setchapchal(char **);
static int setchapintv(char **);
#endif  /* ALLOWCHAP */

static int setipcpaccl(char **);
static int setipcpaccr(char **);
static int setlcpechointv(char **);
static int setlcpechofails(char **);

#ifdef ALLOWCCP
static int setbsdcomp(char **);
static int setnobsdcomp(void);
static int setpred1comp(char **);
static int setnopred1comp(void);
#endif  /* ALLOWCCP */

#ifdef IPX_CHANGE
static int setipproto(void);
static int resetipproto(void);
static int setipxproto(void);
static int resetipxproto(void);
static int setipxanet(void);
static int setipxalcl(void);
static int setipxarmt(void);
static int setipxnetwork(char **);
static int setipxnode(char **);
static int setipxrouter(char **);
static int setipxname(char **);
static int setipxcptimeout(char **);
static int setipxcpterm(char **);
static int setipxcpconf(char **);
static int setipxcpfails(char **);
#endif /* IPX_CHANGE */

#ifdef USE_MS_DNS
static int setdnsaddr(char **);
#endif

static int setnamsrv(char **);

static int setportbase(char **argv);
static int setportirq(char **argv);
static int setpktirq(char **argv);
static int setdevname(char *cp);
static int number_option(const char *, u_int32_t *, int);
static int int_option(const char *, int *);
static void usage(void);

/*
 * Valid arguments.
 */
static struct cmd {
    char *cmd_name;
    int num_args;
    int (*cmd_func)(char **);
} cmds[] = {
    {"-all", 0, noopt},         /* Don't request/allow any options */
    {"-ac", 0, noaccomp},       /* Disable Address/Control compress */
    {"-am", 0, noasyncmap},     /* Disable asyncmap negotiation */
    {"-as", 1, setasyncmap},    /* set the desired async map */
    {"-ip", 0, noipaddr},       /* Disable IP address negotiation */
    {"-mn", 0, nomagicnumber},  /* Disable magic number negotiation */
    {"-mru", 0, nomru},         /* Disable mru negotiation */
    {"-pc", 0, nopcomp},        /* Disable protocol field compress */
    {"+ua", 1, setupapfile},    /* Get PAP user and password from file */
    {"-pap", 0, nopap},         /* Don't allow UPAP authentication with peer */

#ifdef ALLOWCHAP
    {"+chap", 0, reqchap},      /* Require CHAP authentication from peer */
    {"-chap", 0, nochap},       /* Don't allow CHAP authentication with peer */
#endif  /* ALLOWCHAP */

#ifdef ALLOWVJ
    {"-vj", 0, setnovj},        /* disable VJ compression */
    {"-vjccomp", 0, setnovjccomp}, /* disable VJ connection-ID compression */
    {"vj-max-slots", 1, setvjslots}, /* Set maximum VJ header slots */
#endif  /* ALLOWVJ */

    {"asyncmap", 1, setasyncmap}, /* set the desired async map */
    {"escape", 1, setescape},   /* set chars to escape on transmission */
    {"connect", 1, setconnector}, /* A program to set up a connection */
    {"crtscts", 0, setcrtscts}, /* set h/w flow control */
    {"-crtscts", 0, setnocrtscts}, /* clear h/w flow control */
    {"xonxoff", 0, setxonxoff}, /* set s/w flow control */

#ifdef DEBUGMAIN
    {"-d", 0, setdebug},        /* Increase debugging level */
    {"debug", 0, setdebug},     /* Increase debugging level */
    {"kdebug", 1, setkdebug},   /* Enable kernel-level debugging */
#endif  /* DEBUGMAIN */
    {"domain", 1, setdomain},   /* Add given domain name to hostname*/
    {"mru", 1, setmru},         /* Set MRU value for negotiation */
    {"mtu", 1, setmtu},         /* Set our MTU */
    {"netmask", 1, setnetmask}, /* set netmask */
    {"modem", 0, setmodem},     /* Use modem control lines */
    {"local", 0, setlocal},     /* Don't use modem control lines */

#ifdef ALLOWCHAP
    {"name", 1, setname},       /* Set local name for authentication */
#endif  /* ALLOWCHAP */

    {"user", 1, setuser},       /* Set username for PAP auth with peer */
    {"passwd", 1, setpasswd},   /* Set password for PAP auth with peer */
    {"hostname", 1, sethostname}, /* Set hostname for auth. */
    {"remotename", 1, setremote}, /* Set remote name for authentication */
    {"file", 1, readfile},      /* Take options from a file */
    {"lcp-echo-failure", 1, setlcpechofails}, /* consecutive echo failures */
    {"lcp-echo-interval", 1, setlcpechointv}, /* time for lcp echo events */
    {"lcp-restart", 1, setlcptimeout}, /* Set timeout for LCP */
    {"lcp-max-terminate", 1, setlcpterm}, /* Set max #xmits for term-reqs */
    {"lcp-max-configure", 1, setlcpconf}, /* Set max #xmits for conf-reqs */
    {"lcp-max-failure", 1, setlcpfails}, /* Set max #conf-naks for LCP */
    {"ipcp-restart", 1, setipcptimeout}, /* Set timeout for IPCP */
    {"ipcp-max-terminate", 1, setipcpterm}, /* Set max #xmits for term-reqs */
    {"ipcp-max-configure", 1, setipcpconf}, /* Set max #xmits for conf-reqs */
    {"ipcp-max-failure", 1, setipcpfails}, /* Set max #conf-naks for IPCP */
    {"pap-restart", 1, setpaptimeout},  /* Set retransmit timeout for PAP */
    {"pap-max-authreq", 1, setpapreqs}, /* Set max #xmits for auth-reqs */
    {"pap-timeout", 1, setpapreqtime},  /* Set time limit for peer PAP auth. */

#ifdef ALLOWCHAP
    {"chap-restart", 1, setchaptimeout}, /* Set timeout for CHAP */
    {"chap-max-challenge", 1, setchapchal}, /* Set max #xmits for challenge */
    {"chap-interval", 1, setchapintv}, /* Set interval for rechallenge */
#endif  /* ALLOWCHAP */

    {"ipcp-accept-local", 0, setipcpaccl}, /* Accept peer's address for us */
    {"ipcp-accept-remote", 0, setipcpaccr}, /* Accept peer's address for it */

#ifdef ALLOWCCP
    {"bsdcomp", 1, setbsdcomp},         /* request BSD-Compress */
    {"-bsdcomp", 0, setnobsdcomp},      /* don't allow BSD-Compress */
    {"pred1comp", 1, setpred1comp},     /* request Predictor-1 */
    {"-pred1comp", 0, setnopred1comp},  /* don't allow Predictor-1 */
#endif  /* ALLOWCCP */

#ifdef IPX_CHANGE
    {"ipx-network",          1, setipxnetwork}, /* IPX network number */
    {"ipxcp-accept-network", 0, setipxanet},    /* Accept peer netowrk */
    {"ipx-node",             1, setipxnode},    /* IPX node number */
    {"ipxcp-accept-local",   0, setipxalcl},    /* Accept our address */
    {"ipxcp-accept-remote",  0, setipxarmt},    /* Accept peer's address */
    {"ipx-routing",          1, setipxrouter},  /* IPX routing proto number */
    {"ipx-router-name",      1, setipxname},    /* IPX router name */
    {"ipxcp-restart",        1, setipxcptimeout}, /* Set timeout for IPXCP */
    {"ipxcp-max-terminate",  1, setipxcpterm},  /* max #xmits for term-reqs */
    {"ipxcp-max-configure",  1, setipxcpconf},  /* max #xmits for conf-reqs */
    {"ipxcp-max-failure",    1, setipxcpfails}, /* max #conf-naks for IPXCP */

#if 0
    {"ipx-compression", 1, setipxcompression}, /* IPX compression number */
#endif

    {"+ip-protocol",    0, setipproto},         /* Enable IPCP (and IP) */
    {"-ip-protocol",    0, resetipproto},       /* Disable IPCP (and IP) */
    {"+ipx-protocol",   0, setipxproto},        /* Enable IPXCP (and IPX) */
    {"-ipx-protocol",   0, resetipxproto},      /* Disable IPXCP (and IPX) */
#endif /* IPX_CHANGE */

#ifdef USE_MS_DNS
    {"dns-addr", 1, setdnsaddr}, /* DNS address(es) for the peer's use */
#endif

    {"base", 1, setportbase},   /* set COM port base */
    {"irq", 1, setportirq},     /* set COM port irq */
    {"pktvec", 1, setpktirq},   /* set packet driver irq */
    {"namsrv", 1, setnamsrv},   /* set ISP namserver for BOOTP reply */

    {NULL, 0, NULL}
};

/*
 * usage - print out a message telling how to use the program.
 */
static void usage(void)
{
    syslog(LOG_ERR, "Wrong program arguments.\n");
}

/*
 * parse_args - parse a string of arguments, from the command
 * line or from a file.
 */
int parse_args(int argc, char **argv)
{
    char *arg;
    struct cmd *cmdp;
    int ret;

    while ( argc > 0 ) {
        arg = *argv++;
        --argc;

        /*
         * First see if it's a command.
         */
        for ( cmdp = cmds ; cmdp->cmd_name ; cmdp++ )
            if ( !strcmp(arg, cmdp->cmd_name) )
                break;

        if ( cmdp->cmd_name != NULL ) {
            if ( argc < cmdp->num_args ) {
                syslog(LOG_ERR, "Too few parameters for command %s\n", arg);

                return 0;
            }

            if ( ! (*cmdp->cmd_func)(argv) ) {
                usage();

                return 0;
            }

            argc -= cmdp->num_args;
            argv += cmdp->num_args;
        }
        else {
            /*
             * Maybe a tty name, speed or IP address?
             */
            if ( (ret = setdevname(arg)) == 0
                && (ret = setspeed(arg)) == 0
                && (ret = setipaddr(arg)) == 0 ) {
                syslog(LOG_ERR, "%s: unrecognized command\n", arg);
                usage();

                return 0;
            }

            if ( ret < 0 )      /* error */
                return 0;
        }
    }

    return 1;
}

/*
 * options_from_file - Read a string of options from a file,
 * and interpret them.
 */
int options_from_file(const char *filename, int musthave)
{
    int fhandle;
    int i, newline, ret;
    struct cmd *cmdp;
    char cmd[MAXWORDLEN];
    char args[MAXARGS][MAXWORDLEN];
    char *argv[MAXARGS];

    fhandle = open(filename, O_RDONLY);

    if ( 0 > fhandle ) {
        if ( ! musthave && errno == ENOENT )
            return 1;

        syslog(LOG_ERR, "Unable to open %s.\n", filename);

        return 0;
    }

    while ( getword(fhandle, cmd, &newline, filename) ) {
        /*
         * First see if it's a command.
         */
        for ( cmdp = cmds ; cmdp->cmd_name ; cmdp++ )
            if ( ! strcmp(cmd, cmdp->cmd_name) )
                break;

        if ( cmdp->cmd_name != NULL ) {
            for ( i = 0 ; i < cmdp->num_args ; ++i ) {
                if ( ! getword(fhandle, args[i], &newline, filename) ) {
                    close(fhandle);
                    syslog(LOG_ERR,
                           "In file %s: too few parameters for command %s.\n",
                           filename, cmd);

                    return 0;
                }

                argv[i] = args[i];
            }

            if ( ! (*cmdp->cmd_func)(argv) ) {
                close(fhandle);
                usage();

                return 0;
            }

        }
        else {
            /*
             * Maybe a tty name, speed or IP address?
             */
            if ( (ret = setdevname(cmd)) == 0
                 && (ret = setspeed(cmd)) == 0
                 && (ret = setipaddr(cmd)) == 0 ) {
                close(fhandle);
                syslog(LOG_ERR, "In file %s: unrecognized command %s.\n",
                       filename, cmd);
                usage();

                return 0;
            }

            if ( ret < 0 ) {    /* error */
                close(fhandle);

                return 0;
            }
        }
    }

    close(fhandle);

    return 1;
}

/*
 * options_from_user - See if the use has a pppdrc.cfg file,
 * and if so, interpret options from it.
 */
int options_from_user(void)
{
    return options_from_file(_PATH_USEROPT, 0);
}

/*
 * options_for_tty - See if an options file exists for the serial
 * device, and if so, interpret options from it.
 */
int options_for_tty(int comnum)
{
    char path[MAXPATH];

    sprintf(path, "%s%d.cfg", _PATH_TTYOPT, comnum);
    return options_from_file(path, 0);
}

/*
 * number_option - parse an unsigned numeric parameter for an option.
 */
static int number_option(const char *str, u_int32_t *valp, int base)
{
    char *ptr;

    *valp = strtoul(str, &ptr, base);

    if ( ptr == str ) {
        syslog(LOG_ERR, "invalid number: %s.\n", str);

        return 0;
    }

    return 1;
}

/*
 * int_option - like number_option, but valp is int *,
 * the base is assumed to be 0, and *valp is not changed
 * if there is an error.
 */
static int int_option(const char *str, int *valp)
{
    u_int32_t v;

    if ( ! number_option(str, &v, 0) )
        return 0;

    *valp = (int)v;

    return 1;
}

/*
 * setportbase - Set the COM port base address.
 */
static int setportbase(char **argv)
{
    char *ptr;
    long port;

    port = strtoul(*argv, &ptr, 0);

    if ( ptr == *argv || *ptr != 0 || port == 0 )
        return 0;

    portbase = (int)port;

    return 1;
}

/*
 * setportirq - Set the COM port irq.
 */
static int setportirq(char **argv)
{
    char *ptr;
    long irq;

    irq = strtoul(*argv, &ptr, 0);

    if ( ptr == *argv || *ptr != 0 || irq == 0 )
        return 0;

    portirq = (int)irq;

    return 1;
}

/*
 * setpktirq - Set the packet driver irq.
 */
static int setpktirq(char **argv)
{
    char *ptr;
    long pkt;

    pkt = strtoul(*argv, &ptr, 0);

    if ( ptr == *argv || *ptr != 0 || pkt == 0 )
        return 0;

    pktint = (int)pkt;

    return 1;
}

/*
 * setspeed - Set the speed.
 */
static int setspeed(char *arg)
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
 * setdevname - Set the device name.
 */
int setdevname(char *cp)
{
    u_short _seg *combiosp = (u_short _seg *)0x0040;

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
        return 0;
    }

    return 1;

noport:
    syslog(LOG_ERR, "Invalid COM device %s.\n", cp);

    return -1;
}

/*
 * setipaddr - Set the IP address
 */
int setipaddr(char *arg)
{
    char *colon;
    u_int32_t local, remote;
    ipcp_options *wo = &ipcp_wantoptions[0];

    /*
     * IP address pair separated by ":".
     */
    if ( (colon = strchr(arg, ':')) == NULL )
        return 0;

    /*
     * If colon first character, then no local addr.
     */
    if ( colon != arg ) {
        *colon = '\0';

        if ( (local = inet_addr(arg)) == -1 ||
             bad_ip_adrs(local) ) {
            syslog(LOG_ERR, "bad local IP %s\n", ip_ntoa(local));

            return -1;
        }

        if ( local != 0 )
            wo->ouraddr = local;

        *colon = ':';
    }

    /*
     * If colon last character, then no remote addr.
     */
    if ( *++colon != '\0' ) {
        if ( (remote = inet_addr(colon)) == -1 ||
             bad_ip_adrs(remote) ) {
            syslog(LOG_ERR, "bad remote IP %s\n", ip_ntoa(remote));

            return -1;
        }

        if ( remote != 0 )
            wo->hisaddr = remote;
    }

    return 1;
}

/*
 * readfile - take commands from a file.
 */
#pragma argsused
static int readfile(char **argv)
{
    return options_from_file(*argv, 1);
}

#ifdef DEBUGMAIN
/*
 * setdebug - Set debug (command line argument).
 */
#pragma argsused
static int setdebug(char** argv)
{
    debug++;

    return 1;
}

/*
 * setkdebug - Set kernel debugging level.
 */
static int setkdebug(char **argv)
{
    return int_option(*argv, &kdebugflag);
}
#endif  /* DEBUGMAIN */

/*
 * noopt - Disable all options.
 */
#pragma argsused
static int noopt(char ** argv)
{
    BZERO((char *)&lcp_wantoptions[0], sizeof(struct lcp_options));
    BZERO((char *)&lcp_allowoptions[0], sizeof(struct lcp_options));
    BZERO((char *)&ipcp_wantoptions[0], sizeof(struct ipcp_options));
    BZERO((char *)&ipcp_allowoptions[0], sizeof(struct ipcp_options));

#ifdef IPX_CHANGE
    BZERO((char *)&ipxcp_wantoptions[0], sizeof(struct ipxcp_options));
    BZERO((char *)&ipxcp_allowoptions[0], sizeof(struct ipxcp_options));
#endif /* IPX_CHANGE */

    return 1;
}

/*
 * noaccomp - Disable Address/Control field compression negotiation.
 */
#pragma argsused
static int noaccomp(char **argv)
{
    lcp_wantoptions[0].neg_accompression = 0;
    lcp_allowoptions[0].neg_accompression = 0;

    return 1;
}

/*
 * noasyncmap - Disable async map negotiation.
 */
#pragma argsused
static int noasyncmap(char **argv)
{
    lcp_wantoptions[0].neg_asyncmap = 0;
    lcp_allowoptions[0].neg_asyncmap = 0;

    return 1;
}

/*
 * noipaddr - Disable IP address negotiation.
 */
#pragma argsused
static int noipaddr(char **argv)
{
    ipcp_wantoptions[0].neg_addr = 0;
    ipcp_allowoptions[0].neg_addr = 0;

    return 1;
}

/*
 * nomagicnumber - Disable magic number negotiation.
 */
#pragma argsused
static int nomagicnumber(char **argv)
{
    lcp_wantoptions[0].neg_magicnumber = 0;
    lcp_allowoptions[0].neg_magicnumber = 0;

    return 1;
}

/*
 * nomru - Disable mru negotiation.
 */
#pragma argsused
static int nomru(char **argv)
{
    lcp_wantoptions[0].neg_mru = 0;
    lcp_allowoptions[0].neg_mru = 0;

    return 1;
}

/*
 * setmru - Set MRU for negotiation.
 */
static int setmru(char **argv)
{
    u_int32_t mru;

    if ( ! number_option(*argv, &mru, 0) )
        return 0;

    lcp_wantoptions[0].mru = mru;
    lcp_wantoptions[0].neg_mru = 1;

    return 1;
}

/*
 * setmtu - Set the largest MTU we'll use.
 */
static int setmtu(char **argv)
{
    u_int32_t mtu;

    if ( ! number_option(*argv, &mtu, 0) )
        return 0;

    if ( mtu < MINMRU || mtu > MAXMRU ) {
        syslog(LOG_ERR, "mtu option value of %ld is too %s\n", mtu,
               (mtu < MINMRU ? "small" : "large") );

        return 0;
    }

    lcp_allowoptions[0].mru = mtu;

    return 1;
}

/*
 * nopcomp - Disable Protocol field compression negotiation.
 */
#pragma argsused
static int nopcomp(char **argv)
{
    lcp_wantoptions[0].neg_pcompression = 0;
    lcp_allowoptions[0].neg_pcompression = 0;

    return 1;
}

/*
 * nopap - Disable PAP authentication with peer.
 */
#pragma argsused
static int nopap(char **argv)
{
    lcp_allowoptions[0].neg_upap = 0;

    return 1;
}

/*
 * setupapfile - specifies UPAP info for authenticating with peer.
 */
static int setupapfile(char **argv)
{
    int ufile;
    int newline;

    lcp_allowoptions[0].neg_upap = 1;

    /* open user info file */
    ufile = open(*argv, O_RDONLY);
    if ( 0 > ufile ) {
        syslog(LOG_ERR, "Unable to open user login data file %s.\n", *argv);

        return 0;
    }

    if ( ! getword(ufile, user, &newline, *argv) ||
         ! getword(ufile, passwd, &newline, *argv) ) {
        syslog(LOG_ERR, "Error reading user login data file %s.\n", *argv);
        close(ufile);

        return 0;
    }

    close(ufile);

    return 1;
}

/*
 * setconnector - Set a program to connect to a serial line
 */
static int setconnector(char **argv)
{
    connector = strdup(*argv);

    if ( connector == NULL )
        novm("connector string");

    return (1);
}

#if ALLOWCHAP
/*
 * nochap - Disable CHAP authentication with peer.
 */
static int nochap(void)
{
    lcp_allowoptions[0].neg_chap = 0;

    return 1;
}

/*
 * reqchap - Require CHAP authentication from peer.
 */
static int reqchap(void)
{
    lcp_wantoptions[0].neg_chap = 1;
    auth_required = 1;

    return 1;
}
#endif  /* ALLOWCHAP */

#ifdef ALLOWVJ
/*
 * setnovj - disable vj compression
 */
static int setnovj(void)
{
    ipcp_wantoptions[0].neg_vj = 0;
    ipcp_allowoptions[0].neg_vj = 0;

    return 1;
}

/*
 * setnovjccomp - disable VJ connection-ID compression
 */
static int setnovjccomp(void)
{
    ipcp_wantoptions[0].cflag = 0;
    ipcp_allowoptions[0].cflag = 0;

    return 1;
}

/*
 * setvjslots - set maximum number of connection slots for VJ compression
 */
static int setvjslots(char **argv)
{
    int value;

    if ( ! int_option(*argv, &value) )
        return 0;

    if ( value < 2 || value > 16 ) {
        syslog(LOG_ERR, "pppd: vj-max-slots value must be between 2 and 16.\n");

        return 0;
    }

    ipcp_wantoptions [0].maxslotindex =
    ipcp_allowoptions[0].maxslotindex = value - 1;

    return 1;
}
#endif  /* ALLOWVJ */

/*
 * setdomain - Set domain name to append to hostname
 */
static int setdomain(char **argv)
{
    if ( **argv != 0 ) {
        if ( **argv != '.' )
            strncat(hostname, ".", MAXNAMELEN - strlen(hostname));

        strncat(hostname, *argv, MAXNAMELEN - strlen(hostname));
    }

    hostname[MAXNAMELEN-1] = 0;

    return 1;
}

/*
 * setasyncmap - add bits to asyncmap (what we request peer to escape).
 */
static int setasyncmap(char **argv)
{
    u_int32_t asyncmap;

    if ( !number_option(*argv, &asyncmap, 16) )
        return 0;

    lcp_wantoptions[0].asyncmap |= asyncmap;
    lcp_wantoptions[0].neg_asyncmap = 1;

    return 1;
}

/*
 * setescape - add chars to the set we escape on transmission.
 */
static int setescape(char **argv)
{
    u_short n;
    int ret;
    char *p, *endp;

    p = *argv;
    ret = 1;

    while ( *p ) {
        n = (u_short)strtoul(p, &endp, 16);

        if ( p == endp ) {
            syslog(LOG_ERR, "invalid hex number: %s.\n", p);

            return 0;
        }

        p = endp;

        if ( 0x20 <= n && n <= 0x3F || n == 0x5E || n > 0xFF ) {
            syslog(LOG_ERR, "can't escape character 0x%x.\n", n);

            ret = 0;
        }
        else
            xmit_accm[0][(u_short)n >> 5] |= 1L << (n & 0x1F);

        while ( *p == ',' || *p == ' ' )
            ++p;
    }

    return ret;
}

/*
 * setipcpaccl - accept peer's idea of our address
 */
#pragma argsused
static int setipcpaccl(char** argv)
{
    ipcp_wantoptions[0].accept_local = 1;

    return 1;
}

/*
 * setipcpaccr - accept peer's idea of its address
 */
#pragma argsused
static int setipcpaccr(char **argv)
{
    ipcp_wantoptions[0].accept_remote = 1;

    return 1;
}

/*
 * setnetmask - set the netmask to be used on the interface.
 */
static int setnetmask(char **argv)
{
    u_int32_t mask;

    if ( (mask = inet_addr(*argv)) == -1 || (netmask & ~mask) != 0 ) {
        syslog(LOG_ERR, "Invalid netmask %s.\n", *argv);

        return 0;
    }

    netmask = mask;

    return 1;
}

#pragma argsused
static int setcrtscts(char **argv)
{
    crtscts = 1;

    return 1;
}

#pragma argsused
static int setnocrtscts(char **argv)
{
    crtscts = -1;

    return 1;
}

#pragma argsused
static int setxonxoff(char **argv)
{
    /* peer must escape ^S and ^Q in transmission */
    lcp_wantoptions[0].asyncmap |= 0x000A0000LU;
    lcp_wantoptions[0].neg_asyncmap = 1;
    /* we must escape ^S and ^Q in transmission */
    xmit_accm[0][0] |= 0x000A0000LU;
    crtscts = 2;

    return 1;
}

#pragma argsused
static int setmodem(char **argv)
{
    modem = 1;

    return 1;
}

#pragma argsused
static int setlocal(char** argv)
{
    modem = 0;

    return 1;
}

static int sethostname(char **argv)
{
    strncpy(hostname, argv[0], MAXNAMELEN);
    hostname[MAXNAMELEN-1] = 0;

    return 1;
}

#ifdef ALLOWCHAP
static int setname(char **argv)
{
    if ( our_name[0] == 0 ) {
        strncpy(our_name, argv[0], MAXNAMELEN);
        our_name[MAXNAMELEN-1] = 0;
    }

    return 1;
}
#endif  /* ALLOWCHAP */

static int setuser(char **argv)
{
    int i;

    strncpy(user,
            argv[0] + (argv[0][0] == '"' || argv[0][0] == '\''),
            MAXNAMELEN);
    user[MAXNAMELEN-1] = 0;

    if ( user[(i = (strlen(user) - 1))] == '"' || user[i] == '\'' )
        user[i] = 0;

    return 1;
}

static int setpasswd(char **argv)
{
    int i;

    strncpy(passwd,
            argv[0] + (argv[0][0] == '"' || argv[0][0] == '\''),
            MAXNAMELEN);
    passwd[MAXNAMELEN-1] = 0;

    if ( passwd[(i = (strlen(user) - 1))] == '"' || passwd[i] == '\'' )
        passwd[i] = 0;

    return 1;
}

static int setremote(char **argv)
{
    strncpy(remote_name, argv[0], MAXNAMELEN);
    remote_name[MAXNAMELEN-1] = 0;

    return 1;
}

/*
 * Functions to set the echo interval for modem-less monitors
 */

static int setlcpechointv(char **argv)
{
    return int_option(*argv, &lcp_echo_interval);
}

static int setlcpechofails(char **argv)
{
    return int_option(*argv, &lcp_echo_fails);
}

/*
 * Functions to set timeouts, max transmits, etc.
 */
static int setlcptimeout(char **argv)
{
    return int_option(*argv, &lcp_fsm[0].timeouttime);
}

static int setlcpterm(char **argv)
{
    return int_option(*argv, &lcp_fsm[0].maxtermtransmits);
}

static int setlcpconf(char **argv)
{
    return int_option(*argv, &lcp_fsm[0].maxconfreqtransmits);
}

static int setlcpfails(char **argv)
{
    return int_option(*argv, &lcp_fsm[0].maxnakloops);
}

static int setipcptimeout(char **argv)
{
    return int_option(*argv, &ipcp_fsm[0].timeouttime);
}

static int setipcpterm(char **argv)
{
    return int_option(*argv, &ipcp_fsm[0].maxtermtransmits);
}

static int setipcpconf(char **argv)
{
    return int_option(*argv, &ipcp_fsm[0].maxconfreqtransmits);
}

static int setipcpfails(char **argv)
{
    return int_option(*argv, &lcp_fsm[0].maxnakloops);
}

static int setpaptimeout(char **argv)
{
    return int_option(*argv, &upap[0].us_timeouttime);
}

static int setpapreqtime(char **argv)
{
    return int_option(*argv, &upap[0].us_reqtimeout);
}

static int setpapreqs(char **argv)
{
    return int_option(*argv, &upap[0].us_maxtransmits);
}

#ifdef ALLOWCHAP
static int setchaptimeout(char **argv)
{
    return int_option(*argv, &chap[0].timeouttime);
}

static int setchapchal(char **argv)
{
    return int_option(*argv, &chap[0].max_transmits);
}

static int setchapintv(char **argv)
{
    return int_option(*argv, &chap[0].chal_interval);
}
#endif  /* ALLOWCHAP */

#ifdef ALLOWCCP
static int setbsdcomp(char **argv)
{
    long rbits, abits;
    char *str, *endp;

    str = *argv;
    abits = rbits = (long)strtoul(str, &endp, 0);

    if ( endp != str && *endp == ',' ) {
        str = endp + 1;
        abits = (long)strtoul(str, &endp, 0);
    }

    if (*endp != 0 || endp == str) {
        syslog(LOG_ERR, "invalid argument format for bsdcomp option.\n");

        return 0;
    }

    if ( rbits != 0 && (rbits < BSD_MIN_BITS || rbits > BSD_MAX_BITS) ||
         abits != 0 && (abits < BSD_MIN_BITS || abits > BSD_MAX_BITS) ) {
        syslog(LOG_ERR, "bsdcomp option values must be 0 or %d .. %d.\n",
               BSD_MIN_BITS, BSD_MAX_BITS);

        return 0;
    }

    if ( rbits > 0 ) {
        ccp_wantoptions[0].bsd_compress = 1;
        ccp_wantoptions[0].bsd_bits = rbits;
    }
    else
        ccp_wantoptions[0].bsd_compress = 0;

    if ( abits > 0 ) {
        ccp_allowoptions[0].bsd_compress = 1;
        ccp_allowoptions[0].bsd_bits = abits;
    }
    else
        ccp_allowoptions[0].bsd_compress = 0;

    return 1;
}

static int setnobsdcomp(void)
{
    ccp_wantoptions[0].bsd_compress = 0;
    ccp_allowoptions[0].bsd_compress = 0;

    return 1;
}

static int setpred1comp(char **argv)
{
    ccp_wantoptions[0].predictor_1 = 1;
    ccp_allowoptions[0].predictor_1 = 1;

    return 1;
}

static int setnopred1comp(void)
{
    ccp_wantoptions[0].predictor_1 = 0;
    ccp_allowoptions[0].predictor_1 = 0;

    return 1;
}
#endif  /* ALLOWCCP */

#ifdef IPX_CHANGE
static int setipxrouter(char **argv)
{
    ipxcp_wantoptions[0].neg_router  = 1;
    ipxcp_allowoptions[0].neg_router = 1;

    return int_option(*argv, &ipxcp_wantoptions[0].router);
}

static int setipxname(char **argv)
{
    char *dest = ipxcp_wantoptions[0].name;
    char *src = *argv;
    int  count;
    char ch;

    ipxcp_wantoptions[0].neg_name  = 1;
    ipxcp_allowoptions[0].neg_name = 1;
    memset(dest, '\0', sizeof (ipxcp_wantoptions[0].name));

    count = 0;

    while ( *src ) {
        ch = *src++;

        if ( ! isalnum (ch) && ch != '_' ) {
            syslog(LOG_ERR, "IPX router name must be alphanumeric or _.\n");

            return 0;
        }

        if ( count >= sizeof(ipxcp_wantoptions[0].name) ) {
            syslog(LOG_ERR, "IPX router name is limited to %d characters.\n",
                   sizeof(ipxcp_wantoptions[0].name) - 1);

            return 0;
        }

        dest[count++] = toupper (ch);
    }

    return 1;
}

static int setipxcptimeout(char **argv)
{
    return int_option(*argv, &ipxcp_fsm[0].timeouttime);
}

static int setipxcpterm (char **argv)
{
    return int_option(*argv, &ipxcp_fsm[0].maxtermtransmits);
}

static int setipxcpconf (char **argv)
{
    return int_option(*argv, &ipxcp_fsm[0].maxconfreqtransmits);
}

static int setipxcpfails(char **argv)
{
    return int_option(*argv, &ipxcp_fsm[0].maxnakloops);
}

static int setipxnetwork(char **argv)
{
    ipxcp_wantoptions[0].neg_nn = 1;

    return int_option(*argv, &ipxcp_wantoptions[0].our_network);
}

static int setipxanet(void)
{
    ipxcp_wantoptions[0].accept_network = 1;
    ipxcp_allowoptions[0].accept_network = 1;

    return 1;
}

static int setipxalcl(void)
{
    ipxcp_wantoptions[0].accept_local = 1;
    ipxcp_allowoptions[0].accept_local = 1;

    return 1;
}

static int setipxarmt(void)
{
    ipxcp_wantoptions[0].accept_remote = 1;
    ipxcp_allowoptions[0].accept_remote = 1;

    return 1;
}

static u_char *setipxnodevalue(u_char *src, u_char *dst)
{
    int indx;
    int item;

    for (;;) {
        if ( !isxdigit(*src) )
            break;

        for ( indx = 0 ; indx < 5 ; ++indx ) {
            dst[indx] <<= 4;
            dst[indx] |= (dst[indx + 1] >> 4) & 0x0F;
        }

        item = toupper (*src) - '0';

        if ( item > 9 )
            item -= 7;

        dst[5] = (dst[5] << 4) | item;
        ++src;
    }

    return src;
}

static int setipxnode(char **argv)
    char **argv;
{
    char *end;

    memset (&ipxcp_wantoptions[0].our_node[0], 0, 6);
    memset (&ipxcp_wantoptions[0].his_node[0], 0, 6);

    end = setipxnodevalue(*argv, &ipxcp_wantoptions[0].our_node[0]);

    if ( *end == ':' )
        end = setipxnodevalue (++end, &ipxcp_wantoptions[0].his_node[0]);

    if ( *end == '\0' ) {
        ipxcp_wantoptions[0].neg_node = 1;

        return 1;
    }

    syslog(LOG_ERR, "invalid argument for ipx-node option.\n");

    return 0;
}

static int setipproto(void)
{
    ip_enabled = 1;     /* Enable IPCP and IP protocol */

    return 1;
}

static int resetipproto(void)
{
    ip_enabled = 0;     /* Disable IPCP and IP protocol */

    return 1;
}

static int setipxproto(void)
{
    ipx_enabled = 1;    /* Enable IPXCP and IPX protocol */

    return 1;
}

static int resetipxproto(void)
{
    ipx_enabled = 0;    /* Disable IPXCP and IPX protocol */

    return 1;
}
#endif /* IPX_CHANGE */

#ifdef USE_MS_DNS
/*
 * setdnsaddr - set the dns address(es)
 */

static int setdnsaddr(char **argv)
{
    u_long dns;

    dns = inet_addr(*argv);

    if ( dns == -1 ) {
        syslog(LOG_ERR, "Invalid DNS Address %s.\n", *argv);

        return 0;
    }

    if ( ipcp_wantoptions[0].dnsaddr[0] == 0 ) {
        ipcp_wantoptions[0].dnsaddr[0]  = dns;
        ipcp_allowoptions[0].dnsaddr[0] = dns;
    }
    else {
        ipcp_wantoptions[0].dnsaddr[1]  = dns;
        ipcp_allowoptions[0].dnsaddr[1] = dns;
    }

    return 1;
}
#endif /* USE_MS_DNS */

/*
 * setnamsrv - set the ISP dns address(es) for BOOTP reply.
 */

static int setnamsrv(char **argv)
{
    u_int32_t dns;

    dns = inet_addr(*argv);

    if ( dns == -1 ) {
        syslog(LOG_ERR, "Invalid DNS Address %s.\n", *argv);

        return 0;
    }

    if ( namsrvaddr[0][0] == 0 )
        namsrvaddr[0][0] = dns;
    else
        namsrvaddr[0][1] = dns;

    return 1;
}

