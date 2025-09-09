cmake_minimum_required(VERSION 3.16)
project(Simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# If your DLL sources don't use __declspec(dllexport)
if (WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Paths
set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Include interfaces from ../common and ../UserCommon
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../common")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../UserCommon")

# ---- Simulator core ----
set(SIM_SRCS
        Simulator.cpp
        main.cpp
        
        GameManagerRegistrar.h
        GameManagerRegistrar.cpp
        AlgorithmRegistrar.h
        AlgorithmRegistrar.cpp
        TankAlgorithmRegistration.cpp
        PlayerRegistration.cpp
)

add_executable(simulator_212934582_323964676 ${SIM_SRCS})

# Link with membership library
target_link_libraries(simulator_212934582_323964676 membership)

# Dynamic loading flags
if (WIN32)
    target_compile_definitions(simulator_212934582_323964676 PRIVATE WINDOWS_DYNAMIC_LOADING)
else()
    target_link_libraries(simulator_212934582_323964676 dl pthread)
    target_compile_definitions(simulator_212934582_323964676 PRIVATE UNIX_DYNAMIC_LOADING)
    # Export all symbols so dynamically loaded libraries can resolve TankAlgorithmRegistration
    set_target_properties(simulator_212934582_323964676 PROPERTIES LINK_FLAGS "-rdynamic")
endif()

# Run from repo root so relative "lib/..." paths work
set_target_properties(simulator_212934582_323964676 PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${ROOT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Simulator"
)

# Build order: simulator waits for DLLs (we do NOT link them; they're loaded at runtime)
add_dependencies(simulator_212934582_323964676
        Algorithm_Aggressive_212934582_323964676
        Algorithm_Simple_212934582_323964676
)