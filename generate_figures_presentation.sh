#!/usr/bin/env bash
FOLDER="intel-results"
PLATFORM="intel"

echo "1) Baseline and data-layout-picking"
head -n 1 $FOLDER/1/benchmark-prefetch0-smt1-thread1-8bit-colstore.csv > $FOLDER/1.csv # grab one header
tail -n +2 -q $FOLDER/1/*.csv >> $FOLDER/1.csv
python3 generate_figures.py --no-variance $FOLDER/1.csv $PLATFORM --ylim 2 --singlecore
rm $FOLDER/1.csv

echo "2a) Datatype-picking: single threaded, no prefetching, colstore, all datatypes (perf stat)"
head -n 1 $FOLDER/2a/benchmark-prefetch1-smt1-thread1-8bit-colstore.csv > $FOLDER/2a.csv # grab one header
tail -n +2 -q $FOLDER/2a/*.csv >> $FOLDER/2a.csv
python3 generate_figures.py --no-variance $FOLDER/2a.csv $PLATFORM --ylim 16 --singlecore
rm $FOLDER/2a.csv

echo "3) Prefetching: single threaded, with and w/o prefetching, row and colstore, int64"
head -n 1 $FOLDER/3/benchmark-prefetch1-smt1-thread1-64bit-colstore.csv | sed 's/$/,Prefetching/' > $FOLDER/3.csv # grab one header
tail -n +2 -q $FOLDER/3/benchmark-prefetch0*.csv >> $FOLDER/3/prefetch0.csv
tail -n +2 -q $FOLDER/3/benchmark-prefetch1*.csv >> $FOLDER/3/prefetch1.csv
sed 's/$/,No Prefetching/' $FOLDER/3/prefetch0.csv >> $FOLDER/3.csv
sed 's/$/,With Prefetching/' $FOLDER/3/prefetch1.csv >> $FOLDER/3.csv
python3 generate_figures.py --no-variance $FOLDER/3.csv $PLATFORM --ylim 16 --singlecore --dashed
rm $FOLDER/3.csv $FOLDER/3/prefetch*.csv

echo "4) multicore: single-threaded, with prefetching, colstore, int64, multiple cores"
head -n 1 $FOLDER/4/benchmark-prefetch1-smt1-thread1-64bit-colstore.csv > $FOLDER/4.csv # grab one header
tail -n +2 -q $FOLDER/4/*.csv >> $FOLDER/4.csv
python3 generate_figures.py --no-variance $FOLDER/4.csv $PLATFORM --ylim 200
rm $FOLDER/4.csv

echo "5) multithreading: single- and multi-threading, with prefetching, colstore, int64"
head -n 1 $FOLDER/5a/benchmark-prefetch1-smt1-thread15-64bit-colstore.csv > $FOLDER/5a.csv # grab one header
tail -n +2 -q $FOLDER/5a/*.csv >> $FOLDER/5a.csv
python3 generate_figures.py --no-variance $FOLDER/5a.csv $PLATFORM --ylim 350
rm $FOLDER/5a.csv