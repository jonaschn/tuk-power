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

void clear_cache() {
  std::vector<std::int8_t> clear;

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
static std::vector<T> generate_data(size_t size, bool randomInit)
{
    if (randomInit) {
        static std::uniform_int_distribution<T> distribution(std::numeric_limits<T>::min(),
                                                             std::numeric_limits<T>::max());
        static std::default_random_engine generator;

        std::vector<T> data(size);
        std::generate(data.begin(), data.end(), []() { return distribution(generator); });
        return data;
    } else {
        return std::vector<T>(size, 0);
    }
}

static volatile bool thread_flag = false;

std::vector<std::string> parseDataTypes(const std::string &dataTypes);

template <class T>
void thread_func(std::vector<T>& elements, int col_count, size_t start_index, size_t end_index){
    while(!thread_flag)
        ;
    for (size_t j = start_index; j < end_index; j++){
        volatile auto o3_trick = elements[j*col_count + 0]; // read first column
    }
}

template <class T>
std::vector<long long int> benchmark(size_t col_size, int col_count, int thread_count, bool cache, bool randomInit) {
    const size_t col_length = col_size / sizeof(T);

    // Split array into *thread_count* sequential parts
    std::vector<std::thread*> threads;
    size_t part_len = col_length / thread_count, overhang = col_length % thread_count;

    // Average multiple runs
    std::vector<long long int> times;

    int iterations = ITERATIONS;
    if (cache) {
        // first iteration is just for filling the cache
        iterations++;
    }

    for (int i = 0; i < iterations; i++) {
        auto attribute_vector = generate_data<T>(col_length * col_count, randomInit);
        size_t start_index = 0;
        if (!cache) {
            clear_cache();
        }

        for (int j = 0; j < thread_count; j++) {
            size_t end_index = start_index + part_len + (j < overhang ? 1 : 0);
            auto thread = new std::thread(thread_func<T>, std::ref(attribute_vector), col_count, start_index,
                                          end_index);
            threads.push_back(thread);
            start_index = end_index;
        }
        auto start = std::chrono::high_resolution_clock::now();
        thread_flag = true;

        for (std::thread *thread: threads) {
            (*thread).join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        if (!cache || i > 0) {
            times.push_back(time.count());
        }

        while (!threads.empty()) {
            delete threads.back();
            threads.pop_back();
        }
        thread_flag = false;
    }
    return times;
}

int main(int argc, char* argv[]) {
    // USAGE: ./benchmark [column count] [thread count] [cache] [random initialization]

    // col_count>1 --> row-based layout
    int col_count;

    int thread_count;
    bool cache;
    bool noCache;
    bool help;

    // random initialization instead of 0-initialization
    bool randomInit = false;
    if (argc > 4) {
        randomInit = atoi(argv[4]);
    }
    std::string dataTypes;

    Flags flags;

    flags.Var(col_count, 'c', "column-count", 1, "Number of columns to use");
    flags.Var(thread_count, 't', "thread-count", 1, "Number of threads");
    flags.Var(dataTypes, 'd', "data-types", std::string(""), "comma-separated list of types (e.g. 8 for int8_t)");
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

    if (dataTypes.length() > 0) {
        auto result = parseDataTypes(dataTypes);
        useInt8 = (std::find(result.begin(), result.end(), "8") != result.end());
        useInt16 = (std::find(result.begin(), result.end(), "16") != result.end());
        useInt32 = (std::find(result.begin(), result.end(), "32") != result.end());
        useInt64 = (std::find(result.begin(), result.end(), "64") != result.end());
    }

    std::cout << "Column size in KB,Data type,Time in ns" << std::endl;
    for (auto size: DB_SIZES){
        std::cerr << "benchmarking " << (size / 1024.0f) << std::endl;
        if (useInt8) {
            auto int8_time = benchmark<std::int8_t>(size, col_count, thread_count, cache, randomInit);
            for (long long int time: int8_time)
                std::cout << (size / 1024.0f) << ",int8," << time << std::endl;
        }

        if (useInt16) {
            auto int16_time = benchmark<std::int16_t>(size, col_count, thread_count, cache, randomInit);
            for (long long int time: int16_time)
                std::cout << (size / 1024.0f) << ",int16," << time << std::endl;
        }

        if (useInt32) {
            auto int32_time = benchmark<std::int32_t>(size, col_count, thread_count, cache, randomInit);
            for (long long int time: int32_time)
                std::cout << (size / 1024.0f) << ",int32," << time << std::endl;
        }

        if (useInt64) {
            auto int64_time = benchmark<std::int64_t>(size, col_count, thread_count, cache, randomInit);
            for (long long int time: int64_time)
                std::cout << (size / 1024.0f) << ",int64," << time << std::endl;
        }
    }

    return 0;
}

std::vector<std::string> parseDataTypes(const std::string &dataTypes) {
    std::vector<std::string> result;
    std::stringstream ss(dataTypes);
    while (ss.good()) {
        std::string substr;
        getline(ss, substr, ',');
        result.push_back(substr);
    }
    return result;
}
