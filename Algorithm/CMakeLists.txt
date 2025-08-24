cmake_minimum_required(VERSION 3.16)
project(AlgorithmLibs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Auto-export if you don't use __declspec(dllexport)
if (WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# ---- Aggressive DLL ----
set(SOURCES_AGGR
        Player_Aggressive_212934582_323964676.cpp
        AggressiveTankAI_212934582_323964676.cpp
        AggressiveTankAI_212934582_323964676.h
)
add_library(Algorithm_Aggressive_212934582_323964676 SHARED ${SOURCES_AGGR})
target_include_directories(Algorithm_Aggressive_212934582_323964676 PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../common"
)
target_link_libraries(Algorithm_Aggressive_212934582_323964676 PUBLIC registration)

# ---- Simple DLL ----
set(SOURCES_SIMPLE
        Player_Simple_212934582_323964676.cpp
        TankAlgorithm_Simple_212934582_323964676.cpp
        TankAlgorithm_Simple_212934582_323964676.h
)
add_library(Algorithm_Simple_212934582_323964676 SHARED ${SOURCES_SIMPLE})
target_include_directories(Algorithm_Simple_212934582_323964676 PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../common"
)
target_link_libraries(Algorithm_Simple_212934582_323964676 PUBLIC registration)

# Windows-specific library naming (lib prefix)
if (WIN32)
    set_target_properties(Algorithm_Aggressive_212934582_323964676 PROPERTIES PREFIX "lib")
    set_target_properties(Algorithm_Simple_212934582_323964676 PROPERTIES PREFIX "lib")
endif()
