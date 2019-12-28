cmake_minimum_required (VERSION 3.0)

project (aquarium)

file (GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file (GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_CXX_STANDARD 17)
set (THREADS_PREFER_PTHREAD_FLAG ON)
set (CMAKE_THREAD_PREFER_PTHREAD TRUE)

find_package (Threads REQUIRED)

include_directories (${CMAKE_SOURCE_DIR}/timer ${CMAKE_SOURCE_DIR}/atlas)
add_executable (${PROJECT_NAME} ${SOURCES})
target_link_libraries (${PROJECT_NAME} -loniondebug Threads::Threads -lgpiointerruptpp -lonioni2c -ladafruitio -lmosquittopp -lconfig -lmcp3424 ${CMAKE_BINARY_DIR}/atlas/libatlas.a ${CMAKE_BINARY_DIR}/timer/libtimer.a)
