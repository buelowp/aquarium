cmake_minimum_required (VERSION 3.0)

project (configuration)

file (GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file (GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_CXX_STANDARD 17)
set (THREADS_PREFER_PTHREAD_FLAG ON)
set (CMAKE_THREAD_PREFER_PTHREAD TRUE)

find_package (Threads REQUIRED)

include_directories (${CMAKE_SOURCE_DIR}/timer 
                    ${CMAKE_SOURCE_DIR}/app 
                    ${CMAKE_SOURCE_DIR}/atlas 
                    ${CMAKE_SOURCE_DIR}/temperature
                    ${CMAKE_SOURCE_DIR}/mcp3008)

add_library (${PROJECT_NAME} STATIC ${SOURCES})
target_link_libraries (${PROJECT_NAME} ${COMMON_FLAGS} Threads::Threads -lwiringPi -lconfig++ -lpaho-mqttpp3 -lpaho-mqtt3as
                    ${CMAKE_BINARY_DIR}/atlas/libatlas.a)
