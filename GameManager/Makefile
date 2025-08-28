# Makefile for GameManager project

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -fPIC
INCLUDES = -I../common

# Directories
BUILD_DIR = build
SRC_DIR = .
OBJ_DIR = $(BUILD_DIR)/obj

# Source files
SOURCES = GameManager_212934582_323964676.cpp
OBJECTS = $(SOURCES:%.cpp=$(OBJ_DIR)/%.o)

# Target library placed in the project root without 'lib' prefix
TARGET = GameManager_212934582_323964676.so

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR) $(OBJ_DIR)

# Build the shared library
$(TARGET): $(OBJECTS)
	@echo "Linking GameManager library..."
	$(CXX) -shared -o $@ $^
	@echo "GameManager library built: $@"

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning GameManager build artifacts..."
	@rm -rf $(BUILD_DIR) $(TARGET)

# Install library
install: all
	@echo "Installing GameManager library..."
	@cp $(TARGET) ../

# Show help
help:
	@echo "GameManager Makefile targets:"
	@echo "  all      - Build the GameManager library"
	@echo "  clean    - Clean build artifacts"
	@echo "  install  - Copy library to project root"
	@echo "  help     - Show this help message"

.PHONY: all directories clean install help
