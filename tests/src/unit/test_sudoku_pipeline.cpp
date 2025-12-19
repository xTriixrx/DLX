#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace
{

std::string read_file_to_string(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return "";
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void run_pipeline_and_expect_success(const std::string& pipeline_command)
{
    std::string command = "bash -lc \"set -o pipefail; " + pipeline_command + "\"";
    int status = system(command.c_str());
    ASSERT_EQ(status, 0) << "Pipeline command failed: " << pipeline_command;
}

} // namespace

TEST(SudokuPipelineTest, BinaryPipelineProducesAnswersFile)
{
    const std::string answers_path = "build/pipeline_answers_binary.txt";
    std::remove(answers_path.c_str());

    const std::string pipeline =
        "build/sudoku_encoder tests/sudoku_tests/sudoku_test.txt | "
        "build/dlx | "
        "build/sudoku_decoder tests/sudoku_tests/sudoku_test.txt > " +
        answers_path;
    run_pipeline_and_expect_success(pipeline);

    const std::string actual = read_file_to_string(answers_path);
    ASSERT_FALSE(actual.empty());

    const std::string expected =
        read_file_to_string("tests/sudoku_example/sudoku_solution.txt");
    EXPECT_EQ(actual, expected);

    std::remove(answers_path.c_str());
}
