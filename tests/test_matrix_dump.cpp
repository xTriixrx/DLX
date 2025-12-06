#include "core/dlx.h"
#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>
#include <sstream>

namespace
{

TEST(MatrixDumpTest, GeneratesDeterministicLayout)
{
    FILE* cover = fopen("tests/test.txt", "r");
    ASSERT_NE(cover, nullptr);

    int itemCount = dlx::Core::getItemCount(cover);
    int nodeCount = itemCount + dlx::Core::getNodeCount(cover);
    fseek(cover, 0L, SEEK_SET);

    std::ostringstream dump;
    dlx::Core::setMatrixDumpStream(&dump);
    struct node* matrix = dlx::Core::generateMatrix(cover, nodeCount);
    dlx::Core::setMatrixDumpStream(nullptr);
    fclose(cover);
    ASSERT_NE(matrix, nullptr);

    dlx::Core::freeMemory(matrix, nullptr);

    std::string output = dump.str();
    ASSERT_FALSE(output.empty());

    // Expect the dump to describe the head and first column.
    EXPECT_NE(output.find("MATRIX"), std::string::npos);
    EXPECT_NE(output.find("COLUMN index=1"), std::string::npos);
    EXPECT_NE(output.find("SPACER"), std::string::npos);
    EXPECT_NE(output.find("NODE"), std::string::npos);
}

} // namespace
