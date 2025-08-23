cmake_minimum_required(VERSION 3.10)
project(Simulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(../common)

# Source files
# Core simulator sources (registration is provided by shared library)
set(SOURCES
    Simulator.cpp
    DynamicLibraryLoader.cpp
)

# Create library for core functionality
add_library(simulator_lib STATIC ${SOURCES})

# Link registration core
target_link_libraries(simulator_lib PUBLIC registration)

# Create executable
add_executable(simulator_212934582_323964676 main.cpp)

# Link executable with library
target_link_libraries(simulator_212934582_323964676
    simulator_lib
    registration
    pthread  # For threading support
)

# Add platform-specific libraries for dynamic loading
if(WIN32)
    # Windows uses LoadLibrary/GetProcAddress (no extra libs needed)
    target_compile_definitions(simulator_212934582_323964676 PRIVATE WINDOWS_DYNAMIC_LOADING)
else()
    # Linux/Unix uses dlopen/dlsym
    target_link_libraries(simulator_212934582_323964676 dl)
    target_compile_definitions(simulator_212934582_323964676 PRIVATE UNIX_DYNAMIC_LOADING)
endif()

# Dynamic loading support - no static linking of algorithms/gamemanagers
# Libraries will be loaded at runtime using dlopen