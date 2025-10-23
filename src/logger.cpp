#include "logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <string>


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
