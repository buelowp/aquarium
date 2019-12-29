cmake_minimum_required (VERSION 3.0)

project (docal)

file (GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file (GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_CXX_STANDARD 17)
set (THREADS_PREFER_PTHREAD_FLAG ON)
set (CMAKE_THREAD_PREFER_PTHREAD TRUE)

find_package (Threads REQUIRED)

include_directories (${CMAKE_SOURCE_DIR}/timer ${CMAKE_SOURCE_DIR}/atlas ${CMAKE_SOURCE_DIR}/app)
add_executable (${PROJECT_NAME} ${SOURCES})
target_link_libraries (${PROJECT_NAME} Threads::Threads -lgpiointerruptpp -lconfig ${CMAKE_BINARY_DIR}/atlas/libatlas.a ${CMAKE_BINARY_DIR}/timer/libtimer.a)

