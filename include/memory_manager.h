#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <new>
#include <functional>
#include <sys/mman.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <Accelerate/Accelerate.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#define NEON_AVAILABLE 1
#else
#define NEON_AVAILABLE 0
#endif

// RandomX memory requirements
constexpr size_t RANDOMX_FAST_MEMORY = 2080ULL * 1024 * 1024;  // 2080 MiB
constexpr size_t RANDOMX_LIGHT_MEMORY = 256ULL * 1024 * 1024;   // 256 MiB
constexpr size_t RANDOMX_CACHE_SIZE = 64ULL * 1024 * 1024;      // 64 MiB

// Apple Silicon specific constants
constexpr size_t APPLE_SILICON_CACHE_LINE = 128;  // Apple Silicon cache line size
constexpr size_t APPLE_SILICON_PAGE_SIZE = 16384; // 16KB pages on Apple Silicon
constexpr size_t NEON_VECTOR_SIZE = 16;           // 128-bit NEON vectors

enum class MemoryMode {
    FAST,   // 2080 MiB - maximum performance
    LIGHT,  // 256 MiB - reduced memory usage
    AUTO    // Automatically determine based on available resources
};

enum class InstanceType {
    SINGLE,     // Single instance
    MULTI,      // Multiple instances
    AUTO_SCALE  // Auto-scale based on resources
};

enum class MemoryErrorType {
    ALLOCATION_FAILED,
    DEALLOCATION_FAILED,
    ALIGNMENT_FAILED,
    LOCK_FAILED,
    UNLOCK_FAILED,
    POOL_EXHAUSTED,
    INVALID_POINTER,
    RESOURCE_EXHAUSTED,
    HARDWARE_ACCELERATION_FAILED
};

class MemoryException : public std::runtime_error {
public:
    MemoryException(MemoryErrorType type, const std::string& message, size_t size = 0)
        : std::runtime_error(message), m_type(type), m_size(size) {}
    
    MemoryErrorType getType() const { return m_type; }
    size_t getSize() const { return m_size; }
    
private:
    MemoryErrorType m_type;
    size_t m_size;
};

struct MemoryStats {
    size_t totalAllocated;
    size_t totalAvailable;
    size_t instancesRunning;
    double memoryUtilization;
    double cpuUtilization;
    double temperature;
    std::chrono::steady_clock::time_point lastUpdate;
    
    MemoryStats() : totalAllocated(0), totalAvailable(0), instancesRunning(0), 
                   memoryUtilization(0.0), cpuUtilization(0.0), temperature(0.0) {}
};

class MemoryPool {
public:
    MemoryPool(size_t blockSize, size_t poolSize, bool useHardwareAcceleration = true);
    ~MemoryPool();
    
    // Memory allocation
    void* allocate();
    void deallocate(void* ptr);
    
    // Hardware acceleration
    void encodeMemory(void* data, size_t size);
    void decodeMemory(void* data, size_t size);
    
    // Statistics
    size_t getAvailableBlocks() const;
    size_t getAllocatedBlocks() const;
    double getUtilization() const;
    
    // Error handling and logging
    void logMemoryOperation(const std::string& operation, size_t size, void* ptr = nullptr) const;
    void logMemoryError(MemoryErrorType type, const std::string& message, size_t size = 0) const;
    void logMemoryStats() const;
    bool validatePointer(void* ptr) const;
    void setCustomNewHandler();
    void restoreNewHandler();
    
private:
    // Hardware acceleration helpers
    void encodeWithNEON(void* data, size_t size);
    void encodeWithAccelerate(void* data, size_t size);
    void decodeWithNEON(void* data, size_t size);
    void decodeWithAccelerate(void* data, size_t size);
    
    // Memory alignment
    bool isAligned(void* ptr) const;
    void* alignPointer(void* ptr) const;
    
private:
    std::vector<void*> m_blocks;
    std::vector<bool> m_allocated;
    size_t m_blockSize;
    size_t m_poolSize;
    bool m_useHardwareAcceleration;
    mutable std::mutex m_mutex;
    
    // Hardware acceleration buffers
    void* m_neonBuffer;
    void* m_accelerateBuffer;
    
    // Error handling
    std::new_handler m_originalNewHandler;
    
    // Memory alignment helpers
    size_t alignSize(size_t size) const;
    void* allocateAligned(size_t size) const;
    void deallocateAligned(void* ptr) const;
};

class RandomXMemoryManager {
public:
    RandomXMemoryManager();
    ~RandomXMemoryManager();
    
    // Initialization
    bool initialize(MemoryMode mode = MemoryMode::AUTO, InstanceType type = InstanceType::AUTO_SCALE);
    void shutdown();
    
    // Instance management
    bool createInstance();
    bool destroyInstance(size_t instanceId);
    size_t getMaxInstances() const;
    size_t getActiveInstances() const;
    
    // Memory allocation for RandomX
    void* allocateRandomXMemory(size_t instanceId);
    void deallocateRandomXMemory(size_t instanceId, void* ptr);
    
    // Hardware acceleration
    void encodeRandomXData(size_t instanceId, void* data, size_t size);
    void decodeRandomXData(size_t instanceId, void* data, size_t size);
    
    // Memory operations with Apple Silicon optimization
    void fillRandomXCache(size_t instanceId, const void* seed, size_t seedSize);
    void executeRandomXProgram(size_t instanceId, const void* program, size_t programSize);
    
    // Resource monitoring
    MemoryStats getMemoryStats() const;
    bool canCreateInstance() const;
    void updateResourceUsage();
    
    // Auto-scaling
    void enableAutoScaling(bool enable = true);
    void setMaxMemoryUsage(double percentage = 0.8); // 80% max memory usage
    void setMaxCPUUsage(double percentage = 0.9);    // 90% max CPU usage
    
    // Memory mode management
    void setMemoryMode(MemoryMode mode);
    MemoryMode getMemoryMode() const;
    
    // Performance optimization
    void optimizeForAppleSilicon();
    void enableNEONOptimizations(bool enable = true);
    void enableAccelerateFramework(bool enable = true);
    
    // Error handling and logging
    void logMemoryManagerStats() const;
    void logInstanceOperation(size_t instanceId, const std::string& operation) const;
    void logResourceUsage() const;
    void logError(MemoryErrorType type, const std::string& message, size_t instanceId = 0) const;
    bool validateInstance(size_t instanceId) const;
    void emergencyCleanup();
    void setMemoryErrorHandler(std::function<void(MemoryErrorType, const std::string&)> handler);
    
private:
    // Memory pools for different modes
    std::unique_ptr<MemoryPool> m_fastPool;
    std::unique_ptr<MemoryPool> m_lightPool;
    std::unique_ptr<MemoryPool> m_cachePool;
    
    // Instance management
    struct Instance {
        size_t id;
        void* memory;
        size_t memorySize;
        bool isActive;
        std::chrono::steady_clock::time_point created;
        MemoryMode mode;
    };
    
    std::vector<Instance> m_instances;
    std::atomic<size_t> m_nextInstanceId;
    mutable std::mutex m_instanceMutex;
    
    // Resource monitoring
    MemoryStats m_stats;
    std::thread m_monitoringThread;
    std::atomic<bool> m_monitoringActive;
    std::atomic<bool> m_autoScalingEnabled;
    
    // Configuration
    MemoryMode m_memoryMode;
    InstanceType m_instanceType;
    double m_maxMemoryUsage;
    double m_maxCPUUsage;
    
    // Apple Silicon optimizations
    bool m_neonEnabled;
    bool m_accelerateEnabled;
    bool m_hardwareAccelerationEnabled;
    
    // System information
    size_t m_totalMemory;
    size_t m_availableMemory;
    size_t m_cpuCores;
    size_t m_pageSize;
    
    // Error handling
    std::function<void(MemoryErrorType, const std::string&)> m_errorHandler;
    std::new_handler m_originalNewHandler;
    
    // Helper methods
    bool detectSystemResources();
    bool createMemoryPools();
    void destroyMemoryPools();
    void monitoringLoop();
    bool shouldCreateInstance() const;
    bool shouldDestroyInstance() const;
    void optimizeMemoryLayout();
    void* allocateAlignedMemory(size_t size) const;
    void deallocateAlignedMemory(void* ptr, size_t size) const;
    
    // Hardware acceleration helpers
    void encodeWithNEON(void* data, size_t size);
    void encodeWithAccelerate(void* data, size_t size);
    void decodeWithNEON(void* data, size_t size);
    void decodeWithAccelerate(void* data, size_t size);
    
    // Unix memory conventions
    size_t alignToPageSize(size_t size) const;
    size_t alignToCacheLine(size_t size) const;
    bool isPageAligned(void* ptr) const;
    bool isCacheLineAligned(void* ptr) const;
};

// Global memory manager instance
extern std::unique_ptr<RandomXMemoryManager> g_memoryManager;

// Utility functions
namespace MemoryUtils {
    // System resource detection
    size_t getTotalMemory();
    size_t getAvailableMemory();
    size_t getCPUCount();
    size_t getPageSize();
    
    // Memory alignment utilities
    void* alignToPageSize(void* ptr);
    void* alignToCacheLine(void* ptr);
    size_t alignSizeToPageSize(size_t size);
    size_t alignSizeToCacheLine(size_t size);
    
    // Apple Silicon specific
    bool hasNEONSupport();
    bool hasAccelerateFramework();
    void enableNEONOptimizations();
    void enableAccelerateOptimizations();
    
    // Memory performance
    void prefetchMemory(void* ptr, size_t size);
    void flushMemory(void* ptr, size_t size);
    void invalidateMemory(void* ptr, size_t size);
}
