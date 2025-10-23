#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <functional>

/**
 * CPU demand-based throttling system for M5 mining
 * Monitors system CPU usage and adjusts mining intensity accordingly
 */
class CPUThrottleManager {
public:
    CPUThrottleManager();
    ~CPUThrottleManager();

    // Initialize CPU monitoring
    bool initialize();
    
    // Start CPU monitoring
    void start();
    
    // Stop CPU monitoring
    void stop();
    
    // Get current CPU usage percentage
    double getCPUUsage() const;
    
    // Get current throttle level (0.0 = no throttle, 1.0 = max throttle)
    double getThrottleLevel() const { return m_throttleLevel; }
    
    // Check if throttling is active
    bool isThrottling() const { return m_throttling; }
    
    // Set CPU usage thresholds
    void setThresholds(double lowThreshold, double highThreshold, double maxThreshold);
    
    // Set callback for CPU events
    void setCPUCallback(std::function<void(double, double)> callback);
    
    // Get CPU statistics
    struct CPUStats {
        double currentUsage;
        double averageUsage;
        double peakUsage;
        bool throttling;
        double throttleLevel;
        std::chrono::steady_clock::time_point lastUpdate;
    };
    
    CPUStats getStats() const;

private:
    // Main CPU monitoring loop
    void cpuMonitoringLoop();
    
    // Read CPU usage from system
    double readCPUUsage();
    
    // Calculate throttle level based on CPU usage
    double calculateThrottleLevel(double cpuUsage);
    
    // Apply CPU-based throttling
    void applyThrottling(double throttleLevel);
    
    // Reset throttling
    void resetThrottling();

private:
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_shouldStop{false};
    std::atomic<bool> m_throttling{false};
    std::atomic<double> m_throttleLevel{0.0};
    
    std::thread m_cpuThread;
    
    // CPU usage thresholds
    double m_lowThreshold{20.0};    // Below this, no throttling
    double m_highThreshold{60.0};   // Above this, start throttling
    double m_maxThreshold{80.0};    // Above this, aggressive throttling
    
    // Current CPU usage
    std::atomic<double> m_currentUsage{0.0};
    std::atomic<double> m_averageUsage{0.0};
    std::atomic<double> m_peakUsage{0.0};
    
    // Callback for CPU events
    std::function<void(double, double)> m_cpuCallback;
    
    // Monitoring interval
    std::chrono::milliseconds m_monitoringInterval{500}; // 500ms for responsive throttling
    
    // CPU usage history for averaging
    static constexpr size_t MAX_HISTORY_SIZE = 20;
    double m_usageHistory[MAX_HISTORY_SIZE];
    size_t m_historyIndex{0};
    size_t m_historyCount{0};
};
