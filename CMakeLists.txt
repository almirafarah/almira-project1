cmake_minimum_required(VERSION 3.10)
project(TankBattleSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific settings for shared libraries
if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Do not force global output directories; each target defines its own output

# Add subdirectories first (to build simulator_lib)
add_subdirectory(Simulator)

# Registration core library (shared so that all DLLs use the same registrar)
add_library(registration SHARED
    Simulator/AlgorithmRegistrar.cpp
    Simulator/PlayerRegistration.cpp
    Simulator/TankAlgorithmRegistration.cpp
    Simulator/GameManagerRegistration.cpp
    Simulator/GameManagerRegistrar.cpp
)
target_include_directories(registration PUBLIC common Simulator)

# Membership management library
add_library(membership STATIC
    UserCommon/Membership_212934582_323964676.cpp
    UserCommon/User_212934582_323964676.cpp
    UserCommon/MembershipManager_212934582_323964676.cpp
    UserCommon/MembershipCLI_212934582_323964676.cpp
)
target_include_directories(membership PUBLIC UserCommon common)
target_compile_features(membership PRIVATE cxx_std_17)

# Add other subdirectories
add_subdirectory(GameManager)
add_subdirectory(Algorithm)

# Print summary
message(STATUS "Build targets:")
message(STATUS "  - GameManager library: GameManager_212934582_323964676")
message(STATUS "  - Algorithm libraries: Algorithm_Aggressive_212934582_323964676, Algorithm_Simple_212934582_323964676")
message(STATUS "  - Simulator executable: simulator_212934582_323964676")

# Optional: Create a custom target to build everything  
add_custom_target(all_targets
    DEPENDS 
        GameManager_212934582_323964676
        Algorithm_Aggressive_212934582_323964676
        Algorithm_Simple_212934582_323964676
        simulator_212934582_323964676
    COMMENT "Building all targets (.so libraries and simulator)"
)
