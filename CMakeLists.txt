cmake_minimum_required(VERSION 3.10)
project(TankBattleSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Registration core library (must be defined before subdirectories that link it)
add_library(registration STATIC
    Simulator/AlgorithmRegistrar.cpp
    Simulator/PlayerRegistration.cpp
    Simulator/TankAlgorithmRegistration.cpp
    Simulator/GameManagerRegistration.cpp
)
target_include_directories(registration PUBLIC common Simulator)

# Add subdirectories
add_subdirectory(GameManager)
add_subdirectory(Algorithm)
add_subdirectory(Simulator)

# Print summary
message(STATUS "Build targets:")
message(STATUS "  - GameManager library: GameManager_212934582_323964676")
message(STATUS "  - Algorithm library: Algorithm_212934582_323964676")
message(STATUS "  - Simulator executable: simulator_212934582_323964676")

# Optional: Create a custom target to build everything  
add_custom_target(all_targets
    DEPENDS 
        GameManager_212934582_323964676
        Algorithm_212934582_323964676
        simulator_212934582_323964676
    COMMENT "Building all targets (.so libraries and simulator)"
)
