#include "common.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void die(int code, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(code);
}

void *xmalloc(size_t size)
{
    void *p = malloc(size);
    if (p == NULL)
        die(EXIT_ALLOC, "out of memory allocating %zu bytes", size);
    return p;
}

void *xcalloc(size_t nmemb, size_t size)
{
    void *p = calloc(nmemb, size);
    if (p == NULL)
        die(EXIT_ALLOC, "out of memory allocating %zu x %zu bytes", nmemb, size);
    return p;
}

void mutex_init(pthread_mutex_t *m)
{
    int rc = pthread_mutex_init(m, NULL);
    if (rc != 0)
        die(EXIT_PTHREAD, "pthread_mutex_init failed: %s", strerror(rc));
}

void mutex_lock(pthread_mutex_t *m)
{
    int rc = pthread_mutex_lock(m);
    if (rc != 0)
        die(EXIT_PTHREAD, "pthread_mutex_lock failed: %s", strerror(rc));
}

void mutex_unlock(pthread_mutex_t *m)
{
    int rc = pthread_mutex_unlock(m);
    if (rc != 0)
        die(EXIT_PTHREAD, "pthread_mutex_unlock failed: %s", strerror(rc));
}

void mutex_destroy(pthread_mutex_t *m)
{
    int rc = pthread_mutex_destroy(m);
    if (rc != 0)
        die(EXIT_PTHREAD, "pthread_mutex_destroy failed: %s", strerror(rc));
}

int mutex_trylock(pthread_mutex_t *m)
{
    int rc = pthread_mutex_trylock(m);
    if (rc != 0 && rc != EBUSY)
        die(EXIT_PTHREAD, "pthread_mutex_trylock failed: %s", strerror(rc));
    return rc; /* 0 = acquired, EBUSY = contended */
}
