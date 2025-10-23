#include "error_handler.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>

ErrorHandler& ErrorHandler::getInstance() {
    static ErrorHandler instance;
    return instance;
}

bool ErrorHandler::initialize() {
    m_logger = std::make_unique<Logger>();
    if (!m_logger->initialize(Logger::Level::Debug, "error.log", true)) {
        std::cerr << "Failed to initialize error handler logger" << std::endl;
        return false;
    }
    
    // Register default recovery strategies
    registerRecoveryStrategy(ErrorCategory::NETWORK, 
                            std::make_unique<NetworkRecoveryStrategy>());
    registerRecoveryStrategy(ErrorCategory::MEMORY, 
                            std::make_unique<MemoryRecoveryStrategy>());
    registerRecoveryStrategy(ErrorCategory::THERMAL, 
                            std::make_unique<ThermalRecoveryStrategy>());
    
    // Set default error thresholds
    setErrorThreshold(ErrorSeverity::ERROR, 10, std::chrono::seconds(60));
    setErrorThreshold(ErrorSeverity::CRITICAL, 5, std::chrono::seconds(60));
    setErrorThreshold(ErrorSeverity::FATAL, 1, std::chrono::seconds(60));
    
    m_logger->info(Logger::Category::System, "Error handler initialized");
    return true;
}

void ErrorHandler::shutdown() {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    
    m_logger->info(Logger::Category::System, "Error handler shutting down");
    m_logger->info(Logger::Category::System, "Total errors handled: " + std::to_string(m_totalErrors.load()));
    m_logger->info(Logger::Category::System, "Errors recovered: " + std::to_string(m_recoveredErrors.load()));
    
    m_recoveryStrategies.clear();
    m_errorHistory.clear();
    m_suppressedErrors.clear();
}

void ErrorHandler::reportError(const std::string& message, ErrorSeverity severity, 
                              ErrorCategory category, const ErrorContext& context) {
    ErrorInfo error;
    error.errorId = generateErrorId();
    error.message = message;
    error.severity = severity;
    error.category = category;
    error.context = context;
    error.timestamp = std::chrono::steady_clock::now();
    error.stackTrace = getStackTrace();
    error.recoverable = (severity <= ErrorSeverity::ERROR);
    error.recoveryAction = "";
    
    {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        
        // Check if error is suppressed
        if (isErrorSuppressed(error.errorId)) {
            return;
        }
        
        // Add to error history
        m_errorHistory.push_back(error);
        
        // Cleanup old errors (keep last 1000)
        if (m_errorHistory.size() > 1000) {
            m_errorHistory.erase(m_errorHistory.begin(), 
                               m_errorHistory.begin() + (m_errorHistory.size() - 1000));
        }
        
        // Update statistics
        updateErrorStats(error);
        
        // Check error thresholds
        if (checkErrorThreshold(severity)) {
            m_logger->critical(Logger::Category::System, 
                             "Error threshold exceeded for severity: " + 
                             std::to_string(static_cast<int>(severity)));
        }
    }
    
    // Log the error
    logError(error);
    
    // Attempt recovery if possible
    if (error.recoverable) {
        attemptRecovery(error);
    }
    
    // Handle fatal errors
    if (severity == ErrorSeverity::FATAL) {
        emergencyShutdown("Fatal error: " + message);
    }
}

void ErrorHandler::reportError(const MiningSoftException& exception, const ErrorContext& context) {
    reportError(exception.what(), exception.getSeverity(), exception.getCategory(), context);
}

void ErrorHandler::registerRecoveryStrategy(ErrorCategory category, 
                                           std::unique_ptr<ErrorRecoveryStrategy> strategy) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_recoveryStrategies[category] = std::move(strategy);
}

bool ErrorHandler::attemptRecovery(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    
    auto it = m_recoveryStrategies.find(error.category);
    if (it != m_recoveryStrategies.end()) {
        if (it->second->canRecover(error)) {
            m_logger->info(Logger::Category::System, 
                          "Attempting recovery for error: " + error.errorId);
            
            if (it->second->executeRecovery(error)) {
                m_recoveredErrors++;
                m_logger->info(Logger::Category::System, 
                              "Successfully recovered from error: " + error.errorId);
                return true;
            } else {
                m_logger->error(Logger::Category::System, 
                               "Recovery failed for error: " + error.errorId);
            }
        }
    }
    
    return false;
}

void ErrorHandler::setErrorThreshold(ErrorSeverity severity, int maxCount, 
                                    std::chrono::seconds timeWindow) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_errorThresholds[severity] = {maxCount, timeWindow, {}};
}

bool ErrorHandler::isErrorThresholdExceeded(ErrorSeverity severity) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return checkErrorThreshold(severity);
}

ErrorHandler::ErrorStats ErrorHandler::getErrorStats() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    
    ErrorStats stats;
    stats.totalErrors = m_totalErrors.load();
    stats.recoveredErrors = m_recoveredErrors.load();
    
    for (const auto& error : m_errorHistory) {
        stats.errorCounts[error.severity]++;
        stats.categoryCounts[error.category]++;
    }
    
    if (!m_errorHistory.empty()) {
        stats.lastError = m_errorHistory.back().timestamp;
    }
    
    return stats;
}

std::vector<ErrorInfo> ErrorHandler::getRecentErrors(int count) const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    
    int start = std::max(0, static_cast<int>(m_errorHistory.size()) - count);
    return std::vector<ErrorInfo>(m_errorHistory.begin() + start, m_errorHistory.end());
}

void ErrorHandler::suppressError(const std::string& errorId) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_suppressedErrors[errorId] = true;
}

void ErrorHandler::unsuppressError(const std::string& errorId) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_suppressedErrors.erase(errorId);
}

bool ErrorHandler::isErrorSuppressed(const std::string& errorId) const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_suppressedErrors.find(errorId) != m_suppressedErrors.end();
}

void ErrorHandler::emergencyShutdown(const std::string& reason) {
    m_logger->critical(Logger::Category::System, "EMERGENCY SHUTDOWN: " + reason);
    
    // Log final error statistics
    auto stats = getErrorStats();
    m_logger->critical(Logger::Category::System, 
                      "Final error statistics - Total: " + std::to_string(stats.totalErrors) + 
                      ", Recovered: " + std::to_string(stats.recoveredErrors));
    
    // Force exit
    std::exit(1);
}

std::string ErrorHandler::generateErrorId() {
    static std::atomic<int> counter(0);
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << "ERR_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") 
        << "_" << std::setfill('0') << std::setw(3) << ms.count() 
        << "_" << ++counter;
    
    return oss.str();
}

std::string ErrorHandler::getStackTrace() {
    const int maxFrames = 10;
    void* buffer[maxFrames];
    int frames = backtrace(buffer, maxFrames);
    char** symbols = backtrace_symbols(buffer, frames);
    
    std::ostringstream oss;
    for (int i = 0; i < frames; ++i) {
        oss << symbols[i] << "\n";
    }
    
    free(symbols);
    return oss.str();
}

void ErrorHandler::logError(const ErrorInfo& error) {
    std::string severityStr;
    switch (error.severity) {
        case ErrorSeverity::INFO: severityStr = "INFO"; break;
        case ErrorSeverity::WARNING: severityStr = "WARNING"; break;
        case ErrorSeverity::ERROR: severityStr = "ERROR"; break;
        case ErrorSeverity::CRITICAL: severityStr = "CRITICAL"; break;
        case ErrorSeverity::FATAL: severityStr = "FATAL"; break;
    }
    
    std::string categoryStr;
    switch (error.category) {
        case ErrorCategory::GENERAL: categoryStr = "GENERAL"; break;
        case ErrorCategory::MINING: categoryStr = "MINING"; break;
        case ErrorCategory::NETWORK: categoryStr = "NETWORK"; break;
        case ErrorCategory::WALLET: categoryStr = "WALLET"; break;
        case ErrorCategory::PERFORMANCE: categoryStr = "PERFORMANCE"; break;
        case ErrorCategory::THERMAL: categoryStr = "THERMAL"; break;
        case ErrorCategory::MEMORY: categoryStr = "MEMORY"; break;
        case ErrorCategory::RANDOMX: categoryStr = "RANDOMX"; break;
        case ErrorCategory::POOL: categoryStr = "POOL"; break;
        case ErrorCategory::CLI: categoryStr = "CLI"; break;
        case ErrorCategory::CONFIG: categoryStr = "CONFIG"; break;
        case ErrorCategory::SYSTEM: categoryStr = "SYSTEM"; break;
        case ErrorCategory::UNKNOWN: categoryStr = "UNKNOWN"; break;
    }
    
    std::ostringstream logMessage;
    logMessage << "[" << error.errorId << "] " << severityStr << " " << categoryStr 
               << ": " << error.message;
    
    if (!error.context.component.empty()) {
        logMessage << " (Component: " << error.context.component;
        if (!error.context.function.empty()) {
            logMessage << ", Function: " << error.context.function;
        }
        if (error.context.lineNumber > 0) {
            logMessage << ", Line: " << error.context.lineNumber;
        }
        logMessage << ")";
    }
    
    if (!error.context.additionalInfo.empty()) {
        logMessage << " - " << error.context.additionalInfo;
    }
    
    // Log based on severity
    switch (error.severity) {
        case ErrorSeverity::INFO:
            m_logger->info(Logger::Category::System, logMessage.str());
            break;
        case ErrorSeverity::WARNING:
            m_logger->warning(Logger::Category::System, logMessage.str());
            break;
        case ErrorSeverity::ERROR:
            m_logger->error(Logger::Category::System, logMessage.str());
            break;
        case ErrorSeverity::CRITICAL:
            m_logger->critical(Logger::Category::System, logMessage.str());
            break;
        case ErrorSeverity::FATAL:
            m_logger->critical(Logger::Category::System, logMessage.str());
            break;
    }
    
    // Log stack trace for errors and above
    if (error.severity >= ErrorSeverity::ERROR && !error.stackTrace.empty()) {
        m_logger->debug(Logger::Category::System, "Stack trace:\n" + error.stackTrace);
    }
}

void ErrorHandler::updateErrorStats(const ErrorInfo& error) {
    m_totalErrors++;
}

bool ErrorHandler::checkErrorThreshold(ErrorSeverity severity) {
    auto it = m_errorThresholds.find(severity);
    if (it == m_errorThresholds.end()) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto& threshold = it->second;
    
    // Remove old timestamps
    threshold.timestamps.erase(
        std::remove_if(threshold.timestamps.begin(), threshold.timestamps.end(),
                      [&](const std::chrono::steady_clock::time_point& timestamp) {
                          return now - timestamp > threshold.timeWindow;
                      }),
        threshold.timestamps.end());
    
    // Add current timestamp
    threshold.timestamps.push_back(now);
    
    return threshold.timestamps.size() > threshold.maxCount;
}

void ErrorHandler::cleanupOldErrors() {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::hours(24); // Keep errors for 24 hours
    
    m_errorHistory.erase(
        std::remove_if(m_errorHistory.begin(), m_errorHistory.end(),
                      [&](const ErrorInfo& error) {
                          return error.timestamp < cutoff;
                      }),
        m_errorHistory.end());
}

// Recovery strategy implementations
bool NetworkRecoveryStrategy::canRecover(const ErrorInfo& error) {
    return error.category == ErrorCategory::NETWORK && 
           error.severity <= ErrorSeverity::ERROR;
}

bool NetworkRecoveryStrategy::executeRecovery(const ErrorInfo& error) {
    // Implement network recovery logic
    // This would include reconnection attempts, pool switching, etc.
    return true; // Placeholder
}

bool MemoryRecoveryStrategy::canRecover(const ErrorInfo& error) {
    return error.category == ErrorCategory::MEMORY && 
           error.severity <= ErrorSeverity::CRITICAL;
}

bool MemoryRecoveryStrategy::executeRecovery(const ErrorInfo& error) {
    // Implement memory recovery logic
    // This would include garbage collection, memory pool cleanup, etc.
    return true; // Placeholder
}

bool ThermalRecoveryStrategy::canRecover(const ErrorInfo& error) {
    return error.category == ErrorCategory::THERMAL && 
           error.severity <= ErrorSeverity::CRITICAL;
}

bool ThermalRecoveryStrategy::executeRecovery(const ErrorInfo& error) {
    // Implement thermal recovery logic
    // This would include CPU throttling, fan control, etc.
    return true; // Placeholder
}
