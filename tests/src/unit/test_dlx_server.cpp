#include "core/tcp_server.h"
#include "core/binary.h"
#include "ascii_binary_utils.h"
#include "tcp_test_utils.h"
#include <arpa/inet.h>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace tcp_test_utils;

constexpr char kExpectedSudokuRows[] =
    "1 2 8 24 31 32 33 47 48 60 64 75 87 88 95 96 89 97 103 93 99 104 105 113 "
    "73 114 124 128 138 52 53 7 12 45 50 58 63 79 76 67 71 83 106 109 116 119 "
    "34 40 17 16 21 5 27 28 44 122 127 136 140 129 141 142 143 148 151 152 153 "
    "154 156 157 144 158 161 164 170 171 175 177 178 182 183";

class DlxTcpServerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        dlx::TcpServerConfig config{0, 0};
        server_ = std::make_unique<dlx::DlxTcpServer>(config);
        if (!server_->start())
        {
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

TEST_F(DlxTcpServerTest, StreamsSolutionsToSingleClient)
{
    auto expected = ParseRowList(kExpectedSudokuRows);
    std::promise<std::vector<uint32_t>> rows_promise;
    auto future = rows_promise.get_future();

    std::thread solution_thread([&]() {
        int fd = ConnectToPort(server().solution_port());
        ASSERT_GE(fd, 0);
        DescriptorInputStream stream(fd);
        rows_promise.set_value(ReadProblemSolution(stream));
        close(fd);
    });

    std::string ascii_cover = ReadFileToString("tests/sudoku_example/sudoku_cover.txt");
    std::vector<uint8_t> payload = AsciiCoverToBytes(ascii_cover);
    ASSERT_FALSE(payload.empty());

    ASSERT_TRUE(SendProblem(server().request_port(), payload));

    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    std::vector<uint32_t> rows = future.get();
    EXPECT_EQ(rows, expected);

    solution_thread.join();
}

TEST_F(DlxTcpServerTest, BroadcastsToMultipleClients)
{
    auto expected = ParseRowList(kExpectedSudokuRows);

    std::promise<std::vector<uint32_t>> promise_a;
    std::promise<std::vector<uint32_t>> promise_b;
    auto future_a = promise_a.get_future();
    auto future_b = promise_b.get_future();

    std::thread client_a([&]() {
        int fd = ConnectToPort(server().solution_port());
        ASSERT_GE(fd, 0);
        DescriptorInputStream stream(fd);
        promise_a.set_value(ReadProblemSolution(stream));
        close(fd);
    });

    std::thread client_b([&]() {
        int fd = ConnectToPort(server().solution_port());
        ASSERT_GE(fd, 0);
        DescriptorInputStream stream(fd);
        promise_b.set_value(ReadProblemSolution(stream));
        close(fd);
    });

    std::vector<uint8_t> payload = AsciiCoverToBytes(ReadFileToString("tests/sudoku_example/sudoku_cover.txt"));
    ASSERT_FALSE(payload.empty());
    ASSERT_TRUE(SendProblem(server().request_port(), payload));

    ASSERT_EQ(future_a.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    ASSERT_EQ(future_b.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_EQ(future_a.get(), expected);
    EXPECT_EQ(future_b.get(), expected);

    client_a.join();
    client_b.join();
}

TEST_F(DlxTcpServerTest, ReusesSolutionSocketAcrossProblems)
{
    auto expected = ParseRowList(kExpectedSudokuRows);
    std::promise<std::vector<std::vector<uint32_t>>> promise;
    auto future = promise.get_future();

    std::thread solution_thread([&]() {
        int fd = ConnectToPort(server().solution_port());
        ASSERT_GE(fd, 0);
        DescriptorInputStream stream(fd);

        std::vector<std::vector<uint32_t>> results;
        results.push_back(ReadProblemSolution(stream));
        results.push_back(ReadProblemSolution(stream));
        close(fd);
        promise.set_value(results);
    });

    std::vector<uint8_t> payload = AsciiCoverToBytes(ReadFileToString("tests/sudoku_example/sudoku_cover.txt"));
    ASSERT_FALSE(payload.empty());

    ASSERT_TRUE(SendProblem(server().request_port(), payload));
    ASSERT_TRUE(SendProblem(server().request_port(), payload));

    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    const auto& results = future.get();
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0], expected);
    EXPECT_EQ(results[1], expected);

    solution_thread.join();
}

TEST_F(DlxTcpServerTest, MultipleProblemClientsBroadcastToSolutionSubscribers)
{
    auto expected = ParseRowList(kExpectedSudokuRows);

    std::promise<std::vector<std::vector<uint32_t>>> promise_a;
    std::promise<std::vector<std::vector<uint32_t>>> promise_b;
    auto future_a = promise_a.get_future();
    auto future_b = promise_b.get_future();

    auto solution_client = [&](std::promise<std::vector<std::vector<uint32_t>>>& prom) {
        int fd = ConnectToPort(server().solution_port());
        ASSERT_GE(fd, 0);
        DescriptorInputStream stream(fd);

        std::vector<std::vector<uint32_t>> results;
        results.push_back(ReadProblemSolution(stream));
        results.push_back(ReadProblemSolution(stream));
        close(fd);
        prom.set_value(results);
    };

    std::thread client_a(solution_client, std::ref(promise_a));
    std::thread client_b(solution_client, std::ref(promise_b));

    std::vector<uint8_t> payload = AsciiCoverToBytes(ReadFileToString("tests/sudoku_example/sudoku_cover.txt"));
    ASSERT_FALSE(payload.empty());

    ASSERT_TRUE(SendProblem(server().request_port(), payload));
    ASSERT_TRUE(SendProblem(server().request_port(), payload));

    ASSERT_EQ(future_a.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    ASSERT_EQ(future_b.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    auto results_a = future_a.get();
    auto results_b = future_b.get();
    ASSERT_EQ(results_a.size(), 2u);
    ASSERT_EQ(results_b.size(), 2u);
    EXPECT_EQ(results_a[0], expected);
    EXPECT_EQ(results_a[1], expected);
    EXPECT_EQ(results_b[0], expected);
    EXPECT_EQ(results_b[1], expected);

    client_a.join();
    client_b.join();
}

} // namespace
