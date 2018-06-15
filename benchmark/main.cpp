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

void clear_cache() {
  std::vector<std::int8_t> clear;

  // maximum cache size for a node is:
  // POWER8: 768 KiB L1 + 6 MiB L2 + 96 MiB L3 + up to 128MiB L4 (off-chip) = 230.75 MiB
  // Intel E7-8890 v2: 480 KiB L1 + 3.75 MiB L2 + 37.5 MiB L3 = 41.71875 MiB
  // --> 256MiB is enough to clear all cache levels
  clear.resize(256 * MiB, 42);

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
static std::vector<long long int> thread_times;

template <class T>
void thread_func(std::vector<T>& elements, int col_count, size_t start_index, size_t end_index, int thread_id){
    while(!thread_flag)
        ;
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t j = start_index; j < end_index; j++){
        volatile auto o3_trick = elements[j*col_count + 0]; // read first column
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    thread_times[thread_id] = time.count();
}

template <class T>
std::vector<long long int> benchmark(size_t col_size, int col_count, int thread_count, int iterations, bool cache, bool randomInit) {
    const size_t col_length = col_size / sizeof(T);

    // Split array into *thread_count* sequential parts
    std::vector<std::thread*> threads;
    size_t part_len = col_length / thread_count, overhang = col_length % thread_count;

    // Average multiple runs
    std::vector<long long int> times;

    if (cache) {
        // first iteration is just for filling the cache
        iterations++;
    }

    thread_times = std::vector<long long int>(thread_count);
    for (int i = 0; i < iterations; i++) {
        auto attribute_vector = generate_data<T>(col_length * col_count, randomInit);
        size_t start_index = 0;
        if (!cache) {
            clear_cache();
        }

        for (int j = 0; j < thread_count; j++) {
            size_t end_index = start_index + part_len + (j < overhang ? 1 : 0);
            auto thread = new std::thread(thread_func<T>, std::ref(attribute_vector), col_count, start_index,
                                          end_index, j);
            threads.push_back(thread);
            start_index = end_index;
        }
        thread_flag = true;

        for (std::thread *thread: threads) {
            (*thread).join();
        }

        if (!cache || i > 0) {
            long long int time = std::accumulate(thread_times.begin(), thread_times.end(), 0) / thread_count;
            times.push_back(time);
        }
        thread_times.clear();
        thread_times.resize(thread_count);

        while (!threads.empty()) {
            delete threads.back();
            threads.pop_back();
        }
        thread_flag = false;
    }
    return times;
}

int main(int argc, char* argv[]) {

    int col_count; // = 1 --> column-based layout, > 1 --> row-based layout
    int thread_count;
    int iterations;

    bool cache;
    bool noCache;
    bool randomInit;
    bool help;

    std::string dataTypes;

    Flags flags;

    flags.Var(col_count, 'c', "column-count", 1, "Number of columns to use");
    flags.Var(thread_count, 't', "thread-count", 1, "Number of threads");
    flags.Var(iterations, 'i', "iterations", 6, "Number of iterations");
    flags.Var(dataTypes, 'd', "data-types", std::string(""), "Comma-separated list of types (e.g. 8 for int8_t)");
    flags.Bool(cache, 'C', "cache", "Whether to enable the use of caching", "Choose one of them");
    flags.Bool(noCache, 'N', "nocache", "Whether to disable the use of caching", "Choose one of them");
    flags.Bool(randomInit, 'r', "random-init", "Initialize randomly instead of 0-initialization", "Optional");

    flags.Bool(help, 'h', "help", "Show this help and exit", "Help");

    if (!flags.Parse(argc, argv)) {
        flags.PrintHelp(argv[0]);
        return 1;
    } else if (help) {
        flags.PrintHelp(argv[0]);
        return 0;
    }

    if (!(cache ^ noCache)) {
        std::cout << "Exactly one caching option needs to be selected!\n" << std::endl;
        flags.PrintHelp(argv[0]);
        return 1;
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

    auto cached_str = cache ? "Cached" : "Uncached";
    auto thread_count_str = std::to_string(thread_count) + " threads";

    std::cout << "Column size in KB,Data type,Time in ns,Cache,Thread Count" << std::endl;
    for (auto size: DB_SIZES){
        std::cerr << "benchmarking " << (size / 1024.0f) << " KiB" << std::endl;
        if (useInt8) {
            auto int8_time = benchmark<std::int8_t>(size, col_count, thread_count, iterations, cache, randomInit);
            for (long long int time: int8_time)
                std::cout << (size / 1024.0f) << ",int8," << time << "," << cached_str << "," << thread_count_str << std::endl;
        }

        if (useInt16) {
            auto int16_time = benchmark<std::int16_t>(size, col_count, thread_count, iterations, cache, randomInit);
            for (long long int time: int16_time)
                std::cout << (size / 1024.0f) << ",int16," << time << "," << cached_str << "," << thread_count_str << std::endl;
        }

        if (useInt32) {
            auto int32_time = benchmark<std::int32_t>(size, col_count, thread_count, iterations, cache, randomInit);
            for (long long int time: int32_time)
                std::cout << (size / 1024.0f) << ",int32," << time << "," << cached_str << "," << thread_count_str << std::endl;
        }

        if (useInt64) {
            auto int64_time = benchmark<std::int64_t>(size, col_count, thread_count, iterations, cache, randomInit);
            for (long long int time: int64_time)
                std::cout << (size / 1024.0f) << ",int64," << time << "," << cached_str << "," << thread_count_str << std::endl;
        }
    }

    return 0;
}
