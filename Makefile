# Set the C++ version to the one we are using and -Wall -O2 for production
CXXFLAGS := -std=c++20 -Wall -O2
ASMFLAGS := -S -fverbose-asm
PROFILEFLAGS := -pg -fno-omit-frame-pointer

# Use pkg-config to find the opencv directories
OPENCV_CFLAGS := $(shell pkg-config --cflags opencv4)
OPENCV_LIBS := $(shell pkg-config --libs opencv4)

# Add them to the build variables
CXXFLAGS += $(OPENCV_CFLAGS)
LDFLAGS := $(OPENCV_LIBS)

# Define our target location, file and source files
BUILD_DIR := ./build
TARGET := main
SRC := main.cpp VideoProcessor.cpp ObjectHighlighter.cpp ControlNode.cpp

# Create our main from main.cpp using g++ flags
# make will run the first target it sees if no argument given
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/$@ $^ $(LDFLAGS)

# Target for clean (no dependencies, just clear out the executable)
clean:
	rm -f $(BUILD_DIR)/$(TARGET)

profile: $(SRC)
	$(CXX) $(CXXFLAGS) $(PROFILEFLAGS) -o $(BUILD_DIR)/$@ $^ $(LDFLAGS)

asm: ObjectHighlighter.cpp
	$(CXX) $(CXXFLAGS) $(ASMFLAGS) -o $(BUILD_DIR)/ObjectHighlighter.s ObjectHighlighter.cpp