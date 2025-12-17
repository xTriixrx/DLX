#include "core/tcp_server.h"
#include "core/binary.h"
#include "core/dlx.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

namespace dlx {

struct DlxTcpServer::SolutionClient
{
    int fd;
    FILE* stream;
    uint32_t next_solution_id;
    bool header_sent;

    SolutionClient(int socket_fd, FILE* file)
        : fd(socket_fd)
        , stream(file)
        , next_solution_id(1)
        , header_sent(false)
    {}

    ~SolutionClient()
    {
        if (stream != nullptr)
        {
            fclose(stream);
            stream = nullptr;
            fd = -1;
        }
        else if (fd >= 0)
        {
            close(fd);
            fd = -1;
        }
    }
};

namespace {

constexpr size_t kIoBufferSize = 4096;

bool write_solution_header(FILE* stream, uint32_t column_count)
{
    struct DlxSolutionHeader header = {
        .magic = DLX_SOLUTION_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0,
        .column_count = column_count,
    };
    if (dlx_write_solution_header(stream, &header) != 0)
    {
        return false;
    }
    fflush(stream);
    return true;
}

} // namespace

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
    return true;
}

void DlxTcpServer::stop()
{
    if (shutting_down_.exchange(true))
    {
        return;
    }

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

        FILE* stream = fdopen(client_fd, "wb");
        if (stream == nullptr)
        {
            close(client_fd);
            continue;
        }
        setvbuf(stream, nullptr, _IONBF, 0);

        auto client = std::make_shared<SolutionClient>(client_fd, stream);
        {
            std::lock_guard<std::mutex> lock(solution_mutex_);
            solution_clients_.push_back(client);
            if (active_column_count_.has_value())
            {
                if (!write_solution_header(client->stream, active_column_count_.value()))
                {
                    client.reset();
                }
                else
                {
                    client->header_sent = true;
                    client->next_solution_id = 1;
                }
            }
            remove_disconnected_clients_locked();
        }
    }
}

void DlxTcpServer::emit_solution_row(void* ctx, const uint32_t* row_ids, int level)
{
    DlxTcpServer* server = static_cast<DlxTcpServer*>(ctx);
    server->broadcast_solution_row(row_ids, level);
}

void DlxTcpServer::process_problem_connection(int client_fd)
{
    std::string payload;
    std::vector<char> buffer(kIoBufferSize);
    ssize_t bytes = 0;
    while ((bytes = recv(client_fd, buffer.data(), buffer.size(), 0)) > 0)
    {
        payload.append(buffer.data(), static_cast<size_t>(bytes));
    }
    close(client_fd);

    if (bytes < 0)
    {
        return;
    }

    std::istringstream cover_stream(payload);

    char** solutions = NULL;
    int itemCount = 0;
    int optionCount = 0;
    struct node* matrix = dlx_read_binary(cover_stream, &solutions, &itemCount, &optionCount);
    if (matrix == NULL)
    {
        return;
    }

    if (optionCount <= 0)
    {
        dlx::Core::freeMemory(matrix, solutions);
        return;
    }

    std::vector<uint32_t> row_ids(static_cast<size_t>(optionCount));

    std::unique_lock<std::mutex> problem_lock(problem_mutex_);
    begin_solution_stream(static_cast<uint32_t>(itemCount));

    SolutionOutput output;
    output.binary_file = nullptr;
    output.binary_callback = &DlxTcpServer::emit_solution_row;
    output.binary_context = this;

    dlx::Core::search(matrix, 0, solutions, row_ids.data(), output);
    broadcast_problem_complete();
    finish_solution_stream();
    problem_lock.unlock();

    dlx::Core::freeMemory(matrix, solutions);
}

void DlxTcpServer::begin_solution_stream(uint32_t column_count)
{
    std::lock_guard<std::mutex> lock(solution_mutex_);
    active_column_count_ = column_count;
    for (auto& client : solution_clients_)
    {
        if (client == nullptr)
        {
            continue;
        }
        if (!write_solution_header(client->stream, column_count))
        {
            client.reset();
        }
        else
        {
            client->header_sent = true;
            client->next_solution_id = 1;
        }
    }
    remove_disconnected_clients_locked();
}

void DlxTcpServer::finish_solution_stream()
{
    std::lock_guard<std::mutex> lock(solution_mutex_);
    active_column_count_.reset();
    for (auto& client : solution_clients_)
    {
        if (client != nullptr)
        {
            client->header_sent = false;
            client->next_solution_id = 1;
        }
    }
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
        if (client == nullptr || !client->header_sent)
        {
            continue;
        }

        if (dlx_write_solution_row(client->stream,
                                   client->next_solution_id,
                                   row_ids,
                                   static_cast<uint16_t>(level))
            != 0)
        {
            client.reset();
        }
        else
        {
            client->next_solution_id += 1;
            fflush(client->stream);
        }
    }
    remove_disconnected_clients_locked();
}

void DlxTcpServer::broadcast_problem_complete()
{
    std::lock_guard<std::mutex> lock(solution_mutex_);
    for (auto& client : solution_clients_)
    {
        if (client == nullptr || !client->header_sent)
        {
            continue;
        }

        if (dlx_write_solution_row(client->stream, 0, nullptr, 0) != 0)
        {
            client.reset();
        }
        else
        {
            fflush(client->stream);
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
