# Makefile for Algorithm project

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -fPIC
INCLUDES = -I../common

# Directories
BUILD_DIR = build
SRC_DIR = .
OBJ_DIR = $(BUILD_DIR)/obj

# Source files
SOURCES = \
    Player_Aggressive_212934582_323964676.cpp \
    AggressiveTankAI_212934582_323964676.cpp \
    Player_Simple_212934582_323964676.cpp \
    TankAlgorithm_Simple_212934582_323964676.cpp \
    registration_all_212934582_323964676.cpp
OBJECTS = $(SOURCES:%.cpp=$(OBJ_DIR)/%.o)

# Target library
TARGET = $(BUILD_DIR)/libAlgorithm_212934582_323964676.so

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR) $(OBJ_DIR)

# Build the shared library
$(TARGET): $(OBJECTS)
	@echo "Linking Algorithm library..."
	$(CXX) -shared -o $@ $^
	@echo "Algorithm library built: $@"

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning Algorithm build artifacts..."
	@rm -rf $(BUILD_DIR)

# Install library
install: all
	@echo "Installing Algorithm library..."
	@mkdir -p ../build/lib
	@cp $(TARGET) ../build/lib/

# Show help
help:
	@echo "Algorithm Makefile targets:"
	@echo "  all      - Build the Algorithm library"
	@echo "  clean    - Clean build artifacts"
	@echo "  install  - Install library to build/lib directory"
	@echo "  help     - Show this help message"

.PHONY: all directories clean install help
