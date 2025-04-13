CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g
LDFLAGS = 

# Directories
BIN_DIR = bin
OBJ_DIR = obj
SRC_DIR = src

# Detect OS
ifeq ($(OS),Windows_NT)
	MKDIR_CMD = if not exist $(subst /,\,$1) mkdir $(subst /,\,$1)
	RM_CMD = del /Q $(subst /,\,$1)
	EXE_EXT = .exe
else
	MKDIR_CMD = mkdir -p $1
	RM_CMD = rm -f $1
	EXE_EXT =
endif

# Source files
SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/Cache.cpp \
       $(SRC_DIR)/Core.cpp \
       $(SRC_DIR)/Bus.cpp \
       $(SRC_DIR)/TraceReader.cpp \
       $(SRC_DIR)/Simulator.cpp

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Executable
TARGET = $(BIN_DIR)/L1simulate$(EXE_EXT)

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	@$(call MKDIR_CMD,$(BIN_DIR))
	@$(call MKDIR_CMD,$(OBJ_DIR))

# Link object files to create executable
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean up
clean:
	$(call RM_CMD,$(subst /,\,$(OBJS) $(TARGET)))

# Run with default parameters on test traces
run: $(TARGET)
	$(TARGET) -t test_traces/test -s 4 -E 2 -b 5

# Run with help
help: $(TARGET)
	$(TARGET) -h

.PHONY: all clean run help directories 