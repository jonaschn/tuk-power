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
/* POWER8 */
static const size_t DB_SIZES[] = {8 * KiB, 16 * KiB, 32 * KiB, 48 * KiB, 64 * KiB, 96 * KiB, 128 * KiB,
                                    256 * KiB, 512 * KiB, 768 * KiB, /* L1 cache limit */
                                    1 * MiB, 2 * MiB, 4 * MiB, 6 * MiB, /* L2 cache limit */
                                    16 * MiB, 64 * MiB, 96 * MiB, /* L3 cache limit */
                                    112 * MiB, 128 * MiB, /* L4 cache limit */
                                    256 * MiB, 1 * GiB, 4 * GiB,
                                    /*8 * GiB, 16 * GiB, 32 * GiB, 64 * GiB,
                                    128 * GiB, 256 * GiB*/
                                    };
#else
/* Intel E7-8890 v2 */
static const size_t DB_SIZES[] = {8 * KiB, 16 * KiB, 32 * KiB, 48 * KiB, 64 * KiB, 96 * KiB, 128 * KiB,
                                    256 * KiB, 384 * KiB, 480 * KiB, /* L1 cache limit*/
                                    512 * KiB, 1 * MiB, 2* MiB, 3840 * KiB, /* L2 cache limit */
                                    4 * MiB, 8 * MiB, 16 * MiB, 32 * MiB, 38400 * KiB, /* L3 cache limit */
                                    64 * MiB, 128 * MiB, 256 * MiB, 1 * GiB, 4 * GiB};
#endif

static const float ITERATIONS_FACTOR = 1e5;

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

void clearCache() {
  vector<int8_t> clear;

  // maximum cache size for a node is:
  // POWER8: 768 KiB L1 + 6 MiB L2 + 96 MiB L3 + up to 128MiB L4 (off-chip) = 230.75 MiB
  // Intel E7-8890 v2: 480 KiB L1 + 3.75 MiB L2 + 37.5 MiB L3 = 41.71875 MiB
  // --> 256MiB is enough to clear all cache levels
  clear.resize(256 * MiB, 42);

  for (auto entry: clear) {
    entry++;
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

static vector<long long int> threadTimes;

template <class T>
void threadFunc(vector<T>& elements, int colCount, size_t startIndex, size_t endIndex, int threadId, int iterations, int sampleSize){
    while (!threadFlag){};
    for (int s = 0; s < sampleSize; s++) {
      auto start = chrono::high_resolution_clock::now();
      for (int i = 0; i < iterations; i++) {
          for (size_t j = startIndex; j < endIndex; j++) {
              volatile auto o3Trick = elements[j*colCount + 0]; // read first column
          }
      }
      auto end = chrono::high_resolution_clock::now();
      auto time = chrono::duration_cast<chrono::nanoseconds>(end - start);
      threadTimes[threadId*sampleSize + s] = time.count() / iterations;
    }
}

template <class T>
void printResults(vector<long long int> times, size_t size, int threadCount, int colCount) {
    auto dataType = "int" + to_string(sizeof(T) * 8);
    auto threadCountStr = to_string(threadCount) + " threads";
    auto rowStoreStr = colCount > 1 ? "Row store" : "Column store";
    for (auto &time: times) {
        cout << (size / 1024.0f) << "," << dataType << "," << time << "," << threadCountStr << "," << rowStoreStr << endl;
    };
}

template <class T>
void benchmark(size_t colSize, int colCount, int threadCount, int iterations, int sampleSize, bool randomInit) {
    const size_t colLength = colSize / sizeof(T);

    // Split array into *threadCount* sequential parts
    vector<thread*> threads;
    size_t partLength = colLength / threadCount, overhang = colLength % threadCount;

    auto attributeVector = generateData<T>(colLength * colCount, randomInit);
    threadTimes.resize(threadCount*sampleSize);

    size_t startIndex = 0;
    for (int j = 0; j < threadCount - 1; j++) {
        size_t endIndex = startIndex + partLength + (j < overhang ? 1 : 0);
        auto threadInstance = new thread(threadFunc<T>, ref(attributeVector), colCount, startIndex,
                                         endIndex, j, iterations, sampleSize);
        threads.push_back(threadInstance);
        startIndex = endIndex;
    }


    // run threadFunc on main thread
    int j = threadCount - 1;
    size_t endIndex = startIndex + partLength + (j < overhang ? 1 : 0);

    threadFlag = true;
    threadFunc<T>(attributeVector, colCount, startIndex, endIndex, j, iterations, sampleSize);

    for (thread *thread: threads) {
        (*thread).join();
    }

    while (!threads.empty()) {
        delete threads.back();
        threads.pop_back();
    }
    threadFlag = false;

    // Average per run
    vector<long long int> times;
    for (int s=0; s < sampleSize; s++) {
        long long int time = 0;
        for (int j=0; j<threadCount; j++)
            time += threadTimes[j*sampleSize + s];
        times.push_back(time / threadCount);
    }

    printResults<T>(times, colSize, threadCount, colCount);
}

int main(int argc, char* argv[]) {
    int colCount; // = 1 --> column-based layout, > 1 --> row-based layout
    int threadCount;
    int iterations;
    int sampleSize;
    bool randomInit;
    bool help;

    string dataTypes;
    Flags flags;
    flags.Var(colCount, 'c', "column-count", 1, "Number of columns to use");
    flags.Var(threadCount, 't', "thread-count", 1, "Number of threads");
    flags.Var(iterations, 'i', "iterations", 0, "Number of inner iterations");
    flags.Var(sampleSize, 's', "sample-size", 10, "Number of measurements");
    flags.Var(dataTypes, 'd', "data-types", string(""), "Comma-separated list of types (e.g. 8 for int8_t)");
    flags.Bool(randomInit, 'r', "random-init", "Initialize randomly instead of 0-initialization", "Optional");
    flags.Bool(help, 'h', "help", "Show this help and exit", "Help");

    if (!flags.Parse(argc, argv)) {
        flags.PrintHelp(argv[0]);
        return 1;
    } else if (help) {
        flags.PrintHelp(argv[0]);
        return 0;
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

    cout << "Column size in KB,Data type,Time in ns,Thread Count,DB type" << endl;
    for (auto size: DB_SIZES){
        cerr << "benchmarking " << (size / 1024.0f) << " KiB" << endl;

        // For smallest size, iterations will be ITERATIONS_FACTOR. For double the size half of that etc. (but at least 6)
        int dynamicIterations = iterations == 0 ? max(6, (int) (ITERATIONS_FACTOR / size * DB_SIZES[0])) : iterations;

        if (useInt8) {
            benchmark<int8_t>(size, colCount, threadCount, dynamicIterations, sampleSize, randomInit);
        }
        if (useInt16) {
            benchmark<int16_t>(size, colCount, threadCount, dynamicIterations, sampleSize, randomInit);
        }
        if (useInt32) {
            benchmark<int32_t>(size, colCount, threadCount, dynamicIterations, sampleSize, randomInit);
        }
        if (useInt64) {
            benchmark<int64_t>(size, colCount, threadCount, dynamicIterations, sampleSize, randomInit);
        }
    }

    return 0;
}
