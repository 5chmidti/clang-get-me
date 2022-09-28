find_package(benchmark REQUIRED)

add_executable(get_me_benchmarks)
target_sources(get_me_benchmarks PRIVATE general.cpp)
target_link_libraries(get_me_benchmarks PUBLIC get_me_lib)
target_link_libraries(get_me_benchmarks PUBLIC benchmark::benchmark_main)
