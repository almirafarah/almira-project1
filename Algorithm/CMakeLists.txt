cmake_minimum_required(VERSION 3.10)
project(TankAlgorithm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(../common)

# Aggressive algorithm library
set(SOURCES_AGGR
    Player_Aggressive_212934582_323964676.cpp
    AggressiveTankAI_212934582_323964676.cpp
    AggressiveTankAI_212934582_323964676.h
    library_init_aggressive_212934582_323964676.cpp
)

add_library(Algorithm_Aggressive_212934582_323964676 SHARED ${SOURCES_AGGR})
target_link_libraries(Algorithm_Aggressive_212934582_323964676 PRIVATE registration)
set_target_properties(Algorithm_Aggressive_212934582_323964676 PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME "Algorithm_Aggressive_212934582_323964676"
)

# Simple algorithm library
set(SOURCES_SIMPLE
    Player_Simple_212934582_323964676.cpp
    TankAlgorithm_Simple_212934582_323964676.cpp
    TankAlgorithm_Simple_212934582_323964676.h
    library_init_simple_212934582_323964676.cpp
)

add_library(Algorithm_Simple_212934582_323964676 SHARED ${SOURCES_SIMPLE})
target_link_libraries(Algorithm_Simple_212934582_323964676 PRIVATE registration)
set_target_properties(Algorithm_Simple_212934582_323964676 PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME "Algorithm_Simple_212934582_323964676"
)
