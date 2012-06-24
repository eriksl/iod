#include <errno.h>
#include <stdarg.h>
#include <syslog.h>
#include "syslog.h"

bool isdaemon = false;

void vlog(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);

    if(isdaemon)
        vsyslog(LOG_WARNING, format, ap);
    else
        vfprintf(stderr, format, ap);

    va_end(ap);
}
