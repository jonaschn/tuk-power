#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <numeric>
#include <chrono>
#include <thread>
#include <algorithm>
#include <random>

static const uint64_t KiB = 1024;
static const uint64_t MiB = 1024 * KiB;
static const uint64_t GiB = 1024 * MiB;
#ifdef _ARCH_PPC64
static const uint64_t DB_SIZES[] = {8, 16, 32, 64, 128, 512,
                                    1 * KiB, 4 * KiB, 16 * KiB, 64 * KiB,
                                    1 * MiB, 16 * MiB, 64 * MiB, 256 * MiB,
                                    1 * GiB, 4 * GiB,
                                    8 * GiB, 16 * GiB, 32 * GiB, 64 * GiB,
                                    128 * GiB, 256 * GiB};
#else
static const uint64_t DB_SIZES[] = {4 * GiB};
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

  for (uint i = 0; i < clear.size(); i++) {
    clear[i] += 1;
  }

  clear.resize(0);
}

template <class T>
static std::vector<T> generate_data(size_t size)
{
    return std::vector<T>(size, 0);
}

static volatile bool thread_flag = false;
template <class T>
void thread_func(std::vector<T>& elements, int col_count, uint64_t start_index, uint64_t end_index){
    while(!thread_flag)
        ;
    for (uint64_t j = start_index;j < end_index; j++){
        volatile auto o3_trick = elements[j*col_count + 0]; // read first column
    }
}

template <class T>
std::vector<long long int> benchmark(uint64_t col_size, int col_count, int thread_count, bool cache) {
    const uint64_t col_length = col_size / sizeof(T);

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
        auto attribute_vector = generate_data<T>(col_length * col_count);
        uint64_t start_index = 0;

        for (int j = 0; j < thread_count; j++) {
            uint64_t end_index = start_index + part_len + (j < overhang ? 1 : 0);
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
    // USAGE: ./benchmark [column count] [thread count] [cache]

    // col_count>1 --> row-based layout
    int col_count = 1;
    if (argc > 1) {
        col_count = atoi(argv[1]);
    }

    // threads
    int thread_count = 1;
    if (argc > 2) {
        thread_count = atoi(argv[2]);
    }

    // cache
    bool cache = false;
    if (argc > 3) {
        cache = atoi(argv[3]);
    }

    for (auto size: DB_SIZES){
        auto int64_time = benchmark<std::int64_t>(size, col_count, thread_count, cache);
    }

    return 0;
}