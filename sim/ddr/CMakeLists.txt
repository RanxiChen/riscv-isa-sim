cmake_minimum_required(VERSION 3.16)
project(cislc_ddr_probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THERMAL OFF CACHE BOOL "" FORCE)
set(CMD_TRACE OFF CACHE BOOL "" FORCE)
set(ADDR_TRACE OFF CACHE BOOL "" FORCE)
add_subdirectory(../../third_party/DRAMsim3 dramsim3 EXCLUDE_FROM_ALL)
# Upstream otherwise writes the shared library into its source checkout.
set_target_properties(dramsim3 PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
add_executable(ddr_master main.cpp)
target_link_libraries(ddr_master PRIVATE dramsim3 inih)
target_compile_options(ddr_master PRIVATE -Wall -Wextra -Werror)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
enable_testing()
add_test(NAME ddr_contract COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/check.py $<TARGET_FILE:ddr_master>
  ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/DRAMsim3/configs/DDR4_8Gb_x8_2400.ini
  ${CMAKE_BINARY_DIR}/test-results)
