cmake_minimum_required (VERSION 3.0)

project (phcal)

file (GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file (GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_CXX_STANDARD 17)
set (THREADS_PREFER_PTHREAD_FLAG ON)
set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcompare-debug-second")

find_package (Threads REQUIRED)

include_directories (${CMAKE_SOURCE_DIR}/timer 
                    ${CMAKE_SOURCE_DIR}/atlas 
                    ${CMAKE_SOURCE_DIR}/mcp3008 
                    ${CMAKE_SOURCE_DIR}/temperature 
                    ${CMAKE_SOURCE_DIR}/errors 
                    ${CMAKE_SOURCE_DIR}/configuration)
                    
add_executable (${PROJECT_NAME} ${SOURCES})

target_link_libraries (${PROJECT_NAME} ${COMMON_FLAGS} Threads::Threads -lwiringPi -lconfig++ -lpaho-mqttpp3 -lpaho-mqtt3as
                    ${CMAKE_BINARY_DIR}/configuration/libconfiguration.a
                    ${CMAKE_BINARY_DIR}/atlas/libatlas.a 
                    ${CMAKE_BINARY_DIR}/timer/libtimer.a 
                    ${CMAKE_BINARY_DIR}/mcp3008/libmcp3008.a 
                    ${CMAKE_BINARY_DIR}/temperature/libds18b20.a 
                    ${CMAKE_BINARY_DIR}/errors/liberrors.a)

