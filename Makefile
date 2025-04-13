CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g
LDFLAGS = 

# Source files
SRCS = src/main.cpp \
       src/Cache.cpp \
       src/Core.cpp \
       src/Bus.cpp \
       src/TraceReader.cpp \
       src/Simulator.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Executable
TARGET = L1simulate

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

# Run with default parameters on app1
run:
	./$(TARGET) -t app1 -s 7 -E 2 -b 5

# Run with help
help:
	./$(TARGET) -h

.PHONY: all clean run help 