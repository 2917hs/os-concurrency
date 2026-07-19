/*
 * vector-avoid-hold-and-wait.c - Strategy 4: global acquisition lock.
 *
 * Also removes HOLD-AND-WAIT, but atomically rather than via retry. A single
 * global "acquisition" mutex must be held to acquire ANY vector lock. Because
 * a thread grabs both vector locks while holding the global lock, no other
 * thread can be midway through its own acquisition - so no thread ever holds
 * one vector lock while waiting for another. Acquisition of the full lock set
 * is effectively atomic.
 *
 * Main problem (homework Q8): the global lock serializes the acquisition phase
 * of every vector_add across the whole program. Even workers operating on
 * completely disjoint vectors (the -p case) must queue on this one mutex, so
 * the design throws away the parallelism it could otherwise have. It is simple
 * and deadlock-free, but it scales poorly precisely when scaling matters.
 *
 * We release the global lock as soon as both vector locks are held (rather
 * than holding it across the arithmetic), which lets the critical-section work
 * of different pairs overlap and limits the serialization to acquisition.
 */
#include "strategy.h"
#include "common.h"

static pthread_mutex_t g_acquire;

const char *strategy_name(void) { return "avoid-hold-and-wait"; }
void strategy_init(void)        { mutex_init(&g_acquire); }
void strategy_cleanup(void)     { mutex_destroy(&g_acquire); }
void strategy_report(FILE *out) { (void)out; }

void vector_add(vector_t *dst, vector_t *src)
{
    mutex_lock(&g_acquire);            /* gate: only one acquirer at a time */
    mutex_lock(&dst->lock);
    if (dst->id != src->id)            /* avoid double-locking aliased vectors */
        mutex_lock(&src->lock);
    mutex_unlock(&g_acquire);          /* both held; release the gate early */

    vector_add_locked_region(dst, src);

    if (dst->id != src->id)
        mutex_unlock(&src->lock);
    mutex_unlock(&dst->lock);
}
