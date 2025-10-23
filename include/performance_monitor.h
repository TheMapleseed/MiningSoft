#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <map>

/**
 * Performance monitoring system for the Monero miner
 * Tracks various performance metrics and provides optimization insights
 */
class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor();

    // Initialize performance monitoring
    bool initialize();
    
    // Start monitoring
    void start();
    
    // Stop monitoring
    void stop();
    
    // Check if monitoring is active
    bool isActive() const { return m_active; }
    
    // Update mining performance metrics
    void updateMiningMetrics(uint64_t hashes, double hashrate, double temperature);
    
    // Update system performance metrics
    void updateSystemMetrics(double cpuUsage, double memoryUsage, double gpuUsage);
    
    // Record share submission
    void recordShareSubmission(bool accepted, double difficulty);
    
    // Record thermal event
    void recordThermalEvent(double temperature, bool throttling);
    
    // Get current performance summary
    struct PerformanceSummary {
        // Mining metrics
        double currentHashrate;
        double averageHashrate;
        double peakHashrate;
        uint64_t totalHashes;
        uint64_t acceptedShares;
        uint64_t rejectedShares;
        double shareAcceptanceRate;
        
        // System metrics
        double cpuUsage;
        double memoryUsage;
        double gpuUsage;
        double temperature;
        bool thermalThrottling;
        
        // Performance insights
        std::vector<std::string> recommendations;
        double efficiencyScore;
        
        // Time information
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastUpdate;
    };
    
    PerformanceSummary getSummary() const;
    
    // Get historical data
    struct HistoricalData {
        std::vector<double> hashrateHistory;
        std::vector<double> temperatureHistory;
        std::vector<double> cpuUsageHistory;
        std::vector<double> memoryUsageHistory;
        std::vector<std::chrono::steady_clock::time_point> timestamps;
    };
    
    HistoricalData getHistoricalData(int maxPoints = 100) const;
    
    // Get performance recommendations
    std::vector<std::string> getRecommendations() const;
    
    // Export performance data to file
    bool exportToFile(const std::string& filename) const;
    
    // Reset all metrics
    void reset();

private:
    // Main monitoring loop
    void monitoringLoop();
    
    // Calculate performance insights
    void calculateInsights();
    
    // Analyze thermal performance
    void analyzeThermalPerformance();
    
    // Analyze mining efficiency
    void analyzeMiningEfficiency();
    
    // Generate recommendations
    void generateRecommendations();
    
    // Update efficiency score
    void updateEfficiencyScore();
    
    // Clean up old historical data
    void cleanupHistoricalData();

private:
    std::atomic<bool> m_active{false};
    std::atomic<bool> m_shouldStop{false};
    
    std::thread m_monitoringThread;
    std::chrono::milliseconds m_monitoringInterval{5000}; // 5 seconds
    
    // Current metrics
    std::atomic<double> m_currentHashrate{0.0};
    std::atomic<double> m_averageHashrate{0.0};
    std::atomic<double> m_peakHashrate{0.0};
    std::atomic<uint64_t> m_totalHashes{0};
    std::atomic<uint64_t> m_acceptedShares{0};
    std::atomic<uint64_t> m_rejectedShares{0};
    
    std::atomic<double> m_cpuUsage{0.0};
    std::atomic<double> m_memoryUsage{0.0};
    std::atomic<double> m_gpuUsage{0.0};
    std::atomic<double> m_temperature{0.0};
    std::atomic<bool> m_thermalThrottling{false};
    
    // Historical data
    mutable std::mutex m_historicalMutex;
    std::vector<double> m_hashrateHistory;
    std::vector<double> m_temperatureHistory;
    std::vector<double> m_cpuUsageHistory;
    std::vector<double> m_memoryUsageHistory;
    std::vector<std::chrono::steady_clock::time_point> m_timestamps;
    
    // Performance tracking
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_lastUpdate;
    
    // Recommendations
    mutable std::mutex m_recommendationsMutex;
    std::vector<std::string> m_recommendations;
    std::atomic<double> m_efficiencyScore{0.0};
    
    // Configuration
    static constexpr int MAX_HISTORICAL_POINTS = 1000;
    static constexpr double EFFICIENCY_THRESHOLD = 0.8;
    static constexpr double TEMPERATURE_THRESHOLD = 80.0;
};
