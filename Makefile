# Makefile for Monero Miner on Apple Silicon
# No external dependencies required

CXX = clang++
CXXFLAGS = -std=c++23 -O3 -flto -fvectorize -DAPPLE_SILICON_OPTIMIZED -DAPPLE_SILICON_UNIVERSAL -mfloat-abi=hard -mfpu=neon
INCLUDES = -Iinclude -Isrc
SOURCES = src/main.cpp src/miner.cpp src/randomx.cpp src/config_manager.cpp src/logger.cpp src/simple_json.cpp src/cli_manager.cpp src/memory_manager.cpp src/multi_pool_manager.cpp src/performance_monitor.cpp src/test_framework.cpp src/test_runner.cpp src/error_handler.cpp src/startup_tests.cpp
HEADERS = include/miner.h include/randomx.h include/config_manager.h include/logger.h include/simple_json.h include/cli_manager.h include/memory_manager.h include/multi_pool_manager.h include/performance_monitor.h include/test_framework.h include/error_handler.h include/startup_tests.h
TARGET = monero-miner

# Apple Silicon specific frameworks and libraries
FRAMEWORKS = -framework Foundation -framework IOKit -framework Accelerate
LIBS = 

# Test target
TEST_TARGET = test-runner

# Default target
all: $(TARGET)

# Test target
test: $(TEST_TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	@echo "Building Monero Miner for Apple Silicon..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) $(FRAMEWORKS) $(LIBS) -o $(TARGET)
	@echo "Build completed successfully!"

$(TEST_TARGET): src/test_runner.cpp src/test_framework.cpp src/error_handler.cpp $(HEADERS)
	@echo "Building Test Runner..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) src/test_runner.cpp src/test_framework.cpp src/error_handler.cpp $(FRAMEWORKS) $(LIBS) -o $(TEST_TARGET)
	@echo "Test runner built successfully!"
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
