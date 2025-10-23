#include "miner.h"
#include "logger.h"
#include "cpu_throttle_manager.h"
#include "randomx_native.h"
#include "metal_gpu_simple.h"
#include "pool_connection.h"
#include "config_manager.h"
#include "performance_monitor.h"
#include <chrono>
#include <thread>
#include <random>
#include <mutex>
#include <cstring>

Miner::Miner() : m_startTime(std::chrono::steady_clock::now()) {
    LOG_DEBUG("Miner constructor called");
}

Miner::~Miner() {
    stop();
    LOG_DEBUG("Miner destructor called");
}

bool Miner::initialize(const std::string& configFile) {
    LOG_INFO("Initializing miner with config file: {}", configFile);
    
    try {
        // Initialize configuration manager
        m_configManager = std::make_shared<ConfigManager>();
        if (!m_configManager->loadFromFile(configFile)) {
            LOG_ERROR("Failed to load configuration from {}", configFile);
            return false;
        }
        
        return initializeInternal();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during miner initialization: {}", e.what());
        return false;
    }
}

bool Miner::initialize(std::shared_ptr<ConfigManager> configManager) {
    LOG_INFO("Initializing miner with provided configuration");
    
    try {
        // Use provided configuration manager
        m_configManager = configManager;
        
        return initializeInternal();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during miner initialization: {}", e.what());
        return false;
    }
}

bool Miner::initializeInternal() {
    try {
        
        // Initialize logger with configuration
        auto logConfig = m_configManager->getLoggingConfig();
        g_logger = std::make_unique<Logger>();
        if (!g_logger->initialize(
                logConfig.level == "debug" ? Logger::Level::Debug :
                logConfig.level == "warn" ? Logger::Level::Warning :
                logConfig.level == "error" ? Logger::Level::Error : Logger::Level::Info,
                logConfig.file, logConfig.console)) {
            LOG_ERROR("Failed to initialize logger");
            return false;
        }
        
        // Initialize native RandomX
        m_randomX = std::make_unique<RandomXNative>();
        if (!m_randomX->initialize({0x01, 0x02, 0x03, 0x04})) { // Placeholder seed
            LOG_ERROR("Failed to initialize native RandomX");
            return false;
        }
        
        // Initialize Metal GPU if enabled
        auto miningConfig = m_configManager->getMiningConfig();
        if (miningConfig.useGPU) {
            m_metalGPU = std::make_unique<MetalGPUSimple>();
            if (!m_metalGPU->initialize()) {
                LOG_WARNING("Failed to initialize Metal GPU, continuing with CPU only");
                m_metalGPU.reset();
            } else {
                LOG_INFO("Metal GPU initialized successfully");
            }
        }
        
        // Initialize CPU throttle manager
        m_cpuThrottleManager = std::make_unique<CPUThrottleManager>();
        if (!m_cpuThrottleManager->initialize()) {
            LOG_ERROR("Failed to initialize CPU throttle manager");
            return false;
        }
        
        // Set CPU usage thresholds for intelligent throttling
        m_cpuThrottleManager->setThresholds(20.0, 60.0, 80.0); // Low, High, Max thresholds
        
        // Initialize pool connection
        m_poolConnection = std::make_unique<PoolConnection>();
        auto poolConfig = m_configManager->getPoolConfig();
        if (!m_poolConnection->initialize(poolConfig.url, poolConfig.username, 
                                        poolConfig.password, poolConfig.workerId)) {
            LOG_ERROR("Failed to initialize pool connection");
            return false;
        }
        
        // Initialize performance monitor
        m_performanceMonitor = std::make_unique<PerformanceMonitor>();
        if (!m_performanceMonitor->initialize()) {
            LOG_ERROR("Failed to initialize performance monitor");
            return false;
        }
        
        LOG_INFO("Miner initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during miner initialization: {}", e.what());
        return false;
    }
}

void Miner::start() {
    if (m_running) {
        LOG_WARNING("Miner is already running");
        return;
    }
    
    LOG_INFO("Starting miner");
    
    m_running = true;
    m_shouldStop = false;
    m_startTime = std::chrono::steady_clock::now();
    
    // Start CPU demand monitoring
    m_cpuThrottleManager->start();
    
    // Start performance monitoring
    m_performanceMonitor->start();
    
    // Connect to pool
    auto result = m_poolConnection->connect();
    if (result != PoolConnection::ConnectionResult::Success) {
        LOG_WARNING("Failed to connect to pool, continuing with solo mining");
    }
    
    // Start mining thread
    m_miningThread = std::thread(&Miner::miningLoop, this);
    
    LOG_INFO("Miner started successfully");
}

void Miner::stop() {
    if (!m_running) {
        return;
    }
    
    LOG_INFO("Stopping miner");
    
    m_shouldStop = true;
    m_running = false;
    
    // Wait for mining thread to finish
    if (m_miningThread.joinable()) {
        m_miningThread.join();
    }
    
    // Stop monitoring
    if (m_cpuThrottleManager) {
        m_cpuThrottleManager->stop();
    }
    
    if (m_performanceMonitor) {
        m_performanceMonitor->stop();
    }
    
    // Disconnect from pool
    if (m_poolConnection) {
        m_poolConnection->disconnect();
    }
    
    LOG_INFO("Miner stopped");
}

double Miner::getHashrate() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_currentHashrate;
}

Miner::MiningStats Miner::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    MiningStats stats;
    stats.hashrate = m_currentHashrate;
    stats.totalHashes = m_totalHashes.load();
    stats.acceptedShares = m_acceptedShares.load();
    stats.rejectedShares = m_rejectedShares.load();
    stats.temperature = m_temperature;
    stats.powerConsumption = m_powerConsumption;
    stats.startTime = m_startTime;
    
    return stats;
}

void Miner::miningLoop() {
    LOG_INFO("Mining loop started");
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;
    
    auto lastHashTime = std::chrono::steady_clock::now();
    uint64_t hashCount = 0;
    
    while (!m_shouldStop && m_running) {
        try {
            // Get current job from pool
            auto job = m_poolConnection->getCurrentJob();
            if (!job.isValid) {
                // If no valid job from pool, create a test job for solo mining
                job.blob = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
                job.target = "00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
                job.jobId = "test-job";
                job.isValid = true;
            }
            
            // Check CPU demand-based throttling
            handleCPUThrottling();
            
            // Generate random nonce
            uint32_t nonce = dis(gen);
            
            // Prepare input for hashing
            std::vector<uint8_t> input;
            input.insert(input.end(), job.blob.begin(), job.blob.end());
            input.push_back((nonce >> 24) & 0xFF);
            input.push_back((nonce >> 16) & 0xFF);
            input.push_back((nonce >> 8) & 0xFF);
            input.push_back(nonce & 0xFF);
            
            // Hash the input
            std::vector<uint8_t> hash = m_randomX->hash(input);
            
            // Increment hash count
            m_totalHashes++;
            hashCount++;
            
            // Check if hash meets target
            uint64_t target = std::stoull(job.target, nullptr, 16);
            if (m_randomX->verifyHash(hash, target)) {
                LOG_INFO("Found valid share! Nonce: 0x{:08x}", nonce);
                
                // Submit share to pool
                std::string nonceStr = std::to_string(nonce);
                std::string hashStr = bytesToHex(hash);
                
                auto result = m_poolConnection->submitShare(job.jobId, nonceStr, hashStr);
                if (result.accepted) {
                    m_acceptedShares++;
                    LOG_INFO("Share accepted by pool");
                } else {
                    m_rejectedShares++;
                    LOG_WARNING("Share rejected by pool: {}", result.reason);
                }
            }
            
            // Update performance metrics periodically
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHashTime).count() >= 1) {
                updateMetrics();
                
                // Update performance monitor
                if (m_performanceMonitor) {
                    m_performanceMonitor->updateMiningMetrics(
                        hashCount, m_currentHashrate, m_temperature
                    );
                }
                
                lastHashTime = now;
                hashCount = 0;
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in mining loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    LOG_INFO("Mining loop ended");
}

bool Miner::processHash(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    if (!m_randomX) {
        return false;
    }
    
    output = m_randomX->hash(input);
    return !output.empty();
}

void Miner::submitShare(const std::vector<uint8_t>& nonce, const std::vector<uint8_t>& hash) {
    if (!m_poolConnection) {
        return;
    }
    
    // Convert nonce and hash to strings
    std::string nonceStr = bytesToHex(nonce);
    std::string hashStr = bytesToHex(hash);
    
    // Get current job
    auto job = m_poolConnection->getCurrentJob();
    if (!job.isValid) {
        LOG_WARNING("No valid job available for share submission");
        return;
    }
    
    // Submit share
    auto result = m_poolConnection->submitShare(job.jobId, nonceStr, hashStr);
    if (result.accepted) {
        m_acceptedShares++;
        LOG_INFO("Share accepted by pool");
    } else {
        m_rejectedShares++;
        LOG_WARNING("Share rejected by pool: {}", result.reason);
    }
}

void Miner::updateMetrics() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    // Calculate hashrate (simplified)
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();
    if (duration > 0) {
        m_currentHashrate = static_cast<double>(m_totalHashes.load()) / duration;
    }
    
    // Get temperature from CPU throttle manager
    if (m_cpuThrottleManager) {
        m_temperature = 45.0; // Simplified temperature reading
    }
    
    // Estimate power consumption (simplified)
    m_powerConsumption = m_temperature * 0.1; // Rough estimate
}

void Miner::handleCPUThrottling() {
    if (!m_cpuThrottleManager) {
        return;
    }
    
    if (m_cpuThrottleManager->isThrottling()) {
        double throttleLevel = m_cpuThrottleManager->getThrottleLevel();
        double cpuUsage = m_cpuThrottleManager->getCPUUsage();
        LOG_DEBUG("CPU throttling active - Usage: {:.1f}%, Throttle: {:.1f}%", 
                 cpuUsage, throttleLevel * 100);
        
        // Reduce mining intensity based on CPU demand
        if (throttleLevel > 0.3) {
            // Reduce GPU workload when CPU is busy
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(throttleLevel * 200)));
        }
    }
}

double Miner::getCPUUsage() const {
    if (m_cpuThrottleManager) {
        return m_cpuThrottleManager->getCPUUsage();
    }
    return 0.0;
}

// Helper function to convert bytes to hex string
std::string Miner::bytesToHex(const std::vector<uint8_t>& bytes) {
    std::string hex;
    hex.reserve(bytes.size() * 2);
    
    for (uint8_t byte : bytes) {
        char hexByte[3];
        snprintf(hexByte, sizeof(hexByte), "%02x", byte);
        hex += hexByte;
    }
    
    return hex;
}
