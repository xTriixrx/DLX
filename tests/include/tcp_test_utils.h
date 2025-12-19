#ifndef DLX_TCP_TEST_UTILS_H
#define DLX_TCP_TEST_UTILS_H

#include "ascii_binary_utils.h"
#include "core/binary.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <istream>
#include <memory>
#include <netinet/in.h>
#include <ostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace tcp_test_utils {

inline std::string ReadFileToString(const std::string& path)
{
    std::ifstream file(path);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

inline std::vector<uint8_t> AsciiCoverToBytes(const std::string& ascii)
{
    std::ostringstream binary_stream;
    if (ascii_cover_to_binary_stream(ascii, binary_stream) != 0)
    {
        return {};
    }

    const std::string payload = binary_stream.str();
    return std::vector<uint8_t>(payload.begin(), payload.end());
}

inline int ConnectToPort(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return fd;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

inline bool SendProblem(uint16_t port, const std::vector<uint8_t>& data)
{
    int fd = ConnectToPort(port);
    if (fd < 0)
    {
        return false;
    }

    bool success = true;
    size_t offset = 0;
    while (offset < data.size())
    {
        ssize_t written = send(fd, data.data() + offset, data.size() - offset, 0);
        if (written <= 0)
        {
            success = false;
            break;
        }
        offset += static_cast<size_t>(written);
    }
    shutdown(fd, SHUT_WR);
    close(fd);
    return success && offset == data.size();
}

inline std::vector<uint32_t> ParseRowList(const std::string& rows)
{
    std::vector<uint32_t> values;
    const char* cursor = rows.c_str();
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
    ~DescriptorStreamBuf() override = default;

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
    ~DescriptorInputStream() override = default;

private:
    DescriptorStreamBuf buffer_;
};

inline std::vector<uint32_t> ReadProblemSolution(DescriptorInputStream& binary_stream)
{
    struct DlxSolutionHeader header;
    if (dlx_read_solution_header(binary_stream, &header) != 0)
    {
        return {};
    }
    if (header.magic != DLX_SOLUTION_MAGIC)
    {
        return {};
    }

    struct DlxSolutionRow row = {0};
    std::vector<uint32_t> values;
    bool received_solution = false;

    while (true)
    {
        int read_status = dlx_read_solution_row(binary_stream, &row);
        if (read_status != 1)
        {
            break;
        }
        if (row.solution_id == 0 && row.entry_count == 0)
        {
            break;
        }

        values.assign(row.row_indices, row.row_indices + row.entry_count);
        received_solution = true;
    }

    dlx_free_solution_row(&row);
    return received_solution ? values : std::vector<uint32_t>();
}

} // namespace tcp_test_utils

#endif
