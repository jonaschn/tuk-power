#!/bin/bash -x

function join_by { local IFS="$1"; shift; echo "$*"; }

if lscpu | grep -E '^Model name\:\s+POWER'; then
#if true; then
  IS_POWER=true
  IS_INTEL=false

  # schedule 8 threads at most to avoid NUMA side-effects
  PREFETCHER_SETTINGS=(1 0) # be careful: 0 is standard, and 1 is off!
  SMT_SETTINGS=(1 2 4 8)
  THREAD_COUNTS=(8 16 32 64)
  # use seq start+offset 8 end for generating these sequences for taskset
  CORE_BINDINGS=("`seq -s, 0 8 63`"
                 "`seq -s, 0 8 63`,`seq -s, 1 8 63`"
                 "`seq -s, 0 8 63`,`seq -s, 1 8 63`,`seq -s, 2 8 63`,`seq -s, 3 8 63`"
                 "`seq -s, 0 63`")
  FOLDER="power-results"
else
  #TODO for Intel team
  echo "HALLO INTELBOIS"
fi

mkdir -p "$FOLDER"

if $IS_POWER; then
  for PREFETCH_SET in "${PREFETCHER_SETTINGS[@]}"; do
    sudo ppc64_cpu --dscr="$PREFETCH_SET"

    for INDEX in `seq 0 3`; do
      NTHREADS="${THREAD_COUNTS[$INDEX]}"
      SMTLVL="${SMT_SETTINGS[$INDEX]}"
      CORE_BIND="${CORE_BINDINGS[$INDEX]}"

      sudo ppc64_cpu --smt="$SMTLVL"

      FILENAME=benchmark-prefetch"$PREFETCH_SET"-smt"$SMTLVL"-thread"$NTHREADS"
      taskset -c "$CORE_BIND" benchmark/benchmark 1  "$NTHREADS" > $FOLDER/$FILENAME-colstore.csv
      taskset -c "$CORE_BIND" benchmark/benchmark 10 "$NTHREADS" > $FOLDER/$FILENAME-rowstore.csv
    done
  done
fi
