cmake_minimum_required (VERSION 3.0)

project(aquarium)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcompare-debug-second")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
set (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address -static-libasan")

add_subdirectory(timer)
add_subdirectory(errors)
add_subdirectory(atlas)
add_subdirectory(temperature)
add_subdirectory(mcp3008)
add_subdirectory(configuration)
add_subdirectory(app)
add_subdirectory(calibrate_ph)
add_subdirectory(calibrate_do)
