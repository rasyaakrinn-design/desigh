cmake_minimum_required(VERSION 3.10)
project(voicedsp)
add_library(voicedsp SHARED voicedsp.cpp)
target_compile_features(voicedsp PRIVATE cxx_std_17)
