#include "multi_pool_manager.h"
#include "logger.h"
#include "config_manager.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

// PoolConnection Implementation
PoolConnection::PoolConnection(const PoolConfig& config) 
    : m_config(config), m_status(PoolStatus::DISCONNECTED), m_socket(-1) {
    m_stats.poolName = config.name;
}

PoolConnection::~PoolConnection() {
    disconnect();
}

bool PoolConnection::connect() {
    if (isConnected()) {
        return true;
    }
    
    setStatus(PoolStatus::CONNECTING);
    m_stats.connectionAttempts++;
    m_stats.lastConnection = std::chrono::steady_clock::now();
    
    logConnection("Attempting to connect to " + m_config.name + " at " + m_config.url);
    
    if (!connectToHost()) {
        setStatus(PoolStatus::FAILED);
        m_stats.failedConnections++;
        logError("Failed to connect to " + m_config.name);
        return false;
    }
    
    setStatus(PoolStatus::CONNECTED);
    m_stats.successfulConnections++;
    logConnection("Connected to " + m_config.name);
    
    return true;
}

void PoolConnection::disconnect() {
    if (m_socket != -1) {
        close(m_socket);
        m_socket = -1;
    }
    setStatus(PoolStatus::DISCONNECTED);
    m_stats.isActive = false;
    logConnection("Disconnected from " + m_config.name);
}

bool PoolConnection::isConnected() const {
    return m_socket != -1 && m_status == PoolStatus::CONNECTED;
}

bool PoolConnection::authenticate() {
    if (!isConnected()) {
        return false;
    }
    
    logConnection("Authenticating with " + m_config.name);
    
    bool success = false;
    switch (m_config.protocol) {
        case PoolProtocol::STRATUM_V1:
            success = sendStratumV1Login();
            break;
        case PoolProtocol::STRATUM_V2:
            success = sendStratumV2Login();
            break;
        case PoolProtocol::XMRIG_PROTOCOL:
            success = sendXMRigLogin();
            break;
        case PoolProtocol::P2POOL:
            success = sendP2PoolLogin();
            break;
        case PoolProtocol::CUSTOM:
            success = sendCustomLogin();
            break;
    }
    
    if (success) {
        setStatus(PoolStatus::AUTHENTICATED);
        m_stats.isActive = true;
        logConnection("Authenticated with " + m_config.name);
    } else {
        setStatus(PoolStatus::ERROR);
        logError("Authentication failed with " + m_config.name);
    }
    
    return success;
}

bool PoolConnection::sendJobRequest() {
    if (!isConnected() || m_status != PoolStatus::AUTHENTICATED) {
        return false;
    }
    
    // Send job request based on protocol
    std::string request;
    switch (m_config.protocol) {
        case PoolProtocol::STRATUM_V1:
            request = "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"mining.subscribe\",\"params\":[\"MiningSoft/1.0\"]}";
            break;
        case PoolProtocol::STRATUM_V2:
            request = "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"mining.subscribe\",\"params\":[\"MiningSoft/1.0\",\"MiningSoft/1.0\"]}";
            break;
        default:
            request = "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"mining.subscribe\",\"params\":[\"MiningSoft/1.0\"]}";
            break;
    }
    
    return sendMessage(request);
}

bool PoolConnection::submitShare(const std::string& jobId, uint32_t nonce, const std::string& hash) {
    if (!isConnected() || m_status != PoolStatus::AUTHENTICATED) {
        return false;
    }
    
    std::ostringstream request;
    request << "{\"id\":2,\"jsonrpc\":\"2.0\",\"method\":\"mining.submit\",\"params\":[";
    request << "\"" << m_config.username << "\",";
    request << "\"" << jobId << "\",";
    request << "\"" << std::hex << nonce << "\",";
    request << "\"" << hash << "\"";
    request << "]}";
    
    m_stats.sharesSubmitted++;
    m_stats.lastShare = std::chrono::steady_clock::now();
    
    return sendMessage(request.str());
}

bool PoolConnection::receiveMessage(std::string& message) {
    if (!isConnected()) {
        return false;
    }
    
    char buffer[4096];
    int bytes = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        message = std::string(buffer);
        return true;
    } else if (bytes == 0) {
        logConnection("Connection closed by peer");
        disconnect();
        return false;
    } else {
        logError("Failed to receive data: " + std::string(strerror(errno)));
        return false;
    }
}

bool PoolConnection::sendMessage(const std::string& message) {
    if (!isConnected()) {
        return false;
    }
    
    int result = send(m_socket, message.c_str(), message.length(), 0);
    if (result <= 0) {
        logError("Failed to send data: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

void PoolConnection::updateStats() {
    if (m_stats.sharesSubmitted > 0) {
        m_stats.acceptanceRate = static_cast<double>(m_stats.sharesAccepted) / m_stats.sharesSubmitted;
    }
}

bool PoolConnection::connectToHost() {
    // Parse URL
    std::string host = m_config.url;
    int port = m_config.port;
    
    // Remove protocol prefix
    if (host.find("stratum+tcp://") == 0) {
        host = host.substr(14);
    } else if (host.find("stratum+ssl://") == 0) {
        host = host.substr(14);
    }
    
    // Extract port if specified in URL
    size_t colonPos = host.find(':');
    if (colonPos != std::string::npos) {
        port = std::stoi(host.substr(colonPos + 1));
        host = host.substr(0, colonPos);
    }
    
    // Create socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == -1) {
        logError("Failed to create socket: " + std::string(strerror(errno)));
        return false;
    }
    
    // Resolve hostname
    struct hostent* he = gethostbyname(host.c_str());
    if (!he) {
        logError("Failed to resolve hostname: " + host);
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    // Set up address
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = *((struct in_addr*)he->h_addr);
    
    // Connect
    if (::connect(m_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        logError("Failed to connect to " + host + ":" + std::to_string(port) + " - " + strerror(errno));
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    return true;
}

bool PoolConnection::sendStratumV1Login() {
    std::ostringstream loginJson;
    loginJson << "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"login\",\"params\":{";
    loginJson << "\"login\":\"" << m_config.username << "\",";
    loginJson << "\"pass\":\"" << m_config.password << "\",";
    loginJson << "\"agent\":\"MiningSoft/1.0\"";
    loginJson << "}}";
    
    return sendMessage(loginJson.str());
}

bool PoolConnection::sendStratumV2Login() {
    // Stratum v2 login
    std::ostringstream loginJson;
    loginJson << "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"mining.authorize\",\"params\":[";
    loginJson << "\"" << m_config.username << "\",";
    loginJson << "\"" << m_config.password << "\"";
    loginJson << "]}";
    
    return sendMessage(loginJson.str());
}

bool PoolConnection::sendXMRigLogin() {
    // XMRig-specific login format
    std::ostringstream loginJson;
    loginJson << "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"login\",\"params\":{";
    loginJson << "\"login\":\"" << m_config.username << "\",";
    loginJson << "\"pass\":\"" << m_config.password << "\",";
    loginJson << "\"agent\":\"MiningSoft/1.0\",";
    loginJson << "\"algo\":[\"rx/0\"]";
    loginJson << "}}";
    
    return sendMessage(loginJson.str());
}

bool PoolConnection::sendP2PoolLogin() {
    // P2Pool uses different protocol
    std::ostringstream loginJson;
    loginJson << "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"login\",\"params\":{";
    loginJson << "\"login\":\"" << m_config.username << "\",";
    loginJson << "\"pass\":\"" << m_config.password << "\"";
    loginJson << "}}";
    
    return sendMessage(loginJson.str());
}

bool PoolConnection::sendCustomLogin() {
    // Custom protocol - try multiple methods
    return sendStratumV1Login() || sendStratumV2Login() || sendXMRigLogin();
}

void PoolConnection::logConnection(const std::string& message) {
    // Use global logger if available
    if (g_logger) {
        LOG_INFO("[{}] {}", m_config.name, message);
    }
}

void PoolConnection::logError(const std::string& message) {
    if (g_logger) {
        LOG_ERROR("[{}] {}", m_config.name, message);
    }
}

// MultiPoolManager Implementation
MultiPoolManager::MultiPoolManager() 
    : m_activePool(""), m_mining(false), m_initialized(false),
      m_failoverEnabled(true), m_autoSwitchEnabled(true), 
      m_switchInterval(300), m_maxRetries(3), m_retryDelay(5),
      m_running(false) {
}

MultiPoolManager::~MultiPoolManager() {
    shutdown();
}

bool MultiPoolManager::initialize(const ConfigManager& config) {
    if (m_initialized) {
        return true;
    }
    
    logInfo("Initializing Multi-Pool Manager");
    
    // Load pool configurations from config
    // This would typically read from config file or command line
    // For now, we'll add some default pools
    
    m_initialized = true;
    m_running = true;
    
    // Start monitoring thread
    m_monitoringThread = std::thread(&MultiPoolManager::monitoringLoop, this);
    m_connectionThread = std::thread(&MultiPoolManager::connectionLoop, this);
    
    logInfo("Multi-Pool Manager initialized successfully");
    return true;
}

void MultiPoolManager::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    logInfo("Shutting down Multi-Pool Manager");
    
    m_running = false;
    m_mining = false;
    
    // Stop threads
    if (m_monitoringThread.joinable()) {
        m_monitoringThread.join();
    }
    if (m_connectionThread.joinable()) {
        m_connectionThread.join();
    }
    
    // Disconnect all pools
    disconnectAll();
    
    m_initialized = false;
    logInfo("Multi-Pool Manager shutdown complete");
}

bool MultiPoolManager::addPool(const PoolConfig& config) {
    if (!validatePoolConfig(config)) {
        logError("Invalid pool configuration for " + config.name);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_poolsMutex);
    
    // Check if pool already exists
    for (const auto& pool : m_pools) {
        if (pool->getConfig().name == config.name) {
            logWarning("Pool " + config.name + " already exists");
            return false;
        }
    }
    
    auto pool = std::make_unique<PoolConnection>(config);
    m_pools.push_back(std::move(pool));
    
    logInfo("Added pool: " + config.name + " (" + config.url + ")");
    return true;
}

bool MultiPoolManager::removePool(const std::string& poolName) {
    std::lock_guard<std::mutex> lock(m_poolsMutex);
    
    auto it = std::find_if(m_pools.begin(), m_pools.end(),
        [&poolName](const std::unique_ptr<PoolConnection>& pool) {
            return pool->getConfig().name == poolName;
        });
    
    if (it != m_pools.end()) {
        if ((*it)->isConnected()) {
            (*it)->disconnect();
        }
        m_pools.erase(it);
        logInfo("Removed pool: " + poolName);
        return true;
    }
    
    logWarning("Pool not found: " + poolName);
    return false;
}

bool MultiPoolManager::connectToBestPool() {
    std::string bestPool = getBestPool();
    if (bestPool.empty()) {
        logError("No suitable pools available");
        return false;
    }
    
    return connectToPool(bestPool);
}

bool MultiPoolManager::connectToPool(const std::string& poolName) {
    std::lock_guard<std::mutex> lock(m_poolsMutex);
    
    auto it = std::find_if(m_pools.begin(), m_pools.end(),
        [&poolName](const std::unique_ptr<PoolConnection>& pool) {
            return pool->getConfig().name == poolName;
        });
    
    if (it == m_pools.end()) {
        logError("Pool not found: " + poolName);
        return false;
    }
    
    // Disconnect current active pool
    if (!m_activePool.empty() && m_activePool != poolName) {
        auto currentIt = std::find_if(m_pools.begin(), m_pools.end(),
            [this](const std::unique_ptr<PoolConnection>& pool) {
                return pool->getConfig().name == m_activePool;
            });
        if (currentIt != m_pools.end()) {
            (*currentIt)->disconnect();
        }
    }
    
    // Connect to new pool
    if ((*it)->connect() && (*it)->authenticate()) {
        m_activePool = poolName;
        logInfo("Connected to pool: " + poolName);
        
        if (m_onPoolConnected) {
            m_onPoolConnected(poolName);
        }
        
        return true;
    }
    
    logError("Failed to connect to pool: " + poolName);
    return false;
}

void MultiPoolManager::disconnectAll() {
    std::lock_guard<std::mutex> lock(m_poolsMutex);
    
    for (auto& pool : m_pools) {
        if (pool->isConnected()) {
            pool->disconnect();
        }
    }
    
    m_activePool = "";
    logInfo("Disconnected from all pools");
}

bool MultiPoolManager::startMining() {
    if (m_mining) {
        return true;
    }
    
    if (m_activePool.empty()) {
        if (!connectToBestPool()) {
            logError("No active pool for mining");
            return false;
        }
    }
    
    m_mining = true;
    logInfo("Mining started on pool: " + m_activePool);
    return true;
}

void MultiPoolManager::stopMining() {
    m_mining = false;
    logInfo("Mining stopped");
}

bool MultiPoolManager::submitShare(const std::string& jobId, uint32_t nonce, const std::string& hash) {
    if (m_activePool.empty()) {
        logError("No active pool for share submission");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_poolsMutex);
    
    auto it = std::find_if(m_pools.begin(), m_pools.end(),
        [this](const std::unique_ptr<PoolConnection>& pool) {
            return pool->getConfig().name == m_activePool;
        });
    
    if (it == m_pools.end()) {
        logError("Active pool not found: " + m_activePool);
        return false;
    }
    
    return (*it)->submitShare(jobId, nonce, hash);
}

std::string MultiPoolManager::getBestPool() const {
    std::lock_guard<std::mutex> lock(m_poolsMutex);
    
    if (m_pools.empty()) {
        return "";
    }
    
    // Sort pools by priority and status
    std::vector<std::pair<int, std::string>> poolScores;
    
    for (const auto& pool : m_pools) {
        const auto& config = pool->getConfig();
        const auto& stats = pool->getStats();
        
        if (!config.enabled) {
            continue;
        }
        
        int score = config.priority;
        
        // Boost score for better performing pools
        if (stats.acceptanceRate > 0.8) {
            score += 10;
        }
        if (stats.status == PoolStatus::CONNECTED) {
            score += 5;
        }
        
        poolScores.push_back({score, config.name});
    }
    
    if (poolScores.empty()) {
        return "";
    }
    
    // Sort by score (descending)
    std::sort(poolScores.begin(), poolScores.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });
    
    return poolScores[0].second;
}

std::string MultiPoolManager::getActivePool() const {
    return m_activePool;
}

bool MultiPoolManager::switchToBestPool() {
    std::string bestPool = getBestPool();
    if (bestPool.empty() || bestPool == m_activePool) {
        return false;
    }
    
    logInfo("Switching to best pool: " + bestPool);
    return connectToPool(bestPool);
}

std::vector<PoolStats> MultiPoolManager::getAllPoolStats() const {
    std::lock_guard<std::mutex> lock(m_poolsMutex);
    
    std::vector<PoolStats> stats;
    for (const auto& pool : m_pools) {
        stats.push_back(pool->getStats());
    }
    
    return stats;
}

PoolStats MultiPoolManager::getPoolStats(const std::string& poolName) const {
    std::lock_guard<std::mutex> lock(m_poolsMutex);
    
    auto it = std::find_if(m_pools.begin(), m_pools.end(),
        [&poolName](const std::unique_ptr<PoolConnection>& pool) {
            return pool->getConfig().name == poolName;
        });
    
    if (it != m_pools.end()) {
        return (*it)->getStats();
    }
    
    return PoolStats();
}

void MultiPoolManager::logPoolStatistics() const {
    auto stats = getAllPoolStats();
    
    logInfo("=== Pool Statistics ===");
    for (const auto& stat : stats) {
        logInfo("Pool: " + stat.poolName);
        logInfo("  Status: " + std::to_string(static_cast<int>(stat.status)));
        logInfo("  Shares: " + std::to_string(stat.sharesSubmitted) + 
                " submitted, " + std::to_string(stat.sharesAccepted) + " accepted");
        logInfo("  Acceptance Rate: " + std::to_string(stat.acceptanceRate * 100) + "%");
        logInfo("  Latency: " + std::to_string(stat.latency) + "ms");
    }
}

void MultiPoolManager::logConnectionStatus() const {
    logInfo("=== Connection Status ===");
    logInfo("Active Pool: " + (m_activePool.empty() ? "None" : m_activePool));
    logInfo("Mining: " + std::string(m_mining ? "Yes" : "No"));
    logInfo("Failover: " + std::string(m_failoverEnabled ? "Enabled" : "Disabled"));
    logInfo("Auto Switch: " + std::string(m_autoSwitchEnabled ? "Enabled" : "Disabled"));
}

void MultiPoolManager::setOnPoolConnected(std::function<void(const std::string&)> callback) {
    m_onPoolConnected = callback;
}

void MultiPoolManager::setOnPoolDisconnected(std::function<void(const std::string&)> callback) {
    m_onPoolDisconnected = callback;
}

void MultiPoolManager::setOnShareAccepted(std::function<void(const std::string&, const std::string&)> callback) {
    m_onShareAccepted = callback;
}

void MultiPoolManager::setOnShareRejected(std::function<void(const std::string&, const std::string&)> callback) {
    m_onShareRejected = callback;
}

void MultiPoolManager::monitoringLoop() {
    logInfo("Pool monitoring thread started");
    
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        if (m_autoSwitchEnabled && !m_activePool.empty()) {
            // Check if we should switch to a better pool
            std::string bestPool = getBestPool();
            if (!bestPool.empty() && bestPool != m_activePool) {
                logInfo("Auto-switching to better pool: " + bestPool);
                connectToPool(bestPool);
            }
        }
        
        // Update pool statistics
        {
            std::lock_guard<std::mutex> lock(m_poolsMutex);
            for (auto& pool : m_pools) {
                pool->updateStats();
            }
        }
    }
    
    logInfo("Pool monitoring thread stopped");
}

void MultiPoolManager::connectionLoop() {
    logInfo("Pool connection thread started");
    
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        // Check connection health
        if (!m_activePool.empty()) {
            std::lock_guard<std::mutex> lock(m_poolsMutex);
            
            auto it = std::find_if(m_pools.begin(), m_pools.end(),
                [this](const std::unique_ptr<PoolConnection>& pool) {
                    return pool->getConfig().name == m_activePool;
                });
            
            if (it != m_pools.end() && !(*it)->isConnected()) {
                logWarning("Active pool disconnected, attempting reconnection");
                if (!(*it)->connect() || !(*it)->authenticate()) {
                    logError("Failed to reconnect to active pool");
                    if (m_failoverEnabled) {
                        connectToBestPool();
                    }
                }
            }
        }
    }
    
    logInfo("Pool connection thread stopped");
}

bool MultiPoolManager::selectBestPool() {
    return switchToBestPool();
}

void MultiPoolManager::handlePoolFailure(const std::string& poolName) {
    logError("Pool failure detected: " + poolName);
    
    if (m_failoverEnabled && poolName == m_activePool) {
        logInfo("Initiating failover to best available pool");
        connectToBestPool();
    }
}

void MultiPoolManager::updatePoolPriorities() {
    // Update pool priorities based on performance
    std::lock_guard<std::mutex> lock(m_poolsMutex);
    
    for (auto& pool : m_pools) {
        auto& stats = pool->getStats();
        auto& config = const_cast<PoolConfig&>(pool->getConfig());
        
        // Adjust priority based on performance
        if (stats.acceptanceRate > 0.9) {
            config.priority = std::min(config.priority + 1, 10);
        } else if (stats.acceptanceRate < 0.5) {
            config.priority = std::max(config.priority - 1, 1);
        }
    }
}

bool MultiPoolManager::validatePoolConfig(const PoolConfig& config) const {
    if (config.name.empty() || config.url.empty() || config.username.empty()) {
        return false;
    }
    
    if (config.port <= 0 || config.port > 65535) {
        return false;
    }
    
    if (config.priority < 1 || config.priority > 10) {
        return false;
    }
    
    return true;
}

void MultiPoolManager::logInfo(const std::string& message) const {
    if (g_logger) {
        LOG_INFO("[MultiPool] {}", message);
    }
}

void MultiPoolManager::logWarning(const std::string& message) const {
    if (g_logger) {
        LOG_WARNING("[MultiPool] {}", message);
    }
}

void MultiPoolManager::logError(const std::string& message) const {
    if (g_logger) {
        LOG_ERROR("[MultiPool] {}", message);
    }
}

void MultiPoolManager::logDebug(const std::string& message) const {
    if (g_logger) {
        LOG_DEBUG("[MultiPool] {}", message);
    }
}
