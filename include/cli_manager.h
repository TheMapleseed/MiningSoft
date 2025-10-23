#pragma once

#include "miner.h"
#include "config_manager.h"
#include "logger.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

struct MiningStats {
    double hashrate;
    uint64_t totalHashes;
    uint32_t acceptedShares;
    uint32_t rejectedShares;
    double temperature;
    bool isMining;
    bool isConnected;
    std::string currentJob;
    uint32_t nonceCount;
    std::chrono::steady_clock::time_point startTime;
    
    MiningStats() : hashrate(0.0), totalHashes(0), acceptedShares(0), rejectedShares(0), 
                   temperature(0.0), isMining(false), isConnected(false), nonceCount(0) {}
};

class CLIManager {
public:
    CLIManager();
    ~CLIManager();
    
    // Initialize CLI system
    bool initialize();
    
    // Main CLI loop
    void run();
    
    // Command handlers
    void handleStart(const std::vector<std::string>& args);
    void handleStop(const std::vector<std::string>& args);
    void handleStatus(const std::vector<std::string>& args);
    void handleConfig(const std::vector<std::string>& args);
    void handleHelp(const std::vector<std::string>& args);
    void handleExit(const std::vector<std::string>& args);
    void handleStats(const std::vector<std::string>& args);
    void handleConnect(const std::vector<std::string>& args);
    void handleDisconnect(const std::vector<std::string>& args);
    void handleSet(const std::vector<std::string>& args);
    void handleShow(const std::vector<std::string>& args);
    
    // Statistics and monitoring
    void updateStats();
    void printStats();
    void printStatus();
    void printHelp();
    
    // Configuration management
    bool loadConfig(const std::string& filename);
    bool saveConfig(const std::string& filename);
    void printConfig();
    bool setConfigValue(const std::string& key, const std::string& value);
    
    // Interactive mode
    void runInteractive();
    void printPrompt();
    std::vector<std::string> parseCommand(const std::string& input);
    
    // Control methods
    void startMining();
    void stopMining();
    void connectToPool();
    void disconnectFromPool();
    
private:
    // Command registry
    std::map<std::string, std::function<void(const std::vector<std::string>&)>> m_commands;
    
    // Core components
    std::unique_ptr<Miner> m_miner;
    std::unique_ptr<ConfigManager> m_config;
    std::unique_ptr<Logger> m_logger;
    
    // Statistics
    MiningStats m_stats;
    std::mutex m_statsMutex;
    
    // Control
    std::atomic<bool> m_running;
    std::atomic<bool> m_interactive;
    std::thread m_statsThread;
    
    // Helper methods
    void registerCommands();
    void printBanner();
    void printVersion();
    std::string formatHashrate(double hashrate);
    std::string formatTime(std::chrono::steady_clock::time_point start);
    void clearScreen();
    void printSeparator();
};
