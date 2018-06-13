# tuk-power

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
python generate_figures.py intel-results/ intel [flags]
python generate_figures.py power-results/ [flags]
```
