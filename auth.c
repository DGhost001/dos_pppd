/*
 * auth.c - PPP authentication and phase control.
 *
 * Copyright (c) 1993 The Australian National University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Australian National University.  The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
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
#include <dir.h>
#include <stddef.h>
#include <dos.h>
#include <io.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "syslog.h"

#include "pppd.h"
#include "fsm.h"
#include "lcp.h"
#include "upap.h"
#include "chap.h"
#include "ipcp.h"
#include "ccp.h"
#include "pathname.h"

typedef int FILE;

/* Used for storing a sequence of words.  Usually malloced. */
struct wordlist {
    struct wordlist *next;
    char word[1];
};

/* Bits in scan_authfile return value */
#define NONWILD_SERVER  1
#define NONWILD_CLIENT  2

#define ISWILD(word)    (word[0] == '*' && word[1] == 0)

#define FALSE   0
#define TRUE    1

/* Records which authentication operations haven't completed yet. */
static int auth_pending[NUM_PPP];
static int logged_in;
static struct wordlist *addresses[NUM_PPP];

/* Bits in auth_pending[] */
#define UPAP_WITHPEER   1
#define UPAP_PEER       2
#define CHAP_WITHPEER   4
#define CHAP_PEER       8

/* external funcs */
u_int32_t inet_addr(const char *);
void novm(char *);

/* Prototypes */
static void network_phase(int);
#if 0
static int  login(char *, char *, char **, int *);
#endif

static void logout(void);
static int  null_login(int);
#if 0
static int  get_upap_passwd(void);
static int  have_upap_secret(void);
#endif
#ifdef ALLOW_CHAP
static int  have_chap_secret(char *, char *);
#endif  /* ALLOW_CHAP */
#if 0
static int  scan_authfile(FILE *, char *, char *, char *,
                          struct wordlist **, char *);
static void free_wordlist(struct wordlist *);
#endif

extern char *crypt(char *, char *);

/*
 * An Open on LCP has requested a change from Dead to Establish phase.
 * Do what's necessary to bring the physical layer up.
 */
#pragma argsused
void link_required(int unit)
{
}

/*
 * LCP has terminated the link; go to the Dead phase and take the
 * physical layer down.
 */
#pragma argsused
void link_terminated(int unit)
{
    if (phase == PHASE_DEAD)
        return;

    if (logged_in)
        logout();

    phase = PHASE_DEAD;
    AUTHDEBUG((LOG_NOTICE, "Connection terminated.\n"));
}

/*
 * LCP has gone down; it will either die or try to re-establish.
 */
#pragma argsused
void link_down(int unit)
{
    ipcp_close(0);
#ifdef ALLOW_CCP
    ccp_close(0);
#endif  /* ALLOW_CCP */
    phase = PHASE_TERMINATE;
}

/*
 * The link is established.
 * Proceed to the Dead, Authenticate or Network phase as appropriate.
 */
void link_established(int unit)
{
    int auth;
    lcp_options *wo = &lcp_wantoptions[unit];
    lcp_options *go = &lcp_gotoptions[unit];
    lcp_options *ho = &lcp_hisoptions[unit];

    if (auth_required && !(go->neg_chap || go->neg_upap)) {
        /*
         * We wanted the peer to authenticate itself, and it refused:
         * treat it as though it authenticated with PAP using a username
         * of "" and a password of "".  If that's not OK, boot it out.
         */
        if (!wo->neg_upap || !null_login(unit)) {
            AUTHDEBUG((LOG_WARNING, "peer refused to authenticate\n"));
            lcp_close(unit);
            phase = PHASE_TERMINATE;
            return;
        }
    }

    phase = PHASE_AUTHENTICATE;
    auth = 0;

#ifdef ALLOW_CHAP
    if (go->neg_chap) {
        ChapAuthPeer(unit, our_name, go->chap_mdtype);
        auth |= CHAP_PEER;
    } else
#endif  /* ALLOW_CHAP */

    if (go->neg_upap) {
        upap_authpeer(unit);
        auth |= UPAP_PEER;
    }

#ifdef ALLOW_CHAP
    if (ho->neg_chap) {
        ChapAuthWithPeer(unit, our_name, ho->chap_mdtype);
        auth |= CHAP_WITHPEER;
    } else
#endif  /* ALLOW_CHAP */

    if (ho->neg_upap) {
        upap_authwithpeer(unit, user, passwd);
        auth |= UPAP_WITHPEER;
    }

    auth_pending[unit] = auth;

    if (!auth)
        network_phase(unit);
}

/*
 * Proceed to the network phase.
 */
static void network_phase(int unit)
{
    phase = PHASE_NETWORK;
    ipcp_open(unit);
#ifdef IPX_CHANGE
    ipxcp_open(unit);
#endif /* IPX_CHANGE */
#ifdef ALLOW_CCP
    ccp_open(unit);
#endif  /* ALLOW_CCP */
}

/*
 * The peer has failed to authenticate himself using `protocol'.
 */
#pragma argsused
void auth_peer_fail(int unit, int protocol)
{
    /*
     * Authentication failure: take the link down
     */
    lcp_close(unit);
    phase = PHASE_TERMINATE;
}

/*
 * The peer has been successfully authenticated using `protocol'.
 */
void auth_peer_success(int unit, int protocol)
{
    int bit;

    switch (protocol) {
#ifdef ALLOW_CHAP
    case PPP_CHAP:
        bit = CHAP_PEER;
        break;
#endif  /* ALLOW_CHAP */
    case PPP_PAP:
        bit = UPAP_PEER;
        break;
    default:
        AUTHDEBUG((LOG_WARNING, "auth_peer_success: unknown protocol %x\n", protocol));
        return;
    }

    /*
     * If there is no more authentication still to be done,
     * proceed to the network phase.
     */
    if ((auth_pending[unit] &= ~bit) == 0) {
        phase = PHASE_NETWORK;
        ipcp_open(unit);
#ifdef IPX_CHANGE
        ipxcp_open(unit);
#endif /* IPX_CHANGE */
#ifdef ALLOW_CCP
        ccp_open(unit);
#endif  /* ALLOW_CCP */
    }
}

/*
 * We have failed to authenticate ourselves to the peer using `protocol'.
 */
#pragma argsused
void auth_withpeer_fail(int unit, int protocol)
{
    /*
     * We've failed to authenticate ourselves to our peer.
     * He'll probably take the link down, and there's not much
     * we can do except wait for that.
     */
}

/*
 * We have successfully authenticated ourselves with the peer using `protocol'.
 */
void auth_withpeer_success(int unit, int protocol)
{
    int bit;

    switch (protocol) {
#ifdef ALLOW_CHAP
    case PPP_CHAP:
        bit = CHAP_WITHPEER;
        break;
#endif  /* ALLOW_CHAP */
    case PPP_PAP:
        bit = UPAP_WITHPEER;
        break;
    default:
        AUTHDEBUG((LOG_WARNING, "auth_peer_success: unknown protocol %x\n", protocol));
        bit = 0;
    }

    /*
     * If there is no more authentication still being done,
     * proceed to the network phase.
     */
    if ((auth_pending[unit] &= ~bit) == 0)
        network_phase(unit);
}


/*
 * check_auth_options - called to check authentication options.
 */
void check_auth_options(void)
{
#if 0
    lcp_options *wo = &lcp_wantoptions[0];
    lcp_options *ao = &lcp_allowoptions[0];

    /* Default our_name to hostname, and user to our_name */
    if (our_name[0] == 0 || usehostname)
        strcpy(our_name, hostname);

    if (user[0] == 0)
        strcpy(user, our_name);

    /* If authentication is required, ask peer for CHAP or PAP. */
    if (auth_required && !wo->neg_chap && !wo->neg_upap) {
#ifdef ALLOW_CHAP
        wo->neg_chap = 1;
#endif  /* ALLOW_CHAP */
        wo->neg_upap = 1;
    }

    /*
     * Check whether we have appropriate secrets to use
     * to authenticate ourselves and/or the peer.
     */
    if (ao->neg_upap && passwd[0] == 0 && !get_upap_passwd())
        ao->neg_upap = 0;

    if (wo->neg_upap && !uselogin && !have_upap_secret())
        wo->neg_upap = 0;

#ifdef ALLOW_CHAP
    if (ao->neg_chap && !have_chap_secret(our_name, remote_name))
        ao->neg_chap = 0;

    if (wo->neg_chap && !have_chap_secret(remote_name, our_name))
        wo->neg_chap = 0;
#endif  /* ALLOW_CHAP */

    if (auth_required && !wo->neg_chap && !wo->neg_upap) {
        AUTHDEBUG((LOG_ERR, "pppd: peer authentication required but no authentication files accessible.\n"));
        exit(1);
    }
#endif
}


/*
 * check_passwd - Check the user name and passwd against the PAP secrets
 * file.  If requested, also check against the system password database,
 * and login the user if OK.
 *
 * returns:
 *      UPAP_AUTHNAK: Authentication failed.
 *      UPAP_AUTHACK: Authentication succeeded.
 * In either case, msg points to an appropriate message.
 */
#pragma argsused
int check_passwd(int unit, char *auser, int userlen,
                 char *apasswd, int passwdlen, char **msg, int *msglen)
{
#if 0
    int ret;
    char *filename;
    FILE *f;
    struct wordlist *addrs;
    char passwd[MAXWORDLEN], user[MAXWORDLEN];
    char secret[MAXWORDLEN];
    static int attempts = 0;

    /*
     * Make copies of apasswd and auser, then null-terminate them.
     */
    BCOPY(apasswd, passwd, min(passwdlen, MAXWORDLEN - 1));
    passwd[min(passwdlen, MAXWORDLEN)] = '\0';
    BCOPY(auser, user, min(userlen, MAXWORDLEN - 1));
    user[min(userlen, MAXWORDLEN)] = '\0';

    /*
     * Open the file of upap secrets and scan for a suitable secret
     * for authenticating this user.
     */
    filename = _PATH_UPAPFILE;
    addrs = NULL;
    ret = UPAP_AUTHACK;
    f = fopen(filename, "r");

    if (f == NULL) {
        if (!uselogin) {
            AUTHDEBUG((LOG_ERR, "Can't open PAP password file %s: %m\n", filename));
            ret = UPAP_AUTHNAK;
        }
    }
    else {
        if (scan_authfile(f, user, our_name, secret, &addrs, filename) < 0
            || (secret[0] != 0 && (cryptpap || strcmp(passwd, secret) != 0)
                && strcmp(crypt(passwd, secret), secret) != 0)) {
            AUTHDEBUG((LOG_WARNING, "PAP authentication failure for %s\n", user));
            ret = UPAP_AUTHNAK;
        }

        fclose(f);
    }

    if (uselogin && ret == UPAP_AUTHACK) {
        ret = login(user, passwd, msg, msglen);

        if (ret == UPAP_AUTHNAK) {
            AUTHDEBUG((LOG_WARNING, "PAP login failure for %s\n", user));
        }
    }

    if (ret == UPAP_AUTHNAK) {
        *msg = "Login incorrect";
        *msglen = strlen(*msg);
        /*
         * Frustrate passwd stealer programs.
         * Allow 10 tries, but start backing off after 3 (stolen from login).
         * On 10'th, drop the connection.
         */
        if (attempts++ >= 10) {
            AUTHDEBUG((LOG_WARNING, "%d LOGIN FAILURES ON COM%d, %s\n", comopen+1, devnam, user));
            quit();
        }

        if (attempts > 3)
            sleep((u_int) (attempts - 3) * 5);

        if (addrs != NULL)
            free_wordlist(addrs);

    } else {
        attempts = 0;                   /* Reset count */
        *msg = "Login ok";
        *msglen = strlen(*msg);

        if (addresses[unit] != NULL)
            free_wordlist(addresses[unit]);

        addresses[unit] = addrs;
    }

    return ret;
#else
  return UPAP_AUTHNAK;
#endif
}

/*
 * login - Check the user name and password against the system
 * password database, and login the user if OK.
 *
 * returns:
 *      UPAP_AUTHNAK: Login failed.
 *      UPAP_AUTHACK: Login succeeded.
 * In either case, msg points to an appropriate message.
 */
#if 0
static int login(char *user, char *passwd, char **msg, int *msglen)
{
    return (UPAP_AUTHNAK);
}
#endif

/*
 * logout - Logout the user.
 */
static void logout(void)
{
    logged_in = FALSE;
}


/*
 * null_login - Check if a username of "" and a password of "" are
 * acceptable, and if so, set the list of acceptable IP addresses
 * and return 1.
 */
#pragma argsused
static int null_login(int unit)
{
#if 0
    char *filename;
    FILE *f;
    int i, ret;
    struct wordlist *addrs;
    char secret[MAXWORDLEN];

    /*
     * Open the file of upap secrets and scan for a suitable secret.
     * We don't accept a wildcard client.
     */
    filename = _PATH_UPAPFILE;
    addrs = NULL;
    f = fopen(filename, "r");

    if (f == NULL)
        return 0;

    i = scan_authfile(f, "", our_name, secret, &addrs, filename);
    ret = i >= 0 && (i & NONWILD_CLIENT) != 0 && secret[0] == 0;

    if (ret) {
        if (addresses[unit] != NULL)
            free_wordlist(addresses[unit]);

        addresses[unit] = addrs;
    }

    fclose(f);

    return ret;
#else
    return 0;
#endif
}


/*
 * get_upap_passwd - get a password for authenticating ourselves with
 * our peer using PAP.  Returns 1 on success, 0 if no suitable password
 * could be found.
 */
#if 0
static int get_upap_passwd(void)
{
    char *filename;
    FILE *f;
    char secret[MAXWORDLEN];

    filename = _PATH_UPAPFILE;
    f = fopen(filename, "r");

    if (f == NULL)
        return 0;

    if (scan_authfile(f, user, remote_name, secret, NULL, filename) < 0)
        return 0;

    strncpy(passwd, secret, MAXSECRETLEN);
    passwd[MAXSECRETLEN - 1] = 0;

    return 1;
}
#endif

/*
 * have_upap_secret - check whether we have a PAP file with any
 * secrets that we could possibly use for authenticating the peer.
 */
#if 0
static int have_upap_secret(void)
{
    FILE *f;
    int ret;
    char *filename;

    filename = _PATH_UPAPFILE;
    f = fopen(filename, "r");

    if (f == NULL)
        return 0;

    ret = scan_authfile(f, NULL, our_name, NULL, NULL, filename);
    fclose(f);

    if (ret < 0)
        return 0;

    return 1;
}
#endif

#ifdef ALLOW_CHAP
/*
 * have_chap_secret - check whether we have a CHAP file with a
 * secret that we could possibly use for authenticating `client'
 * on `server'.  Either can be the null string, meaning we don't
 * know the identity yet.
 */
static int have_chap_secret(char *client, char *server)
{
    FILE *f;
    int ret;
    char *filename;

    filename = _PATH_CHAPFILE;
    f = fopen(filename, "r");

    if (f == NULL)
        return 0;

    if (client[0] == 0)
        client = NULL;
    else if (server[0] == 0)
        server = NULL;

    ret = scan_authfile(f, client, server, NULL, NULL, filename);
    fclose(f);

    if (ret < 0)
        return 0;

    return 1;
}


/*
 * get_secret - open the CHAP secret file and return the secret
 * for authenticating the given client on the given server.
 * (We could be either client or server).
 */
int get_secret(int unit, char *client, char *server,
               char *secret, int *secret_len, int save_addrs)
{
    FILE *f;
    int ret, len;
    char *filename;
    struct wordlist *addrs;
    char secbuf[MAXWORDLEN];

    filename = _PATH_CHAPFILE;
    addrs = NULL;
    secbuf[0] = 0;

    f = fopen(filename, "r");

    if (f == NULL) {
        AUTHDEBUG((LOG_ERR, "Can't open chap secret file %s: %m\n", filename));
        return 0;
    }

    ret = scan_authfile(f, client, server, secbuf, &addrs, filename);
    fclose(f);

    if (ret < 0)
        return 0;

    if (save_addrs) {
        if (addresses[unit] != NULL)
            free_wordlist(addresses[unit]);
        addresses[unit] = addrs;
    }

    len = strlen(secbuf);

    if (len > MAXSECRETLEN) {
        AUTHDEBUG((LOG_ERR, "Secret for %s on %s is too long\n", client, server));
        len = MAXSECRETLEN;
    }

    BCOPY(secbuf, secret, len);
    *secret_len = len;

    return 1;
}
#endif  /* ALLOW_CHAP */

/*
 * bad_ip_adrs - return 1 if the IP address is one we don't want
 * to use, such as an address in the loopback net or a multicast address.
 * addr is in network byte order.
 */
int bad_ip_adrs(u_int32_t addr)
{
    addr = ntohl(addr);
    return (addr >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET
            || IN_MULTICAST(addr) || IN_BADCLASS(addr);
}


/*
 * auth_ip_addr - check whether the peer is authorized to use
 * a given IP address.  Returns 1 if authorized, 0 otherwise.
 */
int auth_ip_addr(int unit, u_int32_t addr)
{
    u_int32_t a;
    int accept;
    u_int32_t mask;
    u_char *ptr_word, *ptr_mask;
    struct wordlist *addrs;

    /* don't allow loopback or multicast address */
    if ( bad_ip_adrs(addr) )
        return 0;

    if ( (addrs = addresses[unit]) == NULL )
        return 1;               /* no restriction */

    for ( ; addrs != NULL ; addrs = addrs->next ) {
        /* "-" means no addresses authorized */
        ptr_word = addrs->word;

        if ( strcmp(ptr_word, "-") == 0 )
            break;

        accept = 1;

        if ( *ptr_word == '!' ) {
            accept = 0;
            ++ptr_word;
        }

        ptr_mask = strchr(ptr_word, '/');

        if ( ptr_mask == NULL )
            mask = 0xFFFFFFFFUL;
        else {
            u_short bit_count;
            *ptr_mask = '\0';
            bit_count = (u_short)strtoul(ptr_mask, (char **)0, 10);

            if ( bit_count == 0 || bit_count > 32 ) {
                AUTHDEBUG((LOG_WARNING, "invalid address length %s in auth. address list\n", ptr_mask));
                *ptr_mask = '/';
                continue;
            }

            mask = ~((1UL << (32 - bit_count)) - 1UL);
        }

        a = inet_addr(ptr_word);

        if ( ptr_mask )
            *ptr_mask = '/';

        if ( a == -1L ) {
            AUTHDEBUG((LOG_WARNING, "unknown host %s in auth. address list\n", addrs->word));
        }
        else {
            if ( ((addr ^ a) & mask) == 0 )
                return accept;
        }
    }

    return 0;   /* not in list => can't have it */
}


/*
 * Read a word from a file.
 * Words are delimited by white-space or by quotes (" or ').
 * Quotes, white-space and \ may be escaped with \.
 * \<newline> is ignored.
 */

#define EOF -1

static unsigned goterror = 0;
static int _cunget = EOF;

int _dgetc(int fhandle)
{
    char c;
    unsigned nread;

    if ( goterror )
        return EOF;

    if ( _cunget != EOF ) {
      c = (char)_cunget;
      _cunget = EOF;
    }
    else {
        do {
            nread = read(fhandle, &c, 1);
            goterror = 1 != nread;
            if ( goterror )
                return EOF;

            if ( ! nread )
                return EOF;
        } while ( c == '\r' );
    }

    return (unsigned)c;
}

void _dungetc(char c)
{
    _cunget = (unsigned)c;
}

int _dferror(void)
{
    return goterror;
}

int getword(int fhandle, char *word, int *newlinep, const char *filename)
{
    int c, len, escape;
    int quoted, comment;
    int value, digit, got, n;

#define isoctal(c) ((c) >= '0' && (c) < '8')

    *newlinep = 0;
    len = 0;
    escape = 0;
    comment = 0;

    /*
     * First skip white-space and comments.
     */
    for (;;) {
        c = _dgetc(fhandle);

        if ( c == EOF )
            break;

        /*
         * A newline means the end of a comment; backslash-newline
         * is ignored.  Note that we cannot have escape && comment.
         */
        if ( c == '\n' ) {
            if ( !escape ) {
                *newlinep = 1;
                comment = 0;
            }
            else
                escape = 0;

            continue;
        }

        /*
         * Ignore characters other than newline in a comment.
         */
        if ( comment )
            continue;

        /*
         * If this character is escaped, we have a word start.
         */
        if ( escape )
            break;

        /*
         * If this is the escape character, look at the next character.
         */
        if ( c == '\\' ) {
            escape = 1;
            continue;
        }

        /*
         * If this is the start of a comment, ignore the rest of the line.
         */
        if ( c == '#' ) {
            comment = 1;
            continue;
        }

        /*
         * A non-whitespace character is the start of a word.
         */
        if ( !isspace(c) )
            break;
    }

    /*
     * Save the delimiter for quoted strings.
     */
    if ( ! escape && (c == '"' || c == '\'') ) {
        quoted = c;
        c = _dgetc(fhandle);
    }
    else
        quoted = 0;

    /*
     * Process characters until the end of the word.
     */
    while ( c != EOF ) {
        if ( escape ) {
            /*
             * This character is escaped: backslash-newline is ignored,
             * various other characters indicate particular values
             * as for C backslash-escapes.
             */
            escape = 0;

            if ( c == '\n' ) {
                c = _dgetc(fhandle);
                continue;
            }

            got = 0;

            switch ( c ) {
                case 'a':
                    value = '\a';
                break;

                case 'b':
                    value = '\b';
                break;

                case 'f':
                    value = '\f';
                break;

                case 'n':
                    value = '\n';
                break;

                case 'r':
                    value = '\r';
                break;

                case 's':
                    value = ' ';
                break;

                case 't':
                    value = '\t';
                break;

            default:
                if ( isoctal(c) ) {
                    /*
                     * \ddd octal sequence
                     */
                    value = 0;

                    for ( n = 0 ; n < 3 && isoctal(c) ; ++n ) {
                        value = (value << 3) + (c & 07);
                        c = _dgetc(fhandle);
                    }

                    got = 1;
                    break;
                }

                if ( c == 'x' ) {
                    /*
                     * \x<hex_string> sequence
                     */
                    value = 0;
                    c = _dgetc(fhandle);

                    for ( n = 0 ; n < 2 && isxdigit(c) ; ++n ) {
                        digit = toupper(c) - '0';

                        if ( digit > 10 )
                            digit += '0' + 10 - 'A';

                        value = (value << 4) + digit;
                        c = _dgetc(fhandle);
                    }

                    got = 1;
                    break;
                }

                /*
                 * Otherwise the character stands for itself.
                 */
                value = c;
                break;
            }

            /*
             * Store the resulting character for the escape sequence.
             */
            if ( len < MAXWORDLEN-1 )
                word[len] = value;

            ++len;

            if ( ! got )
                c = _dgetc(fhandle);

            continue;

        }

        /*
         * Not escaped: see if we've reached the end of the word.
         */
        if ( quoted ) {
            if ( c == quoted )
                break;
        }
        else {
            if ( isspace(c) || c == '#' ) {
                _dungetc(c);
                break;
            }
        }

        /*
         * Backslash starts an escape sequence.
         */
        if ( c == '\\' ) {
            escape = 1;
            c = _dgetc(fhandle);
            continue;
        }

        /*
         * An ordinary character: store it in the word and get another.
         */
        if ( len < MAXWORDLEN-1 )
            word[len] = c;

        ++len;

        c = _dgetc(fhandle);
    }

    /*
     * End of the word: check for errors.
     */
    if ( c == EOF ) {
        if ( _dferror() ) {
            AUTHDEBUG((LOG_ERR, "I/O error accesing %s.\n", filename));
            die(1);
        }

        /*
         * If len is zero, then we didn't find a word before the
         * end of the file.
         */
        if ( len == 0 )
            return 0;
    }

    /*
     * Warn if the word was too long, and append a terminating null.
     */
    if ( len >= MAXWORDLEN ) {
        AUTHDEBUG((LOG_ERR, "%s: warning: word in file %s too long (%.20s...)\n", progname, filename, word));
        len = MAXWORDLEN - 1;
    }

    word[len] = 0;

    return 1;

#undef isoctal
}

/*
 * scan_authfile - Scan an authorization file for a secret suitable
 * for authenticating `client' on `server'.  The return value is -1
 * if no secret is found, otherwise >= 0.  The return value has
 * NONWILD_CLIENT set if the secret didn't have "*" for the client, and
 * NONWILD_SERVER set if the secret didn't have "*" for the server.
 * Any following words on the line (i.e. address authorization
 * info) are placed in a wordlist and returned in *addrs.
 */
#if 0
static int scan_authfile(FILE *f, char *client,
                         char *server, char *secret,
                         struct wordlist **addrs, char *filename)
{
    int newline, xxx;
    int got_flag, best_flag;
    FILE *sf;
    struct wordlist *ap, *addr_list, *addr_last;
    char word[MAXWORDLEN];
    char atfile[MAXPATH];

    if (addrs != NULL)
        *addrs = NULL;

    addr_list = NULL;

    if (!getword(f, word, &newline, filename))
        return -1;              /* file is empty??? */

    newline = 1;
    best_flag = -1;

    for (;;) {
        /*
         * Skip until we find a word at the start of a line.
         */
        while (!newline && getword(f, word, &newline, filename))
            ;

        if (!newline)
            break;              /* got to end of file */

        /*
         * Got a client - check if it's a match or a wildcard.
         */
        got_flag = 0;
        if (client != NULL && strcmp(word, client) != 0 && !ISWILD(word)) {
            newline = 0;
            continue;
        }

        if (!ISWILD(word))
            got_flag = NONWILD_CLIENT;

        /*
         * Now get a server and check if it matches.
         */
        if (!getword(f, word, &newline, filename))
            break;

        if (newline)
            continue;

        if (server != NULL && strcmp(word, server) != 0 && !ISWILD(word))
            continue;

        if (!ISWILD(word))
            got_flag |= NONWILD_SERVER;

        /*
         * Got some sort of a match - see if it's better than what
         * we have already.
         */
        if (got_flag <= best_flag)
            continue;

        /*
         * Get the secret.
         */
        if (!getword(f, word, &newline, filename))
            break;

        if (newline)
            continue;

        /*
         * Special syntax: @filename means read secret from file.
         */
        if (word[0] == '@') {
            strcpy(atfile, word+1);

            if ((sf = fopen(atfile, "r")) == NULL) {
                AUTHDEBUG((LOG_WARNING, "can't open indirect secret file %s\n", atfile));
                continue;
            }

            if (!getword(sf, word, &xxx, atfile)) {
                AUTHDEBUG((LOG_WARNING, "no secret in indirect secret file %s\n", atfile));
                fclose(sf);
                continue;
            }
            fclose(sf);
        }

        if (secret != NULL)
            strcpy(secret, word);

        best_flag = got_flag;

        /*
         * Now read address authorization info and make a wordlist.
         */
        if (addr_list)
            free_wordlist(addr_list);

        addr_list = addr_last = NULL;

        for (;;) {
            if (!getword(f, word, &newline, filename) || newline)
                break;

            ap = (struct wordlist *)malloc(sizeof(struct wordlist)
                                           + strlen(word));
            if (ap == NULL)
                novm("authorized addresses");

            ap->next = NULL;
            strcpy(ap->word, word);

            if (addr_list == NULL)
                addr_list = ap;
            else
                addr_last->next = ap;

            addr_last = ap;
        }

        if (!newline)
            break;
    }

    if (addrs != NULL)
        *addrs = addr_list;
    else if (addr_list != NULL)
        free_wordlist(addr_list);

    return best_flag;
}
#endif
/*
 * free_wordlist - release memory allocated for a wordlist.
 */
#if 0
static void free_wordlist(struct wordlist *wp)
{
    struct wordlist *next;

    while (wp != NULL) {
        next = wp->next;
        free(wp);
        wp = next;
    }
}
#endif
