#pragma once

#include <vector>
#include <memory>
#include <string>

/**
 * Optimized RandomX implementation for Apple Silicon
 * Leverages ARM64 vectorization and Apple Silicon specific optimizations
 */
class RandomXOptimized {
public:
    RandomXOptimized();
    ~RandomXOptimized();

    // Initialize RandomX with seed
    bool initialize(const std::vector<uint8_t>& seed);
    
    // Hash input data using RandomX algorithm
    std::vector<uint8_t> hash(const std::vector<uint8_t>& input);
    
    // Verify a hash against target difficulty
    bool verifyHash(const std::vector<uint8_t>& hash, uint64_t target);
    
    // Get optimal thread count for current hardware
    int getOptimalThreadCount() const;
    
    // Enable/disable huge pages (may not be available on macOS)
    void setHugePages(bool enabled);
    
    // Get memory usage statistics
    struct MemoryStats {
        size_t allocatedMemory;
        size_t usedMemory;
        size_t availableMemory;
    };
    
    MemoryStats getMemoryStats() const;

private:
    // ARM64 optimized polynomial multiplication
    void optimizedPolynomialMult(const uint64_t* a, const uint64_t* b, uint64_t* result);
    
    // Vectorized operations using ARM64 NEON
    void vectorizedHash(const uint8_t* input, size_t inputSize, uint8_t* output);
    
    // Memory management optimized for Apple Silicon
    void* allocateAlignedMemory(size_t size, size_t alignment = 64);
    void deallocateAlignedMemory(void* ptr, size_t size);
    
    // Cache optimization for Apple Silicon
    void optimizeCacheAccess();
    
    // Thermal-aware performance scaling
    void adjustPerformanceForTemperature(double temperature);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    
    bool m_initialized{false};
    bool m_hugePagesEnabled{false};
    int m_threadCount{0};
    
    // Performance counters
    mutable std::atomic<uint64_t> m_hashCount{0};
    mutable std::atomic<uint64_t> m_cycleCount{0};
};
