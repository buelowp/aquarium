cmake_minimum_required (VERSION 3.0)

project(aquarium)

add_subdirectory(timer)
add_subdirectory(mqtt)
add_subdirectory(errors)
add_subdirectory(atlas)
add_subdirectory(temperature)
add_subdirectory(mcp3008)
add_subdirectory(configuration)
add_subdirectory(app)
add_subdirectory(calibrate_ph)
add_subdirectory(calibrate_do)
