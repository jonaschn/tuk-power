#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <numeric>

static long DB_SIZES[] = {8, 16, 32, 64, 128, 512, 1024, 4096, 16384, 65536, 1048576, 16777216, 67108864, 268435456, 1073741824, 4294967296};
static int ITERATIONS = 3;

template <class T>
long long int benchmark(long col_size, T default_value) {
    std::vector<T> attribute_vector(col_size / sizeof(T), default_value);

    // Average multiple runs
    std::vector<long long int> times;
    for (int i = 0; i < ITERATIONS; i++) {
        auto start = std::chrono::steady_clock::now();
        for (int j = 0;j < attribute_vector.size(); j++){
            volatile auto o3_trick = attribute_vector[j];
        }
        auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start);
        times.push_back(time.count());
    }
    return std::accumulate(times.begin(), times.end(), (long long int) 0) / ITERATIONS;
}

int main() {

    std::cout << "Column size in KB,Data type,Time in ns" << std::endl;
    for (auto size: DB_SIZES){
        double int8_time = benchmark<std::int8_t>(size, 0);
        std::cout << (size / 1024.0f) << ",int8," << int8_time << std::endl;

        double int16_time = benchmark<std::int16_t>(size, 0);
        std::cout << (size / 1024.0f) << ",int16," << int16_time << std::endl;

        double int32_time = benchmark<std::int32_t>(size, 0);
        std::cout << (size / 1024.0f) << ",int32," << int32_time << std::endl;

        double int64_time = benchmark<std::int64_t>(size, 0);
        std::cout << (size / 1024.0f) << ",int64," << int64_time << std::endl;

//        double str_time = benchmark<std::string>(size, "Initial1");
//        std::cout << (size * sizeof("Initial1")) << ",string," << str_time << std::endl;
    }

    return 0;
}