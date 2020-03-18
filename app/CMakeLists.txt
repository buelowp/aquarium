cmake_minimum_required (VERSION 3.0)

project (aquarium)

file (GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file (GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_CXX_STANDARD 17)
set (THREADS_PREFER_PTHREAD_FLAG ON)
set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
set (CMAKE_CXX_FLAGS "-fcompare-debug-second")

find_package (Threads REQUIRED)

include_directories (${CMAKE_SOURCE_DIR}/timer 
                    ${CMAKE_SOURCE_DIR}/atlas 
                    ${CMAKE_SOURCE_DIR}/mcp3008 
                    ${CMAKE_SOURCE_DIR}/DS18B20 
                    ${CMAKE_SOURCE_DIR}/errors 
                    ${CMAKE_SOURCE_DIR}/mqtt)
                    
add_executable (${PROJECT_NAME} ${SOURCES})

target_link_libraries (${PROJECT_NAME} Threads::Threads -lwiringPi -lgpiointerruptpp -ladafruitio -lmosquittopp -lconfig++ -lpaho-mqttpp3 -lpaho-mqtt3as
                    ${CMAKE_BINARY_DIR}/atlas/libatlas.a 
                    ${CMAKE_BINARY_DIR}/timer/libtimer.a 
                    ${CMAKE_BINARY_DIR}/mcp3008/libmcp3008.a 
                    ${CMAKE_BINARY_DIR}/DS18B20/libds18b20.a 
                    ${CMAKE_BINARY_DIR}/errors/liberrors.a 
                    ${CMAKE_BINARY_DIR}/mqtt/libmqtt.a)

