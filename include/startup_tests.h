#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "logger.h"
#include "test_framework.h"
#include "error_handler.h"

/**
 * Startup Test System for MiningSoft
 * Runs comprehensive tests on every application launch
 * Displays results on screen before presenting CLI menu
 */

// Test result with visual indicators
struct StartupTestResult {
    std::string testName;
    bool passed;
    std::string message;
    std::chrono::milliseconds duration;
    std::string category;
    std::string statusIcon;  // ✅ ❌ ⚠️ 🔧
    
    StartupTestResult() : passed(false), duration(0) {}
    StartupTestResult(const std::string& name, bool pass, const std::string& msg = "", 
                     std::chrono::milliseconds dur = std::chrono::milliseconds(0),
                     const std::string& cat = "General")
        : testName(name), passed(pass), message(msg), duration(dur), category(cat) {
        statusIcon = passed ? "✅" : "❌";
    }
};

// Startup test categories
enum class StartupTestCategory {
    SYSTEM,         // System requirements and environment
    CONFIG,         // Configuration loading and validation
    NETWORK,        // Network connectivity and DNS
    MINING,         // Mining components and algorithms
    MEMORY,         // Memory management and allocation
    PERFORMANCE,    // Performance monitoring and metrics
    SECURITY,       // Security and validation
    INTEGRATION     // Full system integration
};

// Startup test manager
class StartupTestManager {
public:
    StartupTestManager();
    ~StartupTestManager();
    
    // Initialize the startup test system
    bool initialize();
    void shutdown();
    
    // Run all startup tests
    std::vector<StartupTestResult> runStartupTests();
    
    // Run tests by category
    std::vector<StartupTestResult> runTestsByCategory(StartupTestCategory category);
    
    // Display test results on screen
    void displayTestResults(const std::vector<StartupTestResult>& results);
    
    // Check if all critical tests passed
    bool allCriticalTestsPassed(const std::vector<StartupTestResult>& results);
    
    // Get startup test statistics
    struct StartupTestStats {
        int totalTests;
        int passedTests;
        int failedTests;
        int criticalTests;
        int criticalFailures;
        std::chrono::milliseconds totalDuration;
        std::string overallStatus;
    };
    
    StartupTestStats getTestStats(const std::vector<StartupTestResult>& results);
    
    // Configuration
    void setAutoStart(bool enabled) { m_autoStart = enabled; }
    bool shouldAutoStart() const { return m_autoStart; }
    
    void setTestTimeout(std::chrono::seconds timeout) { m_testTimeout = timeout; }
    void setDisplayResults(bool display) { m_displayResults = display; }
    
    // Test registration
    void registerStartupTest(const std::string& name, std::function<bool()> testFunc, 
                           StartupTestCategory category, bool critical = false);
    
    // Emergency bypass
    void setEmergencyBypass(bool enabled) { m_emergencyBypass = enabled; }
    bool isEmergencyBypassEnabled() const { return m_emergencyBypass; }

private:
    std::unique_ptr<Logger> m_logger;
    std::unique_ptr<TestFramework> m_testFramework;
    std::unique_ptr<ErrorHandler> m_errorHandler;
    
    // Test registration
    struct StartupTest {
        std::string name;
        std::function<bool()> testFunc;
        StartupTestCategory category;
        bool critical;
    };
    
    std::vector<StartupTest> m_startupTests;
    
    // Configuration
    std::atomic<bool> m_autoStart{false};
    std::atomic<bool> m_displayResults{true};
    std::atomic<bool> m_emergencyBypass{false};
    std::chrono::seconds m_testTimeout{30};
    
    // Test execution
    std::mutex m_testMutex;
    std::condition_variable m_testCV;
    std::atomic<bool> m_testsRunning{false};
    
    // Internal methods
    void registerDefaultTests();
    StartupTestResult executeTest(const StartupTest& test);
    std::string getCategoryString(StartupTestCategory category);
    std::string getCategoryIcon(StartupTestCategory category);
    void logTestResult(const StartupTestResult& result);
    void displayProgressBar(int current, int total, const std::string& currentTest);
    void displaySummary(const StartupTestStats& stats);
    void displayFailedTests(const std::vector<StartupTestResult>& results);
    void displayCriticalFailures(const std::vector<StartupTestResult>& results);
    
    // Test implementations
    bool testSystemRequirements();
    bool testConfigurationLoading();
    bool testNetworkConnectivity();
    bool testMiningComponents();
    bool testMemoryManagement();
    bool testPerformanceMonitoring();
    bool testSecurityValidation();
    bool testSystemIntegration();
    bool testRandomXAlgorithm();
    bool testPoolConnections();
    bool testWalletValidation();
    bool testCLIInterface();
    bool testErrorHandling();
    bool testLoggingSystem();
    bool testThermalManagement();
    bool testMultiPoolSupport();
};

// Global startup test manager instance
extern std::unique_ptr<StartupTestManager> g_startupTestManager;

// Startup test macros for easy registration
#define REGISTER_STARTUP_TEST(name, category, critical, test_func) \
    g_startupTestManager->registerStartupTest(name, test_func, category, critical)

#define STARTUP_TEST(name, category, critical) \
    g_startupTestManager->registerStartupTest(name, [this]() -> bool

// Startup test runner
class StartupTestRunner {
public:
    static bool runStartupTests();
    static void displayStartupBanner();
    static void displayTestResults(const std::vector<StartupTestResult>& results);
    static bool shouldProceedToCLI();
    static bool shouldAutoStartMining();
};
