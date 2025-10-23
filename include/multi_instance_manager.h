#pragma once

#include "memory_manager.h"
#include "miner.h"
#include "config_manager.h"
#include "logger.h"
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <map>

struct InstanceConfig {
    size_t instanceId;
    std::string poolUrl;
    std::string username;
    std::string password;
    std::string workerId;
    int threads;
    bool useGPU;
    int intensity;
    MemoryMode memoryMode;
    bool autoScale;
    
    InstanceConfig() : instanceId(0), threads(0), useGPU(false), intensity(100), 
                      memoryMode(MemoryMode::AUTO), autoScale(true) {}
};

struct InstanceStats {
    size_t instanceId;
    bool isRunning;
    bool isConnected;
    double hashrate;
    uint64_t totalHashes;
    uint32_t acceptedShares;
    uint32_t rejectedShares;
    double temperature;
    double memoryUsage;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastUpdate;
    
    InstanceStats() : instanceId(0), isRunning(false), isConnected(false), 
                     hashrate(0.0), totalHashes(0), acceptedShares(0), rejectedShares(0),
                     temperature(0.0), memoryUsage(0.0) {}
};

class MultiInstanceManager {
public:
    MultiInstanceManager();
    ~MultiInstanceManager();
    
    // Initialization
    bool initialize(const ConfigManager& config);
    void shutdown();
    
    // Instance management
    bool createInstance(const InstanceConfig& config);
    bool destroyInstance(size_t instanceId);
    bool startInstance(size_t instanceId);
    bool stopInstance(size_t instanceId);
    bool restartInstance(size_t instanceId);
    
    // Bulk operations
    bool startAllInstances();
    bool stopAllInstances();
    bool restartAllInstances();
    
    // Auto-scaling
    void enableAutoScaling(bool enable = true);
    void setMaxInstances(size_t maxInstances);
    void setResourceLimits(double maxMemoryUsage = 0.8, double maxCPUUsage = 0.9);
    
    // Monitoring and statistics
    std::vector<InstanceStats> getAllInstanceStats() const;
    InstanceStats getInstanceStats(size_t instanceId) const;
    MemoryStats getMemoryStats() const;
    void updateAllStats();
    
    // Configuration management
    bool updateInstanceConfig(size_t instanceId, const InstanceConfig& config);
    InstanceConfig getInstanceConfig(size_t instanceId) const;
    
    // Resource management
    bool canCreateInstance() const;
    size_t getMaxInstances() const;
    size_t getActiveInstances() const;
    size_t getAvailableInstances() const;
    
    // Performance optimization
    void optimizeForAppleSilicon();
    void enableHardwareAcceleration(bool enable = true);
    void setMemoryMode(MemoryMode mode);
    
    // Control methods
    void pauseAllInstances();
    void resumeAllInstances();
    void emergencyStop();
    
private:
    // Instance management
    struct InstanceData {
        std::unique_ptr<Miner> miner;
        InstanceConfig config;
        InstanceStats stats;
        std::thread monitoringThread;
        std::atomic<bool> monitoringActive;
        std::chrono::steady_clock::time_point created;
        
        InstanceData() : monitoringActive(false) {}
    };
    
    std::map<size_t, InstanceData> m_instances;
    std::atomic<size_t> m_nextInstanceId;
    mutable std::mutex m_instanceMutex;
    
    // Memory management
    std::unique_ptr<RandomXMemoryManager> m_memoryManager;
    
    // Configuration
    ConfigManager m_config;
    std::atomic<bool> m_autoScalingEnabled;
    std::atomic<size_t> m_maxInstances;
    std::atomic<double> m_maxMemoryUsage;
    std::atomic<double> m_maxCPUUsage;
    
    // Monitoring
    std::thread m_globalMonitoringThread;
    std::atomic<bool> m_globalMonitoringActive;
    std::atomic<bool> m_emergencyStop;
    
    // Performance settings
    bool m_hardwareAccelerationEnabled;
    MemoryMode m_globalMemoryMode;
    
    // Helper methods
    void instanceMonitoringLoop(size_t instanceId);
    void globalMonitoringLoop();
    bool shouldCreateInstance() const;
    bool shouldDestroyInstance() const;
    void autoScaleInstances();
    void updateInstanceStats(size_t instanceId);
    void optimizeInstancePerformance(size_t instanceId);
    
    // Resource management
    bool allocateInstanceResources(size_t instanceId);
    void deallocateInstanceResources(size_t instanceId);
    double calculateInstanceMemoryUsage(size_t instanceId) const;
    double calculateInstanceCPUUsage(size_t instanceId) const;
    
    // Configuration helpers
    InstanceConfig createDefaultConfig() const;
    bool validateInstanceConfig(const InstanceConfig& config) const;
    void applyInstanceConfig(size_t instanceId, const InstanceConfig& config);
};

// Global multi-instance manager
extern std::unique_ptr<MultiInstanceManager> g_multiInstanceManager;
