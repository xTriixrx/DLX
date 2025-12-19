#ifndef DLX_PERFORMANCE_TEST_CONFIG_H
#define DLX_PERFORMANCE_TEST_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

struct SearchPerformanceCase
{
    uint32_t column_count = 0;
    uint32_t group_count = 0;
    uint32_t variants_per_group = 0;
};

struct PerformanceTestConfig
{
    bool search_performance_enabled = false;
    bool network_performance_enabled = false;
    std::string search_report_path = "tests/performance/dlx_search_performance.csv";
    uint32_t network_duration_seconds = 10;
    uint32_t network_request_clients = 1;
    uint32_t network_solution_clients = 1;
    uint32_t network_target_solution_rate = 1000;
    std::string network_problem_file = "tests/sudoku_example/sudoku_cover.txt";
    std::string network_report_path = "tests/performance/dlx_network_throughput.csv";
    std::string source_path = "tests/config/performance_config.yaml";
    bool config_loaded = false;
    std::vector<SearchPerformanceCase> search_cases = {
        {1000, 3, 2},
        {10000, 4, 2},
        {100000, 5, 2},
        {1000000, 6, 2},
        {1000, 5, 3},
        {10000, 6, 3},
        {100000, 7, 3},
    };
};

/**
 * Returns the lazily-loaded performance test configuration. The loader reads
 * the file specified by the DLX_PERF_CONFIG environment variable if set, or
 * falls back to tests/config/performance_config.yaml. Missing files leave all tests
 * disabled so suites skip by default.
 */
const PerformanceTestConfig& GetPerformanceTestConfig();

#endif
