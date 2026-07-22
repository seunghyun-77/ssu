cmake_minimum_required(VERSION 3.20)
project(ppsp_simulator VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(ppsp_core
    src/csv.c
    src/model.c
    src/info_state.c
    src/simulation.c
    src/policy.c
    src/experiment.c
)
target_include_directories(ppsp_core PUBLIC include)

if(MSVC)
    target_compile_options(ppsp_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(ppsp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ppsp_sim src/main.c)
target_link_libraries(ppsp_sim PRIVATE ppsp_core)

add_executable(test_info_state tests/test_info_state.c)
target_link_libraries(test_info_state PRIVATE ppsp_core)

add_executable(test_csv tests/test_csv.c)
target_link_libraries(test_csv PRIVATE ppsp_core)

add_executable(test_simulation tests/test_simulation.c)
target_link_libraries(test_simulation PRIVATE ppsp_core)

add_executable(test_policy tests/test_policy.c)
target_link_libraries(test_policy PRIVATE ppsp_core)

enable_testing()
add_test(NAME info_state COMMAND test_info_state)
add_test(NAME csv COMMAND test_csv)
add_test(NAME simulation COMMAND test_simulation)
add_test(NAME policy COMMAND test_policy)
