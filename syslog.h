/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

#ifndef __SYSLOG_H__
#define __SYSLOG_H__

#define LOG_ERR         0
#define LOG_INFO        1
#define LOG_WARNING     2
#define LOG_DEBUG       3
#define LOG_NOTICE      4

int syslog(int, const char *, ...);

#endif  /* __SYSLOG_H__ */

