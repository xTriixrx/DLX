#include "core/dlx.h"
#include "core/matrix.h"
#include "core/solution_sink.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <ios>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

void initialize_column_headers(struct node* matrix, uint32_t column_count)
{
    matrix[0].right = matrix;
    matrix[0].left = matrix;

    for (uint32_t i = 0; i < column_count; ++i)
    {
        struct node* column = &matrix[i + 1];
        column->len = 0;
        column->top = matrix;
        column->left = &matrix[i];
        column->right = matrix;
        column->up = column;
        column->down = column;
        column->data = static_cast<int>(i + 1);

        matrix[i].right = column;
        matrix[0].left = column;
    }
}

struct SyntheticMatrix
{
    struct node* matrix = nullptr;
    char** solutions = nullptr;
    int item_count = 0;
    int option_count = 0;
};

class MatrixGuard
{
public:
    explicit MatrixGuard(SyntheticMatrix& matrix)
        : matrix_(matrix)
    {}

    ~MatrixGuard()
    {
        reset();
    }

    void reset()
    {
        if (matrix_.matrix != nullptr || matrix_.solutions != nullptr)
        {
            dlx::Core::freeMemory(matrix_.matrix, matrix_.solutions);
            matrix_.matrix = nullptr;
            matrix_.solutions = nullptr;
        }
    }

private:
    SyntheticMatrix& matrix_;
};

struct PerformanceRecord
{
    uint32_t columns;
    uint32_t groups;
    uint32_t variants;
    uint64_t solutions;
    double duration_ms;
};

class PerformanceReport
{
public:
    static PerformanceReport& instance()
    {
        static PerformanceReport instance;
        return instance;
    }

    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        records_.clear();
    }

    void add_record(const PerformanceRecord& record)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        records_.push_back(record);
    }

    void write_csv(const std::string& path) const
    {
        std::vector<PerformanceRecord> snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = records_;
        }

        if (snapshot.empty())
        {
            return;
        }

        std::ofstream file(path, std::ios::out | std::ios::trunc);
        if (!file.is_open())
        {
            return;
        }

        file << "columns,groups,variants,solutions,duration_ms\n";
        file << std::fixed << std::setprecision(3);
        for (const PerformanceRecord& record : snapshot)
        {
            file << record.columns << ','
                 << record.groups << ','
                 << record.variants << ','
                 << record.solutions << ','
                 << record.duration_ms << "\n";
        }
    }

private:
    std::vector<PerformanceRecord> records_;
    mutable std::mutex mutex_;
};

class CountingSink : public dlx::SolutionSink
{
public:
    uint64_t solution_count = 0;
    int last_solution_size = 0;

    void on_solution(const dlx::SolutionView& view) override
    {
        solution_count += 1;
        last_solution_size = view.count;
    }
};

uint32_t compute_group_count(uint32_t column_count)
{
    if (column_count == 0)
    {
        return 1;
    }

    uint32_t value = column_count;
    uint32_t count = 0;
    while (value >= 10)
    {
        value /= 10;
        count += 1;
    }

    return count == 0 ? 1 : count;
}

uint32_t select_group_size(uint32_t column_count, uint32_t group_count, uint32_t group_index)
{
    const uint32_t base = column_count / group_count;
    const uint32_t remainder = column_count % group_count;
    return base + (group_index < remainder ? 1 : 0);
}

std::vector<std::vector<uint32_t>> build_group_rows(uint32_t column_count,
                                                     uint32_t group_count,
                                                     uint32_t variants_per_group)
{
    std::vector<std::vector<uint32_t>> rows;
    rows.reserve(static_cast<size_t>(group_count) * variants_per_group);

    uint32_t column_cursor = 0;
    for (uint32_t group = 0; group < group_count; ++group)
    {
        uint32_t group_size = select_group_size(column_count, group_count, group);
        std::vector<uint32_t> columns;
        columns.reserve(group_size);
        for (uint32_t offset = 0; offset < group_size; ++offset)
        {
            columns.push_back(column_cursor + offset);
        }
        column_cursor += group_size;

        for (uint32_t variant = 0; variant < variants_per_group; ++variant)
        {
            rows.push_back(columns);
        }
    }

    return rows;
}

SyntheticMatrix build_synthetic_matrix(uint32_t column_count,
                                       uint32_t group_count,
                                       uint32_t variants_per_group)
{
    SyntheticMatrix matrix;
    if (column_count == 0 || column_count > static_cast<uint32_t>(std::numeric_limits<int>::max()))
    {
        return matrix;
    }
    std::vector<std::vector<uint32_t>> rows = build_group_rows(column_count,
                                                               group_count,
                                                               variants_per_group);
    if (rows.empty())
    {
        return matrix;
    }

    matrix.solutions = static_cast<char**>(calloc(rows.size(), sizeof(char*)));
    if (matrix.solutions == nullptr)
    {
        return matrix;
    }

    size_t total_entries = 0;
    for (const auto& row : rows)
    {
        total_entries += row.size();
    }

    const size_t spacer_nodes = rows.size() + 1;
    const size_t total_nodes = static_cast<size_t>(column_count) + total_entries + spacer_nodes;
    if (total_nodes > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        free(matrix.solutions);
        matrix.solutions = nullptr;
        return matrix;
    }

    matrix.matrix = dlx::matrix::generateHeadNode(static_cast<int>(total_nodes));
    if (matrix.matrix == nullptr)
    {
        free(matrix.solutions);
        matrix.solutions = nullptr;
        return matrix;
    }

    matrix.item_count = static_cast<int>(column_count);
    matrix.option_count = static_cast<int>(rows.size());

    initialize_column_headers(matrix.matrix, column_count);

    int currNodeCount = matrix.item_count;
    int prevRowCount = 0;
    int spaceNodeCount = 0;
    int pending_row_id = 0;
    bool has_pending_row = false;

    for (size_t row_index = 0; row_index < rows.size(); ++row_index)
    {
        dlx::matrix::handleSpacerNodes(matrix.matrix, &spaceNodeCount, currNodeCount, prevRowCount);
        if (has_pending_row)
        {
            matrix.matrix[currNodeCount + 1].data = -pending_row_id;
        }
        pending_row_id = static_cast<int>(row_index + 1);
        has_pending_row = true;

        currNodeCount++;
        prevRowCount = 0;

        const std::vector<uint32_t>& columns = rows[row_index];
        size_t column_cursor = 0;

        for (int assocItemCount = 1; assocItemCount <= matrix.item_count; ++assocItemCount)
        {
            while (column_cursor < columns.size()
                   && columns[column_cursor] + 1 < static_cast<uint32_t>(assocItemCount))
            {
                column_cursor++;
            }

            if (column_cursor >= columns.size())
            {
                break;
            }

            if (columns[column_cursor] + 1 != static_cast<uint32_t>(assocItemCount))
            {
                continue;
            }

            struct node* item = &matrix.matrix[assocItemCount];
            struct node* last = item;
            while (last->down != item)
            {
                last = last->down;
            }

            int new_index = currNodeCount + 1;
            matrix.matrix[new_index].data = new_index;
            item->len += 1;
            item->up = &matrix.matrix[new_index];
            last->down = &matrix.matrix[new_index];
            matrix.matrix[new_index].up = last;
            matrix.matrix[new_index].top = item;
            matrix.matrix[new_index].down = item;

            prevRowCount++;
            currNodeCount++;
            column_cursor++;
        }
    }

    matrix.matrix[currNodeCount + 1].top = matrix.matrix;
    if (has_pending_row)
    {
        matrix.matrix[currNodeCount + 1].data = -pending_row_id;
    }
    else
    {
        matrix.matrix[currNodeCount + 1].data = spaceNodeCount;
    }
    spaceNodeCount--;
    matrix.matrix[currNodeCount + 1].down = matrix.matrix;
    if (prevRowCount == 0)
    {
        matrix.matrix[currNodeCount + 1].up = matrix.matrix;
    }
    else
    {
        matrix.matrix[currNodeCount - prevRowCount].down = &matrix.matrix[currNodeCount];
        matrix.matrix[currNodeCount + 1].up = &matrix.matrix[(currNodeCount + 1) - prevRowCount];
    }

    return matrix;
}

uint64_t expected_solution_count(uint32_t variants_per_group, uint32_t group_count)
{
    uint64_t total = 1;
    for (uint32_t i = 0; i < group_count; ++i)
    {
        total *= variants_per_group;
    }
    return total;
}

struct PerformanceParam
{
    uint32_t column_count;
    uint32_t group_count;
    uint32_t variants_per_group;
};

constexpr uint32_t kDefaultVariantsPerGroup = 2;
constexpr const char kReportPath[] = "dlx_search_performance.csv";

struct CaseResult
{
    PerformanceParam param{};
    bool success = false;
    std::string error;
    PerformanceRecord record{};
};

bool run_performance_case(const PerformanceParam& param,
                          PerformanceRecord* record_out,
                          std::string* error_out)
{
    const uint32_t column_count = param.column_count;
    const uint32_t group_count =
        (param.group_count == 0) ? compute_group_count(column_count) : param.group_count;
    const uint32_t variants_per_group =
        (param.variants_per_group == 0) ? kDefaultVariantsPerGroup : param.variants_per_group;

    SyntheticMatrix matrix = build_synthetic_matrix(column_count, group_count, variants_per_group);
    if (matrix.matrix == nullptr || matrix.solutions == nullptr)
    {
        if (error_out != nullptr)
        {
            *error_out = "Failed to build matrix";
        }
        return false;
    }

    MatrixGuard guard(matrix);

    if (matrix.item_count != static_cast<int>(column_count))
    {
        if (error_out != nullptr)
        {
            *error_out = "Matrix item count mismatch: expected "
                + std::to_string(column_count) + ", got "
                + std::to_string(matrix.item_count);
        }
        return false;
    }

    const int expected_rows = static_cast<int>(group_count * variants_per_group);
    if (matrix.option_count != expected_rows)
    {
        if (error_out != nullptr)
        {
            *error_out = "Matrix option count mismatch: expected "
                + std::to_string(expected_rows) + ", got "
                + std::to_string(matrix.option_count);
        }
        return false;
    }

    std::vector<uint32_t> row_ids(static_cast<size_t>(matrix.option_count));
    CountingSink sink;
    dlx::SolutionOutput output_ctx;
    output_ctx.sink = &sink;

    auto start = std::chrono::steady_clock::now();
    dlx::Core::search(matrix.matrix, 0, matrix.solutions, row_ids.data(), output_ctx);
    auto end = std::chrono::steady_clock::now();

    const double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    const uint64_t expected_solutions = expected_solution_count(variants_per_group, group_count);

    if (sink.solution_count != expected_solutions)
    {
        if (error_out != nullptr)
        {
            *error_out = "Expected " + std::to_string(expected_solutions)
                + " solutions, observed " + std::to_string(sink.solution_count);
        }
        return false;
    }

    if (sink.last_solution_size != static_cast<int>(group_count))
    {
        if (error_out != nullptr)
        {
            *error_out = "Expected solution depth "
                + std::to_string(group_count) + ", observed "
                + std::to_string(sink.last_solution_size);
        }
        return false;
    }

    if (record_out != nullptr)
    {
        *record_out = PerformanceRecord{
            column_count,
            group_count,
            variants_per_group,
            sink.solution_count,
            elapsed_ms,
        };
    }

    return true;
}

class DlxSearchPerformanceTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        PerformanceReport::instance().reset();
    }

    static void TearDownTestSuite()
    {
        if (::testing::UnitTest::GetInstance()->Passed())
        {
            PerformanceReport::instance().write_csv(kReportPath);
        }
        else
        {
            PerformanceReport::instance().reset();
        }
    }
};

constexpr PerformanceParam kPerformanceParams[] = {
    {1000, 3, 2},
    {10000, 4, 2},
    {100000, 5, 2},
    {1000000, 6, 2},
    {1000, 5, 3},
    {10000, 6, 3},
    {100000, 7, 3},
};

TEST_F(DlxSearchPerformanceTest, MeasuresSearchScalingParallel)
{
    constexpr size_t kCaseCount = sizeof(kPerformanceParams) / sizeof(kPerformanceParams[0]);
    std::vector<CaseResult> results(kCaseCount);

    std::mutex index_mutex;
    size_t next_index = 0;
    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
    const unsigned worker_count = std::min<unsigned>(hardware, static_cast<unsigned>(kCaseCount));

    auto worker = [&]() {
        while (true)
        {
            size_t index = 0;
            {
                std::lock_guard<std::mutex> lock(index_mutex);
                if (next_index >= kCaseCount)
                {
                    return;
                }
                index = next_index++;
            }

            CaseResult result;
            result.param = kPerformanceParams[index];
            result.success = run_performance_case(result.param, &result.record, &result.error);
            results[index] = std::move(result);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(worker_count);
    for (unsigned i = 0; i < worker_count; ++i)
    {
        threads.emplace_back(worker);
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }

    bool any_failure = false;
    for (const CaseResult& result : results)
    {
        if (!result.success)
        {
            any_failure = true;
            ADD_FAILURE() << "Columns" << result.param.column_count << ": " << result.error;
        }
        else
        {
            const uint64_t expected_solutions =
                expected_solution_count(result.record.variants, result.record.groups);
            EXPECT_EQ(result.record.solutions, expected_solutions)
                << "Columns" << result.param.column_count << " groups " << result.record.groups
                << " variants " << result.record.variants << ": unexpected solution count";
            PerformanceReport::instance().add_record(result.record);
        }
    }

    ASSERT_FALSE(any_failure);
}

} // namespace
