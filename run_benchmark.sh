#!/bin/bash -x

function join_by { local IFS="$1"; shift; echo "$*"; }

if lscpu | grep -E '^Model name\:\s+POWER'; then
  IS_POWER=true
  FOLDER="power-results"
  SMT_SETTINGS=(1 2 4 8)
  # use seq start+offset 8 end for generating these sequences for taskset
  CPUNODE=1
  MEMNODE=1
  SMT_CONFIGURATIONS=4
  THREAD_COUNTS=(12 24 48 96)
  # schedule 8 threads at most to avoid NUMA side-effects
  PREFETCHER_SETTINGS=(1 0) # be careful: 0 is standard, and 1 is off!
else
  IS_POWER=false
  FOLDER="intel-results"
  PREFETCHER_SETTINGS=(1 0) # be careful: 0 is standard, and 1 is off!
  # on the rapa.eaalab machine, these are the NUMA node0 CPUs
  SMT_CONFIGURATIONS=2
  CORE_BINDINGS=("`seq -s, 0 1 8`"
                 "`seq -s, 0 1 8`,`seq -s, 120 8 134`")
  THREAD_COUNTS=(8 16)
  SMT_SETTINGS=(1 2)
fi



mkdir -p "$FOLDER"

for PREFETCH_SET in "${PREFETCHER_SETTINGS[@]}"; do
  if $IS_POWER; then
	sudo ppc64_cpu --dscr="$PREFETCH_SET"
  else
    if [[ $PREFETCH_SET ]]; then
		benchmark/prefetching_intel -d
	else
		benchmark/prefetching_intel -e
	fi
  fi  

  for ((i=0; i<$SMT_CONFIGURATIONS; i++)); do
    NTHREADS="${THREAD_COUNTS[i]}"
    SMTLVL="${SMT_SETTINGS[i]}"

    if $IS_POWER; then
      sudo ppc64_cpu --smt="$SMTLVL"
    fi
	
    FILENAME=benchmark-prefetch"$PREFETCH_SET"-smt"$SMTLVL"-thread"$NTHREADS"
    numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark 1  "$NTHREADS" > $FOLDER/$FILENAME-colstore.csv
    numactl --cpunodebind=$CPUNODE --membind=$MEMNODE benchmark/benchmark 10 "$NTHREADS" > $FOLDER/$FILENAME-rowstore.csv
  done
done

if [ ! $IS_POWER ]; then
  benchmark/prefetching_intel -e  
fi
