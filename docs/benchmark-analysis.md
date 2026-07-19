# Benchmark Analysis

All numbers below are **real measurements** produced by
`scripts/run-benchmarks.sh` on this host and committed to
`benchmark-results/`. Regenerate with `make benchmarks`.

## Environment

```
uname:    Darwin 25.5.0 arm64
cpu:      Apple M1 Pro (8 cores)
compiler: Apple clang version 21.0.0
method:   mean of 3 runs per configuration; wall-clock via CLOCK_MONOTONIC
```

## Headline table (mean seconds, loops = 100,000)

| strategy | shr n=1 | shr n=2 | shr n=4 | shr n=8 | par n=1 | par n=2 | par n=4 | par n=8 |
|---|---|---|---|---|---|---|---|---|
| deadlock\* | 0.0054 | 0.0169 | 0.0638 | 0.1033 | 0.0052 | 0.0131 | 0.0148 | 0.0227 |
| global-order | 0.0053 | 0.0163 | 0.0647 | 0.1041 | 0.0053 | 0.0097 | 0.0177 | 0.0230 |
| try-wait | 0.0045 | 0.0145 | 0.0565 | 0.0902 | 0.0045 | 0.0112 | 0.0150 | 0.0219 |
| avoid-hold-and-wait | 0.0051 | 0.0185 | 0.0758 | 0.2691 | 0.0050 | 0.0146 | 0.0547 | 0.1298 |
| nolock | 0.0037 | 0.0155 | 0.0433 | 0.1267 | 0.0037 | 0.0045 | 0.0041 | 0.0076 |

\* `deadlock` is run **without** `-d`, where it is safe; benchmarks must
terminate. `shr` = shared vectors, `par` = `-p` disjoint vectors.

## What the numbers say

### 1. Shared mode: everyone is contention-bound, and it does not scale

In shared mode all threads fight over the same two locks. Adding threads adds
*contention*, not throughput: global-order goes 0.005 → 0.104 s from n=1 → n=8,
i.e. it gets **slower**, because the work is serialized through two mutexes plus
growing lock-handoff overhead. This is the expected answer to homework Q5/Q6:
shared-vector work cannot be sped up by more threads.

### 2. Parallel mode separates the winners from the losers

With `-p`, each thread owns private vectors, so the *safe, non-serializing*
strategies scale well and the serializing one does not:

- **nolock** is near-flat (0.0037 → 0.0076 s) — no locks, no contention, close
  to linear speedup. It is fastest **because** it is doing no synchronization…
  and it is only correct here precisely because there is no sharing.
- **global-order / try-wait / deadlock(no-d)** cluster around 0.022–0.023 s at
  n=8: cheap uncontended lock/unlock pairs, good scaling.
- **avoid-hold-and-wait** is the outlier at **0.1298 s** — ~5.6× slower than
  global-order at n=8 *despite disjoint data*. Its single global acquisition
  mutex serializes every `vector_add`, so parallelism cannot help. This is the
  quantitative answer to homework Q8.

### 3. try-wait is slightly *faster* than global-order here — why?

In these benchmarks there is no `-d`, so all threads acquire in the same order
and the trylock **never fails** (retries = 0). try-wait then does the same two
lock/unlocks as global-order but skips the id comparison branch, so it edges
ahead. Turn on `-d` and its retry cost explodes (see below) — the benchmark
configuration flatters it.

### 4. Linear scaling in loop count (homework Q5)

global-order, shared, n=2:

| loops | seconds |
|---|---|
| 100,000 | 0.0142 |
| 200,000 | 0.0306 |
| 400,000 | 0.0597 |

Time is ~linear in loop count (2× work ≈ 2× time), as expected: each loop
iteration is one bounded critical section.

### 5. try-wait retry cost under contention (homework Q7)

Retries counted by the program (`-d`, l=50,000):

| threads | retries |
|---|---|
| 2 | 215 |
| 4 | 1,027 |
| 8 | 4,581 |
| 16 | 25,349 |

Retries grow **super-linearly** with thread count: more threads → more
frequent trylock conflicts → more wasted acquire/release/backoff cycles. This
is the wasted-work cost that the deadlock-free-ness buys.

## Takeaways

1. **More threads on shared data is a trap** — contention makes it slower, not
   faster, for every strategy.
2. **The global acquisition lock is the worst scaler** — it serializes work
   that is otherwise embarrassingly parallel.
3. **nolock's speed is a mirage** — it "wins" only in the disjoint case where
   correctness is not at stake; on shared data it is fast *and wrong*.
4. **Global ordering is the best correct default** — deadlock-free by
   construction with the lowest overhead among the correct strategies.

> Caveat: absolute numbers are specific to this 8-core M1 Pro and this compiler.
> The *relative* ordering of the strategies is the portable result; re-run
> `make benchmarks` to reproduce on other hardware.
