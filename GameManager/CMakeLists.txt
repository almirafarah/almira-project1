cmake_minimum_required(VERSION 3.10)
project(GameManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(../common)

# Source files
set(SOURCES
    GameManager_212934582_323964676.cpp
)

# Create shared library (.so) with unique name
add_library(GameManager_212934582_323964676 SHARED ${SOURCES})

# Link with registration system
target_link_libraries(GameManager_212934582_323964676 PRIVATE registration)

# Set output directory and library name
set_target_properties(GameManager_212934582_323964676 PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/GameManager"
)
