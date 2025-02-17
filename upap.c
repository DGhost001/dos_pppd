/*
 * upap.c - User/Password Authentication Protocol.
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

/*
 * TODO:
 */

#include <sys/types.h>
#include "types.h"
#include <string.h>
#include "syslog.h"

#include "pppd.h"
#include "upap.h"

/* external functions */
void print_string(char *, int, void (*)(void *, char *, ...), void *);
void auth_peer_fail(int, int);
void auth_peer_success(int, int);
void auth_withpeer_fail(int, int);
void auth_withpeer_success(int, int);

upap_state upap[NUM_PPP];               /* UPAP state; one for each unit */


static void upap_timeout(caddr_t);
static void upap_reqtimeout(caddr_t);
static void upap_rauthreq(upap_state *, u_char *, int, int);
static void upap_rauthack(upap_state *, u_char *, int, int);
static void upap_rauthnak(upap_state *, u_char *, int, int);
static void upap_sauthreq(upap_state *);
static void upap_sresp(upap_state *, int, int, char *, int);


/*
 * upap_init - Initialize a UPAP unit.
 */
void upap_init(int unit)
{
    upap_state *u = &upap[unit];

    u->us_unit = unit;
    u->us_user = NULL;
    u->us_userlen = 0;
    u->us_passwd = NULL;
    u->us_passwdlen = 0;
    u->us_clientstate = UPAPCS_INITIAL;
    u->us_serverstate = UPAPSS_INITIAL;
    u->us_id = 0;
    u->us_timeouttime = UPAP_DEFTIMEOUT;
    u->us_maxtransmits = 10;
    u->us_reqtimeout = UPAP_DEFREQTIME;
}


/*
 * upap_authwithpeer - Authenticate us with our peer (start client).
 *
 * Set new state and send authenticate's.
 */
void upap_authwithpeer(int unit, char *user, char *password)
{
    upap_state *u = &upap[unit];

    /* Save the username and password we're given */
    u->us_user = user;
    u->us_userlen = strlen(user);
    u->us_passwd = password;
    u->us_passwdlen = strlen(password);
    u->us_transmits = 0;

    /* Lower layer up yet? */
    if (u->us_clientstate == UPAPCS_INITIAL ||
        u->us_clientstate == UPAPCS_PENDING) {
        u->us_clientstate = UPAPCS_PENDING;
        return;
    }

    upap_sauthreq(u);                   /* Start protocol */
}


/*
 * upap_authpeer - Authenticate our peer (start server).
 *
 * Set new state.
 */
void upap_authpeer(int unit)
{
    upap_state *u = &upap[unit];

    /* Lower layer up yet? */
    if (u->us_serverstate == UPAPSS_INITIAL ||
        u->us_serverstate == UPAPSS_PENDING) {
        u->us_serverstate = UPAPSS_PENDING;
        return;
    }

    u->us_serverstate = UPAPSS_LISTEN;
    if (u->us_reqtimeout > 0)
        TIMEOUT(upap_reqtimeout, (caddr_t) u, u->us_reqtimeout);
}


/*
 * upap_timeout - Retransmission timer for sending auth-reqs expired.
 */
static void upap_timeout(caddr_t arg)
{
    upap_state *u = (upap_state *) arg;

    if (u->us_clientstate != UPAPCS_AUTHREQ)
        return;

    if (u->us_transmits >= u->us_maxtransmits) {
        /* give up in disgust */
        UPAPDEBUG((LOG_ERR, "No response to PAP authenticate-requests\n"));
        u->us_clientstate = UPAPCS_BADAUTH;
        auth_withpeer_fail(u->us_unit, PPP_PAP);
        return;
    }

    upap_sauthreq(u);           /* Send Authenticate-Request */
}


/*
 * upap_reqtimeout - Give up waiting for the peer to send an auth-req.
 */
static void upap_reqtimeout(caddr_t arg)
{
    upap_state *u = (upap_state *) arg;

    if (u->us_serverstate != UPAPSS_LISTEN)
        return;                 /* huh?? */

    auth_peer_fail(u->us_unit, PPP_PAP);
    u->us_serverstate = UPAPSS_BADAUTH;
}


/*
 * upap_lowerup - The lower layer is up.
 *
 * Start authenticating if pending.
 */
void upap_lowerup(int unit)
{
    upap_state *u = &upap[unit];

    if (u->us_clientstate == UPAPCS_INITIAL)
        u->us_clientstate = UPAPCS_CLOSED;
    else if (u->us_clientstate == UPAPCS_PENDING) {
        upap_sauthreq(u);       /* send an auth-request */
    }

    if (u->us_serverstate == UPAPSS_INITIAL)
        u->us_serverstate = UPAPSS_CLOSED;
    else if (u->us_serverstate == UPAPSS_PENDING) {
        u->us_serverstate = UPAPSS_LISTEN;
        if (u->us_reqtimeout > 0)
            TIMEOUT(upap_reqtimeout, (caddr_t) u, u->us_reqtimeout);
    }
}


/*
 * upap_lowerdown - The lower layer is down.
 *
 * Cancel all timeouts.
 */
void upap_lowerdown(int unit)
{
    upap_state *u = &upap[unit];

    if (u->us_clientstate == UPAPCS_AUTHREQ)    /* Timeout pending? */
        UNTIMEOUT(upap_timeout, (caddr_t) u);   /* Cancel timeout */
    if (u->us_serverstate == UPAPSS_LISTEN && u->us_reqtimeout > 0)
        UNTIMEOUT(upap_reqtimeout, (caddr_t) u);

    u->us_clientstate = UPAPCS_INITIAL;
    u->us_serverstate = UPAPSS_INITIAL;
}


/*
 * upap_protrej - Peer doesn't speak this protocol.
 *
 * This shouldn't happen.  In any case, pretend lower layer went down.
 */
void upap_protrej(int unit)
{
    upap_state *u = &upap[unit];

    if (u->us_clientstate == UPAPCS_AUTHREQ) {
        UPAPDEBUG((LOG_ERR, "PAP authentication failed due to protocol-reject\n"));
        auth_withpeer_fail(unit, PPP_PAP);
    }
    if (u->us_serverstate == UPAPSS_LISTEN) {
        UPAPDEBUG((LOG_ERR, "PAP authentication of peer failed (protocol-reject)\n"));
        auth_peer_fail(unit, PPP_PAP);
    }
    upap_lowerdown(unit);
}


/*
 * upap_input - Input UPAP packet.
 */
void upap_input(int unit, u_char *inpacket, int l)
{
    upap_state *u = &upap[unit];
    u_char *inp;
    u_char code, id;
    int len;

    /*
     * Parse header (code, id and length).
     * If packet too short, drop it.
     */
    inp = inpacket;
    if (l < UPAP_HEADERLEN) {
        UPAPDEBUG((LOG_INFO, "upap_input: rcvd short header.\n"));
        return;
    }
    GETCHAR(code, inp);
    GETCHAR(id, inp);
    GETSHORT(len, inp);
    if (len < UPAP_HEADERLEN) {
        UPAPDEBUG((LOG_INFO, "upap_input: rcvd illegal length.\n"));
        return;
    }
    if (len > l) {
        UPAPDEBUG((LOG_INFO, "upap_input: rcvd short packet.\n"));
        return;
    }
    len -= UPAP_HEADERLEN;

    /*
     * Action depends on code.
     */
    switch (code) {
    case UPAP_AUTHREQ:
        upap_rauthreq(u, inp, id, len);
        break;

    case UPAP_AUTHACK:
        upap_rauthack(u, inp, id, len);
        break;

    case UPAP_AUTHNAK:
        upap_rauthnak(u, inp, id, len);
        break;

    default:                            /* XXX Need code reject */
        break;
    }
}


/*
 * upap_rauth - Receive Authenticate.
 */
static void upap_rauthreq(upap_state *u, u_char *inp, int id, int len)
{
    u_char ruserlen, rpasswdlen;
    char *ruser, *rpasswd;
    int retcode;
    char *msg;
    int msglen;

    UPAPDEBUG((LOG_INFO, "upap_rauth: Rcvd id %d.\n", id));

    if (u->us_serverstate < UPAPSS_LISTEN)
        return;

    /*
     * If we receive a duplicate authenticate-request, we are
     * supposed to return the same status as for the first request.
     */
    if (u->us_serverstate == UPAPSS_OPEN) {
        upap_sresp(u, UPAP_AUTHACK, id, "", 0); /* return auth-ack */
        return;
    }
    if (u->us_serverstate == UPAPSS_BADAUTH) {
        upap_sresp(u, UPAP_AUTHNAK, id, "", 0); /* return auth-nak */
        return;
    }

    /*
     * Parse user/passwd.
     */
    if (len < sizeof (u_char)) {
        UPAPDEBUG((LOG_INFO, "upap_rauth: rcvd short packet.\n"));
        return;
    }
    GETCHAR(ruserlen, inp);
    len -= sizeof (u_char) + ruserlen + sizeof (u_char);
    if (len < 0) {
        UPAPDEBUG((LOG_INFO, "upap_rauth: rcvd short packet.\n"));
        return;
    }
    ruser = (char *) inp;
    INCPTR(ruserlen, inp);
    GETCHAR(rpasswdlen, inp);
    if (len < rpasswdlen) {
        UPAPDEBUG((LOG_INFO, "upap_rauth: rcvd short packet.\n"));
        return;
    }
    rpasswd = (char *) inp;

    /*
     * Check the username and password given.
     */
    retcode = check_passwd(u->us_unit, ruser, ruserlen, rpasswd,
                           rpasswdlen, &msg, &msglen);

    upap_sresp(u, retcode, id, msg, msglen);

    if (retcode == UPAP_AUTHACK) {
        u->us_serverstate = UPAPSS_OPEN;
        auth_peer_success(u->us_unit, PPP_PAP);
    } else {
        u->us_serverstate = UPAPSS_BADAUTH;
        auth_peer_fail(u->us_unit, PPP_PAP);
    }

    if (u->us_reqtimeout > 0)
        UNTIMEOUT(upap_reqtimeout, (caddr_t) u);
}


/*
 * upap_rauthack - Receive Authenticate-Ack.
 */
static void upap_rauthack(upap_state *u, u_char *inp, int id, int len)
{
    u_char msglen;
    char *msg;

    UPAPDEBUG((LOG_INFO, "upap_rauthack: Rcvd id %d.\n", id));
    if (u->us_clientstate != UPAPCS_AUTHREQ) /* XXX */
        return;

    /*
     * Parse message.
     */
    if (len < sizeof (u_char)) {
        UPAPDEBUG((LOG_INFO, "upap_rauthack: rcvd short packet.\n"));
        return;
    }
    GETCHAR(msglen, inp);
    len -= sizeof (u_char);
    if (len < msglen) {
        UPAPDEBUG((LOG_INFO, "upap_rauthack: rcvd short packet.\n"));
        return;
    }
    UNTIMEOUT(upap_timeout, (caddr_t) u);       /* Cancel timeout */
    msg = (char *) inp;
    PRINTMSG(msg, msglen);

    u->us_clientstate = UPAPCS_OPEN;

    auth_withpeer_success(u->us_unit, PPP_PAP);
}


/*
 * upap_rauthnak - Receive Authenticate-Nakk.
 */
static void upap_rauthnak(upap_state *u, u_char *inp, int id, int len)
{
    u_char msglen;
    char *msg;

    UPAPDEBUG((LOG_INFO, "upap_rauthnak: Rcvd id %d.\n", id));
    if (u->us_clientstate != UPAPCS_AUTHREQ) /* XXX */
        return;

    /*
     * Parse message.
     */
    if (len < sizeof (u_char)) {
        UPAPDEBUG((LOG_INFO, "upap_rauthnak: rcvd short packet.\n"));
        return;
    }
    GETCHAR(msglen, inp);
    len -= sizeof (u_char);
    if (len < msglen) {
        UPAPDEBUG((LOG_INFO, "upap_rauthnak: rcvd short packet.\n"));
        return;
    }
    msg = (char *) inp;
    PRINTMSG(msg, msglen);

    u->us_clientstate = UPAPCS_BADAUTH;

    UPAPDEBUG((LOG_ERR, "PAP authentication failed\n"));
    auth_withpeer_fail(u->us_unit, PPP_PAP);
}


/*
 * upap_sauthreq - Send an Authenticate-Request.
 */
static void upap_sauthreq(upap_state *u)
{
    u_char *outp;
    int outlen;

    outlen = UPAP_HEADERLEN + 2 * sizeof (u_char) +
        u->us_userlen + u->us_passwdlen;
    outp = outpacket_buf;

    MAKEHEADER(outp, PPP_PAP);

    PUTCHAR(UPAP_AUTHREQ, outp);
    PUTCHAR(++u->us_id, outp);
    PUTSHORT(outlen, outp);
    PUTCHAR(u->us_userlen, outp);
    BCOPY(u->us_user, outp, u->us_userlen);
    INCPTR(u->us_userlen, outp);
    PUTCHAR(u->us_passwdlen, outp);
    BCOPY(u->us_passwd, outp, u->us_passwdlen);

    output(u->us_unit, outpacket_buf, outlen + PPP_HDRLEN);

    UPAPDEBUG((LOG_INFO, "upap_sauth: Sent id %d.\n", u->us_id));

    TIMEOUT(upap_timeout, (caddr_t) u, u->us_timeouttime);
    ++u->us_transmits;
    u->us_clientstate = UPAPCS_AUTHREQ;
}


/*
 * upap_sresp - Send a response (ack or nak).
 */
static void upap_sresp(upap_state *u, int code, int id, char *msg, int msglen)
{
    u_char *outp;
    int outlen;

    outlen = UPAP_HEADERLEN + sizeof (u_char) + msglen;
    outp = outpacket_buf;
    MAKEHEADER(outp, PPP_PAP);

    PUTCHAR(code, outp);
    PUTCHAR(id, outp);
    PUTSHORT(outlen, outp);
    PUTCHAR(msglen, outp);
    BCOPY(msg, outp, msglen);
    output(u->us_unit, outpacket_buf, outlen + PPP_HDRLEN);

    UPAPDEBUG((LOG_INFO, "upap_sresp: Sent code %d, id %d.\n", code, id));
}

/*
 * upap_printpkt - print the contents of a PAP packet.
 */
#ifndef DEBUGUPAP
#pragma argsused
int upap_printpkt(u_char *p, int plen, void (*printer)(void *, char *, ...), void *arg)
{
    return 0;
}
#else
char *upap_codenames[] = {
    "AuthReq", "AuthAck", "AuthNak"
};

int upap_printpkt(u_char *p, int plen, void (*printer)(void *, char *, ...), void *arg)
{
    int code, id, len;
    int mlen, ulen, wlen;
    char *user, *pwd, *msg;
    u_char *pstart;

    if (plen < UPAP_HEADERLEN)
        return 0;

    pstart = p;
    GETCHAR(code, p);
    GETCHAR(id, p);
    GETSHORT(len, p);

    if (len < UPAP_HEADERLEN || len > plen)
        return 0;

    if (code >= 1 && code <= sizeof(upap_codenames) / sizeof(char *))
        printer(arg, " %s", upap_codenames[code-1]);
    else
        printer(arg, " code=0x%x", code);

    printer(arg, " id=0x%x", id);
    len -= UPAP_HEADERLEN;

    switch (code) {
        case UPAP_AUTHREQ:
            if (len < 1)
                break;

            ulen = p[0];

            if (len < ulen + 2)
                break;

            wlen = p[ulen + 1];

            if (len < ulen + wlen + 2)
                break;

            user = (char *) (p + 1);
            pwd = (char *) (p + ulen + 2);
            p += ulen + wlen + 2;
            len -= ulen + wlen + 2;
            printer(arg, " user=");
            print_string(user, ulen, printer, arg);
            printer(arg, " password=");
            print_string(pwd, wlen, printer, arg);
        break;

        case UPAP_AUTHACK:
        case UPAP_AUTHNAK:
            if (len < 1)
                break;

            mlen = p[0];

            if (len < mlen + 1)
                break;

            msg = (char *) (p + 1);
            p += mlen + 1;
            len -= mlen + 1;
            printer(arg, " msg=");
            print_string(msg, mlen, printer, arg);
        break;
    }

    /* print the rest of the bytes in the packet */
    for (; len > 0; --len) {
        GETCHAR(code, p);
        printer(arg, " %.2x", code);
    }

    return p - pstart;
}
#endif

