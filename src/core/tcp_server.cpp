#include "core/tcp_server.h"
#include "core/binary.h"
#include "core/dlx.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <ostream>
#include <streambuf>
#include <utility>
#include <vector>

namespace dlx {

namespace {

class BufferedSocketStreambuf : public std::streambuf
{
public:
    explicit BufferedSocketStreambuf(int fd)
        : fd_(fd)
    {
        setp(buffer_, buffer_ + sizeof(buffer_));
    }

protected:
    std::streamsize xsputn(const char* s, std::streamsize count) override
    {
        std::streamsize total = 0;
        while (total < count)
        {
            std::streamsize available = epptr() - pptr();
            if (available == 0)
            {
                if (flush_buffer() != 0)
                {
                    break;
                }
                available = epptr() - pptr();
            }

            std::streamsize to_copy = std::min(available, count - total);
            memcpy(pptr(), s + total, static_cast<size_t>(to_copy));
            pbump(static_cast<int>(to_copy));
            total += to_copy;
        }
        return total;
    }

    int overflow(int ch) override
    {
        if (ch == traits_type::eof())
        {
            return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
        }

        if (pptr() == epptr() && flush_buffer() != 0)
        {
            return traits_type::eof();
        }

        *pptr() = static_cast<char>(ch);
        pbump(1);
        return ch;
    }

    int sync() override
    {
        return flush_buffer();
    }

private:
    int flush_buffer()
    {
        std::streamsize size = pptr() - pbase();
        std::streamsize sent = 0;
        while (sent < size)
        {
            ssize_t written = send(fd_, pbase() + sent, static_cast<size_t>(size - sent), 0);
            if (written <= 0)
            {
                return -1;
            }
            sent += written;
        }
        setp(buffer_, buffer_ + sizeof(buffer_));
        return 0;
    }

    int fd_;
    char buffer_[65536];
};

class SocketInputStreambuf : public std::streambuf
{
public:
    explicit SocketInputStreambuf(int fd)
        : fd_(fd)
    {
        setg(buffer_, buffer_, buffer_);
    }

protected:
    int_type underflow() override
    {
        if (gptr() < egptr())
        {
            return traits_type::to_int_type(*gptr());
        }

        ssize_t bytes = recv(fd_, buffer_, sizeof(buffer_), 0);
        if (bytes <= 0)
        {
            return traits_type::eof();
        }

        setg(buffer_, buffer_, buffer_ + bytes);
        return traits_type::to_int_type(*gptr());
    }

private:
    int fd_;
    char buffer_[65536];
};

class SocketOutputStream : public std::ostream
{
public:
    explicit SocketOutputStream(int fd)
        : std::ostream(nullptr)
        , buffer_(fd)
    {
        rdbuf(&buffer_);
    }

private:
    BufferedSocketStreambuf buffer_;
};

class SocketInputStream : public std::istream
{
public:
    explicit SocketInputStream(int fd)
        : std::istream(&buffer_)
        , buffer_(fd)
    {}

private:
    SocketInputStreambuf buffer_;
};

} // namespace

struct DlxTcpServer::SolutionClient
{
    int fd;
    std::unique_ptr<SocketOutputStream> stream;
    std::unique_ptr<binary::DlxSolutionStreamWriter> writer;

    SolutionClient(int socket_fd, std::unique_ptr<SocketOutputStream> output_stream)
        : fd(socket_fd)
        , stream(std::move(output_stream))
        , writer(nullptr)
    {}

    ~SolutionClient()
    {
        stream.reset();
        if (fd >= 0)
        {
            close(fd);
            fd = -1;
        }
    }
};


DlxTcpServer::DlxTcpServer(const TcpServerConfig& config)
    : config_(config)
    , request_port_(config.request_port)
    , solution_port_(config.solution_port)
    , request_listen_fd_(-1)
    , solution_listen_fd_(-1)
    , shutting_down_(false)
{
}

DlxTcpServer::~DlxTcpServer()
{
    stop();
    wait();
}

int DlxTcpServer::create_listening_socket(uint16_t requested_port, uint16_t* bound_port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(requested_port);

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }

    if (listen(fd, SOMAXCONN) < 0)
    {
        close(fd);
        return -1;
    }

    sockaddr_in actual_addr;
    socklen_t len = sizeof(actual_addr);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&actual_addr), &len) == 0)
    {
        *bound_port = ntohs(actual_addr.sin_port);
    }

    return fd;
}

bool DlxTcpServer::start()
{
    if (request_listen_fd_ != -1 || solution_listen_fd_ != -1)
    {
        return false;
    }

    shutting_down_ = false;

    request_listen_fd_ = create_listening_socket(config_.request_port, &request_port_);
    if (request_listen_fd_ < 0)
    {
        return false;
    }

    solution_listen_fd_ = create_listening_socket(config_.solution_port, &solution_port_);
    if (solution_listen_fd_ < 0)
    {
        close(request_listen_fd_);
        request_listen_fd_ = -1;
        return false;
    }

    dlx::Core::dlx_set_stdout_suppressed(true);

    request_thread_ = std::thread(&DlxTcpServer::accept_request_loop, this);
    solution_thread_ = std::thread(&DlxTcpServer::accept_solution_loop, this);
    worker_thread_ = std::thread(&DlxTcpServer::process_problem_queue, this);
    output_thread_ = std::thread(&DlxTcpServer::process_solution_queue, this);
    return true;
}

void DlxTcpServer::stop()
{
    if (shutting_down_.exchange(true))
    {
        return;
    }

    problem_queue_cv_.notify_all();
    solution_queue_cv_.notify_all();

    if (request_listen_fd_ >= 0)
    {
        close(request_listen_fd_);
        request_listen_fd_ = -1;
    }
    if (solution_listen_fd_ >= 0)
    {
        close(solution_listen_fd_);
        solution_listen_fd_ = -1;
    }

    {
        std::lock_guard<std::mutex> lock(solution_mutex_);
        for (auto& client : solution_clients_)
        {
            client.reset();
        }
        solution_clients_.clear();
    }

    dlx::Core::dlx_set_stdout_suppressed(false);
}

void DlxTcpServer::wait()
{
    if (request_thread_.joinable())
    {
        request_thread_.join();
    }
    if (solution_thread_.joinable())
    {
        solution_thread_.join();
    }
    if (worker_thread_.joinable())
    {
        worker_thread_.join();
    }
    if (output_thread_.joinable())
    {
        output_thread_.join();
    }
}

void DlxTcpServer::accept_request_loop()
{
    while (!shutting_down_)
    {
        int client_fd = accept(request_listen_fd_, nullptr, nullptr);
        if (client_fd < 0)
        {
            if (shutting_down_)
            {
                break;
            }
            continue;
        }

        std::thread(&DlxTcpServer::process_problem_connection, this, client_fd).detach();
    }
}

void DlxTcpServer::accept_solution_loop()
{
    while (!shutting_down_)
    {
        int client_fd = accept(solution_listen_fd_, nullptr, nullptr);
        if (client_fd < 0)
        {
            if (shutting_down_)
            {
                break;
            }
            continue;
        }

        auto stream = std::make_unique<SocketOutputStream>(client_fd);
        auto client = std::make_shared<SolutionClient>(client_fd, std::move(stream));
        {
            std::lock_guard<std::mutex> lock(solution_mutex_);
            solution_clients_.push_back(client);
            if (active_column_count_.has_value())
            {
                binary::DlxSolutionHeader header = {
                    .magic = DLX_SOLUTION_MAGIC,
                    .version = DLX_BINARY_VERSION,
                    .flags = 0,
                    .column_count = active_column_count_.value(),
                };
                client->writer = std::make_unique<binary::DlxSolutionStreamWriter>(*client->stream, header);
            }
            remove_disconnected_clients_locked();
        }
    }
}

void DlxTcpServer::emit_solution_row(void* ctx, const uint32_t* row_ids, int level)
{
    DlxTcpServer* server = static_cast<DlxTcpServer*>(ctx);
    server->enqueue_solution_row(row_ids, level);
}

void DlxTcpServer::process_problem_queue()
{
    while (true)
    {
        ProblemTask task;
        {
            std::unique_lock<std::mutex> lock(problem_queue_mutex_);
            problem_queue_cv_.wait(lock, [&]() {
                return shutting_down_.load() || !problem_queue_.empty();
            });

            if (problem_queue_.empty())
            {
                if (shutting_down_.load())
                {
                    break;
                }
                continue;
            }

            task = std::move(problem_queue_.front());
            problem_queue_.pop_front();
        }

        char** solutions = NULL;
        int itemCount = 0;
        int optionCount = 0;
        struct node* matrix =
            dlx::Core::generateMatrixBinaryFromRows(task.header, task.rows, &solutions, &itemCount, &optionCount);
        for (auto& row : task.rows)
        {
            free(row.columns);
            row.columns = nullptr;
        }
        task.rows.clear();

        if (matrix == NULL)
        {
            continue;
        }

        if (optionCount <= 0)
        {
            dlx::Core::freeMemory(matrix, solutions);
            continue;
        }

        {
            SolutionEvent event;
            event.type = SolutionEvent::Type::Begin;
            event.column_count = static_cast<uint32_t>(itemCount);
            std::lock_guard<std::mutex> lock(solution_queue_mutex_);
            solution_queue_.push_back(std::move(event));
        }
        solution_queue_cv_.notify_one();

        std::vector<uint32_t> row_ids(static_cast<size_t>(optionCount));

        SolutionOutput output;
        output.binary_stream = nullptr;
        output.binary_callback = &DlxTcpServer::emit_solution_row;
        output.binary_context = this;

        dlx::Core::search(matrix, 0, solutions, row_ids.data(), output);

        {
            SolutionEvent event;
            event.type = SolutionEvent::Type::End;
            event.column_count = 0;
            std::lock_guard<std::mutex> lock(solution_queue_mutex_);
            solution_queue_.push_back(std::move(event));
        }
        solution_queue_cv_.notify_one();

        dlx::Core::freeMemory(matrix, solutions);
    }
}

void DlxTcpServer::process_solution_queue()
{
    while (true)
    {
        SolutionEvent event;
        {
            std::unique_lock<std::mutex> lock(solution_queue_mutex_);
            solution_queue_cv_.wait(lock, [&]() {
                return shutting_down_.load() || !solution_queue_.empty();
            });

            if (solution_queue_.empty())
            {
                if (shutting_down_.load())
                {
                    break;
                }
                continue;
            }

            event = std::move(solution_queue_.front());
            solution_queue_.pop_front();
        }

        switch (event.type)
        {
        case SolutionEvent::Type::Begin:
            begin_solution_stream(event.column_count);
            break;
        case SolutionEvent::Type::Row:
            broadcast_solution_row(event.row_ids.data(), static_cast<int>(event.row_ids.size()));
            break;
        case SolutionEvent::Type::End:
            broadcast_problem_complete();
            finish_solution_stream();
            break;
        }
    }
}

void DlxTcpServer::enqueue_solution_row(const uint32_t* row_ids, int level)
{
    if (row_ids == nullptr || level <= 0)
    {
        return;
    }

    SolutionEvent event;
    event.type = SolutionEvent::Type::Row;
    event.column_count = 0;
    event.row_ids.assign(row_ids, row_ids + level);
    {
        std::lock_guard<std::mutex> lock(solution_queue_mutex_);
        solution_queue_.push_back(std::move(event));
    }
    solution_queue_cv_.notify_one();
}

void DlxTcpServer::process_problem_connection(int client_fd)
{
    SocketInputStream cover_stream(client_fd);
    binary::DlxProblemStreamReader reader(cover_stream);

    while (true)
    {
        binary::DlxCoverHeader header = {0};
        if (reader.read_header(&header) != 0)
        {
            break;
        }

        ProblemTask task;
        task.header = header;
        task.rows.reserve(header.row_count);

        while (true)
        {
            binary::DlxRowChunk chunk = {0};
            int status = reader.read_chunk(&chunk);
            if (status == 0)
            {
                break;
            }
            if (status == -1)
            {
                close(client_fd);
                return;
            }

            task.rows.push_back(chunk);
        }

        task.header.row_count = static_cast<uint32_t>(task.rows.size());

        {
            std::lock_guard<std::mutex> lock(problem_queue_mutex_);
            problem_queue_.push_back(std::move(task));
        }
        problem_queue_cv_.notify_one();
    }

    close(client_fd);
}

void DlxTcpServer::begin_solution_stream(uint32_t column_count)
{
    std::lock_guard<std::mutex> lock(solution_mutex_);
    active_column_count_ = column_count;
    binary::DlxSolutionHeader header = {
        .magic = DLX_SOLUTION_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0,
        .column_count = column_count,
    };
    for (auto& client : solution_clients_)
    {
        if (client == nullptr)
        {
            continue;
        }
        if (client->writer)
        {
            client->writer->start(header);
        }
        else
        {
            client->writer = std::make_unique<binary::DlxSolutionStreamWriter>(*client->stream, header);
        }
    }
    remove_disconnected_clients_locked();
}

void DlxTcpServer::finish_solution_stream()
{
    std::lock_guard<std::mutex> lock(solution_mutex_);
    active_column_count_.reset();
}

void DlxTcpServer::broadcast_solution_row(const uint32_t* row_ids, int level)
{
    if (row_ids == nullptr || level <= 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(solution_mutex_);
    for (auto& client : solution_clients_)
    {
        if (client == nullptr || client->writer == nullptr)
        {
            continue;
        }

        if (client->writer->write_row(row_ids, static_cast<uint16_t>(level)) != 0)
        {
            client.reset();
        }
    }
    remove_disconnected_clients_locked();
}

void DlxTcpServer::broadcast_problem_complete()
{
    std::lock_guard<std::mutex> lock(solution_mutex_);
    for (auto& client : solution_clients_)
    {
        if (client == nullptr || client->writer == nullptr)
        {
            continue;
        }

        if (client->writer->finish() != 0)
        {
            client.reset();
        }
        else
        {
            client->stream->flush();
        }
    }
    remove_disconnected_clients_locked();
}

void DlxTcpServer::remove_disconnected_clients_locked()
{
    solution_clients_.erase(
        std::remove_if(solution_clients_.begin(),
                       solution_clients_.end(),
                       [](const std::shared_ptr<SolutionClient>& client) { return client == nullptr; }),
        solution_clients_.end());
}

} // namespace dlx
