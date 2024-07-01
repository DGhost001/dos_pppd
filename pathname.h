/*
 * define path names
 *
 * $Id: pathnames.h,v 1.6 1995/06/12 11:22:53 paulus Exp $
 */

/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

#define _PATH_UPAPFILE  "pppdpap.cfg"
#define _PATH_CHAPFILE  "pppdchap.cfg"
#define _PATH_SYSOPTIONS "pppd.cfg"
#define _PATH_IPUP      "ip-up"
#define _PATH_IPDOWN    "ip-down"
#define _PATH_TTYOPT    "pppdcom"
#define _PATH_USEROPT   "pppdrc.cfg"

#ifdef IPX_CHANGE
#define _PATH_IPXUP     "ipx-up"
#define _PATH_IPXDOWN   "ipx-down"
#endif /* IPX_CHANGE */

