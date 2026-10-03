cmake_minimum_required(VERSION 3.16)
project(breeze_spike_model LANGUAGES CXX)

add_library(breeze_model STATIC predictor.cc dcache.cc core.cc)
target_compile_features(breeze_model PUBLIC cxx_std_17)
target_include_directories(breeze_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(breeze_model PRIVATE -Wall -Wextra -Wpedantic)

add_executable(breeze_model_test model_test.cc)
target_link_libraries(breeze_model_test PRIVATE breeze_model)
enable_testing()
add_test(NAME breeze_model_contract COMMAND breeze_model_test)
