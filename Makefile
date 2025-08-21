# Root Makefile for Tank Battle Simulator
# Builds all three projects: GameManager, Algorithm, and Simulator

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I./common -I./GameManager -I./Algorithm -I./Simulator

# Directories
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
LIB_DIR = $(BUILD_DIR)/lib

# Targets
all: directories game_manager algorithm simulator test_main

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)

# Build GameManager library
game_manager:
	@echo "Building GameManager..."
	@cd GameManager && $(MAKE) -f Makefile

# Build Algorithm library  
algorithm:
	@echo "Building Algorithm..."
	@cd Algorithm && $(MAKE) -f Makefile

# Build Simulator executable
simulator:
	@echo "Building Simulator..."
	@cd Simulator && $(MAKE) -f Makefile

# Build test executable
test_main: game_manager algorithm
	@echo "Building test executable..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(BIN_DIR)/test_gamemanager test_main.cpp \
		-L$(LIB_DIR) -lGameManager -lAlgorithm -lAlgorithmRegistrar

# Clean all build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@cd GameManager && $(MAKE) -f Makefile clean
	@cd Algorithm && $(MAKE) -f Makefile clean
	@cd Simulator && $(MAKE) -f Makefile clean
	@rm -rf $(BUILD_DIR)

# Install libraries to lib directory
install: all
	@echo "Installing libraries..."
	@cp $(GameManager)/build/*.so $(LIB_DIR)/ 2>/dev/null || true
	@cp $(Algorithm)/build/*.so $(LIB_DIR)/ 2>/dev/null || true

# Show help
help:
	@echo "Available targets:"
	@echo "  all          - Build all projects"
	@echo "  game_manager - Build GameManager library"
	@echo "  algorithm    - Build Algorithm library"
	@echo "  simulator    - Build Simulator executable"
	@echo "  test_main    - Build test executable"
	@echo "  clean        - Clean all build artifacts"
	@echo "  install      - Install libraries to lib directory"
	@echo "  help         - Show this help message"

.PHONY: all directories game_manager algorithm simulator test_main clean install help
