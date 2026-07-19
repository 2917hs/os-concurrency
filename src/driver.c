/*
 * driver.c - Shared program body for every strategy (reconstructed
 * `main-common`). See include/driver.h for the role this plays.
 *
 * Vector layout and the worker's index mapping are the two things that make
 * the deadlock reproducible, so they are documented carefully below.
 */
#include "driver.h"
#include "common.h"
#include "strategy.h"
#include "vector.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Per-worker arguments. Vectors and config are shared read-only (config) or
 * mutated only through the strategy's locking (vectors). */
typedef struct {
    long            tid;
    const config_t *cfg;
    vector_t       *vectors;
    int             num_vectors;
} worker_arg_t;

/*
 * Choose the (dst, src) index pair this worker adds on each iteration.
 *
 *   parallel (-p): worker t owns the disjoint pair (2t, 2t+1). No two workers
 *                  touch the same vector, so there is no contention at all.
 *
 *   shared (default): every worker uses vectors 0 and 1. With -d, odd workers
 *                  reverse the pair (src=0, dst=1) so that even and odd workers
 *                  request the two locks in opposite orders - the circular wait
 *                  that the deadlock strategy exhibits.
 */
static void select_pair(const worker_arg_t *w, int *dst, int *src)
{
    if (w->cfg->parallel) {
        *dst = (int)(2 * w->tid);
        *src = (int)(2 * w->tid + 1);
    } else if (w->cfg->deadlock_order && (w->tid % 2 == 1)) {
        *dst = 1;
        *src = 0;
    } else {
        *dst = 0;
        *src = 1;
    }
}

static void *worker(void *arg)
{
    const worker_arg_t *w = arg;
    int dst, src;
    select_pair(w, &dst, &src);

    for (int i = 0; i < w->cfg->loops; i++)
        vector_add(&w->vectors[dst], &w->vectors[src]);

    return NULL;
}

/* Elapsed seconds between two CLOCK_MONOTONIC samples. */
static double elapsed_seconds(const struct timespec *a, const struct timespec *b)
{
    return (double)(b->tv_sec - a->tv_sec)
         + (double)(b->tv_nsec - a->tv_nsec) / 1e9;
}

/*
 * Verify the one configuration whose result is deterministic: shared vectors
 * (no -p), same lock order (no -d). There, every worker computes
 * vectors[0] += vectors[1] exactly num_threads*loops times while vectors[1]
 * (filled with 1) is never written. So each element of vector 0 must equal
 * num_threads*loops. A correct, properly-locked strategy always passes; the
 * lock-free strategy loses updates under contention and fails.
 *
 * Returns 0 on pass, -1 on mismatch, and 1 when the current config is not one
 * we can check.
 */
static int check_result(const config_t *cfg, const vector_t *vectors)
{
    if (cfg->parallel || cfg->deadlock_order)
        return 1; /* not a checkable configuration */

    long expected = (long)cfg->num_threads * cfg->loops;
    for (int i = 0; i < vectors[0].length; i++) {
        if (vectors[0].values[i] != (int)expected)
            return -1;
    }
    return 0;
}

int driver_run(const config_t *cfg)
{
    strategy_init();

    /* Parallel mode needs a private pair per worker; shared mode needs two. */
    int num_vectors = cfg->parallel ? 2 * cfg->num_threads : 2;
    vector_t *vectors = xmalloc((size_t)num_vectors * sizeof(*vectors));

    /* Vector 0 starts at 0, all others at 1. This makes the shared-mode
     * result (vector 0 += vector 1) exactly checkable; see check_result(). */
    for (int i = 0; i < num_vectors; i++)
        vector_init(&vectors[i], i, cfg->vector_length, i == 0 ? 0 : 1);

    if (cfg->verbose) {
        printf("[%s] before: ", strategy_name());
        printf("n=%d l=%d %s%s\n", cfg->num_threads, cfg->loops,
               cfg->parallel ? "parallel " : "shared ",
               cfg->deadlock_order ? "reversed-order" : "same-order");
        for (int i = 0; i < num_vectors && i < 4; i++)
            vector_print(&vectors[i], stdout);
    }

    pthread_t    *threads = xmalloc((size_t)cfg->num_threads * sizeof(*threads));
    worker_arg_t *args    = xmalloc((size_t)cfg->num_threads * sizeof(*args));

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < cfg->num_threads; i++) {
        args[i] = (worker_arg_t){
            .tid = i, .cfg = cfg,
            .vectors = vectors, .num_vectors = num_vectors,
        };
        int rc = pthread_create(&threads[i], NULL, worker, &args[i]);
        if (rc != 0)
            die(EXIT_PTHREAD, "pthread_create failed: %s", strerror(rc));
    }

    for (int i = 0; i < cfg->num_threads; i++) {
        int rc = pthread_join(threads[i], NULL);
        if (rc != 0)
            die(EXIT_PTHREAD, "pthread_join failed: %s", strerror(rc));
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    if (cfg->timing)
        printf("%-22s n=%-3d l=%-7d %s%s time=%.6f s\n",
               strategy_name(), cfg->num_threads, cfg->loops,
               cfg->parallel ? "par " : "shr ",
               cfg->deadlock_order ? "rev" : "fwd",
               elapsed_seconds(&t0, &t1));

    strategy_report(stdout);

    if (cfg->verbose) {
        printf("[%s] after:\n", strategy_name());
        for (int i = 0; i < num_vectors && i < 4; i++)
            vector_print(&vectors[i], stdout);
    }

    int exit_code = EXIT_OK;
    if (cfg->check) {
        int r = check_result(cfg, vectors);
        if (r == 0) {
            printf("check: PASS (all elements == %ld)\n",
                   (long)cfg->num_threads * cfg->loops);
        } else if (r < 0) {
            printf("check: FAIL (lost updates - result does not match "
                   "num_threads*loops)\n");
            exit_code = EXIT_CHECK_FAIL;
        } else {
            printf("check: SKIP (only meaningful without -p and without -d)\n");
        }
    }

    for (int i = 0; i < num_vectors; i++)
        vector_destroy(&vectors[i]);
    free(vectors);
    free(threads);
    free(args);

    strategy_cleanup();
    return exit_code;
}

/*
 * Shared entry point. Every executable links this main() together with one
 * strategy object, so argument handling and the run life cycle are identical
 * across all five binaries.
 */
int main(int argc, char **argv)
{
    config_t cfg;
    int rc = cli_parse(argc, argv, argv[0], &cfg);
    if (rc == CLI_HELP_REQUESTED)
        return EXIT_OK;
    if (rc != EXIT_OK)
        return rc;
    return driver_run(&cfg);
}
