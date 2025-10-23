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

Miner::Miner() : m_running(false), m_connected(false), m_socket(-1), m_ssl(nullptr), m_sslContext(nullptr), m_idleTime(0), m_miningActive(false) {
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
        
    LOG_INFO("Miner initialized successfully");
    return true;
}

bool Miner::initializeRandomX() {
    LOG_INFO("Initializing RandomX algorithm");
    
    m_randomx = std::make_unique<RandomX>();
    if (!m_randomx->initialize()) {
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
    // Create login request
    std::ostringstream json;
    json << "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"login\",\"params\":{";
    json << "\"login\":\"" << m_config.getPoolConfig().username << "\",";
    json << "\"pass\":\"" << m_config.getPoolConfig().password << "\",";
    json << "\"agent\":\"MiningSoft/1.0\",";
    json << "\"algo\":[\"rx/0\"]";
    json << "}}";
    
    std::string request = json.str();
    LOG_DEBUG("Sending login request: {}", request);
    
    if (!sendData(request)) {
        return false;
    }
    
    // Read response
    std::string response;
    if (!receiveData(response)) {
        return false;
    }
    
    LOG_DEBUG("Received login response: {}", response);
    
    // Parse response (simplified - just check for success)
    if (response.find("\"result\"") != std::string::npos) {
        LOG_INFO("Login successful");
        return true;
    } else {
        LOG_ERROR("Login failed: {}", response);
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
        
        // Create nonce
        uint32_t nonce = m_currentJob.nonce + threadId;
        
        // Hash the job
        uint8_t hash[32];
        m_randomx->calculateHash(blob.data(), blob.size(), hash);
        
        // Check if hash meets target
        if (isValidShare(hash, m_currentJob.target)) {
            LOG_INFO("Valid share found by thread {}: nonce={}", threadId, nonce);
            submitShare(nonce, hash);
        }
        
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
        
        return m_randomx->isValidHash(hash, targetBytes.data());
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in isValidShare: {}", e.what());
        return false;
    }
}

void Miner::submitShare(uint32_t nonce, const uint8_t* hash) {
    // Convert hash to hex
    std::string hashHex = RandomX::bytesToHex(hash, 32);
    
    // Create submit request
    std::ostringstream json;
    json << "{\"id\":2,\"jsonrpc\":\"2.0\",\"method\":\"submit\",\"params\":{";
    json << "\"id\":\"" << m_config.getPoolConfig().workerId << "\",";
    json << "\"job_id\":\"" << m_currentJob.jobId << "\",";
    json << "\"nonce\":\"" << std::hex << nonce << "\",";
    json << "\"result\":\"" << hashHex << "\"";
    json << "}}";
    
    std::string request = json.str();
    LOG_DEBUG("Submitting share: {}", request);
    
    if (sendData(request)) {
        LOG_INFO("Share submitted successfully");
    } else {
        LOG_ERROR("Failed to submit share");
    }
}

void Miner::communicationLoop() {
    LOG_INFO("Communication thread started");
    
    while (m_running) {
        std::string response;
        if (receiveData(response)) {
            processPoolMessage(response);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LOG_INFO("Communication thread stopped");
}

void Miner::processPoolMessage(const std::string& message) {
    LOG_DEBUG("Received pool message: {}", message);
    
    // Parse job assignment (simplified)
    if (message.find("\"method\":\"job\"") != std::string::npos) {
        // Extract job parameters (simplified parsing)
        size_t blobPos = message.find("\"blob\":\"");
        size_t targetPos = message.find("\"target\":\"");
        size_t jobIdPos = message.find("\"job_id\":\"");
        
        if (blobPos != std::string::npos && targetPos != std::string::npos && jobIdPos != std::string::npos) {
            // Extract values (simplified)
            m_currentJob.blob = extractJsonValue(message, "blob");
            m_currentJob.target = extractJsonValue(message, "target");
            m_currentJob.jobId = extractJsonValue(message, "job_id");
            m_currentJob.nonce = 0;
            m_currentJob.isValid = true;
            
            LOG_INFO("New job received: {}", m_currentJob.jobId);
        }
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
