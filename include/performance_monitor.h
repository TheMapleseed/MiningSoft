#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <string>

class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor();
    
    // Initialization
    bool initialize();
    void shutdown();
    
    // Statistics tracking
    void updateHashRate(double hashRate);
    void updateShares(uint64_t submitted, uint64_t accepted, uint64_t rejected);
    void updateJobInfo(const std::string& jobId, const std::string& pool, double difficulty);
    
    // Performance metrics
    double getCurrentHashRate() const;
    double getAverageHashRate() const;
    double getPeakHashRate() const;
    uint64_t getTotalHashes() const;
    uint64_t getSharesSubmitted() const;
    uint64_t getSharesAccepted() const;
    uint64_t getSharesRejected() const;
    double getAcceptanceRate() const;
    std::string getCurrentJob() const;
    std::string getCurrentPool() const;
    double getCurrentDifficulty() const;
    
    // Display
    void displayStats();
    void startRealTimeDisplay();
    void stopRealTimeDisplay();
    
    // Export
    std::string exportStats() const;
    void resetStats();
    
private:
    // Core metrics
    std::atomic<double> m_currentHashRate;
    std::atomic<double> m_averageHashRate;
    std::atomic<double> m_peakHashRate;
    std::atomic<uint64_t> m_totalHashes;
    std::atomic<uint64_t> m_sharesSubmitted;
    std::atomic<uint64_t> m_sharesAccepted;
    std::atomic<uint64_t> m_sharesRejected;
    
    // Job info
    std::string m_currentJob;
    std::string m_currentPool;
    std::atomic<double> m_currentDifficulty;
    
    // Timing
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_lastUpdate;
    std::chrono::steady_clock::time_point m_lastHashTime;
    
    // Threading
    std::thread m_displayThread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_displayActive;
    std::mutex m_statsMutex;
    
    // Internal methods
    void displayLoop();
    void updateAverages();
    void clearScreen();
    void displayHeader();
    void displayHashRate();
    void displayShares();
    void displayJobInfo();
    void displayFooter();
    
    // Utility
    std::string formatHashRate(double rate) const;
    std::string formatTime(std::chrono::seconds duration) const;
    std::string formatPercentage(double percentage) const;
};