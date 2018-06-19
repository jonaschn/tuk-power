ppc64_cpu --dscr=1


FOLDER="power-results-manual"
SMTLVL="1"
ppc64_cpu --smt="$SMTLVL"
NTHREADS="12"
FILENAME=benchmark-cache-prefetch0-smt"$SMTLVL"-thread"$NTHREADS"
numactl --cpunodebind=1 --membind=1 benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --cache --data-types 64 > $FOLDER/$FILENAME-64bit-colstore.csv

SMTLVL="2"
ppc64_cpu --smt="$SMTLVL"
NTHREADS="24"
FILENAME=benchmark-cache-prefetch0-smt"$SMTLVL"-thread"$NTHREADS"
numactl --cpunodebind=1 --membind=1 benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --cache --data-types 64 > $FOLDER/$FILENAME-64bit-colstore.csv

SMTLVL="4"
ppc64_cpu --smt="$SMTLVL"
NTHREADS="48"
FILENAME=benchmark-cache-prefetch0-smt"$SMTLVL"-thread"$NTHREADS"
numactl --cpunodebind=1 --membind=1 benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --cache --data-types 64 > $FOLDER/$FILENAME-64bit-colstore.csv

SMTLVL="8"
ppc64_cpu --smt="$SMTLVL"
NTHREADS="96"
FILENAME=benchmark-cache-prefetch0-smt"$SMTLVL"-thread"$NTHREADS"
numactl --cpunodebind=1 --membind=1 benchmark/benchmark --column-count 1 --thread-count "$NTHREADS" --cache --data-types 64 > $FOLDER/$FILENAME-64bit-colstore.csv



