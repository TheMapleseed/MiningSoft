#include "config_manager.h"
#include "logger.h"
#include "simple_json.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>

ConfigManager::ConfigManager() {
    // Set default values
    resetToDefaults();
}

ConfigManager::ConfigManager(const ConfigManager& other) {
    LOG_DEBUG("ConfigManager copy constructor called");
    m_miningConfig = other.m_miningConfig;
    m_poolConfig = other.m_poolConfig;
    m_loggingConfig = other.m_loggingConfig;
    m_performanceConfig = other.m_performanceConfig;
    m_validationErrors = other.m_validationErrors;
}

ConfigManager::~ConfigManager() = default;

bool ConfigManager::loadFromFile(const std::string& filename) {
    LOG_INFO("Loading configuration from file: {}", filename);
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open configuration file: {}", filename);
        return false;
    }
    
    std::string jsonData((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    if (!parseJsonConfig(jsonData)) {
        LOG_ERROR("Failed to parse configuration file");
        return false;
    }
    
    if (!validate()) {
        LOG_ERROR("Configuration validation failed");
        return false;
    }
    
    LOG_INFO("Configuration loaded successfully from {}", filename);
    return true;
}

bool ConfigManager::saveToFile(const std::string& filename) const {
    LOG_INFO("Saving configuration to file: {}", filename);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Failed to create configuration file: {}", filename);
        return false;
    }
    
    // Generate JSON configuration
    std::ostringstream json;
    json << "{\n";
    json << "  \"mining\": {\n";
    json << "    \"algorithm\": \"" << m_miningConfig.algorithm << "\",\n";
    json << "    \"threads\": " << m_miningConfig.threads << ",\n";
    json << "    \"useGPU\": " << (m_miningConfig.useGPU ? "true" : "false") << ",\n";
    json << "    \"useHugePages\": " << (m_miningConfig.useHugePages ? "true" : "false") << ",\n";
    json << "    \"intensity\": " << m_miningConfig.intensity << "\n";
    json << "  },\n";
    json << "  \"pool\": {\n";
    json << "    \"url\": \"" << m_poolConfig.url << "\",\n";
    json << "    \"username\": \"" << m_poolConfig.username << "\",\n";
    json << "    \"password\": \"" << m_poolConfig.password << "\",\n";
    json << "    \"workerId\": \"" << m_poolConfig.workerId << "\",\n";
    json << "    \"port\": " << m_poolConfig.port << ",\n";
    json << "    \"ssl\": " << (m_poolConfig.ssl ? "true" : "false") << ",\n";
    json << "    \"timeout\": " << m_poolConfig.timeout << ",\n";
    json << "    \"keepAlive\": " << m_poolConfig.keepAlive << "\n";
    json << "  },\n";
    json << "  \"thermal\": {\n";
    json << "    \"maxCpuTemp\": " << m_thermalConfig.maxCpuTemp << ",\n";
    json << "    \"maxGpuTemp\": " << m_thermalConfig.maxGpuTemp << ",\n";
    json << "    \"maxSystemTemp\": " << m_thermalConfig.maxSystemTemp << ",\n";
    json << "    \"enableThrottling\": " << (m_thermalConfig.enableThrottling ? "true" : "false") << ",\n";
    json << "    \"monitoringInterval\": " << m_thermalConfig.monitoringInterval << "\n";
    json << "  },\n";
    json << "  \"logging\": {\n";
    json << "    \"level\": \"" << m_loggingConfig.level << "\",\n";
    json << "    \"file\": \"" << m_loggingConfig.file << "\",\n";
    json << "    \"console\": " << (m_loggingConfig.console ? "true" : "false") << ",\n";
    json << "    \"maxFileSize\": " << m_loggingConfig.maxFileSize << ",\n";
    json << "    \"maxFiles\": " << m_loggingConfig.maxFiles << "\n";
    json << "  },\n";
    json << "  \"performance\": {\n";
    json << "    \"enableMetrics\": " << (m_performanceConfig.enableMetrics ? "true" : "false") << ",\n";
    json << "    \"metricsInterval\": " << m_performanceConfig.metricsInterval << ",\n";
    json << "    \"enableProfiling\": " << (m_performanceConfig.enableProfiling ? "true" : "false") << ",\n";
    json << "    \"profileFile\": \"" << m_performanceConfig.profileFile << "\"\n";
    json << "  }\n";
    json << "}\n";
    
    file << json.str();
    file.close();
    
    LOG_INFO("Configuration saved successfully to {}", filename);
    return true;
}

bool ConfigManager::loadFromArgs(int argc, char* argv[]) {
    LOG_INFO("Loading configuration from command line arguments");
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                return loadFromFile(argv[++i]);
            } else {
                LOG_ERROR("Missing config file argument");
                return false;
            }
        } else if (arg == "-p" || arg == "--pool") {
            if (i + 1 < argc) {
                m_poolConfig.url = argv[++i];
            }
        } else if (arg == "-u" || arg == "--user") {
            if (i + 1 < argc) {
                m_poolConfig.username = argv[++i];
            }
        } else if (arg == "-w" || arg == "--pass") {
            if (i + 1 < argc) {
                m_poolConfig.password = argv[++i];
            }
        } else if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) {
                m_miningConfig.threads = std::stoi(argv[++i]);
            }
        } else if (arg == "--gpu") {
            m_miningConfig.useGPU = true;
        } else if (arg == "--no-gpu") {
            m_miningConfig.useGPU = false;
        } else if (arg == "--intensity") {
            if (i + 1 < argc) {
                m_miningConfig.intensity = std::stoi(argv[++i]);
                m_miningConfig.intensity = std::max(0, std::min(100, m_miningConfig.intensity));
            }
        } else if (arg == "--thermal-limit") {
            if (i + 1 < argc) {
                m_thermalConfig.maxCpuTemp = std::stod(argv[++i]);
            }
        } else if (arg == "--log-level") {
            if (i + 1 < argc) {
                m_loggingConfig.level = argv[++i];
            }
        } else if (arg == "--log-file") {
            if (i + 1 < argc) {
                m_loggingConfig.file = argv[++i];
            }
        } else if (arg == "--help" || arg == "-h") {
            m_config["help"] = "true";
        } else if (arg == "--version" || arg == "-v") {
            m_config["version"] = "true";
        }
    }
    
    if (!validate()) {
        LOG_ERROR("Configuration validation failed");
        return false;
    }
    
    LOG_INFO("Configuration loaded from command line arguments");
    return true;
}

template<typename T>
T ConfigManager::getValue(const std::string& key, const T& defaultValue) const {
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        return stringToValue<T>(it->second);
    }
    return defaultValue;
}

template<typename T>
void ConfigManager::setValue(const std::string& key, const T& value) {
    m_config[key] = valueToString(value);
}

bool ConfigManager::hasKey(const std::string& key) const {
    return m_config.find(key) != m_config.end();
}

std::vector<std::string> ConfigManager::getKeys() const {
    std::vector<std::string> keys;
    for (const auto& pair : m_config) {
        keys.push_back(pair.first);
    }
    return keys;
}

bool ConfigManager::validate() const {
    // Clear validation errors (mutable member)
    const_cast<std::vector<std::string>&>(m_validationErrors).clear();
    
    bool valid = true;
    
    if (!validateMiningConfig()) valid = false;
    if (!validatePoolConfig()) valid = false;
    if (!validateThermalConfig()) valid = false;
    if (!validateLoggingConfig()) valid = false;
    if (!validatePerformanceConfig()) valid = false;
    
    return valid;
}

std::vector<std::string> ConfigManager::getValidationErrors() const {
    return m_validationErrors;
}

void ConfigManager::resetToDefaults() {
    m_config.clear();
    
    // Set default mining configuration
    m_miningConfig.algorithm = "randomx";
    m_miningConfig.threads = 0; // Auto-detect
    m_miningConfig.useGPU = true;
    m_miningConfig.useHugePages = false;
    m_miningConfig.intensity = 100;
    
    // Set default pool configuration
    m_poolConfig.url = "";
    m_poolConfig.username = "";
    m_poolConfig.password = "";
    m_poolConfig.workerId = "";
    m_poolConfig.port = 0;
    m_poolConfig.ssl = false;
    m_poolConfig.timeout = 30;
    m_poolConfig.keepAlive = 60;
    
    // Set default thermal configuration
    m_thermalConfig.maxCpuTemp = 85.0;
    m_thermalConfig.maxGpuTemp = 90.0;
    m_thermalConfig.maxSystemTemp = 80.0;
    m_thermalConfig.enableThrottling = true;
    m_thermalConfig.monitoringInterval = 1000;
    
    // Set default logging configuration
    m_loggingConfig.level = "info";
    m_loggingConfig.file = "";
    m_loggingConfig.console = true;
    m_loggingConfig.maxFileSize = 10485760; // 10MB
    m_loggingConfig.maxFiles = 5;
    
    // Set default performance configuration
    m_performanceConfig.enableMetrics = true;
    m_performanceConfig.metricsInterval = 5000;
    m_performanceConfig.enableProfiling = false;
    m_performanceConfig.profileFile = "profile.json";
}

bool ConfigManager::parseJsonConfig(const std::string& jsonData) {
    LOG_DEBUG("Parsing JSON configuration");
    
    SimpleJSON json;
    if (!json.parse(jsonData)) {
        LOG_ERROR("Failed to parse JSON configuration");
        return false;
    }
    
    // Parse mining configuration
    if (json.hasKey("mining")) {
        // This is a simplified approach - in practice, you'd parse nested objects
        m_miningConfig.algorithm = json.getString("mining.algorithm", "randomx");
        m_miningConfig.threads = json.getInt("mining.threads", 0);
        m_miningConfig.useGPU = json.getBool("mining.useGPU", true);
        m_miningConfig.useHugePages = json.getBool("mining.useHugePages", false);
        m_miningConfig.intensity = json.getInt("mining.intensity", 100);
    }
    
    // Parse pool configuration
    if (json.hasKey("pool")) {
        m_poolConfig.url = json.getString("pool.url", "");
        m_poolConfig.username = json.getString("pool.username", "");
        m_poolConfig.password = json.getString("pool.password", "");
        m_poolConfig.workerId = json.getString("pool.workerId", "");
        m_poolConfig.port = json.getInt("pool.port", 0);
        m_poolConfig.ssl = json.getBool("pool.ssl", false);
        m_poolConfig.timeout = json.getInt("pool.timeout", 30);
        m_poolConfig.keepAlive = json.getInt("pool.keepAlive", 60);
    }
    
    // Parse CPU throttling configuration
    if (json.hasKey("cpuThrottling")) {
        m_thermalConfig.maxCpuTemp = json.getDouble("cpuThrottling.lowThreshold", 20.0);
        m_thermalConfig.maxGpuTemp = json.getDouble("cpuThrottling.highThreshold", 60.0);
        m_thermalConfig.maxSystemTemp = json.getDouble("cpuThrottling.maxThreshold", 80.0);
        m_thermalConfig.enableThrottling = json.getBool("cpuThrottling.enableThrottling", true);
        m_thermalConfig.monitoringInterval = json.getInt("cpuThrottling.monitoringInterval", 500);
    }
    
    // Parse logging configuration
    if (json.hasKey("logging")) {
        m_loggingConfig.level = json.getString("logging.level", "info");
        m_loggingConfig.file = json.getString("logging.file", "");
        m_loggingConfig.console = json.getBool("logging.console", true);
        m_loggingConfig.maxFileSize = json.getInt("logging.maxFileSize", 10485760);
        m_loggingConfig.maxFiles = json.getInt("logging.maxFiles", 5);
    }
    
    // Parse performance configuration
    if (json.hasKey("performance")) {
        m_performanceConfig.enableMetrics = json.getBool("performance.enableMetrics", true);
        m_performanceConfig.metricsInterval = json.getInt("performance.metricsInterval", 5000);
        m_performanceConfig.enableProfiling = json.getBool("performance.enableProfiling", false);
        m_performanceConfig.profileFile = json.getString("performance.profileFile", "profile.json");
    }
    
    LOG_DEBUG("JSON configuration parsed successfully");
    return true;
}

bool ConfigManager::validateMiningConfig() const {
    bool valid = true;
    
    if (m_miningConfig.threads < 0) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("Thread count cannot be negative");
        valid = false;
    }
    
    if (m_miningConfig.intensity < 0 || m_miningConfig.intensity > 100) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("Intensity must be between 0 and 100");
        valid = false;
    }
    
    return valid;
}

bool ConfigManager::validatePoolConfig() const {
    bool valid = true;
    
    if (m_poolConfig.url.empty()) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("Pool URL is required");
        valid = false;
    }
    
    if (m_poolConfig.username.empty()) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("Pool username is required");
        valid = false;
    }
    
    if (m_poolConfig.password.empty()) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("Pool password is required");
        valid = false;
    }
    
    return valid;
}

bool ConfigManager::validateThermalConfig() const {
    bool valid = true;
    
    if (m_thermalConfig.maxCpuTemp <= 0 || m_thermalConfig.maxCpuTemp > 150) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("CPU temperature limit must be between 0 and 150°C");
        valid = false;
    }
    
    if (m_thermalConfig.maxGpuTemp <= 0 || m_thermalConfig.maxGpuTemp > 150) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("GPU temperature limit must be between 0 and 150°C");
        valid = false;
    }
    
    if (m_thermalConfig.maxSystemTemp <= 0 || m_thermalConfig.maxSystemTemp > 150) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("System temperature limit must be between 0 and 150°C");
        valid = false;
    }
    
    return valid;
}

bool ConfigManager::validateLoggingConfig() const {
    bool valid = true;
    
    std::vector<std::string> validLevels = {"debug", "info", "warn", "error"};
    if (std::find(validLevels.begin(), validLevels.end(), m_loggingConfig.level) == validLevels.end()) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("Invalid log level. Must be one of: debug, info, warn, error");
        valid = false;
    }
    
    if (m_loggingConfig.maxFileSize <= 0) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("Max file size must be positive");
        valid = false;
    }
    
    if (m_loggingConfig.maxFiles <= 0) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("Max files must be positive");
        valid = false;
    }
    
    return valid;
}

bool ConfigManager::validatePerformanceConfig() const {
    bool valid = true;
    
    if (m_performanceConfig.metricsInterval <= 0) {
        const_cast<std::vector<std::string>&>(m_validationErrors).push_back("Metrics interval must be positive");
        valid = false;
    }
    
    return valid;
}

template<typename T>
std::string ConfigManager::valueToString(const T& value) const {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

template<typename T>
T ConfigManager::stringToValue(const std::string& str) const {
    std::istringstream iss(str);
    T value;
    iss >> value;
    return value;
}

// Explicit template instantiations
template std::string ConfigManager::getValue<std::string>(const std::string&, const std::string&) const;
template int ConfigManager::getValue<int>(const std::string&, const int&) const;
template double ConfigManager::getValue<double>(const std::string&, const double&) const;
template bool ConfigManager::getValue<bool>(const std::string&, const bool&) const;

template void ConfigManager::setValue<std::string>(const std::string&, const std::string&);
template void ConfigManager::setValue<int>(const std::string&, const int&);
template void ConfigManager::setValue<double>(const std::string&, const double&);
template void ConfigManager::setValue<bool>(const std::string&, const bool&);
