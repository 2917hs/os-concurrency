# Assignment Answers

Answers to the ten homework questions in *Common Concurrency Problems*
(OSTEP ch. 32). Every "Observed result" is a real run on this host (Apple M1
Pro, 8 cores, Apple clang 21). Commands assume `make all` has been run.

> Implementation note: this is a faithful **reconstruction** of the homework
> substrate, so a couple of cosmetic details differ from the original (verbose
> mode prints the vectors before/after rather than inside each `vector_add`).
> Where that affects an answer it is called out explicitly.

---

## Q1. Understand the programs; run `./vector-deadlock -n 2 -l 1 -v`. How does the output change from run to run?

**What was tested.** Baseline behaviour of two threads each doing one vector add.

**Command.**
```bash
./bin/vector-deadlock -n 2 -l 1 -v
```

**Observed result.** Two threads each do `vectors[0] += vectors[1]` once.
Vector 1 is all `1`s and is never written; vector 0 starts at `0` and ends at
`2` (incremented once per thread). The printed *values* are identical every
run.

**Technical explanation.** The end state is deterministic here because both
adds are fully locked and both target the same vector in the same order, so the
two increments simply sum regardless of order (0→1→2 or 0→1→2). What genuinely
varies run to run is invisible in this reconstruction's before/after printing:
the **thread interleaving and scheduling**. In the original OSTEP code, verbose
mode prints *inside* `vector_add`, so the interleaving of those lines is what
"changes from run to run"; here the same nondeterminism exists at the scheduler
level but the observable numeric result is stable.

**Conclusion.** With one locked add per thread on the same vector, the result is
deterministic; only scheduling order varies. The interesting nondeterminism
appears once lock *ordering* conflicts (Q2).

---

## Q2. Add `-d` and raise `-l`. Does it always deadlock?

**What was tested.** Whether reversed lock ordering deadlocks, and whether it
does so deterministically.

**Commands.**
```bash
./bin/vector-deadlock -n 2 -l 1      -d   # usually completes
./bin/vector-deadlock -n 2 -l 100000 -d   # hangs (Ctrl-C / timeout)
```

**Observed result.** `-l 1 -d` almost always completes. `-l 100000 -d` hangs;
in the test suite it was killed by the 3 s timeout (rc 124) on the first
attempt. So: **no, not always** — it is probabilistic.

**Technical explanation.** With `-d`, even workers lock `v0→v1` and odd workers
lock `v1→v0`. Deadlock needs the interleaving where both hold their first lock
before either gets the second (circular wait). At `-l 1` that window occurs once
and is tiny, so it usually slips through; at high `-l` there are 100k windows
per thread, making the coincidence nearly certain.

**Conclusion.** Deadlock is a *latent* bug that appears probabilistically and
more often under load — the worst kind, because light testing misses it.

---

## Q3. How does changing `-n` change the outcome? Any `-n` that guarantees no deadlock?

**What was tested.** Effect of thread count on deadlock likelihood.

**Commands.**
```bash
./bin/vector-deadlock -n 1 -l 1000000 -d   # never deadlocks
./bin/vector-deadlock -n 8 -l 100000  -d   # deadlocks readily
```

**Observed result.** `-n 1` never deadlocks no matter how large `-l` is. More
threads → deadlock sooner and more reliably.

**Technical explanation.** `-n 1` guarantees safety: with a single thread there
is no second thread to form a cycle, and the two locks are always taken and
released by the same thread in order — circular wait is impossible. (`-n 0` is
rejected by the CLI.) For `-n ≥ 2` there exists both an even and, once `-n ≥ 2`,
an odd worker under `-d`, so opposite orderings coexist and a cycle can form.
More threads increase contention and the chance of the fatal interleaving.

**Conclusion.** Only `-n 1` guarantees no deadlock (by removing concurrency).
Any `-n ≥ 2` with `-d` can deadlock; higher `-n` makes it more likely.

---

## Q4. In `vector-global-order.c`, why does it avoid deadlock, and why the same-vector special case?

**What was tested.** Correctness of the total-ordering approach and the aliased
guard.

**Command.**
```bash
./bin/vector-global-order -n 8 -l 200000 -d --check   # completes, no hang
```

**Observed result.** Completes every time under `-d` (test step 7); no deadlock
observed.

**Technical explanation.** It removes **circular wait**: both locks are always
acquired in ascending `id` order regardless of which vector is dst or src, so
all threads agree on the order and the waits-for graph cannot contain a cycle.
The **same-vector special case** exists because a POSIX mutex is *not*
recursive: if `dst` and `src` alias, taking the lock twice from one thread
self-deadlocks. So we lock once and let the arithmetic double the element in
place.

**Conclusion.** A single global lock order provably eliminates deadlock; the
aliased guard prevents a thread from deadlocking against itself on a
non-recursive mutex.

---

## Q5. Run `-t -n 2 -l 100000 -d`. How long? How does time change with loops / threads?

**What was tested.** Timing and its scaling (using the deadlock-free
global-order build so it actually finishes).

**Commands.**
```bash
./bin/vector-global-order -t -n 2 -l 100000 -d
./bin/vector-global-order -t -n 2 -l 200000
./bin/vector-global-order -t -n 2 -l 400000
```

**Observed result.**

| loops (n=2) | seconds |
|---|---|
| 100,000 | 0.0142 |
| 200,000 | 0.0306 |
| 400,000 | 0.0597 |

Adding threads on *shared* vectors makes it slower, not faster
(0.0053 s at n=1 → 0.104 s at n=8, l=100k — see benchmark-analysis.md).

**Technical explanation.** Time scales ~linearly with loop count: each iteration
is one bounded critical section, so 2× iterations ≈ 2× time. Adding threads to
shared-vector work adds *contention* (all threads serialize through the same two
mutexes) rather than parallelism, so wall time rises.

**Conclusion.** Loops scale time linearly; threads on shared data scale it the
*wrong* way. Parallel speedup requires disjoint data (Q6).

---

## Q6. What happens with `-p`? Same vectors vs different vectors?

**What was tested.** Effect of the parallel flag (disjoint vector pairs).

**Commands.**
```bash
./bin/vector-global-order -t -p -n 8 -l 100000   # ~0.023 s
./bin/vector-global-order -t    -n 8 -l 100000   # ~0.104 s (shared)
```

**Observed result.** At n=8, parallel is ~4.5× faster than shared (0.023 s vs
0.104 s).

**Technical explanation.** `-p` gives each worker its own vector pair
`(2t, 2t+1)`, so there is no lock contention and the locks are essentially
uncontended overhead. Shared mode forces every thread through the same two
locks. Expected: parallel should scale with cores; shared should not — confirmed.

**Conclusion.** Independent data lets the strategies scale; shared data caps
them at single-lock throughput.

---

## Q7. In `vector-try-wait.c`: is the first `trylock` needed? Speed vs global order? Retries vs threads?

**What was tested.** Necessity of the first trylock, relative speed, and retry
growth.

**Commands.**
```bash
./bin/vector-try-wait -n 8 -l 100000            # retries: 0 (no -d)
for n in 2 4 8 16; do ./bin/vector-try-wait -n $n -l 50000 -d; done
```

**Observed result.** Retries with `-d`, l=50,000: n=2→215, n=4→1,027,
n=8→4,581, n=16→25,349. Without `-d`: **0 retries**. Speed is on par with (here
marginally faster than) global-order when retries are 0.

**Technical explanation.** **The first `trylock` is not needed.** The
deadlock-avoidance property comes solely from the *second* acquisition being
non-blocking; blocking on the first lock is safe because the thread holds
nothing. So this implementation uses a plain `mutex_lock` first and `trylock`
second. Retries grow super-linearly with threads because more threads mean more
frequent second-lock conflicts, each costing an unlock + backoff + restart.
When lock orders never conflict (no `-d`), the second trylock always succeeds,
so retries are 0 and it matches global-order's cost.

**Conclusion.** Only the second acquisition must be non-blocking. try-wait is
competitive when uncontended but wastes increasing work as contention rises.

---

## Q8. In `vector-avoid-hold-and-wait.c`: main problem? Performance with and without `-p`?

**What was tested.** The scalability cost of the global acquisition lock.

**Commands.**
```bash
./bin/vector-avoid-hold-and-wait -t -p -n 8 -l 100000   # ~0.130 s
./bin/vector-global-order        -t -p -n 8 -l 100000   # ~0.023 s
```

**Observed result.** At n=8 parallel, avoid-hold-and-wait is **0.130 s** vs
global-order's **0.023 s** — ~5.6× slower on identical disjoint work.

**Technical explanation.** The **main problem is serialization**: a single
global mutex must be held to acquire *any* vector lock, so even workers on
completely disjoint vectors queue behind one another during acquisition. This
throws away the parallelism `-p` is supposed to provide. Without `-p` (shared
data) it is closer to the others because shared data was already serialized by
contention.

**Conclusion.** It is deadlock-free and simple but scales the worst; the global
lock is a throughput bottleneck exactly when parallelism matters.

---

## Q9. `vector-nolock.c`: same semantics as the others? Why or why not?

**What was tested.** Correctness of the lock-free version.

**Commands.**
```bash
./bin/vector-nolock -n 8 -l 50000 --check         # -> check: FAIL, exit 5
make tsan && ./bin/tsan/vector-nolock -n 4 -l 20000   # -> data race
```

**Observed result.** `--check` **fails** (exit 5) under contention; TSan reports
a write/write **data race** in `vector_add_locked_region`.

**Technical explanation.** **No, not the same semantics.** `dst[i] += src[i]` is
a non-atomic read-modify-write. Under sharing, two threads can read the same old
value, each add, and both write back — one update is lost — so the final sum is
too small. The concurrent unsynchronized accesses are also a data race
(undefined behaviour), which TSan flags deterministically.

**Conclusion.** nolock does not preserve the locked strategies' semantics on
shared data; it silently loses updates and is UB.

---

## Q10. nolock performance: shared (no `-p`) vs disjoint (`-p`)?

**What was tested.** Speed and correctness of nolock in both modes.

**Commands.**
```bash
./bin/vector-nolock -t    -n 8 -l 100000   # shared:   ~0.127 s, WRONG
./bin/vector-nolock -t -p -n 8 -l 100000   # parallel: ~0.0076 s, correct
make tsan && ./bin/tsan/vector-nolock -p -n 4 -l 20000   # 0 races
```

**Observed result.** Parallel: **0.0076 s** at n=8 — the fastest of any
strategy — with **0 data races** under TSan. Shared: ~0.127 s and an *incorrect*
result (lost updates).

**Technical explanation.** With `-p` the vectors are disjoint, so there is no
sharing, no race, and no lock overhead — near-linear scaling and correctness.
Without `-p` the same code races on shared elements: fast but wrong. Speed here
is a direct consequence of skipping synchronization, which is only safe when
there is nothing to synchronize.

**Conclusion.** nolock is fastest and correct **only** when work is fully
disjoint (`-p`). On shared data it is fast and incorrect — a cautionary tale
that performance without correctness is worthless.
