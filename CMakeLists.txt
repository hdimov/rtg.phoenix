cmake_minimum_required(VERSION 3.5)
project(rtg.phoenix)

## set(RUNTIME_OUTPUT_DIRECTORY "/Users/lisp/ClionProjects/rtg.phoenix/bin")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

include_directories(/opt/local/include/mariadb-10.1/mysql)
include_directories(/opt/local/include/net-snmp)

link_directories(/opt/local/lib/mariadb-10.1/mysql/)
link_directories(/opt/local/lib)

set(SOURCE_FILES rtgutil.c rtghash.c rtgmysql.c putil.c pqueue.c psnmp.c ppoller.c)
add_executable(ppoller ${SOURCE_FILES})

target_link_libraries(ppoller pthread mysqlclient netsnmp)

#set(SOURCE_FILES_2 simple_snmp.c)
#add_executable(simple_snmp ${SOURCE_FILES_2})
#
#target_link_libraries(simple_snmp pthread netsnmp)
#
#set(SOURCE_FILES_3 simple_async_snmp.c)
#add_executable(simple_async_snmp ${SOURCE_FILES_3})
#
#target_link_libraries(simple_async_snmp pthread netsnmp)

##
## nd_package (pthread COMPONENTS system filesystem REQUIRED)
## find_package(mysql COMPONENTS system filesystem REQUIRED)
##
## http://nticformation.com/solutions_problems.php?tuto=2942&subCategory=c+++cmake+include+clion+allegro&Category=C++
##
