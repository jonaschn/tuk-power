#!/bin/bash
# set -x

if lscpu | grep -E '^Model name\:\s+POWER'; then
  IS_POWER=true
  FOLDER="power-results"
  SMT_SETTINGS=(1 2 4 8)
  CPUNODE=1
  MEMNODE=1
  SMT_CONFIGURATIONS=4
  THREAD_COUNTS=(12 24 48 96)
  # schedule 8 threads at most to avoid NUMA side-effects
  PREFETCHER_SETTINGS=(1 0) # be careful: 0 is standard, and 1 is off!
else
  IS_POWER=false
  FOLDER="intel-results"
  PREFETCHER_SETTINGS=(1 0)
  # on the rapa.eaalab machine, these are the NUMA node0 CPUs
  SMT_CONFIGURATIONS=2
  CORE_BINDINGS=("`seq -s, 0 1 8`"
                 "`seq -s, 0 1 8`,`seq -s, 120 8 134`")
  MEMNODE=0
  THREAD_COUNTS=(8 16)
  SMT_SETTINGS=(1 2)
fi
CACHE_SETTINGS=('cache' 'nocache')



mkdir -p "$FOLDER"

for PREFETCH_SET in "${PREFETCHER_SETTINGS[@]}"; do
  if $IS_POWER; then
    ppc64_cpu --dscr="$PREFETCH_SET"
  else
    if [[ $PREFETCH_SET ]]; then
      benchmark/prefetching_intel -d
    else
      benchmark/prefetching_intel -e
    fi
  fi  

  for CACHE_SET in "${CACHE_SETTINGS[@]}"; do
    for ((i=0; i<$SMT_CONFIGURATIONS; i++)); do
	  NTHREADS="${THREAD_COUNTS[i]}"
	  SMTLVL="${SMT_SETTINGS[i]}"


	  if $IS_POWER; then
	    ppc64_cpu --smt="$SMTLVL"
	  fi

	  FILENAME=benchmark-caching"$CACHE_SET"-prefetch"$PREFETCH_SET"-smt"$SMTLVL"-thread"$NTHREADS"
	  if $IS_POWER; then
      perf stat --big-num -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,L1-dcache-store-misses,L1-dcache-prefetches,L1-dcache-prefetch-misses,LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,LLC-prefetches,LLC-prefetch-misses,branches,branch-misses,task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions \
        --output "$FOLDER/$FILENAME-8bit-stats.txt" \
        numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --"$CACHE_SET" --data-types 8 > $FOLDER/$FILENAME-8bit.csv
        "$FOLDER/$FILENAME-8bit-stats.txt"

      perf stat --big-num -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,L1-dcache-store-misses,L1-dcache-prefetches,L1-dcache-prefetch-misses,LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,LLC-prefetches,LLC-prefetch-misses,branches,branch-misses,task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions \
        --output "$FOLDER/$FILENAME-64bit-stats.txt" \
        numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --"$CACHE_SET" --data-types 64 > $FOLDER/$FILENAME-64bit.csv
        "$FOLDER/$FILENAME-64bit-stats.txt"
    else
		CPU="${CORE_BINDINGS[i]}"
		perf stat --big-num -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,L1-dcache-store-misses,L1-dcache-prefetches,L1-dcache-prefetch-misses,LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,LLC-prefetches,LLC-prefetch-misses,branches,branch-misses,task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions \
		    --output "$FOLDER/$FILENAME-8bit-stats.txt" \
		    numactl --physcpubind=$CPU --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --"$CACHE_SET" --data-types 8 > $FOLDER/$FILENAME-8bit.csv
		    "$FOLDER/$FILENAME-8bit-stats.txt"

		perf stat --big-num -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,L1-dcache-store-misses,L1-dcache-prefetches,L1-dcache-prefetch-misses,LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,LLC-prefetches,LLC-prefetch-misses,branches,branch-misses,task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions \
		    --output "$FOLDER/$FILENAME-64bit-stats.txt" \
		    numactl --physcpubind=$CPU --membind=$MEMNODE benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --"$CACHE_SET" --data-types 64 > $FOLDER/$FILENAME-64bit.csv
        "$FOLDER/$FILENAME-64bit-stats.txt"
	  fi
	done
  done
done

if [ ! $IS_POWER ]; then
  benchmark/prefetching_intel -e  
else
  chmod 666 "$FOLDER"/*
fi
