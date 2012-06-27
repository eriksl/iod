#ifndef _syslog_h_
#define _syslog_h_
#include <stdio.h>
extern bool isdaemon;
extern bool debug;
extern void vlog(const char * format, ...);
extern void dlog(const char * format, ...);
#endif
