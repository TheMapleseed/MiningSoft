#pragma once

#include "config_manager.h"
#include "logger.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>

// Forward declaration for RandomX
class RandomX;

struct MiningJob {
    std::string jobId;
    std::string blob;
    std::string target;
    uint32_t nonce;
    bool isValid;
    
    MiningJob() : nonce(0), isValid(false) {}
};

class Miner {
public:
    Miner();
    ~Miner();
    
    // Initialize the miner with configuration
    bool initialize(const ConfigManager& config);
    
    // Start/stop mining
    void start();
    void stop();
    
    // Check if miner is running
    bool isRunning() const { return m_running; }
    
    // Check if connected to pool
    bool isConnected() const { return m_connected; }

private:
    // RandomX initialization
    bool initializeRandomX();
    
    // Pool connection
    bool connectToPool();
    bool parsePoolUrl(const std::string& url, std::string& host, int& port, bool& useSSL);
    bool setupSSL();
    bool sendLogin();
    bool sendData(const std::string& data);
    bool receiveData(std::string& data);
    
    // Mining
    void miningLoop(int threadId);
    void mineJob(int threadId);
    bool isValidShare(const uint8_t* hash, const std::string& target);
    void submitShare(uint32_t nonce, const uint8_t* hash);
    
    // Communication
    void communicationLoop();
    void processPoolMessage(const std::string& message);
    std::string extractJsonValue(const std::string& json, const std::string& key);
    
    // Idle detection
    void idleLoop();
    bool checkSystemIdle();
    void startMining();
    void stopMining();
    
    // Utilities
    std::vector<uint8_t> hexToBytes(const std::string& hex);
    std::string bytesToHex(const uint8_t* bytes, size_t length);
    
    // Configuration
    ConfigManager m_config;
    
    // Mining state
    std::atomic<bool> m_running;
    std::atomic<bool> m_connected;
    MiningJob m_currentJob;
    
    // Threading
    std::vector<std::thread> m_miningThreads;
    std::thread m_communicationThread;
    std::thread m_idleThread;
    
    // Idle detection
    int m_idleTime;
    bool m_miningActive;
    
    // Network
    int m_socket;
    void* m_ssl;  // SSL*
    void* m_sslContext;  // SSL_CTX*
    
    // RandomX
    std::unique_ptr<RandomX> m_randomx;
};
