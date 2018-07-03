#!/usr/bin/env bash
python3 generate_figures.py --no-variance intel-results/1/ intel --ylim 30
head -n 1 intel-results/2/benchmark-prefetch0-smt1-thread15-8bit-colstore.csv intel-results/2.csv
tail -n +1 -q intel-results/2/*.csv >> intel-results/2.csv
python3 generate_figures.py --no-variance intel-results/2.csv intel --ylim 30
python3 generate_figures.py --no-variance intel-results/3/ intel --ylim 200
python3 generate_figures.py --no-variance intel-results/4/ intel --ylim 350