#include "miner.h"
#include "logger.h"
#include "config_manager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <random>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>

#include "randomx.h"

Miner::Miner() : m_running(false), m_connected(false), m_initialized(false), m_socket(-1), m_ssl(nullptr), m_sslContext(nullptr), m_idleTime(0), m_miningActive(false), m_sharesSubmitted(0), m_sharesAccepted(0), m_sharesRejected(0), m_submitId(1) {
    m_performanceMonitor = std::make_unique<PerformanceMonitor>();
}

Miner::~Miner() {
    stop();
    if (m_socket != -1) {
        close(m_socket);
    }
}

bool Miner::initialize(const ConfigManager& config) {
    LOG_INFO("Initializing Monero miner");
    
    // Store configuration
    m_config = config;
    
    // Initialize RandomX
    if (!initializeRandomX()) {
        LOG_ERROR("Failed to initialize RandomX");
        return false;
    }
    
    // Connect to mining pool
    if (!connectToPool()) {
        LOG_ERROR("Failed to connect to mining pool");
            return false;
        }
        
    // Initialize performance monitor
    if (!m_performanceMonitor->initialize()) {
        LOG_ERROR("Failed to initialize performance monitor");
        return false;
    }
    
    // Start real-time performance display
    m_performanceMonitor->startRealTimeDisplay();
    
    m_initialized = true;
    LOG_INFO("Miner initialized successfully");
    return true;
}

bool Miner::initializeRandomX() {
    LOG_INFO("Initializing RandomX algorithm");
    
    m_randomx = std::make_unique<RandomX>();
    // Initialize RandomX with a default key for now
    uint8_t defaultKey[32] = {0};
    if (!m_randomx->initialize(defaultKey, sizeof(defaultKey))) {
        LOG_ERROR("Failed to initialize RandomX");
        return false;
    }
    
    LOG_INFO("RandomX initialized successfully");
    return true;
}

bool Miner::connectToPool() {
    LOG_INFO("Connecting to mining pool: {}", m_config.getPoolConfig().url);
    
    // Parse pool URL
    std::string host;
    int port;
    bool useSSL;
    
    if (!parsePoolUrl(m_config.getPoolConfig().url, host, port, useSSL)) {
        LOG_ERROR("Failed to parse pool URL: {}", m_config.getPoolConfig().url);
        return false;
    }
    
    // Create socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == -1) {
        LOG_ERROR("Failed to create socket: {}", strerror(errno));
        return false;
    }
    
    // Resolve hostname
    struct hostent* he = gethostbyname(host.c_str());
    if (!he) {
        LOG_ERROR("Failed to resolve hostname: {}", host);
        return false;
    }
    
    // Connect to pool
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = *((struct in_addr*)he->h_addr);
    
    if (connect(m_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        LOG_ERROR("Failed to connect to pool {}:{} - {}", host, port, strerror(errno));
        return false;
    }
    
    LOG_INFO("Connected to pool {}:{}", host, port);
        
    // Note: SSL support removed for simplicity
    if (useSSL) {
        LOG_WARNING("SSL support not implemented, using plain TCP");
    }
    
    // Send login request
    if (!sendLogin()) {
        LOG_ERROR("Failed to send login request");
            return false;
        }
        
    m_connected = true;
    LOG_INFO("Connected to mining pool successfully");
    return true;
}

bool Miner::reconnectToPool() {
    // Close existing connection
    if (m_socket != -1) {
        close(m_socket);
        m_socket = -1;
    }
    
    m_connected = false;
    
    // Wait a bit before reconnecting
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Try to reconnect
    return connectToPool();
}

bool Miner::parsePoolUrl(const std::string& url, std::string& host, int& port, bool& useSSL) {
    LOG_DEBUG("Parsing pool URL: '{}' (length: {})", url, url.length());
    
    // Parse stratum+tcp://host:port or stratum+ssl://host:port
    if (url.length() > 14 && url.substr(0, 14) == "stratum+tcp://") {
        useSSL = false;
        std::string hostPort = url.substr(14);
        size_t colonPos = hostPort.find(':');
        if (colonPos == std::string::npos) {
            LOG_ERROR("No port found in URL: {}", url);
            return false;
        }
        host = hostPort.substr(0, colonPos);
        try {
            port = std::stoi(hostPort.substr(colonPos + 1));
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse port: {}", e.what());
            return false;
        }
        LOG_DEBUG("Parsed TCP: host={}, port={}", host, port);
    } else if (url.length() > 15 && url.substr(0, 15) == "stratum+ssl://") {
        useSSL = true;
        std::string hostPort = url.substr(15);
        size_t colonPos = hostPort.find(':');
        if (colonPos == std::string::npos) {
            LOG_ERROR("No port found in URL: {}", url);
            return false;
        }
        host = hostPort.substr(0, colonPos);
        try {
            port = std::stoi(hostPort.substr(colonPos + 1));
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse port: {}", e.what());
            return false;
        }
        LOG_DEBUG("Parsed SSL: host={}, port={}", host, port);
    } else {
        LOG_ERROR("Invalid URL format: '{}' (expected stratum+tcp:// or stratum+ssl://)", url);
        LOG_DEBUG("First 15 chars: '{}'", url.substr(0, 15));
        return false;
    }
    return true;
}

// SSL setup removed for simplicity

bool Miner::sendLogin() {
    // Validate wallet address first
    if (!isValidMoneroAddress(m_config.getPoolConfig().username)) {
        LOG_ERROR("Invalid Monero wallet address: {}", m_config.getPoolConfig().username);
        return false;
    }
    
    // Try different login methods based on pool compatibility
    // Method 1: Direct login (some pools prefer this)
    std::ostringstream loginJson;
    loginJson << "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"login\",\"params\":{";
    loginJson << "\"login\":\"" << m_config.getPoolConfig().username << "\",";
    loginJson << "\"pass\":\"" << m_config.getPoolConfig().password << "\",";
    loginJson << "\"agent\":\"MiningSoft/1.0\"";
    loginJson << "}}";
    
    std::string loginRequest = loginJson.str();
    LOG_DEBUG("Sending login request: {}", loginRequest);
    
    if (!sendData(loginRequest + "\n")) {
        LOG_ERROR("Failed to send login request");
        return false;
    }
    
    // Read login response
    std::string loginResponse;
    if (!receiveData(loginResponse)) {
        LOG_ERROR("Failed to receive login response");
        return false;
    }
    
    LOG_INFO("Received login response: {}", loginResponse);
    
    // Check if direct login worked
    if (loginResponse.find("\"result\"") != std::string::npos && 
        (loginResponse.find("\"error\":null") != std::string::npos || 
         loginResponse.find("\"error\"") == std::string::npos)) {
        LOG_INFO("Direct login successful");
        return true;
    }
    
    // Also check for job notifications (some pools send jobs immediately)
    if (loginResponse.find("\"method\":\"job\"") != std::string::npos) {
        LOG_INFO("Received job notification during login - connection successful");
        return true;
    }
    
    // Method 2: Try standard Stratum protocol if direct login failed
    LOG_INFO("Direct login failed, trying Stratum protocol...");
    
    // Step 1: Send mining.subscribe
    std::ostringstream subscribeJson;
    subscribeJson << "{\"id\":2,\"jsonrpc\":\"2.0\",\"method\":\"mining.subscribe\",\"params\":[\"MiningSoft/1.0\",\"MiningSoft/1.0\"]}";
    
    std::string subscribeRequest = subscribeJson.str();
    LOG_DEBUG("Sending subscribe request: {}", subscribeRequest);
    
    if (!sendData(subscribeRequest + "\n")) {
        LOG_ERROR("Failed to send subscribe request");
        return false;
    }
    
    // Read subscribe response
    std::string subscribeResponse;
    if (!receiveData(subscribeResponse)) {
        LOG_ERROR("Failed to receive subscribe response");
        return false;
    }
    
    LOG_INFO("Received subscribe response: {}", subscribeResponse);
    
    // Step 2: Send mining.authorize
    std::ostringstream authorizeJson;
    authorizeJson << "{\"id\":3,\"jsonrpc\":\"2.0\",\"method\":\"mining.authorize\",\"params\":[\"";
    authorizeJson << m_config.getPoolConfig().username << "\",\"";
    authorizeJson << m_config.getPoolConfig().password << "\"]}";
    
    std::string authorizeRequest = authorizeJson.str();
    LOG_DEBUG("Sending authorize request: {}", authorizeRequest);
    
    if (!sendData(authorizeRequest + "\n")) {
        LOG_ERROR("Failed to send authorize request");
        return false;
    }
    
    // Read authorize response
    std::string authorizeResponse;
    if (!receiveData(authorizeResponse)) {
        LOG_ERROR("Failed to receive authorize response");
        return false;
    }
    
    LOG_INFO("Received authorize response: {}", authorizeResponse);
    
    // Parse authorize response
    if (authorizeResponse.find("\"result\"") != std::string::npos && 
        authorizeResponse.find("\"error\"") == std::string::npos) {
        LOG_INFO("Stratum login successful");
        return true;
    } else {
        LOG_ERROR("Both login methods failed. Last response: {}", authorizeResponse);
        return false;
    }
}

bool Miner::sendData(const std::string& data) {
    int result = send(m_socket, data.c_str(), data.length(), 0);
    if (result <= 0) {
        LOG_ERROR("Failed to send data: {}", strerror(errno));
        return false;
    }
    LOG_DEBUG("Sent {} bytes", result);
    return true;
}

bool Miner::receiveData(std::string& data) {
    char buffer[4096];
    int bytes = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        data = std::string(buffer);
        LOG_DEBUG("Received {} bytes: {}", bytes, data);
        return true;
    } else if (bytes == 0) {
        LOG_ERROR("Connection closed by peer");
        return false;
    } else {
        LOG_ERROR("Failed to receive data: {}", strerror(errno));
        return false;
    }
}

void Miner::start() {
    if (m_running) {
        LOG_WARNING("Miner is already running");
        return;
    }
    
    m_running = true;
    LOG_INFO("Starting miner (idle detection enabled)...");
    
    // Start idle monitoring thread
    m_idleThread = std::thread(&Miner::idleLoop, this);
    
    // Start communication thread
    m_communicationThread = std::thread(&Miner::communicationLoop, this);
}

void Miner::stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    LOG_INFO("Stopping miner...");
    
    // Stop mining if active
    if (m_miningActive) {
        stopMining();
    }
    
    // Wait for idle thread
    if (m_idleThread.joinable()) {
        m_idleThread.join();
    }
    
    // Wait for mining threads
    for (auto& thread : m_miningThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_miningThreads.clear();
    
    // Wait for communication thread
    if (m_communicationThread.joinable()) {
        m_communicationThread.join();
    }
    
    LOG_INFO("Miner stopped");
}

void Miner::miningLoop(int threadId) {
    LOG_INFO("Mining thread {} started", threadId);
    
    while (m_running && m_miningActive) {
        if (!m_currentJob.isValid) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Mine the current job
        mineJob(threadId);
    }
    
    LOG_INFO("Mining thread {} stopped", threadId);
}

void Miner::mineJob(int threadId) {
    try {
        // Convert job blob from hex to bytes
        std::vector<uint8_t> blob = RandomX::hexToBytes(m_currentJob.blob);
        if (blob.empty()) {
            LOG_ERROR("Invalid job blob: {}", m_currentJob.blob);
            return;
        }
        
        // For Monero, we need to insert the nonce into the blob
        // The nonce goes in bytes 39-42 of the blob
        if (blob.size() < 43) {
            LOG_ERROR("Blob too small: {} bytes", blob.size());
            return;
        }
        
        // Create nonce
        uint32_t nonce = m_currentJob.nonce + threadId;
        
        // Insert nonce into blob (little-endian)
        blob[39] = nonce & 0xFF;
        blob[40] = (nonce >> 8) & 0xFF;
        blob[41] = (nonce >> 16) & 0xFF;
        blob[42] = (nonce >> 24) & 0xFF;
        
        // Hash the job
        uint8_t hash[32];
        m_randomx->calculateHash(blob.data(), blob.size(), hash);
        
        // Check if hash meets target
        if (isValidShare(hash, m_currentJob.target)) {
            LOG_INFO("Valid share found by thread {}: nonce={}", threadId, nonce);
            submitShare(nonce, hash);
        }
        
        // Update performance stats
        updatePerformanceStats();
        
        // Update nonce for next iteration
        m_currentJob.nonce += m_miningThreads.size();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in mining loop: {}", e.what());
    }
}

bool Miner::isValidShare(const uint8_t* hash, const std::string& target) {
    try {
        // Convert target from hex to bytes
        std::vector<uint8_t> targetBytes = RandomX::hexToBytes(target);
        if (targetBytes.size() != 32) {
            LOG_ERROR("Invalid target size: {} (expected 32)", targetBytes.size());
            return false;
        }
        
        // Validate share against target (little-endian comparison)
        bool isValid = true;
        for (int i = 31; i >= 0; i--) {
            if (hash[i] < targetBytes[i]) {
                // Hash is less than target - valid share
                break;
            } else if (hash[i] > targetBytes[i]) {
                // Hash is greater than target - invalid share
                isValid = false;
                break;
            }
            // If equal, continue checking next byte
        }
        
        if (isValid) {
            m_sharesSubmitted++;
            LOG_DEBUG("Valid share found! Hash: {}... Target: {}...", 
                     RandomX::bytesToHex(hash, 8), target.substr(0, 16));
        }
        
        return isValid;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in isValidShare: {}", e.what());
        return false;
    }
}

void Miner::submitShare(uint32_t nonce, const uint8_t* hash) {
    if (!m_currentJob.isValid) {
        LOG_WARNING("Cannot submit share - no valid job");
        return;
    }
    
    // Convert hash to hex
    std::string hashHex = RandomX::bytesToHex(hash, 32);
    
    // Create submit request in Stratum format
    std::ostringstream json;
    json << "{\"id\":" << (m_submitId++) << ",\"jsonrpc\":\"2.0\",\"method\":\"mining.submit\",\"params\":[";
    json << "\"" << m_config.getPoolConfig().username << "\",";
    json << "\"" << m_currentJob.jobId << "\",";
    json << "\"" << std::hex << nonce << "\",";
    json << "\"" << hashHex << "\"";
    json << "]}";
    
    std::string request = json.str();
    LOG_INFO("Submitting share: nonce={}, hash={}...", nonce, hashHex.substr(0, 16));
    
    if (sendData(request + "\n")) {
        // Wait for response
        std::string response;
        if (receiveData(response)) {
            processShareResponse(response, nonce);
        } else {
            LOG_ERROR("Failed to receive share response");
            m_sharesRejected++;
        }
    } else {
        LOG_ERROR("Failed to submit share");
        m_sharesRejected++;
    }
}

void Miner::processShareResponse(const std::string& response, uint32_t nonce) {
    LOG_DEBUG("Received share response: {}", response);
    
    // Parse JSON response
    if (response.find("\"result\"") != std::string::npos) {
        if (response.find("\"error\"") == std::string::npos || 
            response.find("\"error\":null") != std::string::npos) {
            // Share accepted
            m_sharesAccepted++;
            LOG_INFO("Share ACCEPTED! Nonce: {}, Total accepted: {}", nonce, m_sharesAccepted.load());
        } else {
            // Share rejected with error
            m_sharesRejected++;
            LOG_WARNING("Share REJECTED! Nonce: {}, Response: {}", nonce, response);
        }
    } else if (response.find("\"error\"") != std::string::npos) {
        // Share rejected
        m_sharesRejected++;
        LOG_WARNING("Share REJECTED! Nonce: {}, Response: {}", nonce, response);
    } else {
        // Unknown response
        LOG_WARNING("Unknown share response: {}", response);
    }
    
    // Log statistics
    uint64_t total = m_sharesAccepted.load() + m_sharesRejected.load();
    if (total > 0) {
        double acceptanceRate = (double)m_sharesAccepted.load() / total * 100.0;
        LOG_INFO("Mining stats: {} submitted, {} accepted, {} rejected ({}% acceptance rate)", 
                m_sharesSubmitted.load(), m_sharesAccepted.load(), m_sharesRejected.load(), acceptanceRate);
    }
}

void Miner::updatePerformanceStats() {
    if (m_performanceMonitor) {
        // Update hash rate
        double hashRate = m_randomx ? m_randomx->getHashRate() : 0.0;
        m_performanceMonitor->updateHashRate(hashRate);
        
        // Update shares
        m_performanceMonitor->updateShares(
            m_sharesSubmitted.load(),
            m_sharesAccepted.load(),
            m_sharesRejected.load()
        );
        
        // Update job info
        if (m_currentJob.isValid) {
            m_performanceMonitor->updateJobInfo(
                m_currentJob.jobId,
                m_config.getPoolConfig().url,
                1.0 // Placeholder difficulty
            );
        }
    }
}

void Miner::communicationLoop() {
    LOG_INFO("Communication thread started");
    
    while (m_running) {
        std::string response;
        if (receiveData(response)) {
            processPoolMessage(response);
        } else {
            // Connection lost, try to reconnect
            LOG_WARNING("Connection lost, attempting to reconnect...");
            if (reconnectToPool()) {
                LOG_INFO("Reconnected to pool successfully");
            } else {
                LOG_ERROR("Failed to reconnect, retrying in 10 seconds...");
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        }
    }
    
    LOG_INFO("Communication thread stopped");
}

void Miner::processPoolMessage(const std::string& message) {
    LOG_DEBUG("Received pool message: {}", message);
    
    // Parse different types of pool messages
    if (message.find("\"method\":\"mining.notify\"") != std::string::npos) {
        // Monero Stratum job notification
        // Format: {"id":null,"jsonrpc":"2.0","method":"mining.notify","params":["job_id","blob","target","algo","height","seed_hash"]}
        
        // Extract job parameters from params array
        size_t paramsStart = message.find("\"params\":[");
        if (paramsStart != std::string::npos) {
            paramsStart += 10; // Skip "params":[
            size_t paramsEnd = message.find("]", paramsStart);
            if (paramsEnd != std::string::npos) {
                std::string params = message.substr(paramsStart, paramsEnd - paramsStart);
                
                // Parse comma-separated parameters
                std::vector<std::string> paramList;
                size_t pos = 0;
                while (pos < params.length()) {
                    size_t nextPos = params.find(",", pos);
                    if (nextPos == std::string::npos) {
                        nextPos = params.length();
                    }
                    
                    std::string param = params.substr(pos, nextPos - pos);
                    // Remove quotes and trim
                    if (param.length() >= 2 && param[0] == '"' && param[param.length()-1] == '"') {
                        param = param.substr(1, param.length() - 2);
                    }
                    paramList.push_back(param);
                    
                    pos = nextPos + 1;
                }
                
                // Monero job format: [job_id, blob, target, algo, height, seed_hash]
                if (paramList.size() >= 3) {
                    m_currentJob.jobId = paramList[0];
                    m_currentJob.blob = paramList[1];
                    m_currentJob.target = paramList[2];
                    m_currentJob.nonce = 0;
                    m_currentJob.isValid = true;
                    
                    LOG_INFO("New Monero job received: {} (blob: {}...)", 
                             m_currentJob.jobId, m_currentJob.blob.substr(0, 16));
                    LOG_DEBUG("Job details - Target: {}, Algo: {}, Height: {}", 
                             m_currentJob.target, 
                             paramList.size() > 3 ? paramList[3] : "unknown",
                             paramList.size() > 4 ? paramList[4] : "unknown");
                } else {
                    LOG_WARNING("Incomplete job parameters: {}", message);
                }
            }
        }
    } else if (message.find("\"method\":\"mining.set_difficulty\"") != std::string::npos) {
        // Handle difficulty change
        LOG_INFO("Difficulty changed: {}", message);
    } else if (message.find("\"result\"") != std::string::npos) {
        // Handle other responses (login, subscribe, etc.)
        LOG_DEBUG("Pool response: {}", message);
    } else if (message.find("\"error\"") != std::string::npos) {
        // Handle error responses
        LOG_ERROR("Pool error: {}", message);
    } else {
        LOG_DEBUG("Unknown pool message: {}", message);
    }
}

std::string Miner::extractJsonValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":\"";
    size_t start = json.find(searchKey);
    if (start == std::string::npos) {
        return "";
    }
    start += searchKey.length();
    size_t end = json.find("\"", start);
    if (end == std::string::npos) {
        return "";
    }
    return json.substr(start, end - start);
}

std::vector<uint8_t> Miner::hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

bool Miner::isValidMoneroAddress(const std::string& address) {
    // Monero address validation
    // Mainnet addresses:
    //   - Standard addresses start with '4' and are 95 characters long
    //   - Integrated addresses start with '4' and are 106 characters long
    //   - Subaddresses start with '8' and are 95 characters long
    // Testnet addresses:
    //   - Start with '9' and are 95 characters long
    
    if (address.empty()) {
        return false;
    }
    
    // Check length
    if (address.length() != 95 && address.length() != 106) {
        LOG_WARNING("Invalid Monero address length: {} (expected 95 or 106)", address.length());
        return false;
    }
    
    // Check prefix
    if (address[0] != '4' && address[0] != '8' && address[0] != '9') {
        LOG_WARNING("Invalid Monero address prefix: {} (expected '4', '8', or '9')", address[0]);
        return false;
    }
    
    // Validate prefix and length combination
    if (address[0] == '9' && address.length() != 95) {
        LOG_WARNING("Invalid testnet address length: {} (expected 95)", address.length());
        return false;
    }
    
    // Check if all characters are valid base58
    for (char c : address) {
        if (!((c >= '1' && c <= '9') || 
              (c >= 'A' && c <= 'H') || 
              (c >= 'J' && c <= 'N') || 
              (c >= 'P' && c <= 'Z') || 
              (c >= 'a' && c <= 'k') || 
              (c >= 'm' && c <= 'z'))) {
            LOG_WARNING("Invalid character in Monero address: {}", c);
            return false;
        }
    }
    
    std::string addressType;
    if (address[0] == '4') {
        addressType = address.length() == 95 ? "mainnet standard" : "mainnet integrated";
    } else if (address[0] == '8') {
        addressType = "mainnet subaddress";
    } else if (address[0] == '9') {
        addressType = "testnet";
    }
    
    LOG_DEBUG("Valid Monero {} address: {} (length: {})", addressType, address, address.length());
    return true;
}

std::string Miner::bytesToHex(const uint8_t* bytes, size_t length) {
    std::ostringstream hex;
    for (size_t i = 0; i < length; i++) {
        hex << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return hex.str();
}

void Miner::idleLoop() {
    LOG_INFO("Idle monitoring started");
    
    while (m_running) {
        // Check if system is idle
        bool isIdle = checkSystemIdle();
        
        if (isIdle && !m_miningActive) {
            m_idleTime++;
            if (m_idleTime >= 30) { // 30 seconds of idle time
                LOG_INFO("System idle for {} seconds, starting mining...", m_idleTime);
                startMining();
            }
        } else if (!isIdle && m_miningActive) {
            LOG_INFO("System activity detected, stopping mining...");
            stopMining();
            m_idleTime = 0;
        } else if (isIdle && m_miningActive) {
            m_idleTime++;
        } else {
            m_idleTime = 0;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    LOG_INFO("Idle monitoring stopped");
}

bool Miner::checkSystemIdle() {
    // Get system load average
    double loadavg[3];
    if (getloadavg(loadavg, 3) == -1) {
        return false;
    }
    
    // Check if load average is low (system idle)
    return loadavg[0] < 0.5; // Less than 0.5 average load
}

void Miner::startMining() {
    if (m_miningActive) {
        return;
    }
    
    m_miningActive = true;
    LOG_INFO("Starting mining threads...");
    
    // Start mining threads
    int numThreads = m_config.getMiningConfig().threads;
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
    }
    
    for (int i = 0; i < numThreads; i++) {
        m_miningThreads.emplace_back(&Miner::miningLoop, this, i);
    }
    
    LOG_INFO("Mining started with {} threads", numThreads);
}

void Miner::stopMining() {
    if (!m_miningActive) {
        return;
    }
    
    m_miningActive = false;
    LOG_INFO("Stopping mining threads...");
    
    // Wait for mining threads to finish
    for (auto& thread : m_miningThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_miningThreads.clear();
    
    LOG_INFO("Mining stopped");
}
