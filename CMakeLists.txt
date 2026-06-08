cmake_minimum_required(VERSION 3.10)
project(traffic_monitor)

set(CMAKE_C_STANDARD 11)

include_directories(
    ${CMAKE_FIND_ROOT_PATH}/usr/include
)

file(GLOB SOURCES "src/*.c")

add_executable(traffic_monitor ${SOURCES})

target_link_libraries(traffic_monitor
    pthread
    ${CMAKE_FIND_ROOT_PATH}/usr/lib/libpcap.so
)
