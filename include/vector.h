/*
 * vector.h - The shared data structure operated on by every strategy.
 *
 * This is the reconstructed equivalent of the OSTEP homework's
 * `vector-header.h`. A vector is a fixed-length array of integers guarded by
 * its own mutex and tagged with a stable, immutable id. The id exists purely
 * so that the global-ordering strategy can impose a deterministic total order
 * over locks (see src/vector-global-order.c).
 *
 * Ownership: a vector owns its `values` buffer and its `lock`. vector_init()
 * establishes both; vector_destroy() releases both. Vectors are never copied
 * by value - doing so would duplicate a pthread_mutex_t, which is undefined
 * behaviour - so they are always passed by pointer.
 */
#ifndef VECTOR_H
#define VECTOR_H

#include <pthread.h>
#include <stdio.h>

typedef struct {
    int             *values;  /* owned heap buffer of `length` ints */
    int              length;  /* element count, > 0                 */
    int              id;      /* stable ordering key, assigned once */
    pthread_mutex_t  lock;    /* guards `values`                    */
} vector_t;

/*
 * Initialise `v` in place: allocate its buffer, set every element to
 * `fill`, record the id and length, and initialise the mutex. Aborts on
 * allocation or mutex-init failure (these are unrecoverable here).
 */
void vector_init(vector_t *v, int id, int length, int fill);

/* Release the buffer and destroy the mutex. Safe to call once per init. */
void vector_destroy(vector_t *v);

/* Element-wise dst += src, performed WITHOUT any locking. Callers that need
 * mutual exclusion must hold the appropriate locks around this call. Keeping
 * the arithmetic in one shared function means every strategy performs the
 * identical critical-section work and differs only in how it locks. */
void vector_add_locked_region(vector_t *dst, const vector_t *src);

/* Print "id: [v0, v1, ...]" to `out`. Used by verbose mode. */
void vector_print(const vector_t *v, FILE *out);

#endif /* VECTOR_H */
