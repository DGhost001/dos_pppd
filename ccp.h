/*
 * ccp.h - Definitions for PPP Compression Control Protocol.
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
 * $Id: ccp.h,v 1.4 1995/04/24 06:00:54 paulus Exp $
 */

/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

typedef struct ccp_options {
    u_int bsd_compress: 1;      /* do BSD Compress? */
    u_int deflate: 1;           /* do Deflate? */
    u_int predictor_1: 1;       /* do Predictor-1? */
    u_int predictor_2: 1;       /* do Predictor-2? */
    u_short bsd_bits;           /* # bits/code for BSD Compress */
    u_short deflate_size;       /* lg(window size) for Deflate */
} ccp_options;

extern fsm ccp_fsm[];
extern ccp_options ccp_wantoptions[];
extern ccp_options ccp_gotoptions[];
extern ccp_options ccp_allowoptions[];
extern ccp_options ccp_hisoptions[];

void ccp_init(int unit);
void ccp_open(int unit);
void ccp_close(int unit);
void ccp_lowerup(int unit);
void ccp_lowerdown(int);
void ccp_input(int, u_char *, int);
void ccp_protrej(int);
int  ccp_printpkt(u_char *, int, void (*)(void *, char *, ...), void *);
void ccp_datainput(int, u_char *, int);
