cmake_minimum_required(VERSION 3.15)
project(DLX LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(dlx_binary STATIC src/dlx_binary.cpp)
target_include_directories(dlx_binary PUBLIC include)

add_library(sudoku_matrix_lib STATIC src/sudoku_matrix.cpp)
target_link_libraries(sudoku_matrix_lib PUBLIC dlx_binary)
target_include_directories(sudoku_matrix_lib PUBLIC include)

add_library(sudoku_decoder_lib STATIC src/sudoku_decoder.cpp)
target_link_libraries(sudoku_decoder_lib PUBLIC sudoku_matrix_lib dlx_binary)
target_include_directories(sudoku_decoder_lib PUBLIC include)

add_executable(dlx src/dlx.c)
target_include_directories(dlx PRIVATE include)
target_link_libraries(dlx PRIVATE dlx_binary)

add_executable(sudoku_matrix src/sudoku_matrix_main.cpp)
target_link_libraries(sudoku_matrix PRIVATE sudoku_matrix_lib)

add_executable(sudoku_decoder src/sudoku_decoder_main.cpp)
target_link_libraries(sudoku_decoder PRIVATE sudoku_decoder_lib)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_sudoku_matrix tests/test_sudoku_matrix.cpp)
    target_link_libraries(test_sudoku_matrix PRIVATE sudoku_matrix_lib dlx_binary)
    add_test(NAME sudoku_matrix_tests COMMAND test_sudoku_matrix)
    set_tests_properties(sudoku_matrix_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_sudoku_decoder tests/test_sudoku_decoder.cpp)
    target_link_libraries(test_sudoku_decoder PRIVATE sudoku_decoder_lib sudoku_matrix_lib dlx_binary)
    add_test(NAME sudoku_decoder_tests COMMAND test_sudoku_decoder)
    set_tests_properties(sudoku_decoder_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_dlx_binary tests/test_dlx_binary.cpp)
    target_link_libraries(test_dlx_binary PRIVATE dlx_binary)
    add_test(NAME dlx_binary_tests COMMAND test_dlx_binary)
    set_tests_properties(dlx_binary_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
