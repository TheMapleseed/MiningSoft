#include "memory_manager.h"
#include "logger.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cstring>
#include <functional>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <mach/mach_host.h>

// Global memory manager instance
std::unique_ptr<RandomXMemoryManager> g_memoryManager = nullptr;

// MemoryPool Implementation
MemoryPool::MemoryPool(size_t blockSize, size_t poolSize, bool useHardwareAcceleration)
    : m_blockSize(alignSize(blockSize))
    , m_poolSize(poolSize)
    , m_useHardwareAcceleration(useHardwareAcceleration)
    , m_neonBuffer(nullptr)
    , m_accelerateBuffer(nullptr) {
    
    m_blocks.reserve(poolSize);
    m_allocated.resize(poolSize, false);
    
    // Allocate aligned memory blocks
    for (size_t i = 0; i < poolSize; ++i) {
        void* block = allocateAligned(m_blockSize);
        if (block) {
            m_blocks.push_back(block);
        } else {
            LOG_ERROR("Failed to allocate memory block {}", i);
            break;
        }
    }
    
    // Initialize hardware acceleration buffers
    if (m_useHardwareAcceleration) {
        m_neonBuffer = allocateAligned(APPLE_SILICON_CACHE_LINE);
        m_accelerateBuffer = allocateAligned(m_blockSize);
        
        if (!m_neonBuffer || !m_accelerateBuffer) {
            logMemoryError(MemoryErrorType::HARDWARE_ACCELERATION_FAILED, 
                          "Failed to allocate hardware acceleration buffers", 0);
        }
    }
    
    // Set custom new handler for this pool
    setCustomNewHandler();
    
    LOG_INFO("MemoryPool created: {} blocks of {} bytes each", m_blocks.size(), m_blockSize);
    logMemoryStats();
}

MemoryPool::~MemoryPool() {
    // Deallocate all blocks
    for (void* block : m_blocks) {
        deallocateAligned(block);
    }
    
    if (m_neonBuffer) {
        deallocateAligned(m_neonBuffer);
    }
    if (m_accelerateBuffer) {
        deallocateAligned(m_accelerateBuffer);
    }
}

void* MemoryPool::allocate() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (size_t i = 0; i < m_blocks.size(); ++i) {
        if (!m_allocated[i]) {
            m_allocated[i] = true;
            logMemoryOperation("ALLOCATED", m_blockSize, m_blocks[i]);
            return m_blocks[i];
        }
    }
    
    logMemoryError(MemoryErrorType::POOL_EXHAUSTED, "No available blocks in pool", m_blockSize);
    return nullptr; // Pool exhausted
}

void MemoryPool::deallocate(void* ptr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!validatePointer(ptr)) {
        return;
    }
    
    for (size_t i = 0; i < m_blocks.size(); ++i) {
        if (m_blocks[i] == ptr) {
            m_allocated[i] = false;
            logMemoryOperation("DEALLOCATED", m_blockSize, ptr);
            return;
        }
    }
    
    logMemoryError(MemoryErrorType::INVALID_POINTER, "Pointer not found in pool during deallocation", 0);
}

void MemoryPool::encodeMemory(void* data, size_t size) {
    if (!m_useHardwareAcceleration) {
        // Simple encoding without hardware acceleration
        uint8_t* bytes = static_cast<uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            bytes[i] ^= 0xAA; // Simple XOR encoding
        }
        return;
    }
    
    // Use Apple Silicon hardware acceleration
    if (size >= NEON_VECTOR_SIZE) {
        encodeWithNEON(data, size);
    } else {
        encodeWithAccelerate(data, size);
    }
}

void MemoryPool::decodeMemory(void* data, size_t size) {
    if (!m_useHardwareAcceleration) {
        // Simple decoding without hardware acceleration
        uint8_t* bytes = static_cast<uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            bytes[i] ^= 0xAA; // Simple XOR decoding
        }
        return;
    }
    
    // Use Apple Silicon hardware acceleration
    if (size >= NEON_VECTOR_SIZE) {
        decodeWithNEON(data, size);
    } else {
        decodeWithAccelerate(data, size);
    }
}

void MemoryPool::encodeWithNEON(void* data, size_t size) {
#if NEON_AVAILABLE
    uint8_t* bytes = static_cast<uint8_t*>(data);
    const uint8_t key = 0xAA;
    
    // Process 16 bytes at a time using NEON
    size_t vectorCount = size / NEON_VECTOR_SIZE;
    for (size_t i = 0; i < vectorCount; ++i) {
        uint8_t* block = bytes + (i * NEON_VECTOR_SIZE);
        
        // Load 16 bytes into NEON register
        uint8x16_t vector = vld1q_u8(block);
        
        // Create key vector
        uint8x16_t keyVector = vdupq_n_u8(key);
        
        // XOR with key
        uint8x16_t result = veorq_u8(vector, keyVector);
        
        // Store result back
        vst1q_u8(block, result);
    }
    
    // Process remaining bytes
    size_t remaining = size % NEON_VECTOR_SIZE;
    for (size_t i = 0; i < remaining; ++i) {
        bytes[vectorCount * NEON_VECTOR_SIZE + i] ^= key;
    }
#else
    // Fallback to simple encoding if NEON not available
    uint8_t* bytes = static_cast<uint8_t*>(data);
    const uint8_t key = 0xAA;
    for (size_t i = 0; i < size; ++i) {
        bytes[i] ^= key;
    }
#endif
}

void MemoryPool::encodeWithAccelerate(void* data, size_t size) {
    // Use Accelerate framework for smaller blocks
    uint8_t* bytes = static_cast<uint8_t*>(data);
    const uint8_t key = 0xAA;
    
    // Simple XOR encoding for now
    for (size_t i = 0; i < size; ++i) {
        bytes[i] ^= key;
    }
}

void MemoryPool::decodeWithNEON(void* data, size_t size) {
    // Decoding is the same as encoding for XOR
    encodeWithNEON(data, size);
}

void MemoryPool::decodeWithAccelerate(void* data, size_t size) {
    // Decoding is the same as encoding for XOR
    encodeWithAccelerate(data, size);
}

size_t MemoryPool::alignSize(size_t size) const {
    // Align to Apple Silicon cache line size
    return ((size + APPLE_SILICON_CACHE_LINE - 1) / APPLE_SILICON_CACHE_LINE) * APPLE_SILICON_CACHE_LINE;
}

void* MemoryPool::allocateAligned(size_t size) const {
    size_t alignedSize = alignSize(size);
    void* ptr = nullptr;
    
    // Use posix_memalign for aligned allocation
    if (posix_memalign(&ptr, APPLE_SILICON_CACHE_LINE, alignedSize) != 0) {
        return nullptr;
    }
    
    // Lock memory to prevent swapping
    if (mlock(ptr, alignedSize) != 0) {
        free(ptr);
        return nullptr;
    }
    
    return ptr;
}

void MemoryPool::deallocateAligned(void* ptr) const {
    if (ptr) {
        munlock(ptr, 0); // Unlock memory
        free(ptr);
    }
}

void MemoryPool::logMemoryOperation(const std::string& operation, size_t size, void* ptr) const {
    LOG_DEBUG("MemoryPool {}: {} {} bytes at {}", 
              m_useHardwareAcceleration ? "HW-ACCEL" : "STANDARD",
              operation, size, ptr ? ptr : "nullptr");
}

void MemoryPool::logMemoryError(MemoryErrorType type, const std::string& message, size_t size) const {
    std::string errorType;
    switch (type) {
        case MemoryErrorType::ALLOCATION_FAILED:
            errorType = "ALLOCATION_FAILED";
            break;
        case MemoryErrorType::DEALLOCATION_FAILED:
            errorType = "DEALLOCATION_FAILED";
            break;
        case MemoryErrorType::ALIGNMENT_FAILED:
            errorType = "ALIGNMENT_FAILED";
            break;
        case MemoryErrorType::LOCK_FAILED:
            errorType = "LOCK_FAILED";
            break;
        case MemoryErrorType::UNLOCK_FAILED:
            errorType = "UNLOCK_FAILED";
            break;
        case MemoryErrorType::POOL_EXHAUSTED:
            errorType = "POOL_EXHAUSTED";
            break;
        case MemoryErrorType::INVALID_POINTER:
            errorType = "INVALID_POINTER";
            break;
        case MemoryErrorType::RESOURCE_EXHAUSTED:
            errorType = "RESOURCE_EXHAUSTED";
            break;
        case MemoryErrorType::HARDWARE_ACCELERATION_FAILED:
            errorType = "HARDWARE_ACCELERATION_FAILED";
            break;
    }
    
    LOG_ERROR("MemoryPool Error [{}]: {} (Size: {} bytes)", errorType, message, size);
}

void MemoryPool::logMemoryStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t allocated = getAllocatedBlocks();
    size_t available = getAvailableBlocks();
    double utilization = getUtilization();
    
    LOG_INFO("MemoryPool Stats: {} allocated, {} available, {:.2f}% utilization", 
             allocated, available, utilization * 100.0);
}

bool MemoryPool::validatePointer(void* ptr) const {
    if (!ptr) {
        logMemoryError(MemoryErrorType::INVALID_POINTER, "Null pointer provided", 0);
        return false;
    }
    
    // Check if pointer is in our pool
    for (const void* block : m_blocks) {
        if (block == ptr) {
            return true;
        }
    }
    
    logMemoryError(MemoryErrorType::INVALID_POINTER, "Pointer not found in pool", 0);
    return false;
}

void MemoryPool::setCustomNewHandler() {
    m_originalNewHandler = std::set_new_handler([]() {
        LOG_CRITICAL("Global memory allocation failed - attempting emergency cleanup");
        
        // Try to free up memory from global memory manager
        if (g_memoryManager) {
            g_memoryManager->emergencyCleanup();
        }
        
        // If we still can't allocate, terminate
        LOG_CRITICAL("Emergency cleanup failed - terminating program");
        std::terminate();
    });
}

void MemoryPool::restoreNewHandler() {
    if (m_originalNewHandler) {
        std::set_new_handler(m_originalNewHandler);
        m_originalNewHandler = nullptr;
    }
}

// RandomXMemoryManager Implementation
RandomXMemoryManager::RandomXMemoryManager()
    : m_nextInstanceId(0)
    , m_monitoringActive(false)
    , m_autoScalingEnabled(false)
    , m_memoryMode(MemoryMode::AUTO)
    , m_instanceType(InstanceType::AUTO_SCALE)
    , m_maxMemoryUsage(0.8)
    , m_maxCPUUsage(0.9)
    , m_neonEnabled(true)
    , m_accelerateEnabled(true)
    , m_hardwareAccelerationEnabled(true)
    , m_totalMemory(0)
    , m_availableMemory(0)
    , m_cpuCores(0)
    , m_pageSize(0) {
}

RandomXMemoryManager::~RandomXMemoryManager() {
    shutdown();
}

bool RandomXMemoryManager::initialize(MemoryMode mode, InstanceType type) {
    LOG_INFO("Initializing RandomX Memory Manager");
    
    m_memoryMode = mode;
    m_instanceType = type;
    
    // Detect system resources
    if (!detectSystemResources()) {
        LOG_ERROR("Failed to detect system resources");
        return false;
    }
    
    // Create memory pools
    if (!createMemoryPools()) {
        LOG_ERROR("Failed to create memory pools");
        return false;
    }
    
    // Start monitoring thread
    m_monitoringActive = true;
    m_monitoringThread = std::thread(&RandomXMemoryManager::monitoringLoop, this);
    
    LOG_INFO("RandomX Memory Manager initialized successfully");
    LOG_INFO("Total Memory: {} MB, Available: {} MB, CPU Cores: {}", 
             m_totalMemory / (1024 * 1024), m_availableMemory / (1024 * 1024), m_cpuCores);
    
    return true;
}

void RandomXMemoryManager::shutdown() {
    LOG_INFO("Shutting down RandomX Memory Manager");
    
    // Stop monitoring
    m_monitoringActive = false;
    if (m_monitoringThread.joinable()) {
        m_monitoringThread.join();
    }
    
    // Destroy all instances
    {
        std::lock_guard<std::mutex> lock(m_instanceMutex);
        for (auto& instance : m_instances) {
            if (instance.isActive) {
                deallocateRandomXMemory(instance.id, instance.memory);
            }
        }
        m_instances.clear();
    }
    
    // Destroy memory pools
    destroyMemoryPools();
    
    LOG_INFO("RandomX Memory Manager shutdown complete");
}

bool RandomXMemoryManager::detectSystemResources() {
    // Get total memory
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    size_t length = sizeof(m_totalMemory);
    if (sysctl(mib, 2, &m_totalMemory, &length, nullptr, 0) != 0) {
        LOG_ERROR("Failed to get total memory size");
        return false;
    }
    
    // Get available memory
    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, 
                         (host_info64_t)&vmStats, &count) != KERN_SUCCESS) {
        LOG_ERROR("Failed to get VM statistics");
        return false;
    }
    
    m_availableMemory = vmStats.free_count * vm_kernel_page_size;
    
    // Get CPU count
    m_cpuCores = std::thread::hardware_concurrency();
    
    // Get page size
    m_pageSize = getpagesize();
    
    // Detect Apple Silicon features
    m_neonEnabled = MemoryUtils::hasNEONSupport();
    m_accelerateEnabled = MemoryUtils::hasAccelerateFramework();
    
    LOG_INFO("System Resources Detected:");
    LOG_INFO("  Total Memory: {} MB", m_totalMemory / (1024 * 1024));
    LOG_INFO("  Available Memory: {} MB", m_availableMemory / (1024 * 1024));
    LOG_INFO("  CPU Cores: {}", m_cpuCores);
    LOG_INFO("  Page Size: {} bytes", m_pageSize);
    LOG_INFO("  NEON Support: {}", m_neonEnabled ? "Yes" : "No");
    LOG_INFO("  Accelerate Framework: {}", m_accelerateEnabled ? "Yes" : "No");
    
    return true;
}

bool RandomXMemoryManager::createMemoryPools() {
    // Determine memory requirements based on mode
    size_t fastMemorySize = RANDOMX_FAST_MEMORY;
    size_t lightMemorySize = RANDOMX_LIGHT_MEMORY;
    size_t cacheSize = RANDOMX_CACHE_SIZE;
    
    if (m_memoryMode == MemoryMode::AUTO) {
        // Auto-determine based on available memory
        if (m_availableMemory > fastMemorySize * 2) {
            m_memoryMode = MemoryMode::FAST;
        } else {
            m_memoryMode = MemoryMode::LIGHT;
        }
    }
    
    // Create fast memory pool
    size_t fastPoolSize = (m_availableMemory * 0.6) / fastMemorySize; // Use 60% of available memory
    fastPoolSize = std::max(size_t(1), std::min(fastPoolSize, size_t(8))); // 1-8 instances max
    
    m_fastPool = std::make_unique<MemoryPool>(fastMemorySize, fastPoolSize, m_hardwareAccelerationEnabled);
    
    // Create light memory pool
    size_t lightPoolSize = (m_availableMemory * 0.8) / lightMemorySize; // Use 80% of available memory
    lightPoolSize = std::max(size_t(1), std::min(lightPoolSize, size_t(16))); // 1-16 instances max
    
    m_lightPool = std::make_unique<MemoryPool>(lightMemorySize, lightPoolSize, m_hardwareAccelerationEnabled);
    
    // Create cache pool
    size_t cachePoolSize = (m_availableMemory * 0.1) / cacheSize; // Use 10% of available memory
    cachePoolSize = std::max(size_t(1), std::min(cachePoolSize, size_t(32))); // 1-32 instances max
    
    m_cachePool = std::make_unique<MemoryPool>(cacheSize, cachePoolSize, m_hardwareAccelerationEnabled);
    
    LOG_INFO("Memory pools created:");
    LOG_INFO("  Fast Pool: {} instances of {} MB each", fastPoolSize, fastMemorySize / (1024 * 1024));
    LOG_INFO("  Light Pool: {} instances of {} MB each", lightPoolSize, lightMemorySize / (1024 * 1024));
    LOG_INFO("  Cache Pool: {} instances of {} MB each", cachePoolSize, cacheSize / (1024 * 1024));
    
    return true;
}

void RandomXMemoryManager::destroyMemoryPools() {
    m_fastPool.reset();
    m_lightPool.reset();
    m_cachePool.reset();
}

bool RandomXMemoryManager::createInstance() {
    std::lock_guard<std::mutex> lock(m_instanceMutex);
    
    if (!canCreateInstance()) {
        LOG_WARNING("Cannot create new instance - resource limits reached");
        return false;
    }
    
    Instance instance;
    instance.id = m_nextInstanceId++;
    instance.isActive = true;
    instance.created = std::chrono::steady_clock::now();
    instance.mode = m_memoryMode;
    
    // Allocate memory based on mode
    if (m_memoryMode == MemoryMode::FAST) {
        instance.memory = m_fastPool->allocate();
        instance.memorySize = RANDOMX_FAST_MEMORY;
    } else {
        instance.memory = m_lightPool->allocate();
        instance.memorySize = RANDOMX_LIGHT_MEMORY;
    }
    
    if (!instance.memory) {
        logError(MemoryErrorType::ALLOCATION_FAILED, "Failed to allocate memory for instance", instance.id);
        return false;
    }
    
    m_instances.push_back(instance);
    
    LOG_INFO("Created RandomX instance {} with {} MB memory", 
             instance.id, instance.memorySize / (1024 * 1024));
    
    return true;
}

bool RandomXMemoryManager::destroyInstance(size_t instanceId) {
    std::lock_guard<std::mutex> lock(m_instanceMutex);
    
    auto it = std::find_if(m_instances.begin(), m_instances.end(),
                          [instanceId](const Instance& inst) { return inst.id == instanceId; });
    
    if (it == m_instances.end()) {
        LOG_WARNING("Instance {} not found", instanceId);
        return false;
    }
    
    // Deallocate memory
    if (it->mode == MemoryMode::FAST) {
        m_fastPool->deallocate(it->memory);
    } else {
        m_lightPool->deallocate(it->memory);
    }
    
    it->isActive = false;
    m_instances.erase(it);
    
    LOG_INFO("Destroyed RandomX instance {}", instanceId);
    return true;
}

void* RandomXMemoryManager::allocateRandomXMemory(size_t instanceId) {
    if (!validateInstance(instanceId)) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(m_instanceMutex);
    
    auto it = std::find_if(m_instances.begin(), m_instances.end(),
                          [instanceId](const Instance& inst) { return inst.id == instanceId && inst.isActive; });
    
    if (it == m_instances.end()) {
        logError(MemoryErrorType::INVALID_POINTER, "Instance not found or not active", instanceId);
        return nullptr;
    }
    
    logInstanceOperation(instanceId, "Memory allocated");
    return it->memory;
}

void RandomXMemoryManager::deallocateRandomXMemory(size_t instanceId, void* ptr) {
    // Memory is managed by the instance, no need to deallocate here
    // The instance will be destroyed when the instance is destroyed
}

void RandomXMemoryManager::encodeRandomXData(size_t instanceId, void* data, size_t size) {
    auto it = std::find_if(m_instances.begin(), m_instances.end(),
                          [instanceId](const Instance& inst) { return inst.id == instanceId && inst.isActive; });
    
    if (it == m_instances.end()) {
        LOG_ERROR("Instance {} not found or not active", instanceId);
        return;
    }
    
    if (it->mode == MemoryMode::FAST) {
        m_fastPool->encodeMemory(data, size);
    } else {
        m_lightPool->encodeMemory(data, size);
    }
}

void RandomXMemoryManager::decodeRandomXData(size_t instanceId, void* data, size_t size) {
    auto it = std::find_if(m_instances.begin(), m_instances.end(),
                          [instanceId](const Instance& inst) { return inst.id == instanceId && inst.isActive; });
    
    if (it == m_instances.end()) {
        LOG_ERROR("Instance {} not found or not active", instanceId);
        return;
    }
    
    if (it->mode == MemoryMode::FAST) {
        m_fastPool->decodeMemory(data, size);
    } else {
        m_lightPool->decodeMemory(data, size);
    }
}

void RandomXMemoryManager::fillRandomXCache(size_t instanceId, const void* seed, size_t seedSize) {
    void* memory = allocateRandomXMemory(instanceId);
    if (!memory) {
        return;
    }
    
    // Fill memory with RandomX cache data
    // This is a simplified implementation - real RandomX would use more complex algorithms
    uint8_t* memBytes = static_cast<uint8_t*>(memory);
    const uint8_t* seedBytes = static_cast<const uint8_t*>(seed);
    
    for (size_t i = 0; i < RANDOMX_CACHE_SIZE; ++i) {
        memBytes[i] = seedBytes[i % seedSize] ^ (i & 0xFF);
    }
    
    // Encode the memory
    encodeRandomXData(instanceId, memory, RANDOMX_CACHE_SIZE);
}

void RandomXMemoryManager::executeRandomXProgram(size_t instanceId, const void* program, size_t programSize) {
    void* memory = allocateRandomXMemory(instanceId);
    if (!memory) {
        return;
    }
    
    // Execute RandomX program
    // This is a simplified implementation - real RandomX would use VM execution
    uint8_t* memBytes = static_cast<uint8_t*>(memory);
    const uint8_t* programBytes = static_cast<const uint8_t*>(program);
    
    // Simulate program execution
    for (size_t i = 0; i < programSize && i < RANDOMX_CACHE_SIZE; ++i) {
        memBytes[i] = programBytes[i] ^ memBytes[i];
    }
    
    // Decode the result
    decodeRandomXData(instanceId, memory, RANDOMX_CACHE_SIZE);
}

void RandomXMemoryManager::monitoringLoop() {
    while (m_monitoringActive) {
        updateResourceUsage();
        
        // Auto-scaling logic
        if (m_autoScalingEnabled) {
            if (shouldCreateInstance()) {
                createInstance();
            } else if (shouldDestroyInstance()) {
                // Destroy oldest instance
                std::lock_guard<std::mutex> lock(m_instanceMutex);
                if (!m_instances.empty()) {
                    auto oldest = std::min_element(m_instances.begin(), m_instances.end(),
                                                 [](const Instance& a, const Instance& b) {
                                                     return a.created < b.created;
                                                 });
                    destroyInstance(oldest->id);
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

bool RandomXMemoryManager::canCreateInstance() const {
    std::lock_guard<std::mutex> lock(m_instanceMutex);
    
    // Check if we have available memory pools
    if (m_memoryMode == MemoryMode::FAST) {
        return m_fastPool->getAvailableBlocks() > 0;
    } else {
        return m_lightPool->getAvailableBlocks() > 0;
    }
}

void RandomXMemoryManager::updateResourceUsage() {
    // Update memory statistics
    m_stats.totalAllocated = 0;
    m_stats.instancesRunning = 0;
    
    {
        std::lock_guard<std::mutex> lock(m_instanceMutex);
        for (const auto& instance : m_instances) {
            if (instance.isActive) {
                m_stats.totalAllocated += instance.memorySize;
                m_stats.instancesRunning++;
            }
        }
    }
    
    m_stats.totalAvailable = m_availableMemory;
    m_stats.memoryUtilization = static_cast<double>(m_stats.totalAllocated) / m_totalMemory;
    m_stats.lastUpdate = std::chrono::steady_clock::now();
    
    // Update CPU utilization (simplified)
    m_stats.cpuUtilization = static_cast<double>(m_stats.instancesRunning) / m_cpuCores;
}

bool RandomXMemoryManager::shouldCreateInstance() const {
    return m_autoScalingEnabled && 
           m_stats.memoryUtilization < m_maxMemoryUsage &&
           m_stats.cpuUtilization < m_maxCPUUsage &&
           canCreateInstance();
}

bool RandomXMemoryManager::shouldDestroyInstance() const {
    return m_autoScalingEnabled && 
           (m_stats.memoryUtilization > m_maxMemoryUsage ||
            m_stats.cpuUtilization > m_maxCPUUsage) &&
           m_stats.instancesRunning > 1;
}

// MemoryUtils Implementation
namespace MemoryUtils {
    size_t getTotalMemory() {
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        size_t totalMemory;
        size_t length = sizeof(totalMemory);
        if (sysctl(mib, 2, &totalMemory, &length, nullptr, 0) == 0) {
            return totalMemory;
        }
        return 0;
    }
    
    size_t getAvailableMemory() {
        vm_statistics64_data_t vmStats;
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64, 
                             (host_info64_t)&vmStats, &count) == KERN_SUCCESS) {
            return vmStats.free_count * vm_kernel_page_size;
        }
        return 0;
    }
    
    size_t getCPUCount() {
        return std::thread::hardware_concurrency();
    }
    
    size_t getPageSize() {
        return getpagesize();
    }
    
    bool hasNEONSupport() {
        // Apple Silicon always has NEON support
        return true;
    }
    
    bool hasAccelerateFramework() {
        // Check if Accelerate framework is available
        return true; // Assume available on Apple Silicon
    }
    
    void enableNEONOptimizations() {
        // NEON optimizations are enabled by default on Apple Silicon
    }
    
    void enableAccelerateOptimizations() {
        // Accelerate framework optimizations are enabled by default
    }
    
    void prefetchMemory(void* ptr, size_t size) {
        // Prefetch memory for better performance
        __builtin_prefetch(ptr, 0, 3); // Read, temporal locality
    }
    
    void flushMemory(void* ptr, size_t size) {
        // Flush memory cache
        __builtin___clear_cache(static_cast<char*>(ptr), 
                               static_cast<char*>(ptr) + size);
    }
    
    void invalidateMemory(void* ptr, size_t size) {
        // Invalidate memory cache
        __builtin___clear_cache(static_cast<char*>(ptr), 
                               static_cast<char*>(ptr) + size);
    }
}

// RandomXMemoryManager Error Handling and Logging Methods
void RandomXMemoryManager::logMemoryManagerStats() const {
    LOG_INFO("=== RandomX Memory Manager Statistics ===");
    LOG_INFO("Total System Memory: {} MB", m_totalMemory / (1024 * 1024));
    LOG_INFO("Available Memory: {} MB", m_availableMemory / (1024 * 1024));
    LOG_INFO("Active Instances: {}", m_stats.instancesRunning);
    LOG_INFO("Total Allocated: {} MB", m_stats.totalAllocated / (1024 * 1024));
    LOG_INFO("Memory Utilization: {:.2f}%", m_stats.memoryUtilization * 100.0);
    LOG_INFO("CPU Utilization: {:.2f}%", m_stats.cpuUtilization * 100.0);
    LOG_INFO("Memory Mode: {}", m_memoryMode == MemoryMode::FAST ? "FAST" : "LIGHT");
    LOG_INFO("Hardware Acceleration: {}", m_hardwareAccelerationEnabled ? "ENABLED" : "DISABLED");
    LOG_INFO("==========================================");
}

void RandomXMemoryManager::logInstanceOperation(size_t instanceId, const std::string& operation) const {
    LOG_DEBUG("Instance {}: {}", instanceId, operation);
}

void RandomXMemoryManager::logResourceUsage() const {
    // Update stats without modifying the object
    MemoryStats stats = m_stats;
    stats.totalAllocated = 0;
    stats.instancesRunning = 0;
    
    {
        std::lock_guard<std::mutex> lock(m_instanceMutex);
        for (const auto& instance : m_instances) {
            if (instance.isActive) {
                stats.totalAllocated += instance.memorySize;
                stats.instancesRunning++;
            }
        }
    }
    
    stats.totalAvailable = m_availableMemory;
    stats.memoryUtilization = static_cast<double>(stats.totalAllocated) / m_totalMemory;
    stats.cpuUtilization = static_cast<double>(stats.instancesRunning) / m_cpuCores;
    
    LOG_INFO("Resource Usage - Memory: {:.2f}%, CPU: {:.2f}%, Instances: {}", 
             stats.memoryUtilization * 100.0, stats.cpuUtilization * 100.0, stats.instancesRunning);
}

void RandomXMemoryManager::logError(MemoryErrorType type, const std::string& message, size_t instanceId) const {
    std::string errorType;
    switch (type) {
        case MemoryErrorType::ALLOCATION_FAILED:
            errorType = "ALLOCATION_FAILED";
            break;
        case MemoryErrorType::DEALLOCATION_FAILED:
            errorType = "DEALLOCATION_FAILED";
            break;
        case MemoryErrorType::ALIGNMENT_FAILED:
            errorType = "ALIGNMENT_FAILED";
            break;
        case MemoryErrorType::LOCK_FAILED:
            errorType = "LOCK_FAILED";
            break;
        case MemoryErrorType::UNLOCK_FAILED:
            errorType = "UNLOCK_FAILED";
            break;
        case MemoryErrorType::POOL_EXHAUSTED:
            errorType = "POOL_EXHAUSTED";
            break;
        case MemoryErrorType::INVALID_POINTER:
            errorType = "INVALID_POINTER";
            break;
        case MemoryErrorType::RESOURCE_EXHAUSTED:
            errorType = "RESOURCE_EXHAUSTED";
            break;
        case MemoryErrorType::HARDWARE_ACCELERATION_FAILED:
            errorType = "HARDWARE_ACCELERATION_FAILED";
            break;
    }
    
    if (instanceId > 0) {
        LOG_ERROR("MemoryManager Error [{}] Instance {}: {}", errorType, instanceId, message);
    } else {
        LOG_ERROR("MemoryManager Error [{}]: {}", errorType, message);
    }
    
    // Call custom error handler if set
    if (m_errorHandler) {
        m_errorHandler(type, message);
    }
}

bool RandomXMemoryManager::validateInstance(size_t instanceId) const {
    std::lock_guard<std::mutex> lock(m_instanceMutex);
    
    auto it = std::find_if(m_instances.begin(), m_instances.end(),
                          [instanceId](const Instance& inst) { return inst.id == instanceId; });
    
    if (it == m_instances.end()) {
        logError(MemoryErrorType::INVALID_POINTER, "Instance not found", instanceId);
        return false;
    }
    
    if (!it->isActive) {
        logError(MemoryErrorType::INVALID_POINTER, "Instance not active", instanceId);
        return false;
    }
    
    return true;
}

void RandomXMemoryManager::emergencyCleanup() {
    LOG_CRITICAL("Performing emergency memory cleanup");
    
    std::lock_guard<std::mutex> lock(m_instanceMutex);
    
    // Destroy all instances
    for (auto& instance : m_instances) {
        if (instance.isActive) {
            LOG_WARNING("Emergency cleanup: destroying instance {}", instance.id);
            
            // Deallocate memory
            if (instance.mode == MemoryMode::FAST) {
                m_fastPool->deallocate(instance.memory);
            } else {
                m_lightPool->deallocate(instance.memory);
            }
            
            instance.isActive = false;
        }
    }
    
    m_instances.clear();
    
    // Log final stats
    logMemoryManagerStats();
    
    LOG_CRITICAL("Emergency cleanup completed");
}

void RandomXMemoryManager::setMemoryErrorHandler(std::function<void(MemoryErrorType, const std::string&)> handler) {
    m_errorHandler = handler;
    LOG_INFO("Custom memory error handler set");
}
