#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iostream>

/**
 * Thread-safe logging system for the Monero miner
 * Supports multiple log levels and output destinations
 */
class Logger {
public:
    enum class Level {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Critical = 4
    };
    
    enum class Category {
        General,
        Mining,
        Network,
        Wallet,
        Performance,
        Thermal,
        Memory,
        RandomX,
        Pool,
        CLI,
        Config,
        System,
        Test
    };

    Logger();
    ~Logger();

    // Initialize logger with configuration
    bool initialize(Level level = Level::Info, 
                   const std::string& logFile = "", 
                   bool console = true);
    
    // Set log level
    void setLevel(Level level);
    
    // Get current log level
    Level getLevel() const { return m_level; }
    
    // Log messages
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void critical(const std::string& message);
    
    // Enhanced logging with categories
    void log(Level level, Category category, const std::string& message);
    void debug(Category category, const std::string& message);
    void info(Category category, const std::string& message);
    void warning(Category category, const std::string& message);
    void error(Category category, const std::string& message);
    void critical(Category category, const std::string& message);
    
    // Structured logging
    void logEvent(const std::string& event, const std::string& details = "", 
                  Level level = Level::Info, Category category = Category::General);
    void logError(const std::string& error, const std::string& context = "", 
                  Category category = Category::General);
    void logPerformance(const std::string& metric, double value, 
                       const std::string& unit = "", Category category = Category::Performance);
    
    // Logging with format string support
    template<typename... Args>
    void debug(const std::string& format, const Args&... args) {
        log(Level::Debug, format, args...);
    }
    
    template<typename... Args>
    void info(const std::string& format, const Args&... args) {
        log(Level::Info, format, args...);
    }
    
    template<typename... Args>
    void warning(const std::string& format, const Args&... args) {
        log(Level::Warning, format, args...);
    }
    
    template<typename... Args>
    void error(const std::string& format, const Args&... args) {
        log(Level::Error, format, args...);
    }
    
    template<typename... Args>
    void critical(const std::string& format, const Args&... args) {
        log(Level::Critical, format, args...);
    }
    
    // Flush log output
    void flush();
    
    // Get log statistics
    struct LogStats {
        uint64_t totalMessages;
        uint64_t debugMessages;
        uint64_t infoMessages;
        uint64_t warningMessages;
        uint64_t errorMessages;
        uint64_t criticalMessages;
        size_t logFileSize;
        std::chrono::steady_clock::time_point lastFlush;
    };
    
    LogStats getStats() const;
    
    // Helper function for category strings
    std::string getCategoryString(Category category);

private:
    // Internal log method
    void log(Level level, const std::string& message);
    
    // Template log method with format string support
    template<typename... Args>
    void log(Level level, const std::string& format, const Args&... args) {
        if (level < m_level) return;
        
        std::ostringstream oss;
        formatMessage(oss, format, args...);
        
        if (m_console) {
            std::cout << getLevelColor(level) << oss.str() << COLOR_RESET << std::endl;
        }
        
        if (m_file && m_fileStream && m_fileStream->is_open()) {
            *m_fileStream << oss.str() << std::endl;
            m_fileStream->flush();
        }
    }
    
    // Format message with variadic arguments
    template<typename... Args>
    void formatMessage(std::ostringstream& oss, const std::string& format, const Args&... args) {
        formatMessageImpl(oss, format, args...);
    }
    
    // Helper function to format arguments one by one
    template<typename T, typename... Args>
    void formatMessageImpl(std::ostringstream& oss, const std::string& format, const T& first, const Args&... rest) {
        size_t pos = 0;
        while (pos < format.length()) {
            size_t placeholder = format.find("{}", pos);
            if (placeholder == std::string::npos) {
                oss << format.substr(pos);
                break;
            }
            oss << format.substr(pos, placeholder - pos);
            oss << first;
            pos = placeholder + 2;
            if constexpr (sizeof...(rest) > 0) {
                formatMessageImpl(oss, format.substr(pos), rest...);
            } else {
                oss << format.substr(pos);
            }
            break;
        }
    }
    
    template<typename... Args>
    void formatMessageImpl(std::ostringstream& oss, const std::string& format) {
        oss << format;
    }
    
    // Format log message
    std::string formatMessage(Level level, const std::string& message);
    
    // Get level string
    std::string levelToString(Level level) const;
    
    // Get level color for console output
    std::string getLevelColor(Level level) const;
    
    // Check if level should be logged
    bool shouldLog(Level level) const;
    
    // Rotate log file if necessary
    void rotateLogFile();
    
    // Write to console with color
    void writeToConsole(const std::string& message, Level level);
    
    // Write to file
    void writeToFile(const std::string& message);

private:
    Level m_level{Level::Info};
    bool m_console{true};
    bool m_file{false};
    
    std::string m_logFile;
    std::unique_ptr<std::ofstream> m_fileStream;
    
    mutable std::mutex m_mutex;
    
    // Statistics
    mutable std::atomic<uint64_t> m_totalMessages{0};
    mutable std::atomic<uint64_t> m_debugMessages{0};
    mutable std::atomic<uint64_t> m_infoMessages{0};
    mutable std::atomic<uint64_t> m_warningMessages{0};
    mutable std::atomic<uint64_t> m_errorMessages{0};
    mutable std::atomic<uint64_t> m_criticalMessages{0};
    
    // File rotation
    size_t m_maxFileSize{10485760}; // 10MB
    int m_maxFiles{5};
    mutable std::atomic<size_t> m_currentFileSize{0};
    
    // Colors for console output
    static constexpr const char* COLOR_RESET = "\033[0m";
    static constexpr const char* COLOR_DEBUG = "\033[36m";    // Cyan
    static constexpr const char* COLOR_INFO = "\033[32m";     // Green
    static constexpr const char* COLOR_WARNING = "\033[33m";  // Yellow
    static constexpr const char* COLOR_ERROR = "\033[31m";    // Red
    static constexpr const char* COLOR_CRITICAL = "\033[35m"; // Magenta
};

// Global logger instance
extern std::unique_ptr<Logger> g_logger;

// Convenience macros
#define LOG_DEBUG(...) if (g_logger) g_logger->debug(__VA_ARGS__)
#define LOG_INFO(...) if (g_logger) g_logger->info(__VA_ARGS__)
#define LOG_WARNING(...) if (g_logger) g_logger->warning(__VA_ARGS__)
#define LOG_ERROR(...) if (g_logger) g_logger->error(__VA_ARGS__)
#define LOG_CRITICAL(...) if (g_logger) g_logger->critical(__VA_ARGS__)
