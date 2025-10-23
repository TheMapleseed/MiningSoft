#pragma once

#include <string>
#include <exception>
#include <functional>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include "logger.h"

/**
 * Comprehensive error handling system for MiningSoft
 * Provides centralized error management, recovery strategies, and monitoring
 */

// Error severity levels
enum class ErrorSeverity {
    INFO,       // Informational (not an error)
    WARNING,    // Warning (non-critical)
    ERROR,      // Error (recoverable)
    CRITICAL,   // Critical (may cause shutdown)
    FATAL       // Fatal (immediate shutdown required)
};

// Error categories
enum class ErrorCategory {
    GENERAL,
    MINING,
    NETWORK,
    WALLET,
    PERFORMANCE,
    THERMAL,
    MEMORY,
    RANDOMX,
    POOL,
    CLI,
    CONFIG,
    SYSTEM,
    UNKNOWN
};

// Error context information
struct ErrorContext {
    std::string component;
    std::string function;
    int lineNumber;
    std::string file;
    std::string additionalInfo;
    
    ErrorContext() : lineNumber(0) {}
    ErrorContext(const std::string& comp, const std::string& func, int line, 
                 const std::string& file, const std::string& info = "")
        : component(comp), function(func), lineNumber(line), file(file), additionalInfo(info) {}
};

// Error information structure
struct ErrorInfo {
    std::string errorId;
    std::string message;
    ErrorSeverity severity;
    ErrorCategory category;
    ErrorContext context;
    std::chrono::steady_clock::time_point timestamp;
    std::string stackTrace;
    bool recoverable;
    std::string recoveryAction;
    
    ErrorInfo() : severity(ErrorSeverity::INFO), category(ErrorCategory::UNKNOWN), 
                  recoverable(false), timestamp(std::chrono::steady_clock::now()) {}
};

// Custom exception classes
class MiningSoftException : public std::exception {
public:
    MiningSoftException(const std::string& message, ErrorSeverity severity = ErrorSeverity::ERROR,
                       ErrorCategory category = ErrorCategory::GENERAL)
        : m_message(message), m_severity(severity), m_category(category) {}
    
    const char* what() const noexcept override { return m_message.c_str(); }
    ErrorSeverity getSeverity() const { return m_severity; }
    ErrorCategory getCategory() const { return m_category; }

private:
    std::string m_message;
    ErrorSeverity m_severity;
    ErrorCategory m_category;
};

class MiningException : public MiningSoftException {
public:
    MiningException(const std::string& message) 
        : MiningSoftException(message, ErrorSeverity::ERROR, ErrorCategory::MINING) {}
};

class NetworkException : public MiningSoftException {
public:
    NetworkException(const std::string& message) 
        : MiningSoftException(message, ErrorSeverity::ERROR, ErrorCategory::NETWORK) {}
};

class WalletException : public MiningSoftException {
public:
    WalletException(const std::string& message) 
        : MiningSoftException(message, ErrorSeverity::ERROR, ErrorCategory::WALLET) {}
};

class MemoryException : public MiningSoftException {
public:
    MemoryException(const std::string& message) 
        : MiningSoftException(message, ErrorSeverity::CRITICAL, ErrorCategory::MEMORY) {}
};

class ThermalException : public MiningSoftException {
public:
    ThermalException(const std::string& message) 
        : MiningSoftException(message, ErrorSeverity::CRITICAL, ErrorCategory::THERMAL) {}
};

// Error recovery strategy
class ErrorRecoveryStrategy {
public:
    virtual ~ErrorRecoveryStrategy() = default;
    virtual bool canRecover(const ErrorInfo& error) = 0;
    virtual bool executeRecovery(const ErrorInfo& error) = 0;
    virtual std::string getRecoveryDescription() const = 0;
};

// Error handler main class
class ErrorHandler {
public:
    static ErrorHandler& getInstance();
    
    // Initialize error handler
    bool initialize();
    void shutdown();
    
    // Error reporting
    void reportError(const std::string& message, ErrorSeverity severity, 
                    ErrorCategory category, const ErrorContext& context = ErrorContext());
    void reportError(const MiningSoftException& exception, const ErrorContext& context = ErrorContext());
    
    // Error recovery
    void registerRecoveryStrategy(ErrorCategory category, 
                                 std::unique_ptr<ErrorRecoveryStrategy> strategy);
    bool attemptRecovery(const ErrorInfo& error);
    
    // Error monitoring
    void setErrorThreshold(ErrorSeverity severity, int maxCount, 
                          std::chrono::seconds timeWindow);
    bool isErrorThresholdExceeded(ErrorSeverity severity);
    
    // Error statistics
    struct ErrorStats {
        std::map<ErrorSeverity, int> errorCounts;
        std::map<ErrorCategory, int> categoryCounts;
        std::chrono::steady_clock::time_point lastError;
        int totalErrors;
        int recoveredErrors;
    };
    
    ErrorStats getErrorStats() const;
    std::vector<ErrorInfo> getRecentErrors(int count = 10) const;
    
    // Error filtering and suppression
    void suppressError(const std::string& errorId);
    void unsuppressError(const std::string& errorId);
    bool isErrorSuppressed(const std::string& errorId) const;
    
    // Emergency shutdown
    void emergencyShutdown(const std::string& reason);

private:
    ErrorHandler() = default;
    ~ErrorHandler() = default;
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;
    
    std::unique_ptr<Logger> m_logger;
    std::map<ErrorCategory, std::unique_ptr<ErrorRecoveryStrategy>> m_recoveryStrategies;
    std::vector<ErrorInfo> m_errorHistory;
    std::map<std::string, bool> m_suppressedErrors;
    
    std::mutex m_errorMutex;
    std::atomic<int> m_totalErrors{0};
    std::atomic<int> m_recoveredErrors{0};
    
    // Error threshold monitoring
    struct ErrorThreshold {
        int maxCount;
        std::chrono::seconds timeWindow;
        std::vector<std::chrono::steady_clock::time_point> timestamps;
    };
    std::map<ErrorSeverity, ErrorThreshold> m_errorThresholds;
    
    // Internal methods
    std::string generateErrorId();
    std::string getStackTrace();
    void logError(const ErrorInfo& error);
    void updateErrorStats(const ErrorInfo& error);
    bool checkErrorThreshold(ErrorSeverity severity);
    void cleanupOldErrors();
};

// Recovery strategy implementations
class NetworkRecoveryStrategy : public ErrorRecoveryStrategy {
public:
    bool canRecover(const ErrorInfo& error) override;
    bool executeRecovery(const ErrorInfo& error) override;
    std::string getRecoveryDescription() const override { return "Network reconnection"; }
};

class MemoryRecoveryStrategy : public ErrorRecoveryStrategy {
public:
    bool canRecover(const ErrorInfo& error) override;
    bool executeRecovery(const ErrorInfo& error) override;
    std::string getRecoveryDescription() const override { return "Memory cleanup and retry"; }
};

class ThermalRecoveryStrategy : public ErrorRecoveryStrategy {
public:
    bool canRecover(const ErrorInfo& error) override;
    bool executeRecovery(const ErrorInfo& error) override;
    std::string getRecoveryDescription() const override { return "Thermal throttling and cooling"; }
};

// Error reporting macros
#define REPORT_ERROR(message, severity, category) \
    ErrorHandler::getInstance().reportError(message, severity, category, \
        ErrorContext(__FUNCTION__, __FUNCTION__, __LINE__, __FILE__))

#define REPORT_EXCEPTION(exception) \
    ErrorHandler::getInstance().reportError(exception, \
        ErrorContext(__FUNCTION__, __FUNCTION__, __LINE__, __FILE__))

#define REPORT_MINING_ERROR(message) \
    REPORT_ERROR(message, ErrorSeverity::ERROR, ErrorCategory::MINING)

#define REPORT_NETWORK_ERROR(message) \
    REPORT_ERROR(message, ErrorSeverity::ERROR, ErrorCategory::NETWORK)

#define REPORT_WALLET_ERROR(message) \
    REPORT_ERROR(message, ErrorSeverity::ERROR, ErrorCategory::WALLET)

#define REPORT_MEMORY_ERROR(message) \
    REPORT_ERROR(message, ErrorSeverity::CRITICAL, ErrorCategory::MEMORY)

#define REPORT_THERMAL_ERROR(message) \
    REPORT_ERROR(message, ErrorSeverity::CRITICAL, ErrorCategory::THERMAL)

// Exception throwing macros
#define THROW_MINING_EXCEPTION(message) \
    throw MiningException(message)

#define THROW_NETWORK_EXCEPTION(message) \
    throw NetworkException(message)

#define THROW_WALLET_EXCEPTION(message) \
    throw WalletException(message)

#define THROW_MEMORY_EXCEPTION(message) \
    throw MemoryException(message)

#define THROW_THERMAL_EXCEPTION(message) \
    throw ThermalException(message)
