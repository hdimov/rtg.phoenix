cmake_minimum_required(VERSION 3.5)
project(rtg_phoenix)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lpthread")

include_directories( /opt/local/include/mariadb-10.1/mysql )

set(SOURCE_FILES rtgutil.c rtghash.c rtgmysql.c rtgpoll.c)
add_executable(rtg_phoenix ${SOURCE_FILES})