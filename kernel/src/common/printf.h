#ifndef _PRINTF_H_
#define _PRINTF_H_

#include <stdarg.h>
#include <os_types.h>

int vsnprintf (char *buf, size_t max, const char *fmt, va_list args);
int snprintf (char *buf, size_t max, const char *fmt, ...);

#endif
