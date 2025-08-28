# Makefile for Algorithm project

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -fPIC -I../common -I../UserCommon -I.

# Source files for Simple algorithm (both Player and TankAlgorithm)
SIMPLE_SOURCES = \
    Player_Simple_212934582_323964676.cpp \
    TankAlgorithm_Simple_212934582_323964676.cpp
SIMPLE_OBJECTS = $(SIMPLE_SOURCES:.cpp=.o)

# Source files for Aggressive algorithm (both Player and TankAlgorithm)
AGGRESSIVE_SOURCES = \
    Player_Aggressive_212934582_323964676.cpp \
    AggressiveTankAI_212934582_323964676.cpp
AGGRESSIVE_OBJECTS = $(AGGRESSIVE_SOURCES:.cpp=.o)

# Target libraries
SIMPLE_TARGET = Algorithm_Simple_212934582_323964676.so
AGGRESSIVE_TARGET = Algorithm_Aggressive_212934582_323964676.so

# Default target
all: $(SIMPLE_TARGET) $(AGGRESSIVE_TARGET)

# Build the Simple algorithm library (contains both Player and TankAlgorithm)
$(SIMPLE_TARGET): $(SIMPLE_OBJECTS)
	@echo "Linking Simple Algorithm library..."
	$(CXX) -shared -o $@ $(SIMPLE_OBJECTS)
	@echo "Simple Algorithm library built: $@"

# Build the Aggressive algorithm library (contains both Player and TankAlgorithm)
$(AGGRESSIVE_TARGET): $(AGGRESSIVE_OBJECTS)
	@echo "Linking Aggressive Algorithm library..."
	$(CXX) -shared -o $@ $(AGGRESSIVE_OBJECTS)
	@echo "Aggressive Algorithm library built: $@"

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(SIMPLE_OBJECTS) $(AGGRESSIVE_OBJECTS) $(SIMPLE_TARGET) $(AGGRESSIVE_TARGET)

# Install library
install: all
	@echo "Installing Algorithm libraries..."
	@cp $(SIMPLE_TARGET) ../
	@cp $(AGGRESSIVE_TARGET) ../

# Show help
help:
	@echo "Algorithm Makefile targets:"
	@echo "  all      - Build both Simple and Aggressive Algorithm libraries"
	@echo "  clean    - Clean build artifacts"
	@echo "  install  - Copy libraries to project root"
	@echo "  help     - Show this help message"

.PHONY: all clean install help
