/* Fallback that allows us to use snprintf when available,
   * but provides a safe alternative when it is not. */

#ifndef SAFE_SNPRINTF_H
#define SAFE_SNPRINTF_H

#include <stdio.h>
#include <stdarg.h>

/* Try to detect if snprintf is available */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
  #define HAS_SNPRINTF 1
#elif defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
  #define HAS_SNPRINTF 1
#else
  #define HAS_SNPRINTF 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * safe_snprintf - portable snprintf fallback
 * Always null-terminates if buffer size > 0.
 */
static int safe_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);

#if HAS_SNPRINTF
    ret = vsnprintf(buf, size, fmt, ap);
#else
    /* Unsafe fallback — must be careful with buffer sizes */
    ret = vsprintf(buf, fmt, ap);
    /* Force null-termination if possible */
    if (size > 0)
        buf[size - 1] = '\0';
#endif

    va_end(ap);
    return ret;
}

#ifdef __cplusplus
}
#endif

#endif /* SAFE_SNPRINTF_H */
