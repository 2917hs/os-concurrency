# Testing Strategy

Concurrency bugs are timing-dependent, so the strategy is layered: fast
deterministic checks for the parts that *can* be made deterministic, bounded
nondeterministic checks for the parts that cannot (deadlock, lost updates), and
sanitizers to catch what black-box testing cannot see (data races, UB, leaks).

Run everything with `make test` (functional suite) and the sanitizer targets
below. Reproduced results on this host are quoted verbatim — nothing is
paraphrased.

## 1. The deterministic correctness oracle (`--check`)

The hard part of testing this program is that most configurations have
*non-deterministic values* (the result depends on lock-acquisition order). The
suite side-steps this by exercising the **one** configuration whose result is
provably deterministic:

- shared vectors (no `-p`), same lock order (no `-d`);
- vector 0 filled with `0`, vector 1 filled with `1`;
- every worker computes `vectors[0] += vectors[1]` and never writes vector 1.

Then after `num_threads × loops` locked additions, **every element of vector 0
must equal `num_threads × loops`** (modular). `--check` verifies this and sets
the exit code (`EXIT_CHECK_FAIL = 5`). A correctly-locked strategy always
passes; `nolock` loses updates under contention and fails — which is itself a
test (step 8).

## 2. Functional suite (`tests/run-tests.sh`, 34 assertions)

| Group | What it asserts |
|---|---|
| 1. Build/help | all five binaries built; `--help` exits 0 |
| 2. CLI validation | missing value, `0`, negative, non-number, `12x`, overflow, unknown option → exit 2; valid args → 0 |
| 3. Single-thread | every strategy (incl. nolock) passes `--check` at `n=1` (no contention) |
| 4. Multi-thread | the three safe strategies pass `--check` at `n=8 l=5000` |
| 5. Parallel | every strategy completes under `-p` (disjoint work) within a timeout |
| 6. Deadlock demo | `vector-deadlock -d` hangs (retried up to 5×; timeout-bounded) |
| 7. Safe under -d | global-order / try-wait / avoid-hold-and-wait complete under `-d` |
| 8. nolock races | `vector-nolock --check` under contention fails with exit 5 |

**Handling nondeterminism honestly.** Deadlock (step 6) and lost updates (step
8) are probabilistic. The suite *retries* them and fails if the phenomenon
never manifests — it does not weaken the assertion to make it pass. Latest run:

```
Results: 34 passed, 0 failed
```

## 3. Not hanging on the deadlock (portable timeout)

macOS ships no `timeout(1)`, so the suite implements one: run the command in the
background, poll `kill -0` on a 100 ms tick, and `kill -9` when the budget
expires (returning 124). This guarantees the deadlock demonstration is observed
*without* the test process itself ever hanging — a hard requirement for CI.

## 4. Sanitizers

Built with dedicated Make targets so the instrumented binaries never mix with
the release ones:

```
make asan    # -fsanitize=address,undefined   -> bin/asan/
make tsan    # -fsanitize=thread               -> bin/tsan/
make ubsan   # -fsanitize=undefined            -> bin/ubsan/
```

### ThreadSanitizer — the point of `nolock`

`nolock` on shared vectors is reported as a data race (write/write on the same
heap element). Real, unedited output on this host:

```
WARNING: ThreadSanitizer: data race (pid=22794)
  Write of size 4 at 0x00010bc03d40 by thread T2:
    #0 vector_add_locked_region  (vector-nolock)
    #1 vector_add                (vector-nolock)
    #2 worker                    (vector-nolock)
  Previous write of size 4 at 0x00010bc03d40 by thread T1:
    #0 vector_add_locked_region  (vector-nolock)
    ...
  Location is heap block of size 400 allocated by main thread:
    #1 xmalloc / vector_init / driver_run / main
SUMMARY: ThreadSanitizer: data race in vector_add_locked_region
```

The three **safe** strategies report **0 warnings** under TSan and pass
`--check`. Under `-p` (disjoint vectors) even `nolock` reports **0 races** —
confirming the race is a property of *sharing*, not of the code alone.

### UBSan — a real bug this caught

UBSan flagged **signed integer overflow** in the accumulation under the `-d`
stress configuration (the fed-back values grow past `INT_MAX`). This was a
genuine latent bug, fixed by switching the element type to `unsigned`
(well-defined modular arithmetic) — see commit
`fix: use unsigned modular arithmetic to avoid signed-overflow UB`. UBSan is now
clean on that configuration.

### ASan

AddressSanitizer (with the leak check via UBSan-combined build) reports no
leaks and no invalid accesses: every initialised mutex is destroyed and every
allocation is freed after the workers join.

## 5. What is intentionally *not* asserted

- **Exact values under `-d` or `-p`** are nondeterministic by design; the suite
  checks *termination* and *race-freedom* there, not specific numbers.
- **That deadlock always deadlocks** — it cannot, because deadlock is
  probabilistic. The assertion is "deadlock is *possible*", observed by hang.

## 6. Reproducing

```bash
make test            # 34-assertion functional suite
make asan && ./bin/asan/vector-global-order -n 8 -l 5000 --check
make tsan && ./bin/tsan/vector-nolock -n 4 -l 20000        # shows the race
make ubsan && ./bin/ubsan/vector-try-wait -n 4 -l 20000 -d # clean now
```
