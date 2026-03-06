# Compiler settings
CXX = g++
CXXFLAGS = -std=c++11 -Wall

# Target executable name
TARGET = huffman

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): main.cpp huffman.hpp
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

# Clean up build files
clean:
	rm -f $(TARGET) $(TARGET).exe compressed.huf restored.txt