# AI Usage Disclosure

The submission guidelines explicitly permit AI-assisted development and ask for
transparency about it. This document records how AI was used, what was
validated by hand, and which parts the candidate is responsible for
understanding and defending.

## How AI was used

An AI coding assistant was used to accelerate the mechanical parts of the work:

- Scaffolding the shared module structure and boilerplate (headers, Makefile,
  the fail-fast wrapper functions).
- Drafting the five `vector_add()` variants from the well-known OSTEP
  homework description.
- Drafting the test and benchmark scripts and this documentation set.

AI was **not** used as an oracle for correctness claims. Every behavioural
statement in this repository was checked by actually building and running the
code (see below).

## What was validated by hand / by execution

Nothing in the docs is asserted without a corresponding real run on the
development host (Apple M1 Pro, Apple clang 21). Concretely, the following were
executed and their real output used:

- **Build:** clean compile of all five binaries with
  `-Wall -Wextra -Wpedantic -Werror` (zero warnings).
- **Functional tests:** `make test` → 34 assertions, 0 failures.
- **Deadlock:** confirmed `vector-deadlock -d` hangs at high `-l`/`-n` and that
  `-n 1` never deadlocks, all under a bounded timeout.
- **Sanitizers:** ThreadSanitizer reproduces the `nolock` data race (and shows
  0 races for the safe strategies and for `nolock -p`); UBSan caught a real
  signed-overflow bug that was then fixed; ASan shows no leaks.
- **Benchmarks:** `make benchmarks` produced the committed CSV/summary; the
  analysis quotes those measured numbers, not estimates.

The one bug found during development — signed integer overflow under the `-d`
stress configuration — was surfaced by UBSan and fixed in its own commit. That
round trip (write → sanitize → find bug → fix) is visible in the git history.

## What the candidate is responsible for explaining

The candidate should be able to explain, without reference to AI, at least:

- The four Coffman conditions and which one each strategy removes.
- Why deadlock is nondeterministic and load-dependent.
- Why a *global* lock order (not per-thread) is what breaks circular wait.
- The aliased-vector special case and why POSIX mutexes are non-recursive.
- Why only the *second* acquisition in try-wait must be non-blocking.
- Why the global acquisition lock serializes even disjoint work.
- The exact interleaving by which `nolock` loses an update, and why `-p` is
  safe.
- The `--check` invariant and why it is the only deterministically-checkable
  configuration.
- Every trade-off in `docs/concurrency-strategies.md` and every number in
  `docs/benchmark-analysis.md`.

These are collected as interview questions at the end of
`docs/concurrency-strategies.md`.

## Honesty policy

No execution output, compiler output, benchmark result, sanitizer result, or
retry count in this repository is fabricated. Where something could not be run,
it would be marked as pending rather than invented — but in practice everything
here was runnable on the development machine, so no such placeholders remain.
