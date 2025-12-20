#ifndef DLX_TCP_SERVER_H
#define DLX_TCP_SERVER_H

#include "core/binary.h"
#include <stdint.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace dlx {

struct TcpServerConfig
{
    uint16_t request_port;
    uint16_t solution_port;
};

class DlxTcpServer
{
public:
    explicit DlxTcpServer(const TcpServerConfig& config);
    ~DlxTcpServer();

    bool start();
    void stop();
    void wait();

    uint16_t request_port() const { return request_port_; }
    uint16_t solution_port() const { return solution_port_; }

private:
    struct SolutionClient;
    struct ProblemTask
    {
        dlx::binary::DlxCoverHeader header;
        std::vector<dlx::binary::DlxRowChunk> rows;
    };
    struct SolutionEvent
    {
        enum class Type
        {
            Begin,
            Row,
            End
        };

        Type type;
        uint32_t column_count;
        std::vector<uint32_t> row_ids;
    };

    static int create_listening_socket(uint16_t requested_port, uint16_t* bound_port);
    static void emit_solution_row(void* ctx, const uint32_t* row_ids, int level);

    void accept_request_loop();
    void accept_solution_loop();
    void process_problem_queue();
    void process_solution_queue();
    void process_problem_connection(int client_fd);
    void enqueue_solution_row(const uint32_t* row_ids, int level);
    void begin_solution_stream(uint32_t column_count);
    void finish_solution_stream();
    void broadcast_solution_row(const uint32_t* row_ids, int level);
    void broadcast_problem_complete();
    void remove_disconnected_clients_locked();

    TcpServerConfig config_;
    uint16_t request_port_;
    uint16_t solution_port_;
    int request_listen_fd_;
    int solution_listen_fd_;
    std::thread request_thread_;
    std::thread solution_thread_;
    std::mutex problem_mutex_;
    std::mutex solution_mutex_;
    std::vector<std::shared_ptr<SolutionClient>> solution_clients_;
    std::mutex problem_queue_mutex_;
    std::condition_variable problem_queue_cv_;
    std::deque<ProblemTask> problem_queue_;
    std::mutex solution_queue_mutex_;
    std::condition_variable solution_queue_cv_;
    std::deque<SolutionEvent> solution_queue_;
    std::thread worker_thread_;
    std::thread output_thread_;
    std::optional<uint32_t> active_column_count_;
    std::atomic<bool> shutting_down_;
};

} // namespace dlx

#endif
