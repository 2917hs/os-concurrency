#include "cli.h"
#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Parse a strictly-positive integer argument. Rejects empty strings, trailing
 * junk ("12x"), non-numbers, zero, negatives, and overflow. Returns 0 on
 * success and writes *out; returns -1 on any malformed value. */
static int parse_positive_int(const char *s, int *out)
{
    if (s == NULL || *s == '\0')
        return -1;

    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);

    if (end == s || *end != '\0')     /* not fully numeric        */
        return -1;
    if (errno == ERANGE || v > INT_MAX) /* overflow                */
        return -1;
    if (v <= 0)                        /* zero and negatives out   */
        return -1;

    *out = (int)v;
    return 0;
}

void cli_usage(const char *prog, FILE *out)
{
    fprintf(out,
        "Usage: %s [options]\n"
        "\n"
        "Multithreaded vector-add deadlock simulator.\n"
        "\n"
        "Options:\n"
        "  -n <num>     number of worker threads   (default 2, must be > 0)\n"
        "  -l <num>     vector_add calls per worker (default 1, must be > 0)\n"
        "  -v           verbose: print vectors before and after the run\n"
        "  -d           deadlock ordering: odd threads lock in reverse order\n"
        "  -p           parallel: each worker owns a disjoint vector pair\n"
        "  -t           print elapsed wall-clock time\n"
        "  --length <n> elements per vector         (default 100, must be > 0)\n"
        "  --check      verify the deterministic result and set the exit code\n"
        "  -h, --help   show this help and exit\n",
        prog);
}

int cli_parse(int argc, char **argv, const char *prog, config_t *cfg)
{
    if (prog == NULL)
        prog = (argc > 0 && argv[0]) ? argv[0] : "vector";

    /* Defaults. */
    cfg->num_threads    = 2;
    cfg->loops          = 1;
    cfg->vector_length  = 100;
    cfg->verbose        = false;
    cfg->deadlock_order = false;
    cfg->parallel       = false;
    cfg->timing         = false;
    cfg->check          = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        /* Options that take a value share this helper. */
        #define TAKE_VALUE(dst)                                              \
            do {                                                             \
                if (i + 1 >= argc) {                                         \
                    fprintf(stderr, "%s: option '%s' requires a value\n",    \
                            prog, arg);                                      \
                    return EXIT_USAGE;                                       \
                }                                                            \
                if (parse_positive_int(argv[++i], (dst)) != 0) {            \
                    fprintf(stderr,                                          \
                            "%s: invalid value '%s' for '%s' "               \
                            "(expected integer > 0)\n",                      \
                            prog, argv[i], arg);                             \
                    return EXIT_USAGE;                                       \
                }                                                            \
            } while (0)

        if (strcmp(arg, "-n") == 0) {
            TAKE_VALUE(&cfg->num_threads);
        } else if (strcmp(arg, "-l") == 0) {
            TAKE_VALUE(&cfg->loops);
        } else if (strcmp(arg, "--length") == 0) {
            TAKE_VALUE(&cfg->vector_length);
        } else if (strcmp(arg, "-v") == 0) {
            cfg->verbose = true;
        } else if (strcmp(arg, "-d") == 0) {
            cfg->deadlock_order = true;
        } else if (strcmp(arg, "-p") == 0) {
            cfg->parallel = true;
        } else if (strcmp(arg, "-t") == 0) {
            cfg->timing = true;
        } else if (strcmp(arg, "--check") == 0) {
            cfg->check = true;
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            cli_usage(prog, stdout);
            return CLI_HELP_REQUESTED;
        } else {
            fprintf(stderr, "%s: unknown option '%s'\n", prog, arg);
            cli_usage(prog, stderr);
            return EXIT_USAGE;
        }

        #undef TAKE_VALUE
    }

    return EXIT_OK;
}
