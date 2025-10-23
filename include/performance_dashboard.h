#pragma once

#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <memory>
#include <functional>
#include <iomanip>
#include <sstream>

// Forward declarations
class Logger;

// Performance metrics
struct PerformanceMetrics {
    // Hash rate metrics
    double currentHashRate;
    double averageHashRate;
    double peakHashRate;
    uint64_t totalHashes;
    uint64_t validHashes;
    double acceptanceRate;
    
    // CPU metrics
    double cpuUsage;
    double cpuTemperature;
    uint64_t cpuCores;
    uint64_t cpuFrequency;
    
    // Memory metrics
    uint64_t memoryUsed;
    uint64_t memoryTotal;
    double memoryUsage;
    uint64_t memoryAllocated;
    uint64_t memoryFreed;
    
    // Network metrics
    uint64_t bytesReceived;
    uint64_t bytesSent;
    double networkLatency;
    uint32_t connectionAttempts;
    uint32_t successfulConnections;
    uint32_t failedConnections;
    
    // Mining metrics
    uint32_t sharesSubmitted;
    uint32_t sharesAccepted;
    uint32_t sharesRejected;
    uint32_t jobsReceived;
    uint32_t jobsProcessed;
    double difficulty;
    std::string currentPool;
    std::string currentJob;
    
    // System metrics
    double systemLoad;
    uint64_t uptime;
    std::string status;
    std::string lastError;
    
    // Timestamps
    std::chrono::steady_clock::time_point lastUpdate;
    std::chrono::steady_clock::time_point startTime;
    
    PerformanceMetrics() : 
        currentHashRate(0.0), averageHashRate(0.0), peakHashRate(0.0),
        totalHashes(0), validHashes(0), acceptanceRate(0.0),
        cpuUsage(0.0), cpuTemperature(0.0), cpuCores(0), cpuFrequency(0),
        memoryUsed(0), memoryTotal(0), memoryUsage(0.0),
        memoryAllocated(0), memoryFreed(0),
        bytesReceived(0), bytesSent(0), networkLatency(0.0),
        connectionAttempts(0), successfulConnections(0), failedConnections(0),
        sharesSubmitted(0), sharesAccepted(0), sharesRejected(0),
        jobsReceived(0), jobsProcessed(0), difficulty(0.0),
        systemLoad(0.0), uptime(0), status("Stopped"),
        lastUpdate(std::chrono::steady_clock::now()),
        startTime(std::chrono::steady_clock::now()) {}
};

// Performance dashboard
class PerformanceDashboard {
public:
    PerformanceDashboard();
    ~PerformanceDashboard();
    
    // Initialization
    bool initialize();
    void shutdown();
    
    // Metrics management
    void updateHashRate(double hashRate);
    void updateCpuMetrics(double usage, double temperature);
    void updateMemoryMetrics(uint64_t used, uint64_t total);
    void updateNetworkMetrics(uint64_t received, uint64_t sent, double latency);
    void updateMiningMetrics(uint32_t sharesSubmitted, uint32_t sharesAccepted, 
                           uint32_t sharesRejected, const std::string& pool);
    void updateJobMetrics(uint32_t jobsReceived, uint32_t jobsProcessed, 
                         const std::string& jobId, double difficulty);
    void updateSystemMetrics(double load, const std::string& status, 
                           const std::string& lastError = "");
    
    // Statistics
    PerformanceMetrics getCurrentMetrics() const;
    std::vector<PerformanceMetrics> getHistoricalMetrics(size_t count = 100) const;
    void resetStatistics();
    
    // Dashboard display
    void displayDashboard();
    void displaySummary();
    void displayDetailedStats();
    void displayRealTimeStats();
    
    // Export/Import
    bool exportToFile(const std::string& filename) const;
    bool importFromFile(const std::string& filename);
    std::string exportToJson() const;
    bool importFromJson(const std::string& json);
    
    // Configuration
    void setUpdateInterval(int milliseconds);
    void setHistorySize(size_t size);
    void setDisplayMode(const std::string& mode);
    void enableAutoSave(bool enable);
    void setAutoSaveInterval(int seconds);
    
    // Callbacks
    void setOnMetricsUpdate(std::function<void(const PerformanceMetrics&)> callback);
    void setOnAlert(std::function<void(const std::string&, const std::string&)> callback);
    
    // Alerts
    void checkAlerts();
    void addAlert(const std::string& condition, const std::string& message);
    void removeAlert(const std::string& condition);
    void clearAlerts();
    
    // Threading
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return m_monitoring; }
    
private:
    // Core data
    PerformanceMetrics m_currentMetrics;
    std::vector<PerformanceMetrics> m_historicalMetrics;
    mutable std::mutex m_metricsMutex;
    
    // Configuration
    int m_updateInterval;
    size_t m_historySize;
    std::string m_displayMode;
    bool m_autoSave;
    int m_autoSaveInterval;
    
    // Threading
    std::thread m_monitoringThread;
    std::atomic<bool> m_monitoring;
    std::atomic<bool> m_running;
    
    // Callbacks
    std::function<void(const PerformanceMetrics&)> m_onMetricsUpdate;
    std::function<void(const std::string&, const std::string&)> m_onAlert;
    
    // Alerts
    std::map<std::string, std::string> m_alerts;
    std::map<std::string, bool> m_alertStates;
    
    // Internal methods
    void monitoringLoop();
    void updateAverages();
    void checkAlertConditions();
    void saveMetrics();
    void loadMetrics();
    
    // Display helpers
    void displayHeader();
    void displayHashRateSection();
    void displayCpuSection();
    void displayMemorySection();
    void displayNetworkSection();
    void displayMiningSection();
    void displaySystemSection();
    void displayFooter();
    
    // Utility functions
    std::string formatBytes(uint64_t bytes) const;
    std::string formatHashRate(double hashRate) const;
    std::string formatPercentage(double percentage) const;
    std::string formatDuration(uint64_t seconds) const;
    std::string formatTemperature(double celsius) const;
    
    // System metrics
    void updateSystemMetrics();
    double getCpuUsage() const;
    double getCpuTemperature() const;
    void getMemoryUsage(uint64_t& used, uint64_t& total) const;
    double getSystemLoad() const;
    
    // Logging
    void logInfo(const std::string& message) const;
    void logWarning(const std::string& message) const;
    void logError(const std::string& message) const;
    void logDebug(const std::string& message) const;
};

// Real-time statistics display
class RealTimeStats {
public:
    RealTimeStats();
    ~RealTimeStats();
    
    void start();
    void stop();
    void update();
    
    void setMetrics(const PerformanceMetrics& metrics);
    void setRefreshRate(int milliseconds);
    
private:
    std::thread m_displayThread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_active;
    int m_refreshRate;
    PerformanceMetrics m_metrics;
    mutable std::mutex m_metricsMutex;
    
    void displayLoop();
    void clearScreen();
    void displayStats();
};

// Performance logger
class PerformanceLogger {
public:
    PerformanceLogger();
    ~PerformanceLogger();
    
    bool initialize(const std::string& logFile);
    void shutdown();
    
    void logMetrics(const PerformanceMetrics& metrics);
    void logEvent(const std::string& event, const std::string& details = "");
    void logAlert(const std::string& alert, const std::string& message);
    
    void setLogLevel(int level);
    void setMaxFileSize(size_t size);
    void setMaxFiles(int count);
    
private:
    std::string m_logFile;
    int m_logLevel;
    size_t m_maxFileSize;
    int m_maxFiles;
    bool m_initialized;
    mutable std::mutex m_logMutex;
    
    void writeLog(const std::string& message);
    void rotateLog();
    std::string formatTimestamp() const;
};
