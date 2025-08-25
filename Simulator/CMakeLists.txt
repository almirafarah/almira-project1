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

# Include interfaces from ../common
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../common")

# ---- Simulator core ----
set(SIM_SRCS
        Simulator.cpp
        DynamicLibraryLoader.cpp
)

add_library(simulator_lib STATIC ${SIM_SRCS})
target_link_libraries(simulator_lib PUBLIC registration)

add_executable(simulator_212934582_323964676 main.cpp)
target_link_libraries(simulator_212934582_323964676
        simulator_lib
        registration
)

# Dynamic loading flags
if (WIN32)
    target_compile_definitions(simulator_212934582_323964676 PRIVATE WINDOWS_DYNAMIC_LOADING)
else()
    target_link_libraries(simulator_212934582_323964676 dl pthread)
    target_compile_definitions(simulator_212934582_323964676 PRIVATE UNIX_DYNAMIC_LOADING)
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