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
python generate_figures.py [flags] intel-results/ intel
python generate_figures.py [flags] power-results/ power
```
```
optional arguments:
  --no-variance         hide the variance in plots
  --only-64             whether to plot only results for int64
```
