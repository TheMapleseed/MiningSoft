# Makefile for Monero Miner on Apple Silicon
# No external dependencies required

CXX = clang++
CXXFLAGS = -std=c++23 -O3 -flto -fvectorize -DAPPLE_SILICON_OPTIMIZED -DAPPLE_SILICON_UNIVERSAL
INCLUDES = -Iinclude -Isrc
SOURCES = src/main.cpp src/miner.cpp src/randomx_native.cpp src/metal_gpu_simple.cpp src/cpu_throttle_manager.cpp src/pool_connection.cpp src/config_manager.cpp src/logger.cpp src/performance_monitor.cpp src/simple_json.cpp
HEADERS = include/miner.h include/randomx_native.h include/metal_gpu_simple.h include/cpu_throttle_manager.h include/pool_connection.h include/config_manager.h include/logger.h include/performance_monitor.h include/simple_json.h
TARGET = monero-miner

# Apple Silicon specific frameworks (simplified)
FRAMEWORKS = -framework Foundation -framework IOKit

# Default target
all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	@echo "Building Monero Miner for Apple Silicon..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) $(FRAMEWORKS) -o $(TARGET)
	@echo "Build completed successfully!"
	@echo "Executable: ./$(TARGET)"

# Clean target
clean:
	rm -f $(TARGET)
	@echo "Clean completed"

# Install target (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin/"

# Test target
test: $(TARGET)
	@echo "Running basic tests..."
	./$(TARGET) --version
	@echo "Tests completed"

# Help target
help:
	@echo "Available targets:"
	@echo "  all     - Build the miner (default)"
	@echo "  clean   - Remove built files"
	@echo "  install - Install to /usr/local/bin/"
	@echo "  test    - Run basic tests"
	@echo "  help    - Show this help"

.PHONY: all clean install test help
