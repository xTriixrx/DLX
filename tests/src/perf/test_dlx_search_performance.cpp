#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <limits>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#include "core/dlx.h"
#include "core/matrix.h"
#include "core/solution_sink.h"
#include "performance_test_config.h"
#include <gtest/gtest.h>

namespace
{

constexpr uint32_t kDefaultVariantsPerGroup = 2;

/**
 * Owning bundle for the matrix allocation returned by
 * @ref build_synthetic_matrix. The search routine expects both the node array
 * and the preallocated solution buffers.
 */
struct SyntheticMatrix
{
    struct node* matrix = nullptr;
    char** solutions = nullptr;
    int item_count = 0;
    int option_count = 0;
};

/**
 * Captures the statistics for a single performance case that will be appended
 * to the CSV report when every test completes successfully.
 *
 * Individual performance parameters (column count, group count, variants per
 * group) are supplied via PerformanceTestConfig::search_cases so the suite runs
 * whatever matrix mix is described in performance_config.yaml.
 */
struct PerformanceRecord
{
    uint32_t columns;
    uint32_t groups;
    uint32_t variants;
    uint64_t solutions;
    double duration_ms;
};

using PerformanceParam = SearchPerformanceCase;

/**
 * Result container used by the worker threads to communicate whether a case
 * succeeded and, if so, which metrics were captured.
 */
struct CaseResult
{
    PerformanceParam param{};
    bool success = false;
    std::string error;
    PerformanceRecord record{};
};

/**
 * RAII helper that guarantees matrices allocated via build_synthetic_matrix are
 * freed even when an assertion or early-return triggers. Tests allocate a large
 * contiguous block per case, so automatic cleanup keeps the suite leak-free.
 */
class MatrixGuard
{
public:
    /**
     * Constructs a guard bound to the provided matrix reference.
     *
     * @param matrix Synthetic matrix whose storage should be released when the
     *               guard is destroyed or reset.
     */
    explicit MatrixGuard(SyntheticMatrix& matrix)
        : matrix_(matrix)
    {}

    /**
     * Destructor automatically frees the underlying matrix allocation to avoid
     * leaking large buffers between test cases.
     */
    ~MatrixGuard()
    {
        // Ensure matrices are reclaimed even if destruction happens during stack unwinding.
        reset();
    }

    /**
     * Manually releases the matrix allocation if it is still owned by the
     * guard. Safe to call multiple times.
     */
    void reset()
    {
        if (matrix_.matrix != nullptr || matrix_.solutions != nullptr)
        {
            // Delegate to the core helper that knows how to free both buffers.
            dlx::Core::freeMemory(matrix_.matrix, matrix_.solutions);
            matrix_.matrix = nullptr;
            matrix_.solutions = nullptr;
        }
    }

private:
    SyntheticMatrix& matrix_;
};

/**
 * Thread-safe collector that aggregates @ref PerformanceRecord instances across
 * the parallel workers and emits them into a deterministic CSV snapshot once the
 * suite finishes. Records are buffered in-memory so disk I/O only happens if the
 * entire suite passes.
 */
class PerformanceReport
{
public:
    static PerformanceReport& instance()
    {
        // Meyers singleton to share the report across tests.
        static PerformanceReport instance;
        return instance;
    }

    /**
     * Clears any accumulated records so subsequent runs start fresh.
     */
    void reset()
    {
        // Protect the shared vector while clearing it.
        std::lock_guard<std::mutex> lock(mutex_);
        records_.clear();
    }

    /**
     * Appends a record produced by an individual case.
     *
     * @param record Measured performance metrics.
     */
    void add_record(const PerformanceRecord& record)
    {
        // Serialize writes so workers can push concurrently.
        std::lock_guard<std::mutex> lock(mutex_);
        records_.push_back(record);
        // Preserve insertion order so the CSV mirrors parameter ordering.
    }

    /**
     * Writes all captured records to @p path if at least one case succeeded.
     *
     * @param path Destination CSV file path.
     */
    void write_csv(const std::string& path) const
    {
        std::vector<PerformanceRecord> snapshot;
        {
            // Copy under lock so the mutex isn't held while writing to disk.
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = records_;
        }

        if (snapshot.empty())
        {
            return;
        }

        std::filesystem::path report_path(path);
        if (!report_path.parent_path().empty())
        {
            std::error_code ec;
            std::filesystem::create_directories(report_path.parent_path(), ec);
        }

        std::ofstream file(path, std::ios::out | std::ios::trunc);
        if (!file.is_open())
        {
            return;
        }

        // Emit deterministic header and rows for downstream tooling.
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
    mutable std::mutex mutex_;
    std::vector<PerformanceRecord> records_;
};

/**
 * Minimal SolutionSink implementation that counts how many solutions the solver
 * emits and remembers the depth of the final solution row. This allows the
 * performance harness to verify both the cardinality and the recursion depth
 * without writing to stdout or files.
 */
class CountingSink : public dlx::sink::SolutionSink
{
public:
    /**
     * Tracks the total number of solution callbacks and the depth of the last
     * solution observed.
     */
    int last_solution_size = 0;
    uint64_t solution_count = 0;

    /**
     * Records the solution count and remembers the row depth.
     *
     * @param view Solution view emitted by the core solver.
     */
    void on_solution(const dlx::sink::SolutionView& view) override
    {
        // Count every callback to verify the solver traversed all permutations.
        solution_count += 1;
        last_solution_size = view.count;
    }
};

/**
 * GoogleTest fixture responsible for resetting the performance report before
 * the suite runs and flushing the aggregated CSV after every case finishes.
 */
class DlxSearchPerformanceTest : public ::testing::Test
{
protected:
    /** Resets the global performance report prior to executing cases. */
    static void SetUpTestSuite()
    {
        // Start each suite run with an empty report.
        PerformanceReport::instance().reset();
    }

    /**
     * Emits the aggregated CSV when the suite passes; otherwise discards
     * partial results produced by failed cases.
     */
    static void TearDownTestSuite()
    {
        const PerformanceTestConfig& config = GetPerformanceTestConfig();
        if (::testing::UnitTest::GetInstance()->Passed())
        {
            // Only emit the CSV when all cases succeeded.
            PerformanceReport::instance().write_csv(config.search_report_path);
        }
        else
        {
            // Discard partial measurements from failed runs.
            PerformanceReport::instance().reset();
        }
    }
};

/**
 * Initializes the circular list of column headers in the contiguous matrix
 * allocation returned by @ref dlx::matrix::generateHeadNode. Slot zero is the
 * matrix head; each subsequent entry becomes a column header whose right/left
 * pointers form the top-level doubly linked list.
 *
 * @param matrix Pointer to the matrix head produced by generateHeadNode.
 * @param column_count Number of constraint columns to initialize.
 */
void initialize_column_headers(struct node* matrix, uint32_t column_count)
{
    // Anchor the head so its left/right form a closed loop initially.
    matrix[0].right = matrix;
    matrix[0].left = matrix;

    for (uint32_t i = 0; i < column_count; ++i)
    {
        struct node* column = &matrix[i + 1];
        // Initialize column metadata and point the column at itself vertically.
        column->len = 0;
        column->top = matrix;
        column->left = &matrix[i];
        column->right = matrix;
        column->up = column;
        column->down = column;
        column->data = static_cast<int>(i + 1);

        // Insert the column into the head's horizontal list.
        matrix[i].right = column;
        matrix[0].left = column;
        // Maintain circular links by always updating the head's left pointer.
    }
}

/**
 * Determines how many column groups the default test case will create for a
 * given column count. The heuristic uses the number of base-10 digits to keep
 * recursion depth proportional to the logarithm of the matrix width.
 *
 * @param column_count Total number of constraint columns in the matrix.
 * @return Computed group count (always >= 1).
 */
uint32_t compute_group_count(uint32_t column_count)
{
    if (column_count == 0)
    {
        // Degenerate cases still need at least one group to avoid divide-by-zero.
        return 1;
    }

    uint32_t value = column_count;
    uint32_t count = 0;
    while (value >= 10)
    {
        // Counting digits by repeatedly dividing by ten.
        value /= 10;
        count += 1;
    }

    return count == 0 ? 1 : count;
}

/**
 * Evenly partitions the column range across @p group_count chunks, distributing
 * any remainder among the earliest groups.
 *
 * @param column_count Total number of constraint columns.
 * @param group_count How many contiguous groups to create.
 * @param group_index Index of the group being sized (0-based).
 * @return Number of columns assigned to the requested group.
 */
uint32_t select_group_size(uint32_t column_count, uint32_t group_count, uint32_t group_index)
{
    // Base size distributes columns evenly.
    const uint32_t base = column_count / group_count;
    const uint32_t remainder = column_count % group_count;
    // First 'remainder' groups get one extra column to cover leftovers.
    return base + (group_index < remainder ? 1 : 0);
}

/**
 * Enumerates every contiguous column group and duplicates it
 * @p variants_per_group times so that each group contributes multiple
 * interchangeable rows to the matrix.
 *
 * @param column_count Total columns participating in the cover matrix.
 * @param group_count Number of disjoint column groups to generate.
 * @param variants_per_group Count of identical rows per group.
 * @return Row definitions, one vector of column indices per row.
 */
std::vector<std::vector<uint32_t>> build_group_rows(uint32_t column_count,
                                                     uint32_t group_count,
                                                     uint32_t variants_per_group)
{
    std::vector<std::vector<uint32_t>> rows;
    
    // Pre-reserve to avoid reallocations during duplication.
    rows.reserve(static_cast<size_t>(group_count) * variants_per_group);

    uint32_t column_cursor = 0;
    for (uint32_t group = 0; group < group_count; ++group)
    {
        // Determine how many consecutive columns live in this group.
        uint32_t group_size = select_group_size(column_count, group_count, group);
        
        // Prepare exact capacity for the contiguous block.
        std::vector<uint32_t> columns;
        columns.reserve(group_size);

        for (uint32_t offset = 0; offset < group_size; ++offset)
        {
            // Assign sequential column indices to keep groups contiguous.
            columns.push_back(column_cursor + offset);
        }
        // Advance the cursor so the next group starts immediately after.
        column_cursor += group_size;

        for (uint32_t variant = 0; variant < variants_per_group; ++variant)
        {
            // Duplicate the group row to create interchangeable options.
            rows.push_back(columns);
        }
    }

    // Caller consumes rows to instantiate actual nodes.
    return rows;
}

/**
 * Allocates and wires a complete Dancing Links matrix using the supplied row
 * definitions. Mirrors the production binary loader so the performance results
 * reflect realistic pointer topology.
 *
 * @param column_count Total number of constraint columns.
 * @param group_count Number of disjoint column groups represented by the rows.
 * @param variants_per_group Number of duplicated rows per group.
 * @return Populated SyntheticMatrix; fields are null when allocation fails.
 */
SyntheticMatrix build_synthetic_matrix(uint32_t column_count,
                                       uint32_t group_count,
                                       uint32_t variants_per_group)
{
    SyntheticMatrix matrix;
    if (column_count == 0 || column_count > static_cast<uint32_t>(std::numeric_limits<int>::max()))
    {
        // Invalid column counts yield an empty structure so callers can detect failure.
        return matrix;
    }
    std::vector<std::vector<uint32_t>> rows = build_group_rows(column_count,
                                                               group_count,
                                                               variants_per_group);
    if (rows.empty())
    {
        // Without rows the matrix is meaningless; surface failure immediately.
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
        // Precompute how many option nodes are needed across all rows.
        total_entries += row.size();
    }

    const size_t spacer_nodes = rows.size() + 1;
    const size_t total_nodes = static_cast<size_t>(column_count) + total_entries + spacer_nodes;
    if (total_nodes > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        // Release allocations when the matrix would overflow our integer space.
        free(matrix.solutions);
        matrix.solutions = nullptr;
        return matrix;
    }

    matrix.matrix = dlx::matrix::generateHeadNode(static_cast<int>(total_nodes));
    if (matrix.matrix == nullptr)
    {
        // Propagate allocation failure to the caller.
        free(matrix.solutions);
        matrix.solutions = nullptr;
        return matrix;
    }

    matrix.item_count = static_cast<int>(column_count);
    matrix.option_count = static_cast<int>(rows.size());

    // Prepare column headers prior to inserting row nodes.
    initialize_column_headers(matrix.matrix, column_count);

    int currNodeCount = matrix.item_count;
    int prevRowCount = 0;
    int spaceNodeCount = 0;
    int pending_row_id = 0;
    bool has_pending_row = false;

    for (size_t row_index = 0; row_index < rows.size(); ++row_index)
    {
        // Each row is preceded by a spacer node that tracks row transitions.
        dlx::matrix::handleSpacerNodes(matrix.matrix, &spaceNodeCount, currNodeCount, prevRowCount);
        if (has_pending_row)
        {
            matrix.matrix[currNodeCount + 1].data = -pending_row_id;
        }

        // Spacer encodes row id (negative until finalized).
        pending_row_id = static_cast<int>(row_index + 1);
        has_pending_row = true;

        currNodeCount++;
        prevRowCount = 0;

        const std::vector<uint32_t>& columns = rows[row_index];
        size_t column_cursor = 0;

        // Attempt to match each column header against the row's column list.
        for (int assocItemCount = 1; assocItemCount <= matrix.item_count; ++assocItemCount)
        {
            while (column_cursor < columns.size()
                   && columns[column_cursor] + 1 < static_cast<uint32_t>(assocItemCount))
            {
                // Advance until the column indices catch up with the current header id.
                column_cursor++;
            }

            if (column_cursor >= columns.size())
            {
                // No more columns referenced by this row.
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
                // Scan to the tail of the column to append the new option node.
                last = last->down;
            }

            int new_index = currNodeCount + 1;
            matrix.matrix[new_index].data = new_index;
            // Link the new option node underneath the column header.
            item->len += 1;
            item->up = &matrix.matrix[new_index];
            last->down = &matrix.matrix[new_index];
            matrix.matrix[new_index].up = last;
            matrix.matrix[new_index].top = item;
            matrix.matrix[new_index].down = item;

            // Keep track of how many option nodes were attached for this row.
            prevRowCount++;
            currNodeCount++;
            column_cursor++;
        }
        // After walking the columns, prevRowCount holds the last row's node count for spacer wiring.
        prevRowCount = static_cast<int>(columns.size());
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
    spaceNodeCount--; // Decrement because we've consumed one spacer identifier.
    matrix.matrix[currNodeCount + 1].down = matrix.matrix;
    if (prevRowCount == 0)
    {
        // First row points back to the head vertically.
        matrix.matrix[currNodeCount + 1].up = matrix.matrix;
    }
    else
    {
        // Hook the spacer into the previous row's final option node.
        matrix.matrix[currNodeCount - prevRowCount].down = &matrix.matrix[currNodeCount];
        matrix.matrix[currNodeCount + 1].up = &matrix.matrix[(currNodeCount + 1) - prevRowCount];
    }

    // Return the fully constructed synthetic matrix.
    return matrix;
}

/**
 * Computes the theoretical number of solutions assuming each column group
 * contributes @p variants_per_group interchangeable rows.
 *
 * @param variants_per_group Row permutations per group.
 * @param group_count Number of groups in the matrix.
 * @return Total expected solutions (variants_per_group^group_count).
 */
uint64_t expected_solution_count(uint32_t variants_per_group, uint32_t group_count)
{
    uint64_t total = 1;
    for (uint32_t i = 0; i < group_count; ++i)
    {
        // Multiply by the number of variants per group since groups are independent.
        total *= variants_per_group;
    }
    return total;
}

/**
 * Executes a single performance scenario: build the synthetic matrix, invoke
 * @ref dlx::Core::search, validate the expected solution count, and populate
 * the timing metrics. Any structural mismatch or solver anomaly returns false
 * alongside a human-readable error description.
 *
 * @param param Case definition (columns/groups/variants).
 * @param record_out Optional destination for the measured metrics.
 * @param error_out Optional buffer describing why the case failed.
 * @return true when the case finishes successfully, false otherwise.
 */
bool run_performance_case(const PerformanceParam& param,
                          PerformanceRecord* record_out,
                          std::string* error_out)
{
    const uint32_t column_count = param.column_count;
    
    // Allow explicit overrides but default to digit heuristic.
    const uint32_t group_count =
        (param.group_count == 0) ? compute_group_count(column_count) : param.group_count;
    
    // Preserve backwards compatibility with previous cases.
    const uint32_t variants_per_group =
        (param.variants_per_group == 0) ? kDefaultVariantsPerGroup : param.variants_per_group;

    SyntheticMatrix matrix = build_synthetic_matrix(column_count, group_count, variants_per_group);
    if (matrix.matrix == nullptr || matrix.solutions == nullptr)
    {
        if (error_out != nullptr)
        {
            // Reserve detailed context so the caller can surface the failure.
            *error_out = "Failed to build matrix";
        }
        return false;
    }

    // Ensure matrix memory is reclaimed on all exit paths.
    MatrixGuard guard(matrix);

    if (matrix.item_count != static_cast<int>(column_count))
    {
        // Sanity-check that column initialization produced the requested width.
        if (error_out != nullptr)
        {
            // Include both expected and observed counts for easier debugging.
            *error_out = "Matrix item count mismatch: expected "
                + std::to_string(column_count) + ", got "
                + std::to_string(matrix.item_count);
        }
        return false;
    }

    const int expected_rows = static_cast<int>(group_count * variants_per_group);
    if (matrix.option_count != expected_rows)
    {
        // Each group should contribute variants_per_group rows.
        if (error_out != nullptr)
        {
            *error_out = "Matrix option count mismatch: expected "
                + std::to_string(expected_rows) + ", got "
                + std::to_string(matrix.option_count);
            // Without matching rows the solver would cover an incorrect matrix.
        }
        return false;
    }

    std::vector<uint32_t> row_ids(static_cast<size_t>(matrix.option_count));
    CountingSink sink;
    dlx::SolutionOutput output_ctx;
    output_ctx.sink = &sink; // Route textual output into the counting sink instead of stdout.

    // Run the core solver while timing its execution.
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
            // Any mismatch signals that search pruned or repeated options.
        }
        return false;
    }

    if (sink.last_solution_size != static_cast<int>(group_count))
    {
        // The recursion depth equals the number of groups because one option per group is chosen.
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
        // Package the measured metrics for the CSV writer.
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

TEST_F(DlxSearchPerformanceTest, MeasuresSearchScalingParallel)
{
    const PerformanceTestConfig& config = GetPerformanceTestConfig();
    if (!config.search_performance_enabled)
    {
        GTEST_SKIP() << "Search performance tests disabled. Provide "
                     << config.source_path
                     << " with tests.search_performance.enabled: true to enable this suite.";
    }

    const auto& params = config.search_cases;
    ASSERT_FALSE(params.empty()) << "No search performance cases configured. "
                                 << "Provide tests.search_performance.cases entries in "
                                 << config.source_path;

    const size_t kCaseCount = params.size();
    
    // Pre-allocate slots for each worker to write into.
    std::vector<CaseResult> results(kCaseCount);

    size_t next_index = 0;
    std::mutex index_mutex;
    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());

    // Never spawn more threads than cases.
    const unsigned worker_count = std::min<unsigned>(hardware, static_cast<unsigned>(kCaseCount));

    // Worker lambda pulls the next case index and executes it.
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
                // Fetch the next unclaimed case.
                index = next_index++;
            }

            CaseResult result;
            result.param = params[index];
            result.success = run_performance_case(result.param, &result.record, &result.error);
            results[index] = std::move(result); // Publish outcome back to the main thread.
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(worker_count);
    for (unsigned i = 0; i < worker_count; ++i)
    {
        // Spawn up to hardware_concurrency workers.
        threads.emplace_back(worker);
    }
    for (std::thread& thread : threads)
    {
        // Ensure all work completes before evaluating results.
        thread.join();
    }

    bool any_failure = false; // Track whether any case reported an error.
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
            // Feed successful runs into the shared report.
            PerformanceReport::instance().add_record(result.record);
        }
    }

    ASSERT_FALSE(any_failure);
}

} // namespace
