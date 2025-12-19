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

add_compile_definitions(DLX_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

find_package(GTest CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(Doxygen)
find_package(Python3 COMPONENTS Interpreter)

option(ENABLE_SCHEDULER_RS "Build the scheduler-rs workspace with Cargo" ON)

add_library(dlx_binary STATIC
    src/core/binary.cpp
    src/core/tcp_server.cpp
    src/core/core.cpp
    src/core/text.cpp
    src/core/matrix.cpp
    src/core/solution_sink.cpp
)
target_include_directories(dlx_binary PUBLIC include)

add_library(sudoku_encoder_lib STATIC src/sudoku/encoder/encoder.cpp)
target_link_libraries(sudoku_encoder_lib PUBLIC dlx_binary)
target_include_directories(sudoku_encoder_lib PUBLIC include)

add_library(sudoku_decoder_lib STATIC src/sudoku/decoder/decoder.cpp)
target_link_libraries(sudoku_decoder_lib PUBLIC sudoku_encoder_lib dlx_binary)
target_include_directories(sudoku_decoder_lib PUBLIC include)

add_executable(dlx src/core/dlx.cpp)
target_include_directories(dlx PRIVATE include)
target_link_libraries(dlx PRIVATE dlx_binary)

add_executable(sudoku_encoder src/sudoku/encoder/encoder_main.cpp)
target_link_libraries(sudoku_encoder PRIVATE sudoku_encoder_lib)

add_executable(sudoku_decoder src/sudoku/decoder/decoder_main.cpp)
target_link_libraries(sudoku_decoder PRIVATE sudoku_decoder_lib)

include(CTest)
if(BUILD_TESTING)
    set(TESTS_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/tests/include)

    add_executable(test_sudoku_encoder tests/src/unit/test_sudoku_encoder.cpp)
    target_include_directories(test_sudoku_encoder PRIVATE ${TESTS_INCLUDE_DIR})
    target_link_libraries(test_sudoku_encoder PRIVATE sudoku_encoder_lib dlx_binary GTest::gtest_main)
    add_test(NAME sudoku_encoder_tests COMMAND test_sudoku_encoder)
    set_tests_properties(sudoku_encoder_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_sudoku_decoder tests/src/unit/test_sudoku_decoder.cpp)
    target_include_directories(test_sudoku_decoder PRIVATE ${TESTS_INCLUDE_DIR})
    target_link_libraries(test_sudoku_decoder PRIVATE sudoku_decoder_lib sudoku_encoder_lib dlx_binary GTest::gtest_main)
    add_test(NAME sudoku_decoder_tests COMMAND test_sudoku_decoder)
    set_tests_properties(sudoku_decoder_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_dlx_binary tests/src/unit/test_dlx_binary.cpp tests/src/unit/ascii_binary_utils.cpp)
    target_include_directories(test_dlx_binary PRIVATE ${TESTS_INCLUDE_DIR})
    target_link_libraries(test_dlx_binary PRIVATE dlx_binary sudoku_encoder_lib GTest::gtest_main)
    add_test(NAME dlx_binary_tests COMMAND test_dlx_binary)
    set_tests_properties(dlx_binary_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_solution_sink tests/src/unit/test_solution_sink.cpp)
    target_include_directories(test_solution_sink PRIVATE ${TESTS_INCLUDE_DIR})
    target_link_libraries(test_solution_sink PRIVATE dlx_binary GTest::gtest_main)
    add_test(NAME solution_sink_tests COMMAND test_solution_sink)
    set_tests_properties(solution_sink_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_matrix_dump tests/src/unit/test_matrix_dump.cpp)
    target_include_directories(test_matrix_dump PRIVATE ${TESTS_INCLUDE_DIR})
    target_link_libraries(test_matrix_dump PRIVATE dlx_binary GTest::gtest_main)
    add_test(NAME matrix_dump_tests COMMAND test_matrix_dump)
    set_tests_properties(matrix_dump_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_dlx_search_performance
        tests/src/perf/test_dlx_search_performance.cpp
        tests/src/perf/performance_test_config.cpp)
    target_include_directories(test_dlx_search_performance PRIVATE ${TESTS_INCLUDE_DIR})
    target_link_libraries(test_dlx_search_performance PRIVATE dlx_binary GTest::gtest_main yaml-cpp::yaml-cpp)
    add_test(NAME dlx_performance_tests COMMAND test_dlx_search_performance)
    set_tests_properties(dlx_performance_tests PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS performance)

    add_executable(test_sudoku_pipeline tests/src/unit/test_sudoku_pipeline.cpp)
    target_include_directories(test_sudoku_pipeline PRIVATE ${TESTS_INCLUDE_DIR})
    add_dependencies(test_sudoku_pipeline dlx sudoku_encoder sudoku_decoder)
    target_link_libraries(test_sudoku_pipeline PRIVATE GTest::gtest_main)
    add_test(NAME sudoku_pipeline_tests COMMAND test_sudoku_pipeline)
    set_tests_properties(sudoku_pipeline_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_executable(test_dlx_server tests/src/unit/test_dlx_server.cpp tests/src/unit/ascii_binary_utils.cpp)
    target_include_directories(test_dlx_server PRIVATE ${TESTS_INCLUDE_DIR})
    target_link_libraries(test_dlx_server PRIVATE dlx_binary sudoku_encoder_lib GTest::gtest_main)
    add_test(NAME dlx_server_tests COMMAND test_dlx_server)

    add_executable(test_dlx_network_performance
        tests/src/perf/test_dlx_network_performance.cpp
        tests/src/unit/ascii_binary_utils.cpp
        tests/src/perf/performance_test_config.cpp)
    target_include_directories(test_dlx_network_performance PRIVATE ${TESTS_INCLUDE_DIR})
    target_link_libraries(test_dlx_network_performance PRIVATE dlx_binary sudoku_encoder_lib GTest::gtest_main yaml-cpp::yaml-cpp)
    add_test(NAME dlx_network_performance_tests COMMAND test_dlx_network_performance)
    set_tests_properties(dlx_server_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(dlx_network_performance_tests PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS performance)

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

if(ENABLE_SCHEDULER_RS)
    find_program(CARGO_EXECUTABLE cargo)
    if(CARGO_EXECUTABLE)
        set(SCHEDULER_RS_DIR ${CMAKE_SOURCE_DIR}/scheduler-rs)
        if(EXISTS ${SCHEDULER_RS_DIR}/Cargo.toml)
            add_custom_target(scheduler-rs ALL
                COMMAND ${CARGO_EXECUTABLE} build --workspace
                WORKING_DIRECTORY ${SCHEDULER_RS_DIR}
                USES_TERMINAL
                COMMENT "Building scheduler-rs workspace via Cargo")

            if(BUILD_TESTING)
                add_custom_target(scheduler-rs-tests
                    COMMAND ${CARGO_EXECUTABLE} test --workspace
                    WORKING_DIRECTORY ${SCHEDULER_RS_DIR}
                    USES_TERMINAL
                    COMMENT "Running scheduler-rs workspace tests")
                add_dependencies(scheduler-rs-tests scheduler-rs)
            endif()
        else()
            message(WARNING "scheduler-rs workspace not found under ${SCHEDULER_RS_DIR}")
        endif()
    else()
        message(WARNING "cargo executable not found; scheduler-rs build skipped.")
    endif()
endif()
