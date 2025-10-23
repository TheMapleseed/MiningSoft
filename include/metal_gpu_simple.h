#pragma once

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <atomic>

/**
 * Simplified Metal GPU implementation for Apple Silicon
 * No Objective-C dependencies
 */
class MetalGPUSimple {
public:
    MetalGPUSimple();
    ~MetalGPUSimple();

    // Initialize Metal GPU
    bool initialize();
    
    // Check if Metal GPU is available
    bool isAvailable() const;
    
    // Get GPU information
    struct GPUInfo {
        std::string name;
        size_t memorySize;
        size_t maxThreadsPerGroup;
        size_t maxThreadgroupsPerGrid;
        bool supportsFloat16;
        bool supportsInt32;
    };
    
    GPUInfo getGPUInfo() const;
    
    // Launch mining kernel
    bool launchMiningKernel(const std::vector<uint8_t>& input,
                           std::vector<uint8_t>& output,
                           uint32_t nonceStart,
                           uint32_t nonceCount);
    
    // Set up compute pipeline
    bool setupComputePipeline();
    
    // Launch M5 GPU mining kernel with Vector Processor support
    bool launchM5MiningKernel(const std::vector<uint8_t>& input,
                             std::vector<uint8_t>& output,
                             uint32_t nonceStart,
                             uint32_t nonceCount);

    // Set up compute pipeline optimized for M5 GPU and Vector Processors
    bool setupM5ComputePipeline();

    // Launch Vector Processor specific operations
    bool launchVectorProcessorKernel(const std::vector<uint8_t>& input,
                                    std::vector<uint8_t>& output,
                                    uint32_t nonceStart,
                                    uint32_t nonceCount);

    // Cleanup resources
    void cleanup();

    // Optimize for Apple Silicon GPU
    void optimizeForAppleSilicon();
    
    // Handle GPU thermal throttling
    void handleGPUThermalThrottling();
    
    // Detect Apple Silicon generation
    int detectAppleSiliconGeneration() const;

private:
    bool m_available{false};
    bool m_initialized{false};
    GPUInfo m_gpuInfo;
    
    // Simplified GPU state
    std::vector<uint8_t> m_inputBuffer;
    std::vector<uint8_t> m_outputBuffer;
    std::vector<uint8_t> m_constantBuffer;
    
    // Apple Silicon generation
    int m_generation{0};
    
    // Performance counters
    mutable std::atomic<uint64_t> m_kernelCount{0};
    mutable std::atomic<uint64_t> m_cycleCount{0};
};
