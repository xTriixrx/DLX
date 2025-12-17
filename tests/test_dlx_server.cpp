#include "core/tcp_server.h"
#include "core/binary.h"
#include "ascii_binary_utils.h"
#include <arpa/inet.h>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

namespace {

constexpr char kExpectedSudokuRows[] =
    "1 2 8 24 31 32 33 47 48 60 64 75 87 88 95 96 89 97 103 93 99 104 105 113 "
    "73 114 124 128 138 52 53 7 12 45 50 58 63 79 76 67 71 83 106 109 116 119 "
    "34 40 17 16 21 5 27 28 44 122 127 136 140 129 141 142 143 148 151 152 153 "
    "154 156 157 144 158 161 164 170 171 175 177 178 182 183";

std::string read_file_to_string(const char* path)
{
    std::ifstream file(path);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<uint8_t> ascii_cover_to_bytes(const std::string& ascii)
{
    FILE* temp = tmpfile();
    if (temp == nullptr)
    {
        return {};
    }

    if (ascii_cover_to_binary_stream(ascii, temp) != 0)
    {
        fclose(temp);
        return {};
    }

    fflush(temp);
    rewind(temp);

    std::vector<uint8_t> data;
    std::vector<uint8_t> buffer(4096);
    size_t read = 0;
    while ((read = fread(buffer.data(), 1, buffer.size(), temp)) > 0)
    {
        data.insert(data.end(), buffer.begin(), buffer.begin() + static_cast<size_t>(read));
    }
    fclose(temp);
    return data;
}

int connect_to_port(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GE(fd, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    int status = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    EXPECT_EQ(status, 0);
    return fd;
}

std::vector<uint32_t> parse_row_list(const char* rows)
{
    std::vector<uint32_t> values;
    const char* cursor = rows;
    while (*cursor != '\0')
    {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r')
        {
            cursor++;
        }
        if (*cursor == '\0')
        {
            break;
        }

        char* endptr = nullptr;
        long value = strtol(cursor, &endptr, 10);
        if (endptr == cursor || value <= 0)
        {
            break;
        }

        values.push_back(static_cast<uint32_t>(value));
        cursor = endptr;
    }

    return values;
}

class DescriptorStreamBuf : public std::streambuf
{
public:
    explicit DescriptorStreamBuf(int fd)
        : fd_(fd)
    {
        setg(buffer_, buffer_, buffer_);
    }
    ~DescriptorStreamBuf() override;

protected:
    int_type underflow() override
    {
        if (fd_ < 0)
        {
            return traits_type::eof();
        }

        ssize_t bytes = 0;
        do
        {
            bytes = ::read(fd_, buffer_, sizeof(buffer_));
        } while (bytes < 0 && errno == EINTR);

        if (bytes <= 0)
        {
            return traits_type::eof();
        }

        setg(buffer_, buffer_, buffer_ + bytes);
        return traits_type::to_int_type(*gptr());
    }

private:
    int fd_;
    char buffer_[4096];
};

class DescriptorInputStream : public std::istream
{
public:
    explicit DescriptorInputStream(int fd)
        : std::istream(&buffer_)
        , buffer_(fd)
    {}
    ~DescriptorInputStream() override;

private:
    DescriptorStreamBuf buffer_;
};

DescriptorStreamBuf::~DescriptorStreamBuf() = default;
DescriptorInputStream::~DescriptorInputStream() = default;

std::vector<uint32_t> read_problem_solution(DescriptorInputStream& binary_stream)
{
    struct DlxSolutionHeader header;
    EXPECT_EQ(dlx_read_solution_header(binary_stream, &header), 0);
    EXPECT_EQ(header.magic, DLX_SOLUTION_MAGIC);

    struct DlxSolutionRow row = {0};
    std::vector<uint32_t> values;
    bool received_solution = false;

    while (true)
    {
        int read_status = dlx_read_solution_row(binary_stream, &row);
        EXPECT_EQ(read_status, 1);
        if (row.solution_id == 0 && row.entry_count == 0)
        {
            break;
        }

        values.assign(row.row_indices, row.row_indices + row.entry_count);
        received_solution = true;
    }

    dlx_free_solution_row(&row);
    EXPECT_TRUE(received_solution);
    return values;
}

void send_problem(uint16_t port, const std::vector<uint8_t>& data)
{
    int fd = connect_to_port(port);
    size_t offset = 0;
    while (offset < data.size())
    {
        ssize_t written = send(fd, data.data() + offset, data.size() - offset, 0);
        if (written <= 0)
        {
            break;
        }
        offset += static_cast<size_t>(written);
    }
    shutdown(fd, SHUT_WR);
    close(fd);
}


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
    auto expected = parse_row_list(kExpectedSudokuRows);
    std::promise<std::vector<uint32_t>> rows_promise;
    auto future = rows_promise.get_future();

    std::thread solution_thread([&]() {
        int fd = connect_to_port(server().solution_port());
        DescriptorInputStream stream(fd);
        rows_promise.set_value(read_problem_solution(stream));
        close(fd);
    });

    std::string ascii_cover = read_file_to_string("tests/sudoku_cover.txt");
    std::vector<uint8_t> payload = ascii_cover_to_bytes(ascii_cover);
    ASSERT_FALSE(payload.empty());

    send_problem(server().request_port(), payload);

    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    std::vector<uint32_t> rows = future.get();
    EXPECT_EQ(rows, expected);

    solution_thread.join();
}

TEST_F(DlxTcpServerTest, BroadcastsToMultipleClients)
{
    auto expected = parse_row_list(kExpectedSudokuRows);

    std::promise<std::vector<uint32_t>> promise_a;
    std::promise<std::vector<uint32_t>> promise_b;
    auto future_a = promise_a.get_future();
    auto future_b = promise_b.get_future();

    std::thread client_a([&]() {
        int fd = connect_to_port(server().solution_port());
        DescriptorInputStream stream(fd);
        promise_a.set_value(read_problem_solution(stream));
        close(fd);
    });

    std::thread client_b([&]() {
        int fd = connect_to_port(server().solution_port());
        DescriptorInputStream stream(fd);
        promise_b.set_value(read_problem_solution(stream));
        close(fd);
    });

    std::vector<uint8_t> payload = ascii_cover_to_bytes(read_file_to_string("tests/sudoku_cover.txt"));
    ASSERT_FALSE(payload.empty());
    send_problem(server().request_port(), payload);

    ASSERT_EQ(future_a.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    ASSERT_EQ(future_b.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_EQ(future_a.get(), expected);
    EXPECT_EQ(future_b.get(), expected);

    client_a.join();
    client_b.join();
}

TEST_F(DlxTcpServerTest, ReusesSolutionSocketAcrossProblems)
{
    auto expected = parse_row_list(kExpectedSudokuRows);
    std::promise<std::vector<std::vector<uint32_t>>> promise;
    auto future = promise.get_future();

    std::thread solution_thread([&]() {
        int fd = connect_to_port(server().solution_port());
        DescriptorInputStream stream(fd);

        std::vector<std::vector<uint32_t>> results;
        results.push_back(read_problem_solution(stream));
        results.push_back(read_problem_solution(stream));
        close(fd);
        promise.set_value(results);
    });

    std::vector<uint8_t> payload = ascii_cover_to_bytes(read_file_to_string("tests/sudoku_cover.txt"));
    ASSERT_FALSE(payload.empty());

    send_problem(server().request_port(), payload);
    send_problem(server().request_port(), payload);

    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    const auto& results = future.get();
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0], expected);
    EXPECT_EQ(results[1], expected);

    solution_thread.join();
}

TEST_F(DlxTcpServerTest, MultipleProblemClientsBroadcastToSolutionSubscribers)
{
    auto expected = parse_row_list(kExpectedSudokuRows);

    std::promise<std::vector<std::vector<uint32_t>>> promise_a;
    std::promise<std::vector<std::vector<uint32_t>>> promise_b;
    auto future_a = promise_a.get_future();
    auto future_b = promise_b.get_future();

    auto solution_client = [&](std::promise<std::vector<std::vector<uint32_t>>>& prom) {
        int fd = connect_to_port(server().solution_port());
        DescriptorInputStream stream(fd);

        std::vector<std::vector<uint32_t>> results;
        results.push_back(read_problem_solution(stream));
        results.push_back(read_problem_solution(stream));
        close(fd);
        prom.set_value(results);
    };

    std::thread client_a(solution_client, std::ref(promise_a));
    std::thread client_b(solution_client, std::ref(promise_b));

    std::vector<uint8_t> payload = ascii_cover_to_bytes(read_file_to_string("tests/sudoku_cover.txt"));
    ASSERT_FALSE(payload.empty());

    send_problem(server().request_port(), payload);
    send_problem(server().request_port(), payload);

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
