#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

/**
 * Configuration manager for the Monero miner
 * Handles loading, saving, and validation of configuration settings
 */
class ConfigManager {
public:
    ConfigManager();
    ConfigManager(const ConfigManager& other);
    ~ConfigManager();

    // Load configuration from file
    bool loadFromFile(const std::string& filename);
    
    // Save configuration to file
    bool saveToFile(const std::string& filename) const;
    
    // Load configuration from command line arguments
    bool loadFromArgs(int argc, char* argv[]);
    
    // Get configuration value
    template<typename T>
    T getValue(const std::string& key, const T& defaultValue = T{}) const;
    
    // Set configuration value
    template<typename T>
    void setValue(const std::string& key, const T& value);
    
    // Check if key exists
    bool hasKey(const std::string& key) const;
    
    // Get all configuration keys
    std::vector<std::string> getKeys() const;
    
    // Validate configuration
    bool validate() const;
    
    // Get validation errors
    std::vector<std::string> getValidationErrors() const;
    
    // Reset to default configuration
    void resetToDefaults();

    // Configuration sections
    struct MiningConfig {
        std::string algorithm{"randomx"};
        int threads{0}; // 0 = auto-detect
        bool useGPU{true};
        bool useHugePages{false};
        int intensity{100}; // 0-100
    };
    
    struct PoolConfig {
        std::string url;
        std::string username;
        std::string password;
        std::string workerId;
        int port{0}; // 0 = auto-detect
        bool ssl{false};
        int timeout{30};
        int keepAlive{60};
    };
    
    struct ThermalConfig {
        double maxCpuTemp{85.0};
        double maxGpuTemp{90.0};
        double maxSystemTemp{80.0};
        bool enableThrottling{true};
        int monitoringInterval{1000}; // milliseconds
    };
    
    struct LoggingConfig {
        std::string level{"info"}; // debug, info, warn, error
        std::string file{""}; // empty = console only
        bool console{true};
        bool fileOutput{false};
        int maxFileSize{10485760}; // 10MB
        int maxFiles{5};
    };
    
    struct PerformanceConfig {
        bool enableMetrics{true};
        int metricsInterval{5000}; // milliseconds
        bool enableProfiling{false};
        std::string profileFile{"profile.json"};
    };

    // Get structured configuration
    const MiningConfig& getMiningConfig() const { return m_miningConfig; }
    const PoolConfig& getPoolConfig() const { return m_poolConfig; }
    const ThermalConfig& getThermalConfig() const { return m_thermalConfig; }
    const LoggingConfig& getLoggingConfig() const { return m_loggingConfig; }
    const PerformanceConfig& getPerformanceConfig() const { return m_performanceConfig; }
    
    // Set structured configuration
    void setMiningConfig(const MiningConfig& config) { m_miningConfig = config; }
    void setPoolConfig(const PoolConfig& config) { m_poolConfig = config; }
    void setThermalConfig(const ThermalConfig& config) { m_thermalConfig = config; }
    void setLoggingConfig(const LoggingConfig& config) { m_loggingConfig = config; }
    void setPerformanceConfig(const PerformanceConfig& config) { m_performanceConfig = config; }

private:
    // Parse JSON configuration
    bool parseJsonConfig(const std::string& jsonData);
    
    // Parse command line arguments
    bool parseCommandLineArgs(int argc, char* argv[]);
    
    // Validate mining configuration
    bool validateMiningConfig() const;
    
    // Validate pool configuration
    bool validatePoolConfig() const;
    
    // Validate thermal configuration
    bool validateThermalConfig() const;
    
    // Validate logging configuration
    bool validateLoggingConfig() const;
    
    // Validate performance configuration
    bool validatePerformanceConfig() const;
    
    // Convert value to string
    template<typename T>
    std::string valueToString(const T& value) const;
    
    // Convert string to value
    template<typename T>
    T stringToValue(const std::string& str) const;

private:
    std::map<std::string, std::string> m_config;
    std::vector<std::string> m_validationErrors;
    
    // Structured configuration
    MiningConfig m_miningConfig;
    PoolConfig m_poolConfig;
    ThermalConfig m_thermalConfig;
    LoggingConfig m_loggingConfig;
    PerformanceConfig m_performanceConfig;
    
    // Default configuration file path
    std::string m_defaultConfigFile{"config.json"};
};
