/*
 * vector-deadlock.c - Strategy 1: naive fixed-order locking (BUGGY BY DESIGN).
 *
 * vector_add() always locks dst first, then src. That is fine when every
 * caller uses the same order, but the driver's -d flag makes odd-numbered
 * workers call vector_add(v1, v0) while even workers call vector_add(v0, v1).
 * The two groups then request the locks in opposite orders:
 *
 *      even worker:  lock(v0) ... wants lock(v1)
 *      odd  worker:  lock(v1) ... wants lock(v0)
 *
 * If the scheduler interleaves them so both hold their first lock before
 * either takes the second, all four Coffman conditions hold simultaneously -
 * mutual exclusion, hold-and-wait, no preemption, and circular wait - and the
 * program deadlocks. This is nondeterministic: with -l 1 the window is tiny
 * and it often completes; with large -l it is almost certain to hang. That
 * nondeterminism is exactly the lesson, so this file is intentionally NOT
 * fixed. Every other strategy removes one Coffman condition to prevent it.
 */
#include "strategy.h"
#include "common.h"

const char *strategy_name(void) { return "deadlock"; }
void strategy_init(void)        { }
void strategy_report(FILE *out) { (void)out; }
void strategy_cleanup(void)     { }

void vector_add(vector_t *dst, vector_t *src)
{
    /* Aliased vectors would self-deadlock on the second lock, so handle that
     * case even though the buggy ordering below is left intact. */
    if (dst->id == src->id) {
        mutex_lock(&dst->lock);
        vector_add_locked_region(dst, src);
        mutex_unlock(&dst->lock);
        return;
    }

    mutex_lock(&dst->lock);
    mutex_lock(&src->lock);          /* <-- circular wait forms here */
    vector_add_locked_region(dst, src);
    mutex_unlock(&src->lock);
    mutex_unlock(&dst->lock);
}
