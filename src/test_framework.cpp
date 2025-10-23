#include "test_framework.h"
#include <iostream>
#include <fstream>
#include <random>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

TestFramework::TestFramework() {
    m_logger = std::make_unique<Logger>();
}

TestFramework::~TestFramework() = default;

bool TestFramework::initialize() {
    if (!m_logger->initialize(Logger::Level::Info, "test_results.log", true)) {
        std::cerr << "Failed to initialize test logger" << std::endl;
        return false;
    }
    
    m_logger->info(Logger::Category::Test, "Test framework initialized");
    return true;
}

void TestFramework::registerTestSuite(std::unique_ptr<TestSuite> suite) {
    m_testSuites.push_back(std::move(suite));
}

void TestFramework::registerTestCase(const std::string& name, std::function<bool()> testFunc, 
                                   const std::string& category) {
    m_testCases.emplace_back(name, testFunc, category);
}

std::vector<TestResult> TestFramework::runAllTests() {
    std::vector<TestResult> allResults;
    
    m_logger->info(Logger::Category::Test, "Starting test execution");
    
    // Run test suites
    for (auto& suite : m_testSuites) {
        m_logger->info(Logger::Category::Test, "Running test suite: " + suite->getName());
        
        try {
            suite->setUp();
            auto results = suite->runTests();
            suite->tearDown();
            
            for (const auto& result : results) {
                allResults.push_back(result);
                logTestResult(result);
            }
        } catch (const std::exception& e) {
            m_logger->error(Logger::Category::Test, "Test suite failed: " + std::string(e.what()));
        }
    }
    
    // Run individual test cases
    for (auto& testCase : m_testCases) {
        m_logger->info(Logger::Category::Test, "Running test case: " + testCase.getName());
        
        auto result = testCase.run();
        allResults.push_back(result);
        logTestResult(result);
    }
    
    m_logger->info(Logger::Category::Test, "Test execution completed");
    return allResults;
}

std::vector<TestResult> TestFramework::runTestsByCategory(const std::string& category) {
    std::vector<TestResult> results;
    
    for (auto& testCase : m_testCases) {
        if (testCase.getCategory() == category) {
            auto result = testCase.run();
            results.push_back(result);
            logTestResult(result);
        }
    }
    
    return results;
}

TestResult TestFramework::runTest(const std::string& testName) {
    for (auto& testCase : m_testCases) {
        if (testCase.getName() == testName) {
            auto result = testCase.run();
            logTestResult(result);
            return result;
        }
    }
    
    return TestResult(testName, false, "Test not found");
}

std::string TestFramework::generateReport(const std::vector<TestResult>& results) {
    std::ostringstream report;
    
    int totalTests = results.size();
    int passedTests = 0;
    int failedTests = 0;
    std::chrono::milliseconds totalDuration(0);
    
    std::map<std::string, int> categoryStats;
    
    for (const auto& result : results) {
        if (result.passed) {
            passedTests++;
        } else {
            failedTests++;
        }
        
        totalDuration += result.duration;
        categoryStats[result.category]++;
    }
    
    report << "========================================\n";
    report << "           TEST EXECUTION REPORT\n";
    report << "========================================\n\n";
    
    report << "Summary:\n";
    report << "  Total Tests: " << totalTests << "\n";
    report << "  Passed: " << passedTests << " (" << std::fixed << std::setprecision(1) 
           << (totalTests > 0 ? (double)passedTests / totalTests * 100 : 0) << "%)\n";
    report << "  Failed: " << failedTests << " (" << std::fixed << std::setprecision(1) 
           << (totalTests > 0 ? (double)failedTests / totalTests * 100 : 0) << "%)\n";
    report << "  Total Duration: " << formatDuration(totalDuration) << "\n\n";
    
    report << "Category Breakdown:\n";
    for (const auto& stat : categoryStats) {
        report << "  " << stat.first << ": " << stat.second << " tests\n";
    }
    report << "\n";
    
    if (failedTests > 0) {
        report << "Failed Tests:\n";
        for (const auto& result : results) {
            if (!result.passed) {
                report << "  ❌ " << result.testName << " (" << result.category << ")\n";
                if (!result.errorMessage.empty()) {
                    report << "     Error: " << result.errorMessage << "\n";
                }
                report << "     Duration: " << formatDuration(result.duration) << "\n\n";
            }
        }
    }
    
    report << "Detailed Results:\n";
    for (const auto& result : results) {
        report << "  " << (result.passed ? "✅" : "❌") << " " << result.testName 
               << " (" << result.category << ") - " << formatDuration(result.duration) << "\n";
    }
    
    return report.str();
}

TestFramework::BenchmarkResult TestFramework::benchmark(const std::string& name, 
                                                       std::function<void()> func, 
                                                       int iterations) {
    std::vector<double> times;
    times.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        times.push_back(duration.count() / 1000.0); // Convert to milliseconds
    }
    
    // Calculate statistics
    double sum = 0;
    for (double time : times) {
        sum += time;
    }
    double average = sum / iterations;
    
    double minTime = *std::min_element(times.begin(), times.end());
    double maxTime = *std::max_element(times.begin(), times.end());
    
    // Calculate standard deviation
    double variance = 0;
    for (double time : times) {
        variance += (time - average) * (time - average);
    }
    double standardDeviation = std::sqrt(variance / iterations);
    
    return {name, average, minTime, maxTime, iterations, standardDeviation};
}

// Assertion helpers
bool TestFramework::assertTrue(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "ASSERT_TRUE failed: " << message << std::endl;
    }
    return condition;
}

bool TestFramework::assertFalse(bool condition, const std::string& message) {
    if (condition) {
        std::cerr << "ASSERT_FALSE failed: " << message << std::endl;
    }
    return !condition;
}

bool TestFramework::assertEqual(const std::string& expected, const std::string& actual, 
                               const std::string& message) {
    if (expected != actual) {
        std::cerr << "ASSERT_EQUAL failed: " << message << std::endl;
        std::cerr << "  Expected: " << expected << std::endl;
        std::cerr << "  Actual: " << actual << std::endl;
    }
    return expected == actual;
}

bool TestFramework::assertNotEqual(const std::string& expected, const std::string& actual, 
                                  const std::string& message) {
    if (expected == actual) {
        std::cerr << "ASSERT_NOT_EQUAL failed: " << message << std::endl;
        std::cerr << "  Value: " << expected << std::endl;
    }
    return expected != actual;
}

bool TestFramework::assertNotNull(const void* ptr, const std::string& message) {
    if (ptr == nullptr) {
        std::cerr << "ASSERT_NOT_NULL failed: " << message << std::endl;
    }
    return ptr != nullptr;
}

bool TestFramework::assertNull(const void* ptr, const std::string& message) {
    if (ptr != nullptr) {
        std::cerr << "ASSERT_NULL failed: " << message << std::endl;
    }
    return ptr == nullptr;
}

bool TestFramework::assertThrows(std::function<void()> func, const std::string& message) {
    try {
        func();
        std::cerr << "ASSERT_THROWS failed: " << message << std::endl;
        return false;
    } catch (...) {
        return true;
    }
}

// Test utilities
std::string TestFramework::generateRandomString(size_t length) {
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);
    
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += chars[dis(gen)];
    }
    return result;
}

std::vector<uint8_t> TestFramework::generateRandomBytes(size_t length) {
    std::vector<uint8_t> result(length);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < length; ++i) {
        result[i] = static_cast<uint8_t>(dis(gen));
    }
    return result;
}

bool TestFramework::fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

bool TestFramework::createTestFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    return file.good();
}

bool TestFramework::deleteTestFile(const std::string& filename) {
    return std::remove(filename.c_str()) == 0;
}

void TestFramework::logTestResult(const TestResult& result) {
    std::string level = result.passed ? "INFO" : "ERROR";
    std::string message = (result.passed ? "✅ " : "❌ ") + result.testName + 
                         " (" + result.category + ") - " + formatDuration(result.duration);
    
    if (!result.passed && !result.errorMessage.empty()) {
        message += " - Error: " + result.errorMessage;
    }
    
    if (result.passed) {
        m_logger->info(Logger::Category::Test, message);
    } else {
        m_logger->error(Logger::Category::Test, message);
    }
}

std::string TestFramework::formatDuration(std::chrono::milliseconds duration) {
    auto ms = duration.count();
    if (ms < 1000) {
        return std::to_string(ms) + "ms";
    } else {
        return std::to_string(ms / 1000) + "." + std::to_string((ms % 1000) / 100) + "s";
    }
}

// Test suite implementations
std::vector<TestResult> RandomXTestSuite::runTests() {
    std::vector<TestResult> results;
    
    // Test RandomX initialization
    results.push_back(TestResult("RandomX Initialization", []() -> bool {
        // This would test RandomX initialization
        return true; // Placeholder
    }(), "", std::chrono::milliseconds(0), "RandomX"));
    
    // Test hash calculation
    results.push_back(TestResult("RandomX Hash Calculation", []() -> bool {
        // This would test hash calculation
        return true; // Placeholder
    }(), "", std::chrono::milliseconds(0), "RandomX"));
    
    return results;
}

std::vector<TestResult> MinerTestSuite::runTests() {
    std::vector<TestResult> results;
    
    // Test miner initialization
    results.push_back(TestResult("Miner Initialization", []() -> bool {
        // This would test miner initialization
        return true; // Placeholder
    }(), "", std::chrono::milliseconds(0), "Miner"));
    
    return results;
}

std::vector<TestResult> PoolConnectionTestSuite::runTests() {
    std::vector<TestResult> results;
    
    // Test pool connection
    results.push_back(TestResult("Pool Connection", []() -> bool {
        // This would test pool connection
        return true; // Placeholder
    }(), "", std::chrono::milliseconds(0), "Pool"));
    
    return results;
}

std::vector<TestResult> WalletTestSuite::runTests() {
    std::vector<TestResult> results;
    
    // Test wallet validation
    results.push_back(TestResult("Wallet Address Validation", []() -> bool {
        // This would test wallet address validation
        return true; // Placeholder
    }(), "", std::chrono::milliseconds(0), "Wallet"));
    
    return results;
}

std::vector<TestResult> CLITestSuite::runTests() {
    std::vector<TestResult> results;
    
    // Test CLI command parsing
    results.push_back(TestResult("CLI Command Parsing", []() -> bool {
        // This would test CLI command parsing
        return true; // Placeholder
    }(), "", std::chrono::milliseconds(0), "CLI"));
    
    return results;
}

std::vector<TestResult> PerformanceTestSuite::runTests() {
    std::vector<TestResult> results;
    
    // Test performance monitoring
    results.push_back(TestResult("Performance Monitoring", []() -> bool {
        // This would test performance monitoring
        return true; // Placeholder
    }(), "", std::chrono::milliseconds(0), "Performance"));
    
    return results;
}

std::vector<TestResult> IntegrationTestSuite::runTests() {
    std::vector<TestResult> results;
    
    // Test full integration
    results.push_back(TestResult("Full Integration Test", []() -> bool {
        // This would test full integration
        return true; // Placeholder
    }(), "", std::chrono::milliseconds(0), "Integration"));
    
    return results;
}
