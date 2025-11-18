cmake_minimum_required(VERSION 3.15)
project(DLX LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_COVERAGE "Enable coverage flags in Debug builds" ON)
if(ENABLE_COVERAGE)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "Enabling coverage flags for Debug configuration")
    endif()
    add_compile_options($<$<CONFIG:Debug>:--coverage>)
    add_link_options($<$<CONFIG:Debug>:--coverage>)
endif()

add_compile_options($<$<CONFIG:Release>:-O3>)

find_package(GTest CONFIG REQUIRED)
find_package(Doxygen)
find_package(Python3 COMPONENTS Interpreter)

add_library(dlx_binary STATIC
    src/core/dlx_binary.cpp
    src/core/dlx_server.cpp
    src/core/dlx_core.cpp
    src/core/solution_sink.cpp
)
target_include_directories(dlx_binary PUBLIC include)

add_library(sudoku_encoder_lib STATIC src/sudoku/encoder/sudoku_encoder.cpp)
target_link_libraries(sudoku_encoder_lib PUBLIC dlx_binary)
target_include_directories(sudoku_encoder_lib PUBLIC include)

add_library(sudoku_decoder_lib STATIC src/sudoku/decoder/sudoku_decoder.cpp)
target_link_libraries(sudoku_decoder_lib PUBLIC sudoku_encoder_lib dlx_binary)
target_include_directories(sudoku_decoder_lib PUBLIC include)

add_executable(dlx src/core/dlx.cpp)
target_include_directories(dlx PRIVATE include)
target_link_libraries(dlx PRIVATE dlx_binary)

add_executable(sudoku_encoder src/sudoku/encoder/sudoku_encoder_main.cpp)
target_link_libraries(sudoku_encoder PRIVATE sudoku_encoder_lib)

add_executable(sudoku_decoder src/sudoku/decoder/sudoku_decoder_main.cpp)
target_link_libraries(sudoku_decoder PRIVATE sudoku_decoder_lib)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_sudoku_encoder tests/test_sudoku_encoder.cpp)
    target_link_libraries(test_sudoku_encoder PRIVATE sudoku_encoder_lib dlx_binary GTest::gtest_main)
    add_test(NAME sudoku_encoder_tests COMMAND test_sudoku_encoder)
    set_tests_properties(sudoku_encoder_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_sudoku_decoder tests/test_sudoku_decoder.cpp)
    target_link_libraries(test_sudoku_decoder PRIVATE sudoku_decoder_lib sudoku_encoder_lib dlx_binary GTest::gtest_main)
    add_test(NAME sudoku_decoder_tests COMMAND test_sudoku_decoder)
    set_tests_properties(sudoku_decoder_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_dlx_binary tests/test_dlx_binary.cpp)
    target_link_libraries(test_dlx_binary PRIVATE dlx_binary sudoku_encoder_lib GTest::gtest_main)
    add_test(NAME dlx_binary_tests COMMAND test_dlx_binary)
    set_tests_properties(dlx_binary_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_solution_sink tests/test_solution_sink.cpp)
    target_link_libraries(test_solution_sink PRIVATE dlx_binary GTest::gtest_main)
    add_test(NAME solution_sink_tests COMMAND test_solution_sink)
    set_tests_properties(solution_sink_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_matrix_dump tests/test_matrix_dump.cpp)
    target_link_libraries(test_matrix_dump PRIVATE dlx_binary GTest::gtest_main)
    add_test(NAME matrix_dump_tests COMMAND test_matrix_dump)
    set_tests_properties(matrix_dump_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_sudoku_pipeline tests/test_sudoku_pipeline.cpp)
    add_dependencies(test_sudoku_pipeline dlx sudoku_encoder sudoku_decoder)
    target_link_libraries(test_sudoku_pipeline PRIVATE GTest::gtest_main)
    add_test(NAME sudoku_pipeline_tests COMMAND test_sudoku_pipeline)
    set_tests_properties(sudoku_pipeline_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    if(Python3_Interpreter_FOUND)
        add_test(NAME conan_execute_tests
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_conan_execute.py)
        set_tests_properties(conan_execute_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endif()
endif()

if(DOXYGEN_FOUND)
    add_custom_target(doxygen-docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/docs/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating Doxygen XML documentation"
        VERBATIM)
else()
    message(STATUS "Doxygen not found. Documentation target will be unavailable.")
endif()
