cmake_minimum_required(VERSION 3.5)
project(rtg_phoenix)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99 -lpthread")

set(SOURCE_FILES rtgutil.c)
add_executable(rtg_phoenix ${SOURCE_FILES})