#pragma once

#include <vector>
#include <memory>
#include <string>
#include <functional>

#ifdef __APPLE__
#import <Metal/Metal.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#endif

/**
 * Metal GPU and Vector Processor acceleration for M5 mining
 * Optimized for M5 GPU cores and Vector Processors
 */
class MetalGPU {
public:
    MetalGPU();
    ~MetalGPU();

    // Initialize Metal GPU context
    bool initialize();
    
    // Check if Metal GPU is available
    bool isAvailable() const { return m_available; }
    
    // Get GPU device information
    struct GPUInfo {
        std::string name;
        size_t memorySize;
        size_t maxThreadsPerGroup;
        size_t maxThreadgroupsPerGrid;
        bool supportsFloat16;
        bool supportsInt32;
    };
    
    GPUInfo getGPUInfo() const;
    
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
    
    // Get GPU utilization percentage
    double getGPUUtilization() const;
    
    // Get GPU temperature
    double getGPUTemperature() const;
    
    // Get GPU memory usage
    struct MemoryUsage {
        size_t used;
        size_t total;
        double percentage;
    };
    
    MemoryUsage getMemoryUsage() const;

private:
    // Initialize Metal device and command queue
    bool initializeMetal();
    
    // Create compute pipeline state
    bool createComputePipeline();
    
    // Setup memory buffers for GPU computation
    bool setupMemoryBuffers();
    
    // Compile Metal shader source
    id<MTLFunction> compileShader(const std::string& source);
    
    // Optimize for Apple Silicon GPU
    void optimizeForAppleSilicon();
    
    // Handle GPU thermal throttling
    void handleGPUThermalThrottling();
    
    // Detect Apple Silicon generation
    int detectAppleSiliconGeneration() const;

private:
    bool m_available{false};
    bool m_initialized{false};
    
#ifdef __APPLE__
    id<MTLDevice> m_device;
    id<MTLCommandQueue> m_commandQueue;
    id<MTLComputePipelineState> m_computePipeline;
    id<MTLBuffer> m_inputBuffer;
    id<MTLBuffer> m_outputBuffer;
    id<MTLBuffer> m_constantBuffer;
#endif
    
    GPUInfo m_gpuInfo;
    
    // Performance monitoring
    mutable std::atomic<double> m_gpuUtilization{0.0};
    mutable std::atomic<double> m_gpuTemperature{0.0};
    mutable std::atomic<size_t> m_memoryUsed{0};
};
