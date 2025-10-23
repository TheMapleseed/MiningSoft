#pragma once

#include <memory>
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <chrono>

class RandomXNative;
class MetalGPUSimple;
class CPUThrottleManager;
class PoolConnection;
class ConfigManager;
class Logger;
class PerformanceMonitor;

/**
 * Main miner class optimized for Apple Silicon
 * Handles coordination between all mining components
 */
class Miner {
public:
    Miner();
    ~Miner();

    // Initialize the miner with configuration
    bool initialize(const std::string& configFile = "config.json");
    bool initialize(std::shared_ptr<ConfigManager> configManager);
    
    // Start mining process
    void start();
    
    // Stop mining process
    void stop();
    
    // Check if miner is running
    bool isRunning() const { return m_running; }
    
    // Get mining statistics
    struct MiningStats {
        double hashrate;
        uint64_t totalHashes;
        uint64_t acceptedShares;
        uint64_t rejectedShares;
        double temperature;
        double powerConsumption;
        std::chrono::steady_clock::time_point startTime;
    };
    
    MiningStats getStats() const;
    
private:
    bool initializeInternal();
    
    // Get current hashrate
    double getHashrate() const;
    
    // Get total hashes computed
    uint64_t getTotalHashes() const { return m_totalHashes; }

private:
    // Core mining loop
    void miningLoop();
    
    // Process a single hash
    bool processHash(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);
    
    // Submit share to pool
    void submitShare(const std::vector<uint8_t>& nonce, const std::vector<uint8_t>& hash);
    
    // Update performance metrics
    void updateMetrics();
    
    // Handle CPU demand-based throttling
    void handleCPUThrottling();
    
    // Get current CPU usage
    double getCPUUsage() const;
    
    // Helper function to convert bytes to hex string
    std::string bytesToHex(const std::vector<uint8_t>& bytes);

private:
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_shouldStop{false};
    std::atomic<uint64_t> m_totalHashes{0};
    std::atomic<uint64_t> m_acceptedShares{0};
    std::atomic<uint64_t> m_rejectedShares{0};
    
    std::unique_ptr<RandomXNative> m_randomX;
    std::unique_ptr<MetalGPUSimple> m_metalGPU;
    std::unique_ptr<CPUThrottleManager> m_cpuThrottleManager;
    std::unique_ptr<PoolConnection> m_poolConnection;
    std::shared_ptr<ConfigManager> m_configManager;
    std::unique_ptr<Logger> m_logger;
    std::unique_ptr<PerformanceMonitor> m_performanceMonitor;
    
    std::thread m_miningThread;
    std::chrono::steady_clock::time_point m_startTime;
    
    // Performance tracking
    mutable std::mutex m_statsMutex;
    double m_currentHashrate{0.0};
    double m_temperature{0.0};
    double m_powerConsumption{0.0};
};
