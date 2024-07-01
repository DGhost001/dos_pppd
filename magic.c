/*
 * magic.c - PPP Magic Number routines.
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
#include <stdlib.h>
#include <dos.h>

#include "pppd.h"
#include "magic.h"


/*
 * magic_init - Initialize the magic number generator.
 *
 * Attempts to compute a random number seed which will not repeat.
 */
void magic_init(void)
{
    u_int32_t far *biostck = (u_int32_t far *)MK_FP(0x40, 0x6C);

    srand((unsigned)*biostck);
}

/*
 * magic - Returns the next magic number.
 */
u_int32_t magic(void)
{
    return (u_int32_t)((u_int32_t)rand() * (u_int32_t)rand());
}
