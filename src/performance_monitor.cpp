#include "performance_monitor.h"
#include "logger.h"
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <thread>
#include <chrono>

PerformanceMonitor::PerformanceMonitor() : m_active(false), m_shouldStop(false) {
    LOG_DEBUG("PerformanceMonitor constructor called");
}

PerformanceMonitor::~PerformanceMonitor() {
    stop();
    LOG_DEBUG("PerformanceMonitor destructor called");
}

bool PerformanceMonitor::initialize() {
    LOG_INFO("Initializing performance monitoring system");
    
    m_startTime = std::chrono::steady_clock::now();
    m_lastUpdate = m_startTime;
    
    LOG_INFO("Performance monitoring system initialized");
    return true;
}

void PerformanceMonitor::start() {
    if (m_active) {
        LOG_WARNING("Performance monitoring is already active");
        return;
    }
    
    LOG_INFO("Starting performance monitoring");
    
    m_active = true;
    m_shouldStop = false;
    
    // Start monitoring thread
    m_monitoringThread = std::thread(&PerformanceMonitor::monitoringLoop, this);
    
    LOG_INFO("Performance monitoring started");
}

void PerformanceMonitor::stop() {
    if (!m_active) {
        return;
    }
    
    LOG_INFO("Stopping performance monitoring");
    
    m_shouldStop = true;
    m_active = false;
    
    // Wait for monitoring thread to finish
    if (m_monitoringThread.joinable()) {
        m_monitoringThread.join();
    }
    
    LOG_INFO("Performance monitoring stopped");
}

void PerformanceMonitor::updateMiningMetrics(uint64_t hashes, double hashrate, double temperature) {
    m_totalHashes = hashes;
    m_currentHashrate = hashrate;
    m_temperature = temperature;
    
    // Update average hashrate using C23 auto and constexpr
    static auto totalHashrate = 0.0;
    static auto sampleCount = 0;
    
    totalHashrate += hashrate;
    sampleCount++;
    m_averageHashrate = totalHashrate / sampleCount;
    
    // Update peak hashrate
    if (hashrate > m_peakHashrate) {
        m_peakHashrate = hashrate;
    }
}

void PerformanceMonitor::updateSystemMetrics(double cpuUsage, double memoryUsage, double gpuUsage) {
    m_cpuUsage = cpuUsage;
    m_memoryUsage = memoryUsage;
    m_gpuUsage = gpuUsage;
}

void PerformanceMonitor::recordShareSubmission(bool accepted, double difficulty) {
    if (accepted) {
        m_acceptedShares++;
    } else {
        m_rejectedShares++;
    }
}

void PerformanceMonitor::recordThermalEvent(double temperature, bool throttling) {
    m_temperature = temperature;
    m_thermalThrottling = throttling;
}

PerformanceMonitor::PerformanceSummary PerformanceMonitor::getSummary() const {
    PerformanceSummary summary;
    
    summary.currentHashrate = m_currentHashrate.load();
    summary.averageHashrate = m_averageHashrate.load();
    summary.peakHashrate = m_peakHashrate.load();
    summary.totalHashes = m_totalHashes.load();
    summary.acceptedShares = m_acceptedShares.load();
    summary.rejectedShares = m_rejectedShares.load();
    
    if (summary.acceptedShares + summary.rejectedShares > 0) {
        summary.shareAcceptanceRate = static_cast<double>(summary.acceptedShares) / 
                                    (summary.acceptedShares + summary.rejectedShares);
    } else {
        summary.shareAcceptanceRate = 0.0;
    }
    
    summary.cpuUsage = m_cpuUsage.load();
    summary.memoryUsage = m_memoryUsage.load();
    summary.gpuUsage = m_gpuUsage.load();
    summary.temperature = m_temperature.load();
    summary.thermalThrottling = m_thermalThrottling.load();
    
    summary.startTime = m_startTime;
    summary.lastUpdate = m_lastUpdate;
    
    // Get recommendations
    {
        std::lock_guard<std::mutex> lock(m_recommendationsMutex);
        summary.recommendations = m_recommendations;
    }
    
    summary.efficiencyScore = m_efficiencyScore.load();
    
    return summary;
}

PerformanceMonitor::HistoricalData PerformanceMonitor::getHistoricalData(int maxPoints) const {
    std::lock_guard<std::mutex> lock(m_historicalMutex);
    
    HistoricalData data;
    int startIndex = std::max(0, static_cast<int>(m_hashrateHistory.size()) - maxPoints);
    
    data.hashrateHistory.assign(m_hashrateHistory.begin() + startIndex, m_hashrateHistory.end());
    data.temperatureHistory.assign(m_temperatureHistory.begin() + startIndex, m_temperatureHistory.end());
    data.cpuUsageHistory.assign(m_cpuUsageHistory.begin() + startIndex, m_cpuUsageHistory.end());
    data.memoryUsageHistory.assign(m_memoryUsageHistory.begin() + startIndex, m_memoryUsageHistory.end());
    data.timestamps.assign(m_timestamps.begin() + startIndex, m_timestamps.end());
    
    return data;
}

std::vector<std::string> PerformanceMonitor::getRecommendations() const {
    std::lock_guard<std::mutex> lock(m_recommendationsMutex);
    return m_recommendations;
}

bool PerformanceMonitor::exportToFile(const std::string& filename) const {
    LOG_INFO("Exporting performance data to {}", filename);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for export: {}", filename);
        return false;
    }
    
    // Export as JSON
    file << "{\n";
    file << "  \"summary\": {\n";
    
    auto summary = getSummary();
    file << "    \"currentHashrate\": " << summary.currentHashrate << ",\n";
    file << "    \"averageHashrate\": " << summary.averageHashrate << ",\n";
    file << "    \"peakHashrate\": " << summary.peakHashrate << ",\n";
    file << "    \"totalHashes\": " << summary.totalHashes << ",\n";
    file << "    \"acceptedShares\": " << summary.acceptedShares << ",\n";
    file << "    \"rejectedShares\": " << summary.rejectedShares << ",\n";
    file << "    \"shareAcceptanceRate\": " << summary.shareAcceptanceRate << ",\n";
    file << "    \"cpuUsage\": " << summary.cpuUsage << ",\n";
    file << "    \"memoryUsage\": " << summary.memoryUsage << ",\n";
    file << "    \"gpuUsage\": " << summary.gpuUsage << ",\n";
    file << "    \"temperature\": " << summary.temperature << ",\n";
    file << "    \"thermalThrottling\": " << (summary.thermalThrottling ? "true" : "false") << ",\n";
    file << "    \"efficiencyScore\": " << summary.efficiencyScore << "\n";
    
    file << "  },\n";
    file << "  \"recommendations\": [\n";
    
    for (size_t i = 0; i < summary.recommendations.size(); i++) {
        file << "    \"" << summary.recommendations[i] << "\"";
        if (i < summary.recommendations.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    
    file << "  ]\n";
    file << "}\n";
    
    file.close();
    
    LOG_INFO("Performance data exported successfully to {}", filename);
    return true;
}

void PerformanceMonitor::reset() {
    LOG_INFO("Resetting performance metrics");
    
    m_totalHashes = 0;
    m_currentHashrate = 0.0;
    m_averageHashrate = 0.0;
    m_peakHashrate = 0.0;
    m_acceptedShares = 0;
    m_rejectedShares = 0;
    m_cpuUsage = 0.0;
    m_memoryUsage = 0.0;
    m_gpuUsage = 0.0;
    m_temperature = 0.0;
    m_thermalThrottling = false;
    m_efficiencyScore = 0.0;
    
    {
        std::lock_guard<std::mutex> lock(m_historicalMutex);
        m_hashrateHistory.clear();
        m_temperatureHistory.clear();
        m_cpuUsageHistory.clear();
        m_memoryUsageHistory.clear();
        m_timestamps.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(m_recommendationsMutex);
        m_recommendations.clear();
    }
    
    m_startTime = std::chrono::steady_clock::now();
    m_lastUpdate = m_startTime;
}

void PerformanceMonitor::monitoringLoop() {
    LOG_INFO("Performance monitoring loop started");
    
    while (!m_shouldStop && m_active) {
        try {
            // Update historical data
            {
                std::lock_guard<std::mutex> lock(m_historicalMutex);
                
                m_hashrateHistory.push_back(m_currentHashrate.load());
                m_temperatureHistory.push_back(m_temperature.load());
                m_cpuUsageHistory.push_back(m_cpuUsage.load());
                m_memoryUsageHistory.push_back(m_memoryUsage.load());
                m_timestamps.push_back(std::chrono::steady_clock::now());
                
                // Clean up old data
                cleanupHistoricalData();
            }
            
            // Calculate insights and recommendations
            calculateInsights();
            
            // Update efficiency score
            updateEfficiencyScore();
            
            m_lastUpdate = std::chrono::steady_clock::now();
            
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in performance monitoring loop: {}", e.what());
        }
        
        // Wait before next update
        std::this_thread::sleep_for(m_monitoringInterval);
    }
    
    LOG_INFO("Performance monitoring loop ended");
}

void PerformanceMonitor::calculateInsights() {
    analyzeThermalPerformance();
    analyzeMiningEfficiency();
    generateRecommendations();
}

void PerformanceMonitor::analyzeThermalPerformance() {
    double temp = m_temperature.load();
    bool throttling = m_thermalThrottling.load();
    
    if (temp > TEMPERATURE_THRESHOLD) {
        LOG_WARNING("High temperature detected: {:.1f}°C", temp);
    }
    
    if (throttling) {
        LOG_WARNING("Thermal throttling is active");
    }
}

void PerformanceMonitor::analyzeMiningEfficiency() {
    double hashrate = m_currentHashrate.load();
    double cpuUsage = m_cpuUsage.load();
    double memoryUsage = m_memoryUsage.load();
    
    // Analyze efficiency based on resource usage vs hashrate
    if (cpuUsage > 90.0 && hashrate < 1000.0) {
        LOG_WARNING("High CPU usage with low hashrate - possible inefficiency");
    }
    
    if (memoryUsage > 80.0) {
        LOG_WARNING("High memory usage: {:.1f}%", memoryUsage);
    }
}

void PerformanceMonitor::generateRecommendations() {
    std::lock_guard<std::mutex> lock(m_recommendationsMutex);
    m_recommendations.clear();
    
    double hashrate = m_currentHashrate.load();
    double cpuUsage = m_cpuUsage.load();
    double memoryUsage = m_memoryUsage.load();
    double temp = m_temperature.load();
    bool throttling = m_thermalThrottling.load();
    
    if (temp > TEMPERATURE_THRESHOLD) {
        m_recommendations.push_back("Consider improving cooling or reducing mining intensity");
    }
    
    if (throttling) {
        m_recommendations.push_back("Thermal throttling is active - check cooling system");
    }
    
    if (cpuUsage > 90.0 && hashrate < 1000.0) {
        m_recommendations.push_back("High CPU usage with low hashrate - consider optimizing mining parameters");
    }
    
    if (memoryUsage > 80.0) {
        m_recommendations.push_back("High memory usage - consider reducing memory footprint");
    }
    
    if (hashrate < 500.0) {
        m_recommendations.push_back("Low hashrate - check mining configuration and hardware");
    }
    
    if (m_recommendations.empty()) {
        m_recommendations.push_back("Performance is optimal");
    }
}

void PerformanceMonitor::updateEfficiencyScore() {
    double hashrate = m_currentHashrate.load();
    double cpuUsage = m_cpuUsage.load();
    double memoryUsage = m_memoryUsage.load();
    double temp = m_temperature.load();
    bool throttling = m_thermalThrottling.load();
    
    // Calculate efficiency score (0.0 to 1.0)
    double score = 1.0;
    
    // Penalize for high temperature
    if (temp > TEMPERATURE_THRESHOLD) {
        score -= 0.3;
    }
    
    // Penalize for throttling
    if (throttling) {
        score -= 0.2;
    }
    
    // Penalize for high resource usage with low hashrate
    if (cpuUsage > 90.0 && hashrate < 1000.0) {
        score -= 0.2;
    }
    
    if (memoryUsage > 80.0) {
        score -= 0.1;
    }
    
    // Ensure score is between 0.0 and 1.0
    score = std::max(0.0, std::min(1.0, score));
    
    m_efficiencyScore = score;
}

void PerformanceMonitor::cleanupHistoricalData() {
    if (m_hashrateHistory.size() > MAX_HISTORICAL_POINTS) {
        int removeCount = m_hashrateHistory.size() - MAX_HISTORICAL_POINTS;
        
        m_hashrateHistory.erase(m_hashrateHistory.begin(), m_hashrateHistory.begin() + removeCount);
        m_temperatureHistory.erase(m_temperatureHistory.begin(), m_temperatureHistory.begin() + removeCount);
        m_cpuUsageHistory.erase(m_cpuUsageHistory.begin(), m_cpuUsageHistory.begin() + removeCount);
        m_memoryUsageHistory.erase(m_memoryUsageHistory.begin(), m_memoryUsageHistory.begin() + removeCount);
        m_timestamps.erase(m_timestamps.begin(), m_timestamps.begin() + removeCount);
    }
}
