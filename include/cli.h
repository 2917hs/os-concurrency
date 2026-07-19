/*
 * cli.h - Command-line parsing shared by all five executables.
 *
 * Every strategy accepts an identical option set so that benchmark and test
 * scripts can drive any binary with the same flags. Parsing lives here, apart
 * from the driver, so it can be unit-reasoned about and reused verbatim.
 */
#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stdio.h>

/* Fully-validated run configuration produced by cli_parse(). */
typedef struct {
    int  num_threads;   /* -n : worker threads, >= 1                       */
    int  loops;         /* -l : vector_add calls per worker, >= 1          */
    int  vector_length; /* --length : elements per vector, >= 1            */
    bool verbose;       /* -v : print vectors before/after                 */
    bool deadlock_order;/* -d : odd threads lock in reversed order         */
    bool parallel;      /* -p : each worker owns a disjoint vector pair     */
    bool timing;        /* -t : print elapsed wall-clock time              */
    bool check;         /* --check : verify deterministic result, set code */
} config_t;

/*
 * Parse argv into `cfg`. On success returns EXIT_OK. On a usage error prints a
 * specific message to stderr and returns EXIT_USAGE. If -h/--help is present it
 * prints help to stdout and returns a sentinel so main() can exit 0 without
 * running. `prog` is used in messages and defaults sensibly if NULL.
 */
enum { CLI_HELP_REQUESTED = 100 };
int cli_parse(int argc, char **argv, const char *prog, config_t *cfg);

/* Write the help / usage text for `prog` to `out`. */
void cli_usage(const char *prog, FILE *out);

#endif /* CLI_H */
