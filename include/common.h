/*
 * common.h - Shared error-handling and allocation helpers.
 *
 * These helpers give every executable identical, fail-fast behaviour for the
 * handful of system calls that must never silently fail (allocation and the
 * pthread primitives). Keeping them in one place avoids the copy/pasted
 * `if (rc != 0) { perror(...); exit(...); }` blocks that otherwise litter
 * concurrency code and drift out of sync.
 */
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

/* Process exit codes. Distinct values make failures scriptable in tests. */
enum {
    EXIT_OK          = 0,
    EXIT_USAGE       = 2,  /* bad command-line arguments                */
    EXIT_ALLOC       = 3,  /* out of memory                             */
    EXIT_PTHREAD     = 4,  /* a pthread_* call failed                   */
    EXIT_CHECK_FAIL  = 5   /* post-run correctness check failed         */
};

/*
 * Print "prog: msg\n" to stderr and exit with `code`. Centralising this keeps
 * error messages uniform across the five executables.
 */
void die(int code, const char *fmt, ...)
    __attribute__((noreturn, format(printf, 2, 3)));

/* Allocation wrappers that abort (EXIT_ALLOC) instead of returning NULL. */
void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);

/*
 * Thin wrappers around the pthread mutex calls that abort on failure. A
 * pthread_mutex_lock returning non-zero indicates a programming error (an
 * uninitialised or destroyed mutex), never a recoverable runtime condition,
 * so aborting loudly is the correct response.
 */
struct pthread_mutex_t_fwd; /* documentation only */
#include <pthread.h>
void mutex_init(pthread_mutex_t *m);
void mutex_lock(pthread_mutex_t *m);
void mutex_unlock(pthread_mutex_t *m);
void mutex_destroy(pthread_mutex_t *m);
/* Returns 0 on success, EBUSY if the lock is held; aborts on any other error. */
int  mutex_trylock(pthread_mutex_t *m);

#endif /* COMMON_H */
