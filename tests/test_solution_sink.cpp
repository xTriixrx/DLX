#include "core/solution_sink.h"
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

namespace
{

TEST(SolutionSinkTest, OstreamSinkSerializesValues)
{
    char value1[] = "42";
    char value2[] = "84";
    char* values[] = {value1, value2};
    dlx::SolutionView view{values, 2};

    std::ostringstream output;
    dlx::OstreamSolutionSink sink(output);

    sink.on_solution(view);
    sink.flush();

    EXPECT_EQ(output.str(), "42 84\n");
}

class RecordingSink : public dlx::SolutionSink
{
public:
    std::vector<std::vector<std::string>> emissions;
    int flush_count = 0;

    void on_solution(const dlx::SolutionView& view) override
    {
        std::vector<std::string> row;
        for (int i = 0; i < view.count; i++)
        {
            row.emplace_back(view.values[i]);
        }
        emissions.push_back(row);
    }

    void flush() override { flush_count++; }
};

TEST(SolutionSinkTest, CompositeSinkBroadcastsToAllSinks)
{
    char value1[] = "7";
    char value2[] = "14";
    char value3[] = "21";
    char* values[] = {value1, value2, value3};
    dlx::SolutionView view{values, 3};

    RecordingSink first;
    RecordingSink second;

    dlx::CompositeSolutionSink composite;
    composite.add_sink(&first);
    composite.add_sink(&second);

    composite.on_solution(view);
    composite.flush();

    ASSERT_EQ(first.emissions.size(), 1u);
    ASSERT_EQ(second.emissions.size(), 1u);
    EXPECT_EQ(first.emissions[0], (std::vector<std::string>{"7", "14", "21"}));
    EXPECT_EQ(second.emissions[0], (std::vector<std::string>{"7", "14", "21"}));
    EXPECT_EQ(first.flush_count, 1);
    EXPECT_EQ(second.flush_count, 1);
}

} // namespace
