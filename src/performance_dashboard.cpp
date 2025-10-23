#include "performance_dashboard.h"
#include "logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_time.h>
#include <unistd.h>

// PerformanceDashboard Implementation
PerformanceDashboard::PerformanceDashboard() 
    : m_updateInterval(1000), m_historySize(1000), m_displayMode("full"),
      m_autoSave(false), m_autoSaveInterval(60), m_monitoring(false), m_running(false) {
}

PerformanceDashboard::~PerformanceDashboard() {
    shutdown();
}

bool PerformanceDashboard::initialize() {
    if (m_running) {
        return true;
    }
    
    logInfo("Initializing Performance Dashboard");
    
    // Initialize metrics
    m_currentMetrics = PerformanceMetrics();
    m_historicalMetrics.clear();
    m_historicalMetrics.reserve(m_historySize);
    
    // Start monitoring thread
    m_running = true;
    m_monitoring = true;
    m_monitoringThread = std::thread(&PerformanceDashboard::monitoringLoop, this);
    
    logInfo("Performance Dashboard initialized successfully");
    return true;
}

void PerformanceDashboard::shutdown() {
    if (!m_running) {
        return;
    }
    
    logInfo("Shutting down Performance Dashboard");
    
    m_running = false;
    m_monitoring = false;
    
    if (m_monitoringThread.joinable()) {
        m_monitoringThread.join();
    }
    
    // Save final metrics
    if (m_autoSave) {
        saveMetrics();
    }
    
    logInfo("Performance Dashboard shutdown complete");
}

void PerformanceDashboard::updateHashRate(double hashRate) {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    m_currentMetrics.currentHashRate = hashRate;
    m_currentMetrics.lastUpdate = std::chrono::steady_clock::now();
    
    // Update peak hash rate
    if (hashRate > m_currentMetrics.peakHashRate) {
        m_currentMetrics.peakHashRate = hashRate;
    }
    
    // Update acceptance rate
    if (m_currentMetrics.totalHashes > 0) {
        m_currentMetrics.acceptanceRate = static_cast<double>(m_currentMetrics.validHashes) / m_currentMetrics.totalHashes;
    }
}

void PerformanceDashboard::updateCpuMetrics(double usage, double temperature) {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    m_currentMetrics.cpuUsage = usage;
    m_currentMetrics.cpuTemperature = temperature;
    m_currentMetrics.lastUpdate = std::chrono::steady_clock::now();
}

void PerformanceDashboard::updateMemoryMetrics(uint64_t used, uint64_t total) {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    m_currentMetrics.memoryUsed = used;
    m_currentMetrics.memoryTotal = total;
    m_currentMetrics.memoryUsage = total > 0 ? (static_cast<double>(used) / total) * 100.0 : 0.0;
    m_currentMetrics.lastUpdate = std::chrono::steady_clock::now();
}

void PerformanceDashboard::updateNetworkMetrics(uint64_t received, uint64_t sent, double latency) {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    m_currentMetrics.bytesReceived = received;
    m_currentMetrics.bytesSent = sent;
    m_currentMetrics.networkLatency = latency;
    m_currentMetrics.lastUpdate = std::chrono::steady_clock::now();
}

void PerformanceDashboard::updateMiningMetrics(uint32_t sharesSubmitted, uint32_t sharesAccepted, 
                                             uint32_t sharesRejected, const std::string& pool) {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    m_currentMetrics.sharesSubmitted = sharesSubmitted;
    m_currentMetrics.sharesAccepted = sharesAccepted;
    m_currentMetrics.sharesRejected = sharesRejected;
    m_currentMetrics.currentPool = pool;
    m_currentMetrics.lastUpdate = std::chrono::steady_clock::now();
}

void PerformanceDashboard::updateJobMetrics(uint32_t jobsReceived, uint32_t jobsProcessed, 
                                          const std::string& jobId, double difficulty) {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    m_currentMetrics.jobsReceived = jobsReceived;
    m_currentMetrics.jobsProcessed = jobsProcessed;
    m_currentMetrics.currentJob = jobId;
    m_currentMetrics.difficulty = difficulty;
    m_currentMetrics.lastUpdate = std::chrono::steady_clock::now();
}

void PerformanceDashboard::updateSystemMetrics(double load, const std::string& status, 
                                              const std::string& lastError) {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    m_currentMetrics.systemLoad = load;
    m_currentMetrics.status = status;
    m_currentMetrics.lastError = lastError;
    m_currentMetrics.uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_currentMetrics.startTime).count();
    m_currentMetrics.lastUpdate = std::chrono::steady_clock::now();
}

PerformanceMetrics PerformanceDashboard::getCurrentMetrics() const {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    return m_currentMetrics;
}

std::vector<PerformanceMetrics> PerformanceDashboard::getHistoricalMetrics(size_t count) const {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    size_t start = m_historicalMetrics.size() > count ? m_historicalMetrics.size() - count : 0;
    return std::vector<PerformanceMetrics>(m_historicalMetrics.begin() + start, m_historicalMetrics.end());
}

void PerformanceDashboard::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    m_currentMetrics = PerformanceMetrics();
    m_historicalMetrics.clear();
    
    logInfo("Performance statistics reset");
}

void PerformanceDashboard::displayDashboard() {
    if (m_displayMode == "summary") {
        displaySummary();
    } else if (m_displayMode == "detailed") {
        displayDetailedStats();
    } else {
        displayRealTimeStats();
    }
}

void PerformanceDashboard::displaySummary() {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    MININGSOFT PERFORMANCE                    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    
    std::cout << "\n📊 HASH RATE:\n";
    std::cout << "   Current: " << formatHashRate(m_currentMetrics.currentHashRate) << "\n";
    std::cout << "   Average: " << formatHashRate(m_currentMetrics.averageHashRate) << "\n";
    std::cout << "   Peak:    " << formatHashRate(m_currentMetrics.peakHashRate) << "\n";
    
    std::cout << "\n💻 CPU:\n";
    std::cout << "   Usage:      " << formatPercentage(m_currentMetrics.cpuUsage) << "\n";
    std::cout << "   Temperature: " << formatTemperature(m_currentMetrics.cpuTemperature) << "\n";
    std::cout << "   Cores:      " << m_currentMetrics.cpuCores << "\n";
    
    std::cout << "\n🧠 MEMORY:\n";
    std::cout << "   Used:  " << formatBytes(m_currentMetrics.memoryUsed) << "\n";
    std::cout << "   Total: " << formatBytes(m_currentMetrics.memoryTotal) << "\n";
    std::cout << "   Usage: " << formatPercentage(m_currentMetrics.memoryUsage) << "\n";
    
    std::cout << "\n🌐 NETWORK:\n";
    std::cout << "   Pool:     " << m_currentMetrics.currentPool << "\n";
    std::cout << "   Latency:  " << std::fixed << std::setprecision(2) << m_currentMetrics.networkLatency << " ms\n";
    std::cout << "   Received: " << formatBytes(m_currentMetrics.bytesReceived) << "\n";
    std::cout << "   Sent:     " << formatBytes(m_currentMetrics.bytesSent) << "\n";
    
    std::cout << "\n⛏️  MINING:\n";
    std::cout << "   Shares: " << m_currentMetrics.sharesSubmitted << " submitted, " 
              << m_currentMetrics.sharesAccepted << " accepted\n";
    std::cout << "   Rate:   " << formatPercentage(m_currentMetrics.acceptanceRate * 100) << "\n";
    std::cout << "   Jobs:   " << m_currentMetrics.jobsProcessed << " processed\n";
    
    std::cout << "\n⚙️  SYSTEM:\n";
    std::cout << "   Status: " << m_currentMetrics.status << "\n";
    std::cout << "   Uptime: " << formatDuration(m_currentMetrics.uptime) << "\n";
    std::cout << "   Load:   " << std::fixed << std::setprecision(2) << m_currentMetrics.systemLoad << "\n";
    
    if (!m_currentMetrics.lastError.empty()) {
        std::cout << "\n❌ LAST ERROR:\n";
        std::cout << "   " << m_currentMetrics.lastError << "\n";
    }
    
    std::cout << "\n";
}

void PerformanceDashboard::displayDetailedStats() {
    displaySummary();
    
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    std::cout << "📈 HISTORICAL DATA:\n";
    std::cout << "   Data Points: " << m_historicalMetrics.size() << "\n";
    
    if (!m_historicalMetrics.empty()) {
        // Calculate statistics
        std::vector<double> hashRates;
        for (const auto& metrics : m_historicalMetrics) {
            hashRates.push_back(metrics.currentHashRate);
        }
        
        if (!hashRates.empty()) {
            std::sort(hashRates.begin(), hashRates.end());
            double median = hashRates[hashRates.size() / 2];
            double min = hashRates.front();
            double max = hashRates.back();
            
            std::cout << "   Hash Rate - Min: " << formatHashRate(min) 
                      << ", Median: " << formatHashRate(median)
                      << ", Max: " << formatHashRate(max) << "\n";
        }
    }
    
    std::cout << "\n";
}

void PerformanceDashboard::displayRealTimeStats() {
    // Clear screen and display real-time stats
    std::cout << "\033[2J\033[H"; // Clear screen and move cursor to top
    
    displayHeader();
    displayHashRateSection();
    displayCpuSection();
    displayMemorySection();
    displayNetworkSection();
    displayMiningSection();
    displaySystemSection();
    displayFooter();
}

void PerformanceDashboard::displayHeader() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    MININGSOFT v1.0.0                        ║\n";
    std::cout << "║              Real-Time Performance Monitor                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
}

void PerformanceDashboard::displayHashRateSection() {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    std::cout << "\n📊 HASH RATE\n";
    std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ Current: " << std::setw(12) << formatHashRate(m_currentMetrics.currentHashRate) 
              << " │ Average: " << std::setw(12) << formatHashRate(m_currentMetrics.averageHashRate)
              << " │ Peak: " << std::setw(12) << formatHashRate(m_currentMetrics.peakHashRate) << " │\n";
    std::cout << "└─────────────────────────────────────────────────────────────┘\n";
}

void PerformanceDashboard::displayCpuSection() {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    std::cout << "\n💻 CPU PERFORMANCE\n";
    std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ Usage: " << std::setw(8) << formatPercentage(m_currentMetrics.cpuUsage)
              << " │ Temp: " << std::setw(8) << formatTemperature(m_currentMetrics.cpuTemperature)
              << " │ Cores: " << std::setw(4) << m_currentMetrics.cpuCores << " │\n";
    std::cout << "└─────────────────────────────────────────────────────────────┘\n";
}

void PerformanceDashboard::displayMemorySection() {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    std::cout << "\n🧠 MEMORY USAGE\n";
    std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ Used: " << std::setw(10) << formatBytes(m_currentMetrics.memoryUsed)
              << " │ Total: " << std::setw(10) << formatBytes(m_currentMetrics.memoryTotal)
              << " │ Usage: " << std::setw(6) << formatPercentage(m_currentMetrics.memoryUsage) << " │\n";
    std::cout << "└─────────────────────────────────────────────────────────────┘\n";
}

void PerformanceDashboard::displayNetworkSection() {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    std::cout << "\n🌐 NETWORK STATUS\n";
    std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ Pool: " << std::setw(20) << m_currentMetrics.currentPool
              << " │ Latency: " << std::setw(8) << std::fixed << std::setprecision(2) 
              << m_currentMetrics.networkLatency << " ms │\n";
    std::cout << "│ RX: " << std::setw(12) << formatBytes(m_currentMetrics.bytesReceived)
              << " │ TX: " << std::setw(12) << formatBytes(m_currentMetrics.bytesSent) << " │\n";
    std::cout << "└─────────────────────────────────────────────────────────────┘\n";
}

void PerformanceDashboard::displayMiningSection() {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    std::cout << "\n⛏️  MINING STATISTICS\n";
    std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ Shares: " << std::setw(6) << m_currentMetrics.sharesSubmitted 
              << " submitted │ " << std::setw(6) << m_currentMetrics.sharesAccepted 
              << " accepted │ " << std::setw(6) << m_currentMetrics.sharesRejected << " rejected │\n";
    std::cout << "│ Rate: " << std::setw(8) << formatPercentage(m_currentMetrics.acceptanceRate * 100)
              << " │ Jobs: " << std::setw(6) << m_currentMetrics.jobsProcessed 
              << " processed │ Difficulty: " << std::setw(8) << std::fixed << std::setprecision(2) 
              << m_currentMetrics.difficulty << " │\n";
    std::cout << "└─────────────────────────────────────────────────────────────┘\n";
}

void PerformanceDashboard::displaySystemSection() {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    std::cout << "\n⚙️  SYSTEM STATUS\n";
    std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ Status: " << std::setw(12) << m_currentMetrics.status
              << " │ Uptime: " << std::setw(12) << formatDuration(m_currentMetrics.uptime)
              << " │ Load: " << std::setw(8) << std::fixed << std::setprecision(2) 
              << m_currentMetrics.systemLoad << " │\n";
    std::cout << "└─────────────────────────────────────────────────────────────┘\n";
}

void PerformanceDashboard::displayFooter() {
    std::cout << "\n🔄 Press Ctrl+C to stop monitoring\n";
    std::cout << "📊 Dashboard updates every " << m_updateInterval << "ms\n";
}

void PerformanceDashboard::monitoringLoop() {
    logInfo("Performance monitoring started");
    
    while (m_running && m_monitoring) {
        // Update system metrics
        updateSystemMetrics();
        
        // Update averages
        updateAverages();
        
        // Check alerts
        checkAlerts();
        
        // Save metrics if auto-save is enabled
        if (m_autoSave) {
            saveMetrics();
        }
        
        // Call metrics update callback
        if (m_onMetricsUpdate) {
            m_onMetricsUpdate(m_currentMetrics);
        }
        
        // Sleep for update interval
        std::this_thread::sleep_for(std::chrono::milliseconds(m_updateInterval));
    }
    
    logInfo("Performance monitoring stopped");
}

void PerformanceDashboard::updateSystemMetrics() {
    // Get CPU usage
    double cpuUsage = getCpuUsage();
    double cpuTemperature = getCpuTemperature();
    
    // Get memory usage
    uint64_t memoryUsed = 0, memoryTotal = 0;
    getMemoryUsage(memoryUsed, memoryTotal);
    
    // Get system load
    double systemLoad = getSystemLoad();
    
    // Update metrics
    updateCpuMetrics(cpuUsage, cpuTemperature);
    updateMemoryMetrics(memoryUsed, memoryTotal);
    updateSystemMetrics(systemLoad, "Running");
}

void PerformanceDashboard::updateAverages() {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    // Add current metrics to history
    m_historicalMetrics.push_back(m_currentMetrics);
    
    // Trim history if it exceeds max size
    if (m_historicalMetrics.size() > m_historySize) {
        m_historicalMetrics.erase(m_historicalMetrics.begin());
    }
    
    // Calculate average hash rate
    if (!m_historicalMetrics.empty()) {
        double totalHashRate = 0.0;
        for (const auto& metrics : m_historicalMetrics) {
            totalHashRate += metrics.currentHashRate;
        }
        m_currentMetrics.averageHashRate = totalHashRate / m_historicalMetrics.size();
    }
}

void PerformanceDashboard::checkAlerts() {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    
    for (const auto& alert : m_alerts) {
        bool triggered = false;
        
        if (alert.first == "high_cpu" && m_currentMetrics.cpuUsage > 90.0) {
            triggered = true;
        } else if (alert.first == "high_temp" && m_currentMetrics.cpuTemperature > 80.0) {
            triggered = true;
        } else if (alert.first == "low_hash_rate" && m_currentMetrics.currentHashRate < 100.0) {
            triggered = true;
        } else if (alert.first == "high_memory" && m_currentMetrics.memoryUsage > 90.0) {
            triggered = true;
        }
        
        if (triggered && !m_alertStates[alert.first]) {
            m_alertStates[alert.first] = true;
            if (m_onAlert) {
                m_onAlert(alert.first, alert.second);
            }
            logWarning("Alert triggered: " + alert.first + " - " + alert.second);
        } else if (!triggered) {
            m_alertStates[alert.first] = false;
        }
    }
}

// Utility functions
std::string PerformanceDashboard::formatBytes(uint64_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}

std::string PerformanceDashboard::formatHashRate(double hashRate) const {
    const char* units[] = {"H/s", "KH/s", "MH/s", "GH/s", "TH/s"};
    int unit = 0;
    double rate = hashRate;
    
    while (rate >= 1000.0 && unit < 4) {
        rate /= 1000.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << rate << " " << units[unit];
    return oss.str();
}

std::string PerformanceDashboard::formatPercentage(double percentage) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << percentage << "%";
    return oss.str();
}

std::string PerformanceDashboard::formatDuration(uint64_t seconds) const {
    uint64_t hours = seconds / 3600;
    uint64_t minutes = (seconds % 3600) / 60;
    uint64_t secs = seconds % 60;
    
    std::ostringstream oss;
    oss << hours << "h " << minutes << "m " << secs << "s";
    return oss.str();
}

std::string PerformanceDashboard::formatTemperature(double celsius) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << celsius << "°C";
    return oss.str();
}

// System metrics functions
double PerformanceDashboard::getCpuUsage() const {
    // Simplified CPU usage calculation
    // In a real implementation, this would use proper system calls
    return 0.0; // Placeholder
}

double PerformanceDashboard::getCpuTemperature() const {
    // Simplified temperature reading
    // In a real implementation, this would read from thermal sensors
    return 0.0; // Placeholder
}

void PerformanceDashboard::getMemoryUsage(uint64_t& used, uint64_t& total) const {
    // Simplified memory usage calculation
    // In a real implementation, this would use proper system calls
    used = 0;   // Placeholder
    total = 0;  // Placeholder
}

double PerformanceDashboard::getSystemLoad() const {
    double load[3];
    if (getloadavg(load, 3) == 3) {
        return load[0]; // 1-minute load average
    }
    return 0.0;
}

// Logging functions
void PerformanceDashboard::logInfo(const std::string& message) const {
    if (g_logger) {
        LOG_INFO("[Dashboard] {}", message);
    }
}

void PerformanceDashboard::logWarning(const std::string& message) const {
    if (g_logger) {
        LOG_WARNING("[Dashboard] {}", message);
    }
}

void PerformanceDashboard::logError(const std::string& message) const {
    if (g_logger) {
        LOG_ERROR("[Dashboard] {}", message);
    }
}

void PerformanceDashboard::logDebug(const std::string& message) const {
    if (g_logger) {
        LOG_DEBUG("[Dashboard] {}", message);
    }
}

// Additional methods would be implemented here...
void PerformanceDashboard::setUpdateInterval(int milliseconds) {
    m_updateInterval = milliseconds;
}

void PerformanceDashboard::setHistorySize(size_t size) {
    m_historySize = size;
}

void PerformanceDashboard::setDisplayMode(const std::string& mode) {
    m_displayMode = mode;
}

void PerformanceDashboard::enableAutoSave(bool enable) {
    m_autoSave = enable;
}

void PerformanceDashboard::setAutoSaveInterval(int seconds) {
    m_autoSaveInterval = seconds;
}

void PerformanceDashboard::setOnMetricsUpdate(std::function<void(const PerformanceMetrics&)> callback) {
    m_onMetricsUpdate = callback;
}

void PerformanceDashboard::setOnAlert(std::function<void(const std::string&, const std::string&)> callback) {
    m_onAlert = callback;
}

void PerformanceDashboard::addAlert(const std::string& condition, const std::string& message) {
    m_alerts[condition] = message;
    m_alertStates[condition] = false;
}

void PerformanceDashboard::removeAlert(const std::string& condition) {
    m_alerts.erase(condition);
    m_alertStates.erase(condition);
}

void PerformanceDashboard::clearAlerts() {
    m_alerts.clear();
    m_alertStates.clear();
}

void PerformanceDashboard::startMonitoring() {
    if (!m_monitoring) {
        m_monitoring = true;
    }
}

void PerformanceDashboard::stopMonitoring() {
    m_monitoring = false;
}

bool PerformanceDashboard::exportToFile(const std::string& filename) const {
    // Implementation for exporting metrics to file
    return true; // Placeholder
}

bool PerformanceDashboard::importFromFile(const std::string& filename) {
    // Implementation for importing metrics from file
    return true; // Placeholder
}

std::string PerformanceDashboard::exportToJson() const {
    // Implementation for exporting metrics to JSON
    return "{}"; // Placeholder
}

bool PerformanceDashboard::importFromJson(const std::string& json) {
    // Implementation for importing metrics from JSON
    return true; // Placeholder
}

void PerformanceDashboard::saveMetrics() {
    // Implementation for saving metrics
}

void PerformanceDashboard::loadMetrics() {
    // Implementation for loading metrics
}
