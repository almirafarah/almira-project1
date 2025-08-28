CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -O2 -I../common -I../UserCommon -I.
# export host symbols so the .so can resolve Simulator::register* during its static registration
LDFLAGS  := -rdynamic -ldl
TARGET   := simulator_212934582_323964676

# Build all .cpp files in this folder automatically
SOURCES  := $(wildcard *.cpp)
OBJECTS  := $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)