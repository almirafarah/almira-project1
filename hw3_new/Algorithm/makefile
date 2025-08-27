CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -O2 -fPIC -I../common -I../UserCommon -I.
TARGET   := Algorithm_322837089_212141253.so

# adjust these if your filenames differ:
SOURCES  := Player_322837089_212141253.cpp \
            TankAlgorithm_322837089_212141253.cpp
OBJECTS  := $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -shared -o $@ $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
