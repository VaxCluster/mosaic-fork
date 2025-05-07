#ifndef COMPAT_H
#define COMPAT_H

/* Define caddr_t if not already defined */
#if !defined(HAVE_CADDR_T) && !defined(_CADDR_T) && !defined(__caddr_t_defined) && !defined(__CADDR_T_TYPE)
typedef void *caddr_t;
#define _CADDR_T 1
#endif

/**
 * Define u_long if not already defined.
 * This is a workaround for systems that don't have it defined in <sys/types.h>.
 */
#ifndef HAVE_U_LONG
typedef unsigned long u_long;
#endif

/** Define strdup if not already defined */
#ifndef HAVE_STRDUP
char *strdup(const char *s);
#endif


/* Define struct timezone if not on macOS or other platforms that provide it */
#if !defined(__APPLE__) && !defined(__MACH__)
#ifndef HAVE_STRUCT_TIMEZONE
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif
#endif

#endif /* COMPAT_H */
