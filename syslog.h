#ifndef _syslog_h_
#define _syslog_h_
#include <stdio.h>
extern bool isdaemon;
extern void vlog(const char * format, ...);
#endif
