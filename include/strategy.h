/*
 * strategy.h - The contract every synchronization strategy implements.
 *
 * The driver (src/driver.c) is completely strategy-agnostic: it links against
 * exactly one object file that provides these four symbols. This is the seam
 * that lets deadlock / global-order / try-wait / avoid-hold-and-wait / nolock
 * share 100% of the surrounding infrastructure and differ only in how
 * vector_add() acquires locks.
 */
#ifndef STRATEGY_H
#define STRATEGY_H

#include <stdio.h>
#include "vector.h"

/* Human-readable name, e.g. "global-order". Used in verbose/timing output. */
const char *strategy_name(void);

/* One-time setup before any worker starts (e.g. the avoid-hold-and-wait
 * strategy initialises its global acquisition mutex here). No-op for most. */
void strategy_init(void);

/*
 * The core operation: dst += src, element-wise, with whatever locking
 * discipline this strategy defines. The two vectors may alias (dst == src);
 * every implementation must handle that without locking one mutex twice.
 */
void vector_add(vector_t *dst, vector_t *src);

/* Print any strategy-specific statistics (e.g. trylock retry count) to `out`.
 * Called once after all workers join. No-op for strategies with no stats. */
void strategy_report(FILE *out);

/* Release anything strategy_init() acquired. No-op for most. */
void strategy_cleanup(void);

#endif /* STRATEGY_H */
