# Hardware optimization benchmarks
Lecture Trends and Concepts I

## Build benchmark
```
cd benchmark/
cmake CMakeLists.txt
make
```
## Run benchmark
```
sudo ./run-benchmark.sh
```

## Plot graphs
```
pip install -r requirements.txt
python generate_figures.py --system intel [flags] intel-results/
python generate_figures.py --system power [flags] power-results/
```
```
optional arguments:
  --no-variance         hide the variance in plots
  --only-64             whether to plot only results for int64
```
