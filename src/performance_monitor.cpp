#include "performance_monitor.h"
#include "logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>

PerformanceMonitor::PerformanceMonitor() 
    : m_currentHashRate(0.0), m_averageHashRate(0.0), m_peakHashRate(0.0),
      m_totalHashes(0), m_sharesSubmitted(0), m_sharesAccepted(0), m_sharesRejected(0),
      m_currentDifficulty(0.0), m_running(false), m_displayActive(false) {
    m_startTime = std::chrono::steady_clock::now();
    m_lastUpdate = m_startTime;
    m_lastHashTime = m_startTime;
}

PerformanceMonitor::~PerformanceMonitor() {
    shutdown();
}

bool PerformanceMonitor::initialize() {
    if (m_running) {
        return true;
    }
    
    m_running = true;
    LOG_INFO("Performance monitor initialized");
    return true;
}

void PerformanceMonitor::shutdown() {
    if (m_running) {
        m_running = false;
        stopRealTimeDisplay();
        LOG_INFO("Performance monitor shutdown");
    }
}

void PerformanceMonitor::updateHashRate(double hashRate) {
    m_currentHashRate = hashRate;
    m_lastHashTime = std::chrono::steady_clock::now();
    
    // Update peak hash rate
    double current = m_currentHashRate.load();
    double peak = m_peakHashRate.load();
    if (current > peak) {
        m_peakHashRate = current;
    }
    
    // Update averages
    updateAverages();
}

void PerformanceMonitor::updateShares(uint64_t submitted, uint64_t accepted, uint64_t rejected) {
    m_sharesSubmitted = submitted;
    m_sharesAccepted = accepted;
    m_sharesRejected = rejected;
    m_lastUpdate = std::chrono::steady_clock::now();
}

void PerformanceMonitor::updateJobInfo(const std::string& jobId, const std::string& pool, double difficulty) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_currentJob = jobId;
    m_currentPool = pool;
    m_currentDifficulty = difficulty;
    m_lastUpdate = std::chrono::steady_clock::now();
}

double PerformanceMonitor::getCurrentHashRate() const {
    return m_currentHashRate.load();
}

double PerformanceMonitor::getAverageHashRate() const {
    return m_averageHashRate.load();
}

double PerformanceMonitor::getPeakHashRate() const {
    return m_peakHashRate.load();
}

uint64_t PerformanceMonitor::getTotalHashes() const {
    return m_totalHashes.load();
}

uint64_t PerformanceMonitor::getSharesSubmitted() const {
    return m_sharesSubmitted.load();
}

uint64_t PerformanceMonitor::getSharesAccepted() const {
    return m_sharesAccepted.load();
}

uint64_t PerformanceMonitor::getSharesRejected() const {
    return m_sharesRejected.load();
}

double PerformanceMonitor::getAcceptanceRate() const {
    uint64_t submitted = m_sharesSubmitted.load();
    uint64_t accepted = m_sharesAccepted.load();
    
    if (submitted == 0) {
        return 0.0;
    }
    
    return (double)accepted / submitted * 100.0;
}

std::string PerformanceMonitor::getCurrentJob() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_statsMutex));
    return m_currentJob;
}

std::string PerformanceMonitor::getCurrentPool() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_statsMutex));
    return m_currentPool;
}

double PerformanceMonitor::getCurrentDifficulty() const {
    return m_currentDifficulty.load();
}

void PerformanceMonitor::displayStats() {
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    MININGSOFT PERFORMANCE                    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    
    std::cout << "\n📊 HASH RATE:\n";
    std::cout << "   Current: " << formatHashRate(getCurrentHashRate()) << "\n";
    std::cout << "   Average: " << formatHashRate(getAverageHashRate()) << "\n";
    std::cout << "   Peak:    " << formatHashRate(getPeakHashRate()) << "\n";
    
    std::cout << "\n⛏️  MINING STATISTICS:\n";
    std::cout << "   Shares: " << getSharesSubmitted() << " submitted, " 
              << getSharesAccepted() << " accepted, " 
              << getSharesRejected() << " rejected\n";
    std::cout << "   Rate:   " << formatPercentage(getAcceptanceRate()) << " acceptance\n";
    
    std::cout << "\n🌐 POOL INFO:\n";
    std::cout << "   Pool:       " << getCurrentPool() << "\n";
    std::cout << "   Job:        " << getCurrentJob() << "\n";
    std::cout << "   Difficulty: " << std::fixed << std::setprecision(2) << getCurrentDifficulty() << "\n";
    
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime);
    std::cout << "\n⏱️  UPTIME: " << formatTime(uptime) << "\n";
    
    std::cout << "\n";
}

void PerformanceMonitor::startRealTimeDisplay() {
    if (m_displayActive) {
        return;
    }
    
    m_displayActive = true;
    m_displayThread = std::thread(&PerformanceMonitor::displayLoop, this);
    LOG_INFO("Real-time performance display started");
}

void PerformanceMonitor::stopRealTimeDisplay() {
    if (m_displayActive) {
        m_displayActive = false;
        if (m_displayThread.joinable()) {
            m_displayThread.join();
        }
        LOG_INFO("Real-time performance display stopped");
    }
}

std::string PerformanceMonitor::exportStats() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"hashRate\": {\n";
    json << "    \"current\": " << getCurrentHashRate() << ",\n";
    json << "    \"average\": " << getAverageHashRate() << ",\n";
    json << "    \"peak\": " << getPeakHashRate() << "\n";
    json << "  },\n";
    json << "  \"shares\": {\n";
    json << "    \"submitted\": " << getSharesSubmitted() << ",\n";
    json << "    \"accepted\": " << getSharesAccepted() << ",\n";
    json << "    \"rejected\": " << getSharesRejected() << ",\n";
    json << "    \"acceptanceRate\": " << getAcceptanceRate() << "\n";
    json << "  },\n";
    json << "  \"pool\": {\n";
    json << "    \"name\": \"" << getCurrentPool() << "\",\n";
    json << "    \"job\": \"" << getCurrentJob() << "\",\n";
    json << "    \"difficulty\": " << getCurrentDifficulty() << "\n";
    json << "  }\n";
    json << "}\n";
    
    return json.str();
}

void PerformanceMonitor::resetStats() {
    m_currentHashRate = 0.0;
    m_averageHashRate = 0.0;
    m_peakHashRate = 0.0;
    m_totalHashes = 0;
    m_sharesSubmitted = 0;
    m_sharesAccepted = 0;
    m_sharesRejected = 0;
    m_currentDifficulty = 0.0;
    
    m_startTime = std::chrono::steady_clock::now();
    m_lastUpdate = m_startTime;
    m_lastHashTime = m_startTime;
    
    LOG_INFO("Performance statistics reset");
}

void PerformanceMonitor::displayLoop() {
    while (m_displayActive && m_running) {
        clearScreen();
        displayHeader();
        displayHashRate();
        displayShares();
        displayJobInfo();
        displayFooter();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void PerformanceMonitor::updateAverages() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime);
    
    if (duration.count() > 0) {
        // Simple moving average
        double current = m_currentHashRate.load();
        double average = m_averageHashRate.load();
        
        // Weighted average (70% old, 30% new)
        double newAverage = (average * 0.7) + (current * 0.3);
        m_averageHashRate = newAverage;
    }
}

void PerformanceMonitor::clearScreen() {
    std::cout << "\033[2J\033[H";
}

void PerformanceMonitor::displayHeader() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    MININGSOFT v1.0.0                        ║\n";
    std::cout << "║              Real-Time Performance Monitor                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
}

void PerformanceMonitor::displayHashRate() {
    std::cout << "\n📊 HASH RATE\n";
    std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ Current: " << std::setw(12) << formatHashRate(getCurrentHashRate()) 
              << " │ Average: " << std::setw(12) << formatHashRate(getAverageHashRate())
              << " │ Peak: " << std::setw(12) << formatHashRate(getPeakHashRate()) << " │\n";
    std::cout << "└─────────────────────────────────────────────────────────────┘\n";
}

void PerformanceMonitor::displayShares() {
    std::cout << "\n⛏️  MINING STATISTICS\n";
    std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ Shares: " << std::setw(6) << getSharesSubmitted() 
              << " submitted │ " << std::setw(6) << getSharesAccepted() 
              << " accepted │ " << std::setw(6) << getSharesRejected() << " rejected │\n";
    std::cout << "│ Rate: " << std::setw(8) << formatPercentage(getAcceptanceRate())
              << " acceptance rate │\n";
    std::cout << "└─────────────────────────────────────────────────────────────┘\n";
}

void PerformanceMonitor::displayJobInfo() {
    std::cout << "\n🌐 POOL INFORMATION\n";
    std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ Pool: " << std::setw(20) << getCurrentPool()
              << " │ Difficulty: " << std::setw(8) << std::fixed << std::setprecision(2) 
              << getCurrentDifficulty() << " │\n";
    std::cout << "│ Job: " << std::setw(25) << getCurrentJob() << " │\n";
    std::cout << "└─────────────────────────────────────────────────────────────┘\n";
}

void PerformanceMonitor::displayFooter() {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime);
    
    std::cout << "\n⏱️  UPTIME: " << formatTime(uptime) << "\n";
    std::cout << "🔄 Press Ctrl+C to stop mining\n";
}

std::string PerformanceMonitor::formatHashRate(double rate) const {
    const char* units[] = {"H/s", "KH/s", "MH/s", "GH/s", "TH/s"};
    int unit = 0;
    double value = rate;
    
    while (value >= 1000.0 && unit < 4) {
        value /= 1000.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value << " " << units[unit];
    return oss.str();
}

std::string PerformanceMonitor::formatTime(std::chrono::seconds duration) const {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration - hours);
    auto seconds = duration - hours - minutes;
    
    std::ostringstream oss;
    oss << hours.count() << "h " << minutes.count() << "m " << seconds.count() << "s";
    return oss.str();
}

std::string PerformanceMonitor::formatPercentage(double percentage) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << percentage << "%";
    return oss.str();
}