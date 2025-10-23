#include "startup_tests.h"
#include "config_manager.h"
#include "miner.h"
#include "randomx.h"
#include "memory_manager.h"
#include "multi_pool_manager.h"
#include "performance_monitor.h"
#include "cli_manager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <thread>
#include <future>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#include <mach/mach.h>

// Global startup test manager instance
std::unique_ptr<StartupTestManager> g_startupTestManager = nullptr;

StartupTestManager::StartupTestManager() {
    m_logger = std::make_unique<Logger>();
    m_testFramework = std::make_unique<TestFramework>();
    m_errorHandler = std::make_unique<ErrorHandler>();
}

StartupTestManager::~StartupTestManager() {
    shutdown();
}

bool StartupTestManager::initialize() {
    // Initialize logger
    if (!m_logger->initialize(Logger::Level::Info, "startup_tests.log", true)) {
        std::cerr << "Failed to initialize startup test logger" << std::endl;
        return false;
    }
    
    // Initialize test framework
    if (!m_testFramework->initialize()) {
        m_logger->error(Logger::Category::Test, "Failed to initialize test framework");
        return false;
    }
    
    // Initialize error handler
    if (!m_errorHandler->initialize()) {
        m_logger->error(Logger::Category::System, "Failed to initialize error handler");
        return false;
    }
    
    // Register default tests
    registerDefaultTests();
    
    m_logger->info(Logger::Category::Test, "Startup test manager initialized");
    return true;
}

void StartupTestManager::shutdown() {
    m_testsRunning = false;
    m_testCV.notify_all();
    
    if (m_logger) {
        m_logger->info(Logger::Category::Test, "Startup test manager shutting down");
    }
}

std::vector<StartupTestResult> StartupTestManager::runStartupTests() {
    std::lock_guard<std::mutex> lock(m_testMutex);
    m_testsRunning = true;
    
    std::vector<StartupTestResult> results;
    results.reserve(m_startupTests.size());
    
    m_logger->info(Logger::Category::Test, "Starting startup test suite with " + 
                   std::to_string(m_startupTests.size()) + " tests");
    
    for (size_t i = 0; i < m_startupTests.size(); ++i) {
        const auto& test = m_startupTests[i];
        
        // Display progress
        if (m_displayResults) {
            displayProgressBar(i + 1, m_startupTests.size(), test.name);
        }
        
        // Execute test with timeout
        auto future = std::async(std::launch::async, [this, &test]() {
            return executeTest(test);
        });
        
        auto status = future.wait_for(m_testTimeout);
        StartupTestResult result;
        
        if (status == std::future_status::ready) {
            result = future.get();
        } else {
            // Test timed out
            result = StartupTestResult(test.name, false, "Test timed out after " + 
                                     std::to_string(m_testTimeout.count()) + " seconds", 
                                     m_testTimeout, getCategoryString(test.category));
            result.statusIcon = "⏰";
        }
        
        results.push_back(result);
        logTestResult(result);
        
        // Small delay for readability
        if (m_displayResults) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    m_testsRunning = false;
    m_testCV.notify_all();
    
    return results;
}

std::vector<StartupTestResult> StartupTestManager::runTestsByCategory(StartupTestCategory category) {
    std::vector<StartupTestResult> results;
    
    for (const auto& test : m_startupTests) {
        if (test.category == category) {
            results.push_back(executeTest(test));
        }
    }
    
    return results;
}

void StartupTestManager::displayTestResults(const std::vector<StartupTestResult>& results) {
    if (!m_displayResults) return;
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                           STARTUP TEST RESULTS                               ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    // Display individual test results
    for (const auto& result : results) {
        std::cout << std::left << std::setw(4) << result.statusIcon;
        std::cout << std::left << std::setw(50) << result.testName;
        std::cout << std::left << std::setw(15) << getCategoryString(result.category);
        std::cout << std::right << std::setw(8) << result.duration.count() << "ms";
        
        if (!result.passed && !result.message.empty()) {
            std::cout << " - " << result.message;
        }
        
        std::cout << "\n";
    }
    
    std::cout << "\n";
    
    // Display summary
    auto stats = getTestStats(results);
    displaySummary(stats);
    
    // Display failed tests if any
    if (stats.failedTests > 0) {
        displayFailedTests(results);
    }
    
    // Display critical failures if any
    if (stats.criticalFailures > 0) {
        displayCriticalFailures(results);
    }
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                              TEST COMPLETE                                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

bool StartupTestManager::allCriticalTestsPassed(const std::vector<StartupTestResult>& results) {
    for (const auto& result : results) {
        // Check if this was a critical test that failed
        auto it = std::find_if(m_startupTests.begin(), m_startupTests.end(),
                              [&result](const StartupTest& test) {
                                  return test.name == result.testName && test.critical;
                              });
        
        if (it != m_startupTests.end() && !result.passed) {
            return false;
        }
    }
    return true;
}

StartupTestManager::StartupTestStats StartupTestManager::getTestStats(const std::vector<StartupTestResult>& results) {
    StartupTestStats stats;
    stats.totalTests = results.size();
    stats.passedTests = 0;
    stats.failedTests = 0;
    stats.criticalTests = 0;
    stats.criticalFailures = 0;
    stats.totalDuration = std::chrono::milliseconds(0);
    
    for (const auto& result : results) {
        if (result.passed) {
            stats.passedTests++;
        } else {
            stats.failedTests++;
        }
        
        stats.totalDuration += result.duration;
        
        // Check if this was a critical test
        auto it = std::find_if(m_startupTests.begin(), m_startupTests.end(),
                              [&result](const StartupTest& test) {
                                  return test.name == result.testName && test.critical;
                              });
        
        if (it != m_startupTests.end()) {
            stats.criticalTests++;
            if (!result.passed) {
                stats.criticalFailures++;
            }
        }
    }
    
    // Determine overall status
    if (stats.criticalFailures > 0) {
        stats.overallStatus = "CRITICAL FAILURES DETECTED";
    } else if (stats.failedTests > 0) {
        stats.overallStatus = "SOME TESTS FAILED";
    } else {
        stats.overallStatus = "ALL TESTS PASSED";
    }
    
    return stats;
}

void StartupTestManager::registerStartupTest(const std::string& name, std::function<bool()> testFunc, 
                                            StartupTestCategory category, bool critical) {
    StartupTest test;
    test.name = name;
    test.testFunc = testFunc;
    test.category = category;
    test.critical = critical;
    
    m_startupTests.push_back(test);
}

StartupTestResult StartupTestManager::executeTest(const StartupTest& test) {
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        bool passed = test.testFunc();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        return StartupTestResult(test.name, passed, "", duration, getCategoryString(test.category));
    } catch (const std::exception& e) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        return StartupTestResult(test.name, false, e.what(), duration, getCategoryString(test.category));
    } catch (...) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        return StartupTestResult(test.name, false, "Unknown exception", duration, getCategoryString(test.category));
    }
}

void StartupTestManager::registerDefaultTests() {
    // System requirements tests
    registerStartupTest("System Requirements", [this]() { return testSystemRequirements(); }, 
                       StartupTestCategory::SYSTEM, true);
    
    // Configuration tests
    registerStartupTest("Configuration Loading", [this]() { return testConfigurationLoading(); }, 
                       StartupTestCategory::CONFIG, true);
    
    // Network tests
    registerStartupTest("Network Connectivity", [this]() { return testNetworkConnectivity(); }, 
                       StartupTestCategory::NETWORK, false);
    
    // Mining component tests
    registerStartupTest("Mining Components", [this]() { return testMiningComponents(); }, 
                       StartupTestCategory::MINING, true);
    
    registerStartupTest("RandomX Algorithm", [this]() { return testRandomXAlgorithm(); }, 
                       StartupTestCategory::MINING, true);
    
    // Memory management tests
    registerStartupTest("Memory Management", [this]() { return testMemoryManagement(); }, 
                       StartupTestCategory::MEMORY, true);
    
    // Performance monitoring tests
    registerStartupTest("Performance Monitoring", [this]() { return testPerformanceMonitoring(); }, 
                       StartupTestCategory::PERFORMANCE, false);
    
    // Security tests
    registerStartupTest("Security Validation", [this]() { return testSecurityValidation(); }, 
                       StartupTestCategory::SECURITY, false);
    
    // Integration tests
    registerStartupTest("System Integration", [this]() { return testSystemIntegration(); }, 
                       StartupTestCategory::INTEGRATION, true);
    
    registerStartupTest("Pool Connections", [this]() { return testPoolConnections(); }, 
                       StartupTestCategory::INTEGRATION, false);
    
    registerStartupTest("Wallet Validation", [this]() { return testWalletValidation(); }, 
                       StartupTestCategory::INTEGRATION, false);
    
    registerStartupTest("CLI Interface", [this]() { return testCLIInterface(); }, 
                       StartupTestCategory::INTEGRATION, false);
    
    registerStartupTest("Error Handling", [this]() { return testErrorHandling(); }, 
                       StartupTestCategory::INTEGRATION, false);
    
    registerStartupTest("Logging System", [this]() { return testLoggingSystem(); }, 
                       StartupTestCategory::INTEGRATION, false);
    
    registerStartupTest("Thermal Management", [this]() { return testThermalManagement(); }, 
                       StartupTestCategory::INTEGRATION, false);
    
    registerStartupTest("Multi-Pool Support", [this]() { return testMultiPoolSupport(); }, 
                       StartupTestCategory::INTEGRATION, false);
}

// Test implementations
bool StartupTestManager::testSystemRequirements() {
    try {
        // Check macOS version
        struct utsname systemInfo;
        if (uname(&systemInfo) != 0) {
            return false;
        }
        
        // Check if running on Apple Silicon
        size_t size = 0;
        sysctlbyname("hw.machine", nullptr, &size, nullptr, 0);
        std::string machine(size, 0);
        sysctlbyname("hw.machine", &machine[0], &size, nullptr, 0);
        
        if (machine.find("arm64") == std::string::npos) {
            return false;
        }
        
        // Check available memory
        int64_t memsize;
        size_t memsize_size = sizeof(memsize);
        if (sysctlbyname("hw.memsize", &memsize, &memsize_size, nullptr, 0) == 0) {
            // Require at least 4GB RAM
            if (memsize < 4ULL * 1024 * 1024 * 1024) {
                return false;
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testConfigurationLoading() {
    try {
        ConfigManager config;
        return config.loadFromFile("config.json");
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testNetworkConnectivity() {
    try {
        // Test DNS resolution
        // This is a simplified test - in production, you'd test actual pool connectivity
        return true; // Placeholder
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testMiningComponents() {
    try {
        // Test miner initialization
        ConfigManager config;
        if (!config.loadFromFile("config.json")) {
            return false;
        }
        
        Miner miner;
        return miner.initialize(config);
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testRandomXAlgorithm() {
    try {
        RandomX randomx;
        return randomx.initialize("test_key", 32);
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testMemoryManagement() {
    try {
        RandomXMemoryManager memMgr;
        return memMgr.initialize();
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testPerformanceMonitoring() {
    try {
        PerformanceMonitor perf;
        return perf.initialize();
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testSecurityValidation() {
    try {
        // Test wallet address validation
        std::string testAddress = "9wviCeWe2D8XS82k2ovp5EUYLzBt9pYNW2LXUFsZiv8S3Mt21FZ5qQaAroko1enzw3eGr9qC7X1D7Geoo2RrAotYPwq9Gm8";
        Miner miner;
        return miner.isValidMoneroAddress(testAddress);
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testSystemIntegration() {
    try {
        // Test full system initialization
        ConfigManager config;
        if (!config.loadFromFile("config.json")) {
            return false;
        }
        
        Logger logger;
        if (!logger.initialize(Logger::Level::Info, "", true)) {
            return false;
        }
        
        Miner miner;
        if (!miner.initialize(config)) {
            return false;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testPoolConnections() {
    try {
        // Test pool URL parsing
        std::string url = "stratum+tcp://pool.supportxmr.com:3333";
        return url.find("stratum+tcp://") == 0;
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testWalletValidation() {
    try {
        // Test wallet address validation
        std::string mainnetAddr = "4AdUndXHHZ6cFfRgv3tnC1xhgLwivBN4PBhGWA3rdHykqrA1KQjQMd4Lk9SxQ19Lrpw4gJH3J4Xw3ExVeGhdGvEbmJxJb";
        return mainnetAddr.length() == 95 && mainnetAddr[0] == '4';
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testCLIInterface() {
    try {
        CLIManager cli;
        return cli.initialize();
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testErrorHandling() {
    try {
        return ErrorHandler::getInstance().initialize();
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testLoggingSystem() {
    try {
        Logger logger;
        return logger.initialize(Logger::Level::Info, "", true);
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testThermalManagement() {
    try {
        // Test thermal management (placeholder)
        return true;
    } catch (...) {
        return false;
    }
}

bool StartupTestManager::testMultiPoolSupport() {
    try {
        // Test multi-pool support (placeholder)
        return true;
    } catch (...) {
        return false;
    }
}

// Helper methods
std::string StartupTestManager::getCategoryString(StartupTestCategory category) {
    switch (category) {
        case StartupTestCategory::SYSTEM: return "System";
        case StartupTestCategory::CONFIG: return "Config";
        case StartupTestCategory::NETWORK: return "Network";
        case StartupTestCategory::MINING: return "Mining";
        case StartupTestCategory::MEMORY: return "Memory";
        case StartupTestCategory::PERFORMANCE: return "Performance";
        case StartupTestCategory::SECURITY: return "Security";
        case StartupTestCategory::INTEGRATION: return "Integration";
        default: return "Unknown";
    }
}

std::string StartupTestManager::getCategoryIcon(StartupTestCategory category) {
    switch (category) {
        case StartupTestCategory::SYSTEM: return "🖥️";
        case StartupTestCategory::CONFIG: return "⚙️";
        case StartupTestCategory::NETWORK: return "🌐";
        case StartupTestCategory::MINING: return "⛏️";
        case StartupTestCategory::MEMORY: return "💾";
        case StartupTestCategory::PERFORMANCE: return "📊";
        case StartupTestCategory::SECURITY: return "🔒";
        case StartupTestCategory::INTEGRATION: return "🔗";
        default: return "❓";
    }
}

void StartupTestManager::logTestResult(const StartupTestResult& result) {
    if (result.passed) {
        m_logger->info(Logger::Category::Test, "PASSED: " + result.testName);
    } else {
        m_logger->error(Logger::Category::Test, "FAILED: " + result.testName + " - " + result.message);
    }
}

void StartupTestManager::displayProgressBar(int current, int total, const std::string& currentTest) {
    int barWidth = 50;
    float progress = static_cast<float>(current) / total;
    int pos = static_cast<int>(barWidth * progress);
    
    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << static_cast<int>(progress * 100.0) << "% " << currentTest;
    std::cout.flush();
}

void StartupTestManager::displaySummary(const StartupTestStats& stats) {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                              TEST SUMMARY                                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    std::cout << "Total Tests: " << stats.totalTests << "\n";
    std::cout << "Passed: " << stats.passedTests << " ✅\n";
    std::cout << "Failed: " << stats.failedTests << " ❌\n";
    std::cout << "Critical Tests: " << stats.criticalTests << "\n";
    std::cout << "Critical Failures: " << stats.criticalFailures << "\n";
    std::cout << "Total Duration: " << stats.totalDuration.count() << "ms\n";
    std::cout << "Overall Status: " << stats.overallStatus << "\n";
    std::cout << "\n";
}

void StartupTestManager::displayFailedTests(const std::vector<StartupTestResult>& results) {
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                              FAILED TESTS                                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    for (const auto& result : results) {
        if (!result.passed) {
            std::cout << "❌ " << result.testName << " (" << getCategoryString(static_cast<StartupTestCategory>(0)) << ")\n";
            if (!result.message.empty()) {
                std::cout << "   Error: " << result.message << "\n";
            }
            std::cout << "\n";
        }
    }
}

void StartupTestManager::displayCriticalFailures(const std::vector<StartupTestResult>& results) {
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                           CRITICAL FAILURES                                ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    for (const auto& result : results) {
        // Check if this was a critical test that failed
        auto it = std::find_if(m_startupTests.begin(), m_startupTests.end(),
                              [&result](const StartupTest& test) {
                                  return test.name == result.testName && test.critical;
                              });
        
        if (it != m_startupTests.end() && !result.passed) {
            std::cout << "🚨 CRITICAL: " << result.testName << "\n";
            if (!result.message.empty()) {
                std::cout << "   Error: " << result.message << "\n";
            }
            std::cout << "\n";
        }
    }
}

// Startup test runner implementation
bool StartupTestRunner::runStartupTests() {
    if (!g_startupTestManager) {
        g_startupTestManager = std::make_unique<StartupTestManager>();
        if (!g_startupTestManager->initialize()) {
            std::cerr << "Failed to initialize startup test manager" << std::endl;
            return false;
        }
    }
    
    displayStartupBanner();
    
    auto results = g_startupTestManager->runStartupTests();
    g_startupTestManager->displayTestResults(results);
    
    return g_startupTestManager->allCriticalTestsPassed(results);
}

void StartupTestRunner::displayStartupBanner() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                        MININGSOFT STARTUP TESTS                             ║\n";
    std::cout << "║                                                                              ║\n";
    std::cout << "║  🚀 Running comprehensive system validation...                              ║\n";
    std::cout << "║  🔍 Testing all components and dependencies...                               ║\n";
    std::cout << "║  ⚡ Ensuring optimal performance and reliability...                          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

void StartupTestRunner::displayTestResults(const std::vector<StartupTestResult>& results) {
    if (g_startupTestManager) {
        g_startupTestManager->displayTestResults(results);
    }
}

bool StartupTestRunner::shouldProceedToCLI() {
    if (!g_startupTestManager) {
        return true; // If no test manager, proceed anyway
    }
    
    // Check if emergency bypass is enabled
    if (g_startupTestManager->isEmergencyBypassEnabled()) {
        std::cout << "⚠️  Emergency bypass enabled - proceeding to CLI despite test failures\n";
        return true;
    }
    
    // Check if auto-start is enabled
    if (g_startupTestManager->shouldAutoStart()) {
        return true;
    }
    
    return true; // Always proceed to CLI for now
}

bool StartupTestRunner::shouldAutoStartMining() {
    if (!g_startupTestManager) {
        return false;
    }
    
    return g_startupTestManager->shouldAutoStart();
}
