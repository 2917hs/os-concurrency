/*
 * vector-global-order.c - Strategy 2: total lock ordering.
 *
 * Removes the CIRCULAR WAIT Coffman condition. Every thread acquires the two
 * mutexes in the same global order - ascending vector id - regardless of which
 * vector is the destination. Because all threads agree on the order, no cycle
 * can form in the wait-for graph, so deadlock is impossible.
 *
 * The special case for aliased vectors (dst->id == src->id) is essential: a
 * mutex is not recursive, so locking the same one twice would deadlock a
 * single thread against itself. We lock once and let the arithmetic double the
 * elements in place. This is the "why is there a special case when source and
 * destination are the same?" from the homework.
 */
#include "strategy.h"
#include "common.h"

const char *strategy_name(void) { return "global-order"; }
void strategy_init(void)        { }
void strategy_report(FILE *out) { (void)out; }
void strategy_cleanup(void)     { }

void vector_add(vector_t *dst, vector_t *src)
{
    if (dst->id == src->id) {              /* same vector: lock once */
        mutex_lock(&dst->lock);
        vector_add_locked_region(dst, src);
        mutex_unlock(&dst->lock);
        return;
    }

    /* Acquire in ascending-id order; release in the reverse order. */
    vector_t *first  = dst->id < src->id ? dst : src;
    vector_t *second = dst->id < src->id ? src : dst;

    mutex_lock(&first->lock);
    mutex_lock(&second->lock);
    vector_add_locked_region(dst, src);
    mutex_unlock(&second->lock);
    mutex_unlock(&first->lock);
}
