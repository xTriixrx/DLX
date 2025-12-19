#include "core/tcp_server.h"
#include "core/binary.h"
#include "performance_test_config.h"
#include "tcp_test_utils.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <gtest/gtest.h>
#include <iomanip>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <system_error>
#include <string>
#include <thread>
#include <vector>

namespace binary = dlx::binary;

namespace {

using namespace tcp_test_utils;

class DlxTcpNetworkPerformanceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        dlx::TcpServerConfig config{0, 0};
        server_ = std::make_unique<dlx::DlxTcpServer>(config);
        if (!server_->start())
        {
            server_.reset();
            GTEST_SKIP() << "Unable to bind TCP server sockets in this environment";
        }
    }

    void TearDown() override
    {
        if (server_ != nullptr)
        {
            server_->stop();
            server_->wait();
        }
    }

    dlx::DlxTcpServer& server()
    {
        return *server_;
    }

private:
    std::unique_ptr<dlx::DlxTcpServer> server_;
};

size_t BucketForTimestamp(const std::chrono::steady_clock::time_point& start,
                          const std::chrono::steady_clock::time_point& timestamp,
                          size_t bucket_count)
{
    const auto elapsed = timestamp - start;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    if (bucket_count == 0)
    {
        return 0;
    }
    const size_t index = seconds < 0 ? 0 : static_cast<size_t>(seconds);
    return std::min(index, bucket_count - 1);
}

TEST_F(DlxTcpNetworkPerformanceTest, MeasuresEndToEndThroughput)
{
    const PerformanceTestConfig& config = GetPerformanceTestConfig();
    if (!config.network_performance_enabled)
    {
        GTEST_SKIP() << "Network performance tests disabled. Provide "
                     << config.source_path
                     << " with tests.network_performance.enabled: true and related parameters to run.";
    }

    const uint32_t duration_seconds = std::max<uint32_t>(1, config.network_duration_seconds);
    std::string ascii_cover = ReadFileToString(config.network_problem_file);
    ASSERT_FALSE(ascii_cover.empty()) << "Unable to read ASCII cover from " << config.network_problem_file;
    std::vector<uint8_t> payload = AsciiCoverToBytes(ascii_cover);
    ASSERT_FALSE(payload.empty()) << "Failed to encode ASCII cover into DLXB payload";

    const size_t bucket_count = static_cast<size_t>(duration_seconds);
    struct BucketStats
    {
        std::atomic<uint64_t> submitted{0};
        std::atomic<uint64_t> completed{0};
        std::atomic<uint64_t> latency_sum{0};
        std::atomic<uint64_t> latency_count{0};
    };
    std::unique_ptr<BucketStats[]> buckets(new BucketStats[bucket_count]);

    std::mutex submission_mutex;
    std::queue<std::chrono::steady_clock::time_point> submission_times;
    std::atomic<uint64_t> total_submitted{0};
    std::atomic<uint64_t> total_completed{0};

    auto start_time = std::chrono::steady_clock::now();
    auto stop_time = start_time + std::chrono::seconds(duration_seconds);

    auto record_submission = [&](const std::chrono::steady_clock::time_point& ts) {
        const size_t bucket = BucketForTimestamp(start_time, ts, bucket_count);
        buckets[bucket].submitted.fetch_add(1, std::memory_order_relaxed);
    };

    auto record_completion = [&](const std::chrono::steady_clock::time_point& ts, uint64_t latency_ns, bool include_latency) {
        const size_t bucket = BucketForTimestamp(start_time, ts, bucket_count);
        buckets[bucket].completed.fetch_add(1, std::memory_order_relaxed);
        if (include_latency)
        {
            buckets[bucket].latency_sum.fetch_add(latency_ns, std::memory_order_relaxed);
            buckets[bucket].latency_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<int> solution_fds;
    std::mutex solution_fd_mutex;
    const uint32_t listener_count = std::max<uint32_t>(1, config.network_solution_clients);
    solution_fds.assign(listener_count, -1);
    std::atomic<uint32_t> outstanding{0};
    std::mutex outstanding_mutex;
    std::condition_variable outstanding_cv;
    const uint32_t min_rate_target = std::max<uint32_t>(1, config.network_solution_clients);
    const uint32_t configured_target =
        std::max<uint32_t>(min_rate_target, config.network_target_solution_rate);
    const uint32_t derived_capacity = config.network_request_clients * config.network_solution_clients * 64;
    const uint32_t max_rate_target =
        std::max<uint32_t>(std::max<uint32_t>(min_rate_target, configured_target), derived_capacity);
    const uint32_t initial_rate =
        std::clamp(configured_target, min_rate_target, max_rate_target);
    const uint32_t min_rate_floor = std::max<uint32_t>(min_rate_target, std::max<uint32_t>(initial_rate / 4, 64u));
    std::atomic<uint32_t> target_rate{initial_rate};
    const uint32_t max_inflight = std::max<uint32_t>(min_rate_target * 4, max_rate_target);

    std::atomic<bool> stop_listeners{false};

    auto solution_worker = [&](uint32_t slot) {
        int fd = ConnectToPort(server().solution_port());
        if (fd < 0)
        {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(solution_fd_mutex);
            solution_fds[slot] = fd;
        }
        DescriptorInputStream stream(fd);
        binary::DlxSolutionRow row = {0};
        bool fatal_error = false;
        while (true)
        {
            if (stop_listeners.load(std::memory_order_relaxed)
                && total_completed.load(std::memory_order_relaxed) >= total_submitted.load(std::memory_order_relaxed))
            {
                break;
            }

            binary::DlxSolutionHeader header;
            if (binary::dlx_read_solution_header(stream, &header) != 0 || header.magic != DLX_SOLUTION_MAGIC)
            {
                fatal_error = true;
                stop_listeners.store(true, std::memory_order_relaxed);
                break;
            }

            while (true)
            {
                int status = binary::dlx_read_solution_row(stream, &row);
                if (status != 1)
                {
                    fatal_error = true;
                    stop_listeners.store(true, std::memory_order_relaxed);
                    break;
                }
                if (row.solution_id != 0 || row.entry_count != 0)
                {
                    continue;
                }

                const auto completion_time = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point submitted_at = completion_time;
                bool have_submission = false;
                {
                    std::lock_guard<std::mutex> lock(submission_mutex);
                    if (!submission_times.empty())
                    {
                        submitted_at = submission_times.front();
                        submission_times.pop();
                        have_submission = true;
                    }
                }

                uint64_t latency_ns = 0;
                if (have_submission)
                {
                    latency_ns = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(completion_time - submitted_at).count());
                }

                total_completed.fetch_add(1, std::memory_order_relaxed);
                record_completion(completion_time, latency_ns, have_submission);
                if (have_submission)
                {
                    const double latency_ms = static_cast<double>(latency_ns) / 1'000'000.0;
                    auto adjust_target = [&](bool increase) {
                        uint32_t current = target_rate.load(std::memory_order_relaxed);
                        while (true)
                        {
                            const uint32_t delta = std::max<uint32_t>(1, current / 16);
                            uint32_t desired = increase
                                                   ? current + delta
                                                   : (current > delta ? current - delta : min_rate_floor);
                            desired = std::clamp(desired, min_rate_floor, max_rate_target);
                            if (desired == current)
                            {
                                break;
                            }
                            if (target_rate.compare_exchange_weak(current, desired, std::memory_order_relaxed))
                            {
                                break;
                            }
                        }
                    };

                    if (latency_ms < 5.0)
                    {
                        adjust_target(true);
                    }
                    else if (latency_ms > 25.0)
                    {
                        adjust_target(false);
                    }
                }
                outstanding.fetch_sub(1, std::memory_order_relaxed);
                outstanding_cv.notify_one();
                break; // Wait for the next header for the following problem.
            }

            if (fatal_error)
            {
                break;
            }
        }

        dlx_free_solution_row(&row);
        int local_fd = -1;
        {
            std::lock_guard<std::mutex> lock(solution_fd_mutex);
            local_fd = solution_fds[slot];
            solution_fds[slot] = -1;
        }
        if (local_fd >= 0)
        {
            shutdown(local_fd, SHUT_RDWR);
            close(local_fd);
        }
    };

    std::vector<std::thread> listener_threads;
    listener_threads.reserve(listener_count);
    for (uint32_t i = 0; i < listener_count; ++i)
    {
        listener_threads.emplace_back(solution_worker, i);
    }

    auto submit_problem = [&]() -> bool {
        if (stop_listeners.load(std::memory_order_relaxed))
        {
            return false;
        }

        auto now = std::chrono::steady_clock::now();
        if (now >= stop_time)
        {
            return false;
        }

        if (!SendProblem(server().request_port(), payload))
        {
            return false;
        }

        const auto submission_time = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(submission_mutex);
            submission_times.push(submission_time);
        }
        total_submitted.fetch_add(1, std::memory_order_relaxed);
        record_submission(submission_time);
        outstanding.fetch_add(1, std::memory_order_relaxed);
        return true;
    };

    auto request_worker = [&]() {
        while (true)
        {
            if (stop_listeners.load(std::memory_order_relaxed))
            {
                break;
            }
            auto now = std::chrono::steady_clock::now();
            if (now >= stop_time)
            {
                break;
            }

            if (outstanding.load(std::memory_order_relaxed) >= max_inflight)
            {
                std::unique_lock<std::mutex> lock(outstanding_mutex);
                outstanding_cv.wait_for(lock, std::chrono::milliseconds(1));
                continue;
            }

            const uint64_t elapsed_seconds =
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count());
            if (elapsed_seconds >= duration_seconds)
            {
                break;
            }

            const size_t bucket = static_cast<size_t>(elapsed_seconds);
            const uint64_t submitted_this_bucket = buckets[bucket].submitted.load(std::memory_order_relaxed);
            const uint32_t desired_rate = target_rate.load(std::memory_order_relaxed);

            if (submitted_this_bucket >= desired_rate)
            {
                const auto next_second = start_time + std::chrono::seconds(elapsed_seconds + 1);
                const auto sleep_duration =
                    std::max(std::chrono::milliseconds(1),
                             std::chrono::duration_cast<std::chrono::milliseconds>(next_second - now));
                std::unique_lock<std::mutex> lock(outstanding_mutex);
                outstanding_cv.wait_for(lock, sleep_duration);
                continue;
            }

            if (!submit_problem())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    };

    const uint32_t request_clients = std::max<uint32_t>(1, config.network_request_clients);
    std::vector<std::thread> request_threads;
    request_threads.reserve(request_clients);
    for (uint32_t i = 0; i < request_clients; ++i)
    {
        request_threads.emplace_back(request_worker);
    }
    for (std::thread& thread : request_threads)
    {
        thread.join();
    }

    const auto completion_deadline = stop_time + std::chrono::seconds(5);
    while (total_completed.load(std::memory_order_relaxed) < total_submitted.load(std::memory_order_relaxed)
           && std::chrono::steady_clock::now() < completion_deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    stop_listeners.store(true, std::memory_order_relaxed);
    outstanding_cv.notify_all();
    for (size_t i = 0; i < solution_fds.size(); ++i)
    {
        int fd = -1;
        {
            std::lock_guard<std::mutex> lock(solution_fd_mutex);
            fd = solution_fds[i];
        }
        if (fd >= 0)
        {
            shutdown(fd, SHUT_RDWR);
        }
    }
    for (std::thread& thread : listener_threads)
    {
        thread.join();
    }

    const uint64_t submitted_total = total_submitted.load(std::memory_order_relaxed);
    const uint64_t completed_total = total_completed.load(std::memory_order_relaxed);
    EXPECT_GT(submitted_total, 0u);
    EXPECT_GT(completed_total, 0u);

    std::filesystem::path report_path(config.network_report_path);
    if (!report_path.parent_path().empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(report_path.parent_path(), ec);
    }
    std::ofstream report(config.network_report_path, std::ios::out | std::ios::trunc);
    ASSERT_TRUE(report.is_open()) << "Unable to open report path " << config.network_report_path;
    report << "Time Interval (s),Solution Rate,Average Solution Rate,Latency,Average Latency\n";

    auto format_rate = [](double value, bool fractional) {
        std::ostringstream oss;
        if (fractional)
        {
            oss << std::fixed << std::setprecision(2) << value;
        }
        else
        {
            oss << static_cast<long long>(std::llround(value));
        }
        oss << " s/s";
        return oss.str();
    };

    auto format_latency = [](double value_ms, bool fractional) {
        std::ostringstream oss;
        if (fractional)
        {
            oss << std::fixed << std::setprecision(2) << value_ms;
        }
        else
        {
            oss << static_cast<long long>(std::llround(value_ms));
        }
        oss << " ms";
        return oss.str();
    };

    uint64_t running_completed = 0;
    uint64_t running_latency_sum = 0;
    uint64_t running_latency_count = 0;

    for (size_t i = 0; i < bucket_count; ++i)
    {
        const uint64_t completed_this_second = buckets[i].completed.load(std::memory_order_relaxed);
        running_completed += completed_this_second;

        const uint64_t latency_sum = buckets[i].latency_sum.load(std::memory_order_relaxed);
        const uint64_t latency_count = buckets[i].latency_count.load(std::memory_order_relaxed);
        running_latency_sum += latency_sum;
        running_latency_count += latency_count;

        const double instantaneous_rate = static_cast<double>(completed_this_second);
        const double elapsed_seconds = static_cast<double>(i + 1);
        const double average_rate = elapsed_seconds > 0 ? static_cast<double>(running_completed) / elapsed_seconds : 0.0;

        const double bucket_latency_ms = latency_count > 0
                                             ? static_cast<double>(latency_sum) / static_cast<double>(latency_count) / 1'000'000.0
                                             : 0.0;
        const double average_latency_ms = running_latency_count > 0
                                              ? static_cast<double>(running_latency_sum)
                                                    / static_cast<double>(running_latency_count) / 1'000'000.0
                                              : 0.0;

        report << (i + 1) << ','
               << format_rate(instantaneous_rate, false) << ','
               << format_rate(average_rate, true) << ','
               << format_latency(bucket_latency_ms, false) << ','
               << format_latency(average_latency_ms, true) << '\n';
    }
    report << "\nConfiguration\n";
    report << "Duration Seconds," << config.network_duration_seconds << '\n';
    report << "Request Clients," << config.network_request_clients << '\n';
    report << "Solution Clients," << config.network_solution_clients << '\n';
    report << "Target Solution Rate," << config.network_target_solution_rate << " s/s\n";
    report << "Problem File," << config.network_problem_file << '\n';
    report.close();
}

} // namespace
