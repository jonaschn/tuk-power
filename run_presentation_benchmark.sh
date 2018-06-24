#!/bin/bash
# set -x

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (necessary for prefetcher and SMT settings)"
  exit
fi

if lscpu | grep -E '^Model name\:\s+POWER'; then
  IS_POWER=true
  FOLDER="power-results-presentation"
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
  FOLDER="intel-results-presentation"
  SMT_CONFIGURATIONS=2
  SMT_SETTINGS=(1 2)
  THREAD_COUNTS=(15 30)
  # on the rapa.eaalab machine, these are the NUMA node0 CPUs
  CORE_BINDINGS=("`seq -s, 0 1 14`"
                 "`seq -s, 0 1 14`,`seq -s, 120 1 134`")
  MEMNODE=0
  PREFETCHER_SETTINGS=(1 0) # 0 means off, 1 means on (no other options available)

fi

SINGLE_THREAD_COUNT="${THREAD_COUNTS[0]}"


# 1) Baseline and data-layout-picking: single threaded, no prefetching, row- and colstore, all datatypes
DIR=1
mkdir -p "$FOLDER/$DIR/"
FILENAME=benchmark-prefetch0-smt1-thread"$SINGLE_THREAD_COUNT"
if $IS_POWER; then
  ppc64_cpu --smt=1 # SMT 1
  ppc64_cpu --dscr=1 # no prefetching
  
  numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$SINGLE_THREAD_COUNT" --data-types 8,16,32,64 > $FOLDER/$DIR/$FILENAME-colstore.csv
  numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 10 --thread-count "$SINGLE_THREAD_COUNT" --data-types 8,16,32,64 > $FOLDER/$DIR/$FILENAME-rowstore.csv

else
  CPU="${CORE_BINDINGS[0]}" # SMT 1
  benchmark/prefetching_intel -d # no prefetching

  numactl --physcpubind=$CPU --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$SINGLE_THREAD_COUNT" --data-types 8,16,32,64 > $FOLDER/$DIR/$FILENAME-colstore.csv
  numactl --physcpubind=$CPU --membind=$MEMNODE benchmark/benchmark --column-count 10 --thread-count "$SINGLE_THREAD_COUNT" --data-types 8,16,32,64 > $FOLDER/$DIR/$FILENAME-rowstore.csv
fi

# 2) prefetching: single threaded, with and without prefetching, row- and colstore, int8
DIR=2
mkdir -p "$FOLDER/$DIR/"


for PREFETCH_SET in "${PREFETCHER_SETTINGS[@]}"; do
  FILENAME=benchmark-prefetch"$PREFETCH_SET"-smt1-thread"$SINGLE_THREAD_COUNT"-8bit-colstore.csv

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

  if $IS_POWER; then
    ppc64_cpu --smt=1 # SMT 1  
    numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 1  --thread-count "$SINGLE_THREAD_COUNT" --data-types 8 > $FOLDER/$DIR/$FILENAME
    FILENAME=benchmark-prefetch"$PREFETCH_SET"-smt1-thread"$SINGLE_THREAD_COUNT"-8bit-rowstore.csv
    numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 10 --thread-count "$SINGLE_THREAD_COUNT" --data-types 8 > $FOLDER/$DIR/$FILENAME
  else
    CPU="${CORE_BINDINGS[0]}" # SMT 1
    numactl --physcpubind=$CPU --membind=$MEMNODE benchmark/benchmark --column-count 1  --thread-count "$SINGLE_THREAD_COUNT" --data-types 8 > $FOLDER/$DIR/$FILENAME
    FILENAME=benchmark-prefetch"$PREFETCH_SET"-smt1-thread"$SINGLE_THREAD_COUNT"-8bit-rowstore.csv
    numactl --physcpubind=$CPU --membind=$MEMNODE benchmark/benchmark --column-count 10 --thread-count "$SINGLE_THREAD_COUNT" --data-types 8 > $FOLDER/$DIR/$FILENAME
  fi
done

# 3) Datatype-picking: single threaded, prefetching, colstore, all datatypes (perf stat)
DIR=3
mkdir -p "$FOLDER/$DIR/"
EVENTS="cache-references,cache-misses,branches,branch-misses,task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions"
DATA_TYPES=(8 16 32 64)

for DATA_TYPE in "${DATA_TYPES[@]}"; do
  FILENAME=benchmark-prefetch1-smt1-thread"$SINGLE_THREAD_COUNT"-"$DATA_TYPE"bit-colstore
  echo $FILENAME
  if $IS_POWER; then
    ppc64_cpu --smt=1 # SMT 1
    ppc64_cpu --dscr=0 # prefetching

    perf stat -e "$EVENTS" \
    --output "$FOLDER/$DIR/$FILENAME-stats.txt" \
    numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$SINGLE_THREAD_COUNT" --data-types "$DATA_TYPE" > $FOLDER/$DIR/$FILENAME.csv

  else
    CPU="${CORE_BINDINGS[0]}" # SMT 1
    benchmark/prefetching_intel -e # no prefetching

    perf stat -e "$EVENTS" \
    --output "$FOLDER/$DIR/$FILENAME-stats.txt" \
    numactl --physcpubind=$CPU --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$SINGLE_THREAD_COUNT" --data-types "$DATA_TYPE" > $FOLDER/$DIR/$FILENAME.csv
  fi
done


# 4) multithreading: single- and multi-threading, with prefetching, colstore, int64
DIR=4
mkdir -p "$FOLDER/$DIR/"

# turn prefetcher on
if $IS_POWER; then
  ppc64_cpu --dscr=0
else
  benchmark/prefetching_intel -e
fi

for ((i=0; i<$SMT_CONFIGURATIONS; i++)); do
  NTHREADS="${THREAD_COUNTS[i]}"
  SMTLVL="${SMT_SETTINGS[i]}"


  FILENAME=benchmark-prefetch1-smt"$SMTLVL"-thread"$NTHREADS"-64bit-colstore.csv
  if $IS_POWER; then
    ppc64_cpu --smt="$SMTLVL" # set SMT level
    numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --data-types 64 > $FOLDER/$DIR/$FILENAME
  else
    CPU="${CORE_BINDINGS[i]}" # set SMT level
    numactl --physcpubind=$CPU --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --data-types 64 > $FOLDER/$DIR/$FILENAME
  fi
done

# Restore default settings
if $IS_POWER; then
  ppc64_cpu --dscr=0 # restore prefetcher settings
  ppc64_cpu --smt=4 # restore smt settings
  chmod 666 -R "$FOLDER"/ # necessary for files
  chmod +X -R "$FOLDER"/ # necessary for directories
else
  benchmark/prefetching_intel -e
fi
