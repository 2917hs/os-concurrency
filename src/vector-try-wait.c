/*
 * vector-try-wait.c - Strategy 3: trylock with back-off and retry.
 *
 * Removes the HOLD-AND-WAIT condition by never blocking while holding a lock.
 * We take dst normally, then *try* for src with pthread_mutex_trylock(). If
 * that fails we release dst, count a retry, back off briefly, and start over,
 * so a thread never sits waiting on one lock while holding another - no cycle
 * can persist.
 *
 * "Is the first pthread_mutex_trylock() really needed?" (homework Q7): no. The
 * deadlock-breaking property comes entirely from the SECOND acquisition being
 * non-blocking. Blocking on the first lock cannot deadlock because at that
 * point the thread holds nothing to wait on. So the first acquisition is a
 * plain mutex_lock() here; only the second is a trylock.
 *
 * The cost this trades for is potential LIVELOCK and wasted work: two threads
 * can repeatedly grab their first lock, fail the second, and back off in
 * lockstep. A randomized back-off breaks that symmetry probabilistically, and
 * we track the retry count so the overhead is observable (homework asks how it
 * grows with load).
 */
#include "strategy.h"
#include "common.h"

#include <sched.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

/* Contended: many threads increment, so use a lock-free atomic counter to
 * avoid adding a second synchronization problem on top of the one we study. */
static _Atomic long g_retries = 0;

const char *strategy_name(void) { return "try-wait"; }
void strategy_init(void)        { atomic_store(&g_retries, 0); }
void strategy_cleanup(void)     { }

void strategy_report(FILE *out)
{
    fprintf(out, "retries: %ld\n", atomic_load(&g_retries));
}

/* Yield the CPU and sleep a small randomized interval (0-63 us) to desynchronize
 * threads that would otherwise retry in lockstep. rand() bias is irrelevant for
 * a jitter source, so its non-thread-safety is acceptable here. */
static void backoff(void)
{
    sched_yield();
    struct timespec ts = { .tv_sec = 0, .tv_nsec = (rand() & 63) * 1000L };
    nanosleep(&ts, NULL);
}

void vector_add(vector_t *dst, vector_t *src)
{
    if (dst->id == src->id) {          /* aliased: a single lock suffices */
        mutex_lock(&dst->lock);
        vector_add_locked_region(dst, src);
        mutex_unlock(&dst->lock);
        return;
    }

    for (;;) {
        mutex_lock(&dst->lock);        /* first lock: plain, cannot deadlock */
        if (mutex_trylock(&src->lock) == 0) {
            vector_add_locked_region(dst, src);
            mutex_unlock(&src->lock);
            mutex_unlock(&dst->lock);
            return;
        }
        /* Could not get src without waiting: drop everything and retry. */
        mutex_unlock(&dst->lock);
        atomic_fetch_add(&g_retries, 1);
        backoff();
    }
}
