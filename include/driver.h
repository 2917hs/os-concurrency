/*
 * driver.h - The shared program body (reconstructed `main-common`).
 *
 * driver_run() contains the whole life cycle: allocate vectors, spawn workers,
 * time them, join, optionally verify, and clean up. Each executable's main()
 * is a two-line shim that parses arguments and calls driver_run(), so all five
 * binaries share identical orchestration and differ only in the linked
 * strategy object.
 */
#ifndef DRIVER_H
#define DRIVER_H

#include "cli.h"

/* Run the full simulation described by `cfg`. Returns a process exit code
 * (EXIT_OK, or EXIT_CHECK_FAIL if --check detected an incorrect result). */
int driver_run(const config_t *cfg);

#endif /* DRIVER_H */
