#!/usr/bin/env bash
#
# run-tests.sh - functional test suite for the vector-add strategies.
#
# Covers: CLI validation, single- and multi-threaded correctness, the aliased
# / parallel configurations, the deadlock demonstration (bounded by a portable
# timeout so CI never hangs), and the nolock lost-update demonstration.
#
# Exit status is non-zero if any assertion fails. Nondeterministic phenomena
# (deadlock occurring, nolock losing updates) are retried and reported honestly:
# they are expected to manifest and the suite fails if they never do.

set -u
cd "$(dirname "$0")/.."

BIN=./bin
STRATEGIES=(deadlock global-order try-wait avoid-hold-and-wait nolock)
SAFE=(global-order try-wait avoid-hold-and-wait)   # deadlock-free + correct

pass=0
fail=0
ok()   { printf '  ok   - %s\n' "$1"; pass=$((pass + 1)); }
no()   { printf '  FAIL - %s\n' "$1"; fail=$((fail + 1)); }

# timeout_run SECONDS cmd... -> exits 124 if the command outlives the budget.
timeout_run() {
    local secs=$1; shift
    "$@" & local pid=$!
    local ticks=$((secs * 10))
    while kill -0 "$pid" 2>/dev/null; do
        sleep 0.1
        ticks=$((ticks - 1))
        if [ "$ticks" -le 0 ]; then
            kill -9 "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null
            return 124
        fi
    done
    wait "$pid"
}

# assert_rc EXPECTED NAME cmd...
assert_rc() {
    local want=$1 name=$2; shift 2
    "$@" >/dev/null 2>&1
    local got=$?
    if [ "$got" -eq "$want" ]; then ok "$name"; else
        no "$name (expected rc=$want, got rc=$got)"; fi
}

echo "==> Building (make all)"
make all >/dev/null || { echo "build failed"; exit 1; }

echo "==> 1. Binaries exist and --help works"
for s in "${STRATEGIES[@]}"; do
    [ -x "$BIN/vector-$s" ] && ok "vector-$s built" || no "vector-$s missing"
done
assert_rc 0 "help exits 0" "$BIN/vector-deadlock" --help

echo "==> 2. CLI validation (bad input rejected with rc=2)"
B=$BIN/vector-global-order
assert_rc 2 "missing value for -n"  "$B" -n
assert_rc 2 "zero threads (-n 0)"   "$B" -n 0
assert_rc 2 "negative (-n -1)"      "$B" -n -1
assert_rc 2 "non-number (-n abc)"   "$B" -n abc
assert_rc 2 "trailing junk (12x)"   "$B" -n 12x
assert_rc 2 "zero loops (-l 0)"     "$B" -l 0
assert_rc 2 "zero length"           "$B" --length 0
assert_rc 2 "overflow"              "$B" -n 99999999999999999999
assert_rc 2 "unknown option"        "$B" -z
assert_rc 0 "valid args accepted"   "$B" -n 3 -l 10

echo "==> 3. Single-threaded correctness (--check, n=1)"
for s in "${STRATEGIES[@]}"; do
    # With one thread there is no contention, so even nolock is correct.
    assert_rc 0 "vector-$s single-thread check" "$BIN/vector-$s" -n 1 -l 2000 --check
done

echo "==> 4. Multi-threaded correctness of the safe strategies (--check)"
for s in "${SAFE[@]}"; do
    assert_rc 0 "vector-$s 8 threads check" "$BIN/vector-$s" -n 8 -l 5000 --check
done

echo "==> 5. Parallel mode completes for every strategy (-p, disjoint work)"
for s in "${STRATEGIES[@]}"; do
    # -p gives each worker private vectors: no contention, no deadlock, and even
    # nolock is correct. Bounded by timeout as a safety net.
    if timeout_run 10 "$BIN/vector-$s" -p -n 4 -l 20000; then
        ok "vector-$s -p completes"
    else
        no "vector-$s -p did not complete"
    fi
done

echo "==> 6. Deadlock demonstration (bounded, retried)"
# The naive strategy under -d can circular-wait. It is nondeterministic, so try
# a few times; observing at least one hang demonstrates the bug. The timeout
# guarantees the suite itself never hangs.
deadlocked=no
for attempt in 1 2 3 4 5; do
    if ! timeout_run 3 "$BIN/vector-deadlock" -n 2 -l 1000000 -d; then
        deadlocked=yes; break
    fi
done
if [ "$deadlocked" = yes ]; then
    ok "vector-deadlock hangs under -d (circular wait reproduced)"
else
    no "vector-deadlock never hung in 5 attempts (expected a deadlock)"
fi

echo "==> 7. Safe strategies do NOT deadlock under -d"
for s in "${SAFE[@]}"; do
    if timeout_run 10 "$BIN/vector-$s" -n 4 -l 200000 -d; then
        ok "vector-$s completes under -d"
    else
        no "vector-$s hung under -d (should be deadlock-free)"
    fi
done

echo "==> 8. nolock loses updates on shared vectors (rc=5), retried"
lost=no
for attempt in 1 2 3; do
    "$BIN/vector-nolock" -n 8 -l 50000 --check >/dev/null 2>&1
    [ $? -eq 5 ] && { lost=yes; break; }
done
if [ "$lost" = yes ]; then
    ok "vector-nolock fails --check under contention (lost updates shown)"
else
    no "vector-nolock unexpectedly passed --check every time"
fi

echo
echo "==================================================="
printf "Results: %d passed, %d failed\n" "$pass" "$fail"
echo "==================================================="
[ "$fail" -eq 0 ]
