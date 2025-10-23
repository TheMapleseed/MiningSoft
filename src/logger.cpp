#include "logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <string>
#include <mutex>
#include <atomic>


// Global logger instance
std::unique_ptr<Logger> g_logger;

Logger::Logger() : m_level(Level::Info), m_console(true), m_file(false) {
    LOG_DEBUG("Logger constructor called");
}

Logger::~Logger() {
    if (m_fileStream && m_fileStream->is_open()) {
        m_fileStream->close();
    }
    LOG_DEBUG("Logger destructor called");
}

bool Logger::initialize(Level level, const std::string& logFile, bool console) {
    LOG_INFO("Initializing logger - Level: {}, File: {}, Console: {}", 
             static_cast<int>(level), logFile, console);
    
    m_level = level;
    m_console = console;
    m_file = !logFile.empty();
    m_logFile = logFile;
    
    if (m_file) {
        // Open log file
        m_fileStream = std::make_unique<std::ofstream>(logFile, std::ios::app);
        if (!m_fileStream->is_open()) {
            LOG_ERROR("Failed to open log file: {}", logFile);
            m_file = false;
            m_fileStream.reset();
        } else {
            LOG_INFO("Log file opened: {}", logFile);
        }
    }
    
    LOG_INFO("Logger initialized successfully");
    return true;
}

void Logger::setLevel(Level level) {
    m_level = level;
    LOG_DEBUG("Log level set to {}", static_cast<int>(level));
}

void Logger::debug(const std::string& message) {
    log(Level::Debug, message);
}

void Logger::info(const std::string& message) {
    log(Level::Info, message);
}

void Logger::warning(const std::string& message) {
    log(Level::Warning, message);
}

void Logger::error(const std::string& message) {
    log(Level::Error, message);
}

void Logger::critical(const std::string& message) {
    log(Level::Critical, message);
}

// Enhanced logging with categories
void Logger::log(Level level, Category category, const std::string& message) {
    std::string categoryStr = getCategoryString(category);
    std::string fullMessage = "[" + categoryStr + "] " + message;
    log(level, fullMessage);
}

void Logger::debug(Category category, const std::string& message) {
    log(Level::Debug, category, message);
}

void Logger::info(Category category, const std::string& message) {
    log(Level::Info, category, message);
}

void Logger::warning(Category category, const std::string& message) {
    log(Level::Warning, category, message);
}

void Logger::error(Category category, const std::string& message) {
    log(Level::Error, category, message);
}

void Logger::critical(Category category, const std::string& message) {
    log(Level::Critical, category, message);
}

// Structured logging
void Logger::logEvent(const std::string& event, const std::string& details, 
                     Level level, Category category) {
    std::string message = "EVENT: " + event;
    if (!details.empty()) {
        message += " - " + details;
    }
    log(level, category, message);
}

void Logger::logError(const std::string& error, const std::string& context, Category category) {
    std::string message = "ERROR: " + error;
    if (!context.empty()) {
        message += " (Context: " + context + ")";
    }
    log(Level::Error, category, message);
}

void Logger::logPerformance(const std::string& metric, double value, 
                           const std::string& unit, Category category) {
    std::ostringstream oss;
    oss << "PERF: " << metric << " = " << std::fixed << std::setprecision(2) << value;
    if (!unit.empty()) {
        oss << " " << unit;
    }
    log(Level::Info, category, oss.str());
}

// Template methods are implemented in header

void Logger::flush() {
    if (m_fileStream && m_fileStream->is_open()) {
        m_fileStream->flush();
    }
    std::cout.flush();
}

Logger::LogStats Logger::getStats() const {
    LogStats stats;
    stats.totalMessages = m_totalMessages.load();
    stats.debugMessages = m_debugMessages.load();
    stats.infoMessages = m_infoMessages.load();
    stats.warningMessages = m_warningMessages.load();
    stats.errorMessages = m_errorMessages.load();
    stats.criticalMessages = m_criticalMessages.load();
    stats.logFileSize = m_currentFileSize.load();
    stats.lastFlush = std::chrono::steady_clock::now();
    
    return stats;
}

std::string Logger::getCategoryString(Category category) {
    switch (category) {
        case Category::General: return "GEN";
        case Category::Mining: return "MIN";
        case Category::Network: return "NET";
        case Category::Wallet: return "WLT";
        case Category::Performance: return "PERF";
        case Category::Thermal: return "TEMP";
        case Category::Memory: return "MEM";
        case Category::RandomX: return "RX";
        case Category::Pool: return "POOL";
        case Category::CLI: return "CLI";
        case Category::Config: return "CFG";
        case Category::System: return "SYS";
        case Category::Test: return "TEST";
        default: return "UNK";
    }
}

void Logger::log(Level level, const std::string& message) {
    if (!shouldLog(level)) {
        return;
    }
    
    std::string formattedMessage = formatMessage(level, message);
    
    // Update statistics
    m_totalMessages++;
    switch (level) {
        case Level::Debug:
            m_debugMessages++;
            break;
        case Level::Info:
            m_infoMessages++;
            break;
        case Level::Warning:
            m_warningMessages++;
            break;
        case Level::Error:
            m_errorMessages++;
            break;
        case Level::Critical:
            m_criticalMessages++;
            break;
    }
    
    // Write to console
    if (m_console) {
        writeToConsole(formattedMessage, level);
    }
    
    // Write to file
    if (m_file && m_fileStream && m_fileStream->is_open()) {
        writeToFile(formattedMessage);
    }
    
    // Check for log rotation
    if (m_file && m_currentFileSize.load() > m_maxFileSize) {
        rotateLogFile();
    }
}


std::string Logger::formatMessage(Level level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    oss << " [" << levelToString(level) << "] " << message << std::endl;
    
    return oss.str();
}

std::string Logger::levelToString(Level level) const {
    switch (level) {
        case Level::Debug:    return "DEBUG";
        case Level::Info:     return "INFO ";
        case Level::Warning:  return "WARN ";
        case Level::Error:    return "ERROR";
        case Level::Critical: return "CRIT ";
        default:              return "UNKNOWN";
    }
}

std::string Logger::getLevelColor(Level level) const {
    switch (level) {
        case Level::Debug:    return COLOR_DEBUG;
        case Level::Info:     return COLOR_INFO;
        case Level::Warning:  return COLOR_WARNING;
        case Level::Error:    return COLOR_ERROR;
        case Level::Critical: return COLOR_CRITICAL;
        default:              return COLOR_RESET;
    }
}

bool Logger::shouldLog(Level level) const {
    return static_cast<int>(level) >= static_cast<int>(m_level);
}

void Logger::rotateLogFile() {
    if (!m_file || !m_fileStream) {
        return;
    }
    
    LOG_INFO("Rotating log file (size: {} bytes)", m_currentFileSize.load());
    
    // Close current file
    m_fileStream->close();
    
    // Rename existing files
    for (int i = m_maxFiles - 1; i > 0; i--) {
        std::string oldFile = m_logFile + "." + std::to_string(i);
        std::string newFile = m_logFile + "." + std::to_string(i + 1);
        
        if (std::filesystem::exists(oldFile)) {
            if (i == m_maxFiles - 1) {
                std::filesystem::remove(oldFile);
            } else {
                std::filesystem::rename(oldFile, newFile);
            }
        }
    }
    
    // Rename current file to .1
    if (std::filesystem::exists(m_logFile)) {
        std::filesystem::rename(m_logFile, m_logFile + ".1");
    }
    
    // Open new file
    m_fileStream->open(m_logFile, std::ios::out);
    m_currentFileSize = 0;
    
    LOG_INFO("Log file rotated successfully");
}

void Logger::writeToConsole(const std::string& message, Level level) {
    if (m_console) {
        std::cout << getLevelColor(level) << message << COLOR_RESET;
    }
}

void Logger::writeToFile(const std::string& message) {
    if (m_fileStream && m_fileStream->is_open()) {
        *m_fileStream << message;
        m_currentFileSize += message.length();
    }
}
