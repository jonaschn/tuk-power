#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <numeric>
#include <chrono>
#include <thread>

static const uint64_t DB_SIZES[] = {8, 16, 32, 64, 128, 512, 1024, 4096, 16384, 65536,
                                    1048576, 16777216, 67108864, 268435456, 1073741824, 4294967296};
static const int ITERATIONS = 3;

template <class T>
void thread_func(std::vector<T>& elements, int col_count, uint64_t start_index, uint64_t end_index){
    for (uint64_t j = start_index;j < end_index; j++){
        volatile auto o3_trick = elements[j*col_count + 0]; // read first column
    }
}

template <class T>
long long int benchmark(uint64_t col_size, T default_value, int col_count, int thread_count) {
    uint64_t col_length = col_size / sizeof(T);
    std::vector<T> attribute_vector(col_length * col_count, default_value);

    // Average multiple runs
    std::vector<long long int> times;
    for (int i = 0; i < ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        // Split array into *thread_count* sequential parts
        std::vector<std::thread*> threads;
        size_t part_len = col_length / thread_count,
               overhang = col_length % thread_count;
        uint64_t start_index = 0;
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
    }
    return std::accumulate(times.begin(), times.end(), (long long int) 0) / ITERATIONS;
}

int main(int argc, char* argv[]) {

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
        double int8_time = benchmark<std::int8_t>(size, 0, col_count, thread_count);
        std::cout << (size / 1024.0f) << ",int8," << int8_time << std::endl;

        double int16_time = benchmark<std::int16_t>(size, 0, col_count, thread_count);
        std::cout << (size / 1024.0f) << ",int16," << int16_time << std::endl;

        double int32_time = benchmark<std::int32_t>(size, 0, col_count, thread_count);
        std::cout << (size / 1024.0f) << ",int32," << int32_time << std::endl;

        double int64_time = benchmark<std::int64_t>(size, 0, col_count, thread_count);
        std::cout << (size / 1024.0f) << ",int64," << int64_time << std::endl;

//        double str_time = benchmark<std::string>(size, "Initial1");
//        std::cout << (size * sizeof("Initial1")) << ",string," << str_time << std::endl;
    }

    return 0;
}
