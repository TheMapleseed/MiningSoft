#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include <functional>

// Forward declarations
class Logger;
class ConfigManager;

enum class PoolProtocol {
    STRATUM_V1,      // Standard Stratum v1
    STRATUM_V2,      // Stratum v2
    XMRIG_PROTOCOL,  // XMRig-specific protocol
    P2POOL,          // P2Pool decentralized
    CUSTOM           // Custom protocol
};

enum class PoolStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AUTHENTICATED,
    MINING,
    ERROR,
    FAILED
};

struct PoolConfig {
    std::string name;
    std::string url;
    std::string username;
    std::string password;
    std::string workerId;
    int port;
    bool ssl;
    int timeout;
    int keepAlive;
    PoolProtocol protocol;
    int priority;  // Higher number = higher priority
    bool enabled;
    
    PoolConfig() : port(3333), ssl(false), timeout(30), keepAlive(60), 
                   protocol(PoolProtocol::STRATUM_V1), priority(1), enabled(true) {}
};

struct PoolStats {
    std::string poolName;
    PoolStatus status;
    int connectionAttempts;
    int successfulConnections;
    int failedConnections;
    int sharesSubmitted;
    int sharesAccepted;
    int sharesRejected;
    double acceptanceRate;
    std::chrono::steady_clock::time_point lastConnection;
    std::chrono::steady_clock::time_point lastShare;
    double latency;
    bool isActive;
    
    PoolStats() : status(PoolStatus::DISCONNECTED), connectionAttempts(0),
                  successfulConnections(0), failedConnections(0), sharesSubmitted(0),
                  sharesAccepted(0), sharesRejected(0), acceptanceRate(0.0),
                  latency(0.0), isActive(false) {}
};

class PoolConnection {
public:
    PoolConnection(const PoolConfig& config);
    ~PoolConnection();
    
    bool connect();
    void disconnect();
    bool isConnected() const;
    bool authenticate();
    bool sendJobRequest();
    bool submitShare(const std::string& jobId, uint32_t nonce, const std::string& hash);
    bool receiveMessage(std::string& message);
    bool sendMessage(const std::string& message);
    
    PoolStatus getStatus() const { return m_status; }
    const PoolConfig& getConfig() const { return m_config; }
    PoolStats& getStats() { return m_stats; }
    
    void setStatus(PoolStatus status) { m_status = status; }
    void updateStats();
    
private:
    PoolConfig m_config;
    PoolStats m_stats;
    PoolStatus m_status;
    
    int m_socket;
    std::string m_lastJobId;
    std::string m_lastBlob;
    std::string m_lastTarget;
    
    bool connectToHost();
    bool sendStratumV1Login();
    bool sendStratumV2Login();
    bool sendXMRigLogin();
    bool sendP2PoolLogin();
    bool sendCustomLogin();
    
    void logConnection(const std::string& message);
    void logError(const std::string& message);
};

class MultiPoolManager {
public:
    MultiPoolManager();
    ~MultiPoolManager();
    
    bool initialize(const ConfigManager& config);
    void shutdown();
    
    // Pool management
    bool addPool(const PoolConfig& config);
    bool removePool(const std::string& poolName);
    bool enablePool(const std::string& poolName);
    bool disablePool(const std::string& poolName);
    
    // Connection management
    bool connectToBestPool();
    bool connectToPool(const std::string& poolName);
    void disconnectAll();
    
    // Mining operations
    bool startMining();
    void stopMining();
    bool submitShare(const std::string& jobId, uint32_t nonce, const std::string& hash);
    
    // Pool selection
    std::string getBestPool() const;
    std::string getActivePool() const;
    bool switchToBestPool();
    
    // Statistics and monitoring
    std::vector<PoolStats> getAllPoolStats() const;
    PoolStats getPoolStats(const std::string& poolName) const;
    void logPoolStatistics() const;
    void logConnectionStatus() const;
    
    // Event callbacks
    void setOnPoolConnected(std::function<void(const std::string&)> callback);
    void setOnPoolDisconnected(std::function<void(const std::string&)> callback);
    void setOnShareAccepted(std::function<void(const std::string&, const std::string&)> callback);
    void setOnShareRejected(std::function<void(const std::string&, const std::string&)> callback);
    
    // Configuration
    void setFailoverEnabled(bool enabled) { m_failoverEnabled = enabled; }
    void setAutoSwitchEnabled(bool enabled) { m_autoSwitchEnabled = enabled; }
    void setSwitchInterval(int seconds) { m_switchInterval = seconds; }
    
private:
    std::vector<std::unique_ptr<PoolConnection>> m_pools;
    std::string m_activePool;
    std::atomic<bool> m_mining;
    std::atomic<bool> m_initialized;
    
    // Configuration
    bool m_failoverEnabled;
    bool m_autoSwitchEnabled;
    int m_switchInterval;
    int m_maxRetries;
    int m_retryDelay;
    
    // Threading
    std::thread m_monitoringThread;
    std::thread m_connectionThread;
    std::atomic<bool> m_running;
    mutable std::mutex m_poolsMutex;
    
    // Callbacks
    std::function<void(const std::string&)> m_onPoolConnected;
    std::function<void(const std::string&)> m_onPoolDisconnected;
    std::function<void(const std::string&, const std::string&)> m_onShareAccepted;
    std::function<void(const std::string&, const std::string&)> m_onShareRejected;
    
    // Internal methods
    void monitoringLoop();
    void connectionLoop();
    bool selectBestPool();
    void handlePoolFailure(const std::string& poolName);
    void updatePoolPriorities();
    bool validatePoolConfig(const PoolConfig& config) const;
    
    // Logging
    void logInfo(const std::string& message) const;
    void logWarning(const std::string& message) const;
    void logError(const std::string& message) const;
    void logDebug(const std::string& message) const;
};
