# set the project name and version
cmake_minimum_required(VERSION 3.10)
project(radiosaber VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(/usr/local/boost_1_82_0)
# add the util library
add_subdirectory(util)

# add the executable
add_executable(test_util test_util.cpp)
add_executable(radiosaber main.cpp)

# add the flags
set(DEBUGFLAGS "-Wall -g -O2 -pthread")
set(RELEASEFLAGS "-O3 -Wall -g")

target_link_libraries(test_util PUBLIC util)
target_link_libraries(radiosaber PUBLIC util pthread)

# add the binary tree to the search path for include files
target_include_directories(test_util PUBLIC
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/util"
    )

target_include_directories(radiosaber PUBLIC
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/util"
    )

# set_target_properties(main PROPERTIES
#     COMPILE_FLAGS "${DEBUGFLAGS}")
set_target_properties(test_util PROPERTIES
    COMPILE_FLAGS "${DEBUGFLAGS}")
set_target_properties(radiosaber PROPERTIES
   COMPILE_FLAGS "${RELEASEFLAGS}")
