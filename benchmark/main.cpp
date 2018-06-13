#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <numeric>
#include <chrono>
#include <thread>
#include <algorithm>
#include <random>
#include "flags.h"

using namespace std;

static const size_t KiB = 1024;
static const size_t MiB = 1024 * KiB;
static const size_t GiB = 1024 * MiB;
#ifdef _ARCH_PPC64
static const size_t DB_SIZES[] = {8, 16, 32, 64, 128, 512,
                                    1 * KiB, 4 * KiB, 16 * KiB, 64 * KiB,
                                    1 * MiB, 16 * MiB, 64 * MiB, 256 * MiB,
                                    1 * GiB, 4 * GiB,
                                    8 * GiB, 16 * GiB, 32 * GiB, 64 * GiB,
                                    128 * GiB, 256 * GiB};
#else
static const size_t DB_SIZES[] = {4 * KiB, 16 * KiB, 64 * KiB,
                                  1 * MiB, 16 * MiB, 64 * MiB, 256 * MiB,
                                  1 * GiB, 4 * GiB};
#endif
static const int ITERATIONS = 6;

void clearCache() {
  vector<int8_t> clear;

#ifdef _ARCH_PPC64
  // maximum cache size for a CPU is:
  // 512KB (L2 inclusive) + 96MB L3 + 128MB L4
  // ==> 224.5 MB ~256MB
  clear.resize(256 * 1024 * 1024, 42);
#else
  clear.resize(500 * 1000 * 1000, 42);
#endif

  for (size_t i = 0; i < clear.size(); i++) {
    clear[i] += 1;
  }

  clear.resize(0);
}

template <class T>
static vector<T> generateData(size_t size, bool randomInit)
{
    if (randomInit) {
        static uniform_int_distribution<T> distribution(numeric_limits<T>::min(), numeric_limits<T>::max());
        static default_random_engine generator;

        vector<T> data(size);
        generate(data.begin(), data.end(), []() { return distribution(generator); });
        return data;
    } else {
        return vector<T>(size, 0);
    }
}

static volatile bool threadFlag = false;

vector<string> parseDataTypes(const string &dataTypes);

template <class T>
void thread_func(vector<T>& elements, int colCount, size_t startIndex, size_t endIndex){
    while(!threadFlag)
        ;
    for (size_t j = startIndex; j < endIndex; j++){
        volatile auto o3Trick = elements[j*colCount + 0]; // read first column
    }
}

template <class T>
vector<uint64_t> benchmark(size_t colSize, int colCount, int threadCount, bool cache, bool randomInit) {
    const size_t colLength = colSize / sizeof(T);

    // Split array into *threadCount* sequential parts
    vector<thread*> threads;
    size_t partLength = colLength / threadCount, overhang = colLength % threadCount;

    // Average multiple runs
    vector<uint64_t> times;

    int iterations = ITERATIONS;
    if (cache) {
        // first iteration is just for filling the cache
        iterations++;
    }

    for (int i = 0; i < iterations; i++) {
        auto attributeVector = generateData<T>(colLength * colCount, randomInit);
        size_t startIndex = 0;
        if (!cache) {
            clearCache();
        }

        for (int j = 0; j < threadCount; j++) {
            size_t endIndex = startIndex + partLength + (j < overhang ? 1 : 0);
            auto thread = new thread(thread_func<T>, ref(attributeVector), colCount, startIndex,
                                          endIndex);
            threads.push_back(thread);
            startIndex = endIndex;
        }
        auto start = chrono::high_resolution_clock::now();
        threadFlag = true;

        for (thread *thread: threads) {
            (*thread).join();
        }

        auto end = chrono::high_resolution_clock::now();
        auto time = chrono::duration_cast<chrono::nanoseconds>(end - start);
        if (!cache || i > 0) {
            times.push_back(time.count());
        }

        while (!threads.empty()) {
            delete threads.back();
            threads.pop_back();
        }
        threadFlag = false;
    }
    return times;
}

vector<string> parseDataTypes(const string &dataTypes) {
    vector<string> result;
    stringstream ss(dataTypes);
    while (ss.good()) {
        string substr;
        getline(ss, substr, ',');
        result.push_back(substr);
    }
    return result;
}

void printResults(vector<uint64_t> times, size_t size, const string &dataType) {
    for (auto &time: times) {
        cout << (size / 1024.0f) << "," << dataType << "," << time << endl;
    };
}

int main(int argc, char* argv[]) {
    // USAGE: ./benchmark [column count] [thread count] [cache] [random initialization]

    // colCount > 1 --> row-based layout
    int colCount;
    int threadCount;
    bool cache;
    bool noCache;
    bool help;

    // random initialization instead of 0-initialization
    bool randomInit = false;
    if (argc > 4) {
        randomInit = atoi(argv[4]);
    }
    string dataTypes;

    Flags flags;

    flags.Var(colCount, 'c', "column-count", 1, "Number of columns to use");
    flags.Var(threadCount, 't', "thread-count", 1, "Number of threads");
    flags.Var(dataTypes, 'd', "data-types", string(""), "comma-separated list of types (e.g. 8 for int8_t)");
    flags.Bool(cache, 'C', "cache", "Whether to enable the use of caching", "Group 2");
    flags.Bool(noCache, 'N', "nocache", "Whether to disable the use of caching", "Group 2");
    flags.Bool(randomInit, 'r', "random-init", "Initialize randomly", "Group 2");

    flags.Bool(help, 'h', "help", "show this help and exit", "Group 3");

    if (!flags.Parse(argc, argv)) {
        flags.PrintHelp(argv[0]);
        return 1;
    } else if (help) {
        flags.PrintHelp(argv[0]);
        return 0;
    }

    if (noCache) {
        if (cache) {
            flags.PrintHelp(argv[0]);
            return 1;
        }
        cache = false;
    }

    bool useInt8 = true;
    bool useInt16 = true;
    bool useInt32 = true;
    bool useInt64 = true;

    if (!dataTypes.empty()) {
        auto result = parseDataTypes(dataTypes);
        useInt8 = (find(result.begin(), result.end(), "8") != result.end());
        useInt16 = (find(result.begin(), result.end(), "16") != result.end());
        useInt32 = (find(result.begin(), result.end(), "32") != result.end());
        useInt64 = (find(result.begin(), result.end(), "64") != result.end());
    }

    cout << "Column size in KB,Data type,Time in ns" << endl;
    for (auto size: DB_SIZES){
        cerr << "benchmarking " << (size / 1024.0f) << endl;
        if (useInt8) {
            printResults(benchmark<int8_t>(size, colCount, threadCount, cache, randomInit), size, "int8");
        }
        if (useInt16) {
            printResults(benchmark<int16_t >(size, colCount, threadCount, cache, randomInit), size, "int16");
        }
        if (useInt32) {
            printResults(benchmark<int32_t>(size, colCount, threadCount, cache, randomInit), size, "int32");
        }
        if (useInt64) {
            printResults(benchmark<int64_t>(size, colCount, threadCount, cache, randomInit), size, "int64");
        }
    }

    return 0;
}
