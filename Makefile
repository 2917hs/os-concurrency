# Makefile - builds the five vector-add strategies from a shared substrate.
#
# Each executable is one strategy object linked against the shared driver, CLI,
# vector, and common objects. Sanitizer builds recurse into this same Makefile
# with a different BUILDDIR/BINDIR and extra flags, so the build rules live in
# exactly one place.

CC      ?= cc
CSTD     = -std=c11
WARN     = -Wall -Wextra -Wpedantic -Werror
# _POSIX_C_SOURCE exposes clock_gettime/nanosleep/sched_yield on Linux too.
CPPFLAGS = -Iinclude -D_POSIX_C_SOURCE=200809L
OPT     ?= -O2
CFLAGS   = $(CSTD) $(WARN) $(OPT) -g $(CPPFLAGS)
LDFLAGS  = -pthread
SANFLAGS ?=

BUILDDIR ?= build
BINDIR   ?= bin

STRATEGIES = deadlock global-order try-wait avoid-hold-and-wait nolock
SHARED     = common vector cli driver

BINS        = $(addprefix $(BINDIR)/vector-,$(STRATEGIES))
SHARED_OBJS = $(addprefix $(BUILDDIR)/,$(addsuffix .o,$(SHARED)))

.PHONY: all clean debug asan tsan ubsan test benchmarks help

all: $(BINS)

# Link: one strategy object + the shared objects.
$(BINDIR)/vector-%: $(BUILDDIR)/vector-%.o $(SHARED_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(SANFLAGS) $^ -o $@ $(LDFLAGS)

# Compile any source file into the active build directory.
$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(SANFLAGS) -c $< -o $@

$(BUILDDIR) $(BINDIR):
	mkdir -p $@

# --- Convenience configurations -------------------------------------------

# Unoptimised build with assertions for debugging under a debugger.
debug:
	$(MAKE) all OPT="-O0" BUILDDIR=build/debug BINDIR=bin/debug

# AddressSanitizer + UBSan: use-after-free, leaks, overflow, UB.
asan:
	$(MAKE) all OPT="-O1" BUILDDIR=build/asan BINDIR=bin/asan \
		SANFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"

# ThreadSanitizer: data races (the point of the nolock demonstration).
tsan:
	$(MAKE) all OPT="-O1" BUILDDIR=build/tsan BINDIR=bin/tsan \
		SANFLAGS="-fsanitize=thread -fno-omit-frame-pointer"

# UndefinedBehaviorSanitizer on its own.
ubsan:
	$(MAKE) all OPT="-O1" BUILDDIR=build/ubsan BINDIR=bin/ubsan \
		SANFLAGS="-fsanitize=undefined -fno-omit-frame-pointer"

# --- Test / benchmark entry points ----------------------------------------

test: all
	./tests/run-tests.sh

benchmarks: all
	./scripts/run-benchmarks.sh

clean:
	rm -rf build bin benchmark-results/raw-results.csv

help:
	@echo "Targets:"
	@echo "  make            build all five binaries into bin/ (-O2)"
	@echo "  make debug      unoptimised build into bin/debug"
	@echo "  make asan       AddressSanitizer+UBSan build into bin/asan"
	@echo "  make tsan       ThreadSanitizer build into bin/tsan"
	@echo "  make ubsan      UndefinedBehaviorSanitizer build into bin/ubsan"
	@echo "  make test       build then run the test suite"
	@echo "  make benchmarks build then run the benchmark sweep"
	@echo "  make clean      remove build artifacts"
