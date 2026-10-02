cmake_minimum_required(VERSION 3.16)
project(cislc_spike_module_models LANGUAGES CXX)

add_library(cislc_ubtb STATIC ubtb.cc)
target_compile_features(cislc_ubtb PUBLIC cxx_std_17)
target_include_directories(cislc_ubtb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cislc_ubtb_runner ubtb_runner.cc)
target_link_libraries(cislc_ubtb_runner PRIVATE cislc_ubtb)
