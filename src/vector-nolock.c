/*
 * vector-nolock.c - Strategy 5: no locking at all (INTENTIONALLY INCORRECT).
 *
 * This version cannot deadlock for the trivial reason that it never takes a
 * lock - but it is NOT correct, and it is included to make that trade-off
 * concrete, not to be used.
 *
 * "Does it provide the same semantics as the other versions?" (homework Q9):
 * No. Each element update is a read-modify-write (dst[i] += src[i]) that is not
 * atomic. When several threads share the same vectors, two threads can read the
 * same old value, each add to it, and both write back - one update is lost. The
 * concurrent reads/writes of the same int are also a data race and therefore
 * undefined behaviour under the C memory model; ThreadSanitizer reports them.
 *
 * The one case where it happens to be correct is fully disjoint work (the -p
 * case with independent vector pairs): with no sharing there is no race, which
 * is why the benchmark shows this version "winning" on -p while failing the
 * correctness check on shared vectors. Speed without correctness is not a
 * win - that is the point.
 */
#include "strategy.h"

const char *strategy_name(void) { return "nolock"; }
void strategy_init(void)        { }
void strategy_report(FILE *out) { (void)out; }
void strategy_cleanup(void)     { }

void vector_add(vector_t *dst, vector_t *src)
{
    /* No mutual exclusion: correct only when dst/src are not shared across
     * threads. Deliberately racy; see the file header. */
    vector_add_locked_region(dst, src);
}
