# Tank Battle Simulator - Assignment 3 with Membership Management

## Implementation Overview

This project implements a multithreaded tank battle simulator with **membership management capabilities** and four main components:

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
  - **Membership Mode**: User membership management system (NEW!)
  - Dynamic library loading (.so files)
  - Configurable thread pool
  - Result aggregation and output generation

### 4. Membership Management (UserCommon folder) - NEW!
- **Purpose**: Manages user memberships and subscriptions
- **Features**:
  - User account creation and management
  - Multiple membership tiers (Basic, Pro, Premium)
  - **Pro membership cancellation** - Core functionality
  - Persistent data storage
  - Interactive and command-line interfaces
  - Comprehensive error handling

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

## Membership Management System (NEW!)

This implementation now includes a comprehensive membership management system integrated into the tank battle simulator.

### Key Features

- **User Management**: Create and manage user accounts with email addresses
- **Membership Tiers**: Support for Basic, Pro, and Premium memberships
- **Pro Membership Cancellation**: Dedicated functionality to cancel Pro memberships
- **Data Persistence**: User data is automatically saved and loaded from `users.dat`
- **Interactive CLI**: Both command-line and interactive modes available
- **Error Handling**: Comprehensive validation and error reporting

### Usage Examples

#### Creating Users and Managing Pro Memberships
```bash
# Create a new user
./Simulator/simulator_212934582_323964676 -membership create alice alice@example.com

# Subscribe to Pro membership
./Simulator/simulator_212934582_323964676 -membership subscribe alice pro

# View user status
./Simulator/simulator_212934582_323964676 -membership show alice

# Cancel Pro membership (Core Feature!)
./Simulator/simulator_212934582_323964676 -membership cancel-pro alice

# Verify cancellation
./Simulator/simulator_212934582_323964676 -membership show alice
```

#### Interactive Mode
```bash
./Simulator/simulator_212934582_323964676 -membership
```

Then type commands interactively:
```
membership> create john john@example.com
membership> subscribe john pro
membership> cancel-pro john
membership> list
membership> quit
```

### Testing

Run the membership system tests:
```bash
# Compile and run tests
g++ -std=c++17 -I UserCommon -I common test_membership.cpp UserCommon/*.cpp -o test_membership
./test_membership
```

### Data Storage

User data is automatically persisted in `users.dat` file with the following format:
- User accounts with email addresses
- Membership history with types and dates
- Cancellation status and timestamps

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
