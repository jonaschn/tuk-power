#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <numeric>
#include <chrono>
#include <thread>
#include <random>

static const uint64_t DB_SIZES[] = {8, 16, 32, 64, 128, 512, 1024, 4096, 16384, 65536,
                                    1048576, 16777216, 67108864, 268435456, 1073741824, 4294967296};
static const int ITERATIONS = 3;

void clear_cache() {
  std::vector<sdt::int8_t> clear = std::vector<>();

#ifdef _ARCH_PPC64
  // maximum cache size for a CPU is:
  // 512KB (L2 inclusive) + 96MB L3 + 128MB L4
  // ==> 224.5 MB ~256MB
  clear.resize(256 * 1024 * 1024, 1336);
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
    static std::uniform_int_distribution<T> distribution(std::numeric_limits<T>::min(),
                                                         std::numeric_limits<T>::max());
    static std::default_random_engine generator;

    std::vector<T> data(size);
    std::generate(data.begin(), data.end(), []() { return distribution(generator); });
    return data;
}

template <class T>
void thread_func(std::vector<T>& elements, int col_count, uint64_t start_index, uint64_t end_index){
    for (uint64_t j = start_index;j < end_index; j++){
        volatile auto o3_trick = elements[j*col_count + 0]; // read first column
    }
}

template <class T>
std::vector<long long int> benchmark(uint64_t col_size, int col_count, int thread_count) {
    const uint64_t col_length = col_size / sizeof(T);

    // Split array into *thread_count* sequential parts
    std::vector<std::thread*> threads;
    size_t part_len = col_length / thread_count, overhang = col_length % thread_count;

    // Average multiple runs
    std::vector<long long int> times;
    for (int i = 0; i < ITERATIONS; i++) {
        auto attribute_vector = generate_data<T>(col_length * col_count);
        uint64_t start_index = 0;
        clear_cache()

        auto start = std::chrono::high_resolution_clock::now();

        for(int j=0;j < thread_count;j++){
            uint64_t end_index = start_index + part_len + (j < overhang ? 1 : 0);
            auto thread = new std::thread(thread_func<T>, std::ref(attribute_vector), col_count, start_index, end_index);
            threads.push_back(thread);
            start_index = end_index;
        }

        for(std::thread* thread: threads)
            (*thread).join();

        auto end = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        times.push_back(time.count());
        threads.clear();
    }
    return times;
}

int main(int argc, char* argv[]) {
    // USAGE: ./benchmark <column count> <thread count>
    // col_count>1 --> row-based layout
    int col_count = 1;
    if(argc > 1)
        col_count = atoi(argv[1]);
    // threads
    int thread_count = 1;
    if(argc > 2)
        thread_count = atoi(argv[2]);

    std::cout << "Column size in KB,Data type,Time in ns" << std::endl;
    for (auto size: DB_SIZES){
        auto int8_time = benchmark<std::int8_t>(size, col_count, thread_count);
        for(long long int time: int8_time)
            std::cout << (size / 1024.0f) << ",int8," << time << std::endl;

        auto int16_time = benchmark<std::int16_t>(size, col_count, thread_count);
        for(long long int time: int16_time)
            std::cout << (size / 1024.0f) << ",int16," << time << std::endl;

        auto int32_time = benchmark<std::int32_t>(size, col_count, thread_count);
        for(long long int time: int32_time)
            std::cout << (size / 1024.0f) << ",int32," << time << std::endl;

        auto int64_time = benchmark<std::int64_t>(size, col_count, thread_count);
        for(long long int time: int64_time)
            std::cout << (size / 1024.0f) << ",int64," << time << std::endl;
    }

    return 0;
}
