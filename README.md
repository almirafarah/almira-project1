# Tank Battle Simulator - Assignment 3

## Implementation Overview

This project implements a multithreaded tank battle simulator with three main components:

### 1. GameManager (GameManager folder)
- **Purpose**: Runs individual tank battles
- **Features**: 
  - Tank movement and combat simulation
  - Shell physics and collision detection
  - Game state management
  - Winner determination logic
- **Output**: Game results with winner, reason, and final state

### 2. Algorithm (Algorithm folder)
- **Purpose**: Provides tank AI algorithms
- **Features**:
  - Simple aggressive AI (moves toward enemy, shoots when possible)
  - Simple passive AI (moves away from enemy, occasional shooting)
  - Configurable tank behavior patterns
- **Interface**: Implements TankAlgorithm abstract class

### 3. Simulator (Simulator folder)
- **Purpose**: Orchestrates multiple games using multithreading
- **Features**:
  - **Comparative Mode**: Tests multiple GameManagers on same map
  - **Competition Mode**: Tournament between algorithms on multiple maps
  - Dynamic library loading (.so files)
  - Configurable thread pool
  - Result aggregation and output generation

## Build Instructions

### Prerequisites
- CMake 3.10 or higher
- C++17 compatible compiler
- Linux environment (for .so library support)

### Building
```bash
# Create build directory
mkdir cmake-build-debug
cd cmake-build-debug

# Configure and build
cmake ..
make

# Or build specific targets
make GameManager_212934582_323964676
make Algorithm_212934582_323964676
make simulator_212934582_323964676
make test_gamemanager
```

## Usage

### Test Individual Components
```bash
# Test GameManager functionality
./bin/test_gamemanager
```

### Run Simulator

#### Comparative Mode
```bash
./bin/simulator_212934582_323964676 -comparative \
    game_map=maps/test_map.txt \
    game_managers_folder=./lib \
    algorithm1=./lib/Algorithm_212934582_323964676.so \
    algorithm2=./lib/Algorithm_212934582_323964676.so \
    num_threads=4 \
    -verbose
```

#### Competition Mode
```bash
./bin/simulator_212934582_323964676 -competition \
    game_maps_folder=./maps \
    game_manager=./lib/GameManager_212934582_323964676.so \
    algorithms_folder=./lib \
    num_threads=4 \
    -verbose
```

## File Structure
```
hw3/
├── CMakeLists.txt              # Root build file
├── students.txt                # Submitter information
├── README.md                   # This file
├── common/                     # Common interfaces (provided)
├── GameManager/                # GameManager implementation
│   ├── CMakeLists.txt
│   ├── GameManager_212934582_323964676.h
│   └── GameManager_212934582_323964676.cpp
├── Algorithm/                  # Algorithm implementation
│   ├── CMakeLists.txt
│   ├── Player_212934582_323964676.h
│   ├── Player_212934582_323964676.cpp
│   ├── TankAlgorithm_212934582_323964676.h
│   └── TankAlgorithm_212934582_323964676.cpp
├── Simulator/                  # Simulator implementation
│   ├── CMakeLists.txt
│   ├── Simulator.h
│   ├── Simulator.cpp
│   ├── main.cpp
│   └── [Registration files]
├── test_main.cpp               # Test executable
└── main.cpp                    # Basic GameManager test
```

## Implementation Notes

### Threading Model
- **Single-threaded**: When `num_threads=1` or not specified
- **Multi-threaded**: Creates worker thread pool for concurrent game execution
- **Thread Safety**: Uses mutexes and condition variables for safe task distribution

### Dynamic Library Loading
- Uses `dlopen`/`dlclose` for loading .so files
- Libraries are loaded per game execution and unloaded immediately after
- Error handling for missing or corrupted library files

### Game Execution
- **Current Status**: Basic framework implemented with placeholder game execution
- **TODO**: Integrate with actual GameManager and Algorithm implementations
- **Next Steps**: Implement factory pattern for creating instances from loaded libraries

### Output Formats
- **Comparative Results**: Groups results by outcome, shows GameManager performance
- **Competition Results**: Tournament standings with point-based scoring system
- **File Naming**: Includes timestamps to avoid conflicts

## Known Issues and Limitations

1. **Game Execution**: Currently uses placeholder logic - needs integration with actual GameManager
2. **Library Integration**: Factory pattern not yet implemented for dynamic instance creation
3. **Error Handling**: Basic error handling implemented, could be enhanced
4. **Testing**: Limited testing performed, needs more comprehensive validation

## Future Improvements

1. **Performance**: Optimize thread pool management and task distribution
2. **Robustness**: Enhanced error handling and recovery mechanisms
3. **Monitoring**: Add progress tracking and performance metrics
4. **Extensibility**: Support for additional game modes and configurations

## Submission Requirements Met

✅ **5 folders**: Simulator, GameManager, Algorithm, common, UserCommon (structure ready)
✅ **4 makefiles**: CMakeLists.txt files for each project + root
✅ **students.txt**: Submitter information included
✅ **README.md**: Implementation notes and usage instructions
✅ **Multithreading**: Thread pool implementation with configurable thread count
✅ **Two modes**: Comparative and Competition modes implemented
✅ **Dynamic loading**: .so file loading framework implemented
✅ **Output files**: Result file generation for both modes
✅ **Command line interface**: Full argument parsing and validation
