#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>

/**
 * Mining pool connection handler
 * Manages communication with Monero mining pools
 */
class PoolConnection {
public:
    PoolConnection();
    ~PoolConnection();

    // Connection result
    enum class ConnectionResult {
        Success,
        Failed,
        Timeout,
        InvalidCredentials,
        PoolFull,
        NetworkError
    };

    // Job information from pool
    struct MiningJob {
        std::string jobId;
        std::string blob;
        std::string target;
        std::string seedHash;
        uint32_t difficulty;
        uint64_t height;
        bool isValid;
    };

    // Share submission result
    struct ShareResult {
        bool accepted;
        std::string reason;
        uint32_t difficulty;
    };

    // Initialize pool connection
    bool initialize(const std::string& poolUrl, 
                   const std::string& username, 
                   const std::string& password,
                   const std::string& workerId = "");

    // Connect to mining pool
    ConnectionResult connect();
    
    // Disconnect from mining pool
    void disconnect();
    
    // Check if connected
    bool isConnected() const { return m_connected; }
    
    // Get current mining job
    MiningJob getCurrentJob() const;
    
    // Submit share to pool
    ShareResult submitShare(const std::string& jobId, 
                           const std::string& nonce, 
                           const std::string& result);
    
    // Set callback for new job notifications
    void setJobCallback(std::function<void(const MiningJob&)> callback);
    
    // Set callback for connection status changes
    void setConnectionCallback(std::function<void(bool)> callback);
    
    // Get pool statistics
    struct PoolStats {
        std::string poolName;
        std::string poolVersion;
        uint32_t difficulty;
        uint64_t height;
        double hashrate;
        uint64_t totalShares;
        uint64_t acceptedShares;
        uint64_t rejectedShares;
        std::chrono::steady_clock::time_point lastUpdate;
    };
    
    PoolStats getPoolStats() const;

private:
    // Main communication loop
    void communicationLoop();
    
    // Send login request
    bool sendLogin();
    
    // Send job request
    bool requestJob();
    
    // Send share submission
    ShareResult sendShare(const std::string& jobId, 
                         const std::string& nonce, 
                         const std::string& result);
    
    // Parse pool response
    bool parseResponse(const std::string& response);
    
    // Handle job update
    void handleJobUpdate(const std::string& jobData);
    
    // Handle share result
    void handleShareResult(const std::string& resultData);
    
    // Reconnect to pool
    void reconnect();
    
    // Validate pool URL
    bool validatePoolUrl(const std::string& url);

private:
    std::string m_poolUrl;
    std::string m_username;
    std::string m_password;
    std::string m_workerId;
    
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_shouldStop{false};
    
    std::thread m_communicationThread;
    mutable std::mutex m_jobMutex;
    mutable std::mutex m_statsMutex;
    
    MiningJob m_currentJob;
    PoolStats m_poolStats;
    
    // Callbacks
    std::function<void(const MiningJob&)> m_jobCallback;
    std::function<void(bool)> m_connectionCallback;
    
    // Connection parameters
    int m_connectionTimeout{30}; // seconds
    int m_keepAliveInterval{60}; // seconds
    int m_maxReconnectAttempts{5};
    
    // Statistics
    std::atomic<uint64_t> m_totalShares{0};
    std::atomic<uint64_t> m_acceptedShares{0};
    std::atomic<uint64_t> m_rejectedShares{0};
};
