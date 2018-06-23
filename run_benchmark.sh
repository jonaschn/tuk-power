#!/bin/bash
# set -x

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (necessary for prefetcher and SMT settings)"
  exit
fi

if lscpu | grep -E '^Model name\:\s+POWER'; then
  IS_POWER=true
  FOLDER="power-results-perf"
  SMT_CONFIGURATIONS=4
  SMT_SETTINGS=(1 2 4 8)
  THREAD_COUNTS=(12 24 48 96)
  # on the FSOC Lab machine, these are the NUMA cpus: 80 88 96 104 112 120 128 136 144 152 160 168
  CPUNODE=1
  MEMNODE=1
  # be careful: 0 means off, and 1 means on (medium-depth hardware pre-fetching)
  # you can set other values for the DSCR register as well:
  # e.g. 2 for the least aggressive hardware pre-fetching or 7 for the most aggressive setting
  PREFETCHER_SETTINGS=(1 0)
else
  IS_POWER=false
  FOLDER="intel-results"
  SMT_CONFIGURATIONS=2
  SMT_SETTINGS=(1 2)
  THREAD_COUNTS=(15 30)
  # on the rapa.eaalab machine, these are the NUMA node0 CPUs
  CORE_BINDINGS=("`seq -s, 0 1 14`"
                 "`seq -s, 0 1 14`,`seq -s, 120 1 134`")
  MEMNODE=0
  PREFETCHER_SETTINGS=(1 0) # 0 means off, 1 means on (no other options available)

fi



mkdir -p "$FOLDER"

for PREFETCH_SET in "${PREFETCHER_SETTINGS[@]}"; do
  if $IS_POWER; then
    if [[ "$PREFETCH_SET" -eq 0 ]]; then
      ppc64_cpu --dscr=1 # turn prefetcher off
    elif [[ "$PREFETCH_SET" -eq 1 ]]; then
      ppc64_cpu --dscr=0 # turn prefetcher on (default)
    else
      ppc64_cpu --dscr="$PREFETCH_SET" # prefetcher dscr settings 2-7
    fi

  else
    if [[ "$PREFETCH_SET" -eq 0 ]]; then
      benchmark/prefetching_intel -d
    else
      benchmark/prefetching_intel -e
    fi
  fi  

  for ((i=0; i<$SMT_CONFIGURATIONS; i++)); do
    NTHREADS="${THREAD_COUNTS[i]}"
    SMTLVL="${SMT_SETTINGS[i]}"


    if $IS_POWER; then
      ppc64_cpu --smt="$SMTLVL"
    fi

    FILENAME=benchmark-prefetch"$PREFETCH_SET"-smt"$SMTLVL"-thread"$NTHREADS"
    EVENTS="cache-references,cache-misses,branches,branch-misses,task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions"
    if $IS_POWER; then

      perf stat --big-num -e "$EVENTS" \
      --output "$FOLDER/$FILENAME-8bit-stats.txt" \
      numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --data-types 8 > $FOLDER/$FILENAME-8bit-colstore.csv

      perf stat --big-num -e "$EVENTS" \
      --output "$FOLDER/$FILENAME-64bit-stats.txt" \
      numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --data-types 64 > $FOLDER/$FILENAME-64bit-colstore.csv

      #numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --data-types 8,16,32,64 > $FOLDER/$FILENAME-colstore.csv

      #numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 10 --thread-count "$NTHREADS" --data-types 8,16,32,64 > $FOLDER/$FILENAME-rowstore.csv
    else
      CPU="${CORE_BINDINGS[i]}"
      perf stat --big-num -e "$EVENTS" \
        --output "$FOLDER/$FILENAME-8bit-stats.txt" \
        numactl --physcpubind=$CPU --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --data-types 8 > $FOLDER/$FILENAME-8bit.csv

      perf stat --big-num -e "$EVENTS" \
        --output "$FOLDER/$FILENAME-64bit-stats.txt" \
        numactl --physcpubind=$CPU --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --data-types 64 > $FOLDER/$FILENAME-64bit.csv
    fi
  done
done

if $IS_POWER; then
  ppc64_cpu --dscr=0 # restore prefetcher settings
  ppc64_cpu --smt=4 # restore smt settings
  chmod 666 "$FOLDER"/*
else
  benchmark/prefetching_intel -e
fi
