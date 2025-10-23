#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <memory>
#include <map>
#include <sstream>
#include "logger.h"

/**
 * Comprehensive testing framework for MiningSoft
 * Provides unit tests, integration tests, and performance benchmarks
 */

// Test result structure
struct TestResult {
    std::string testName;
    bool passed;
    std::string errorMessage;
    std::chrono::milliseconds duration;
    std::string category;
    
    TestResult() : passed(false), duration(0) {}
    TestResult(const std::string& name, bool pass, const std::string& error = "", 
               std::chrono::milliseconds dur = std::chrono::milliseconds(0),
               const std::string& cat = "General")
        : testName(name), passed(pass), errorMessage(error), duration(dur), category(cat) {}
};

// Test suite base class
class TestSuite {
public:
    virtual ~TestSuite() = default;
    virtual std::string getName() const = 0;
    virtual std::vector<TestResult> runTests() = 0;
    virtual void setUp() {}
    virtual void tearDown() {}
};

// Individual test case
class TestCase {
public:
    TestCase(const std::string& name, std::function<bool()> testFunc, 
             const std::string& category = "General")
        : m_name(name), m_testFunc(testFunc), m_category(category) {}
    
    TestResult run() {
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            bool passed = m_testFunc();
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            return TestResult(m_name, passed, "", duration, m_category);
        } catch (const std::exception& e) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            return TestResult(m_name, false, e.what(), duration, m_category);
        }
    }
    
    const std::string& getName() const { return m_name; }
    const std::string& getCategory() const { return m_category; }

private:
    std::string m_name;
    std::function<bool()> m_testFunc;
    std::string m_category;
};

// Test framework main class
class TestFramework {
public:
    TestFramework();
    ~TestFramework();
    
    // Initialize the test framework
    bool initialize();
    
    // Register test suites
    void registerTestSuite(std::unique_ptr<TestSuite> suite);
    
    // Register individual test cases
    void registerTestCase(const std::string& name, std::function<bool()> testFunc, 
                         const std::string& category = "General");
    
    // Run all tests
    std::vector<TestResult> runAllTests();
    
    // Run tests by category
    std::vector<TestResult> runTestsByCategory(const std::string& category);
    
    // Run specific test
    TestResult runTest(const std::string& testName);
    
    // Generate test report
    std::string generateReport(const std::vector<TestResult>& results);
    
    // Performance benchmarking
    struct BenchmarkResult {
        std::string name;
        double averageTimeMs;
        double minTimeMs;
        double maxTimeMs;
        int iterations;
        double standardDeviation;
    };
    
    BenchmarkResult benchmark(const std::string& name, std::function<void()> func, 
                             int iterations = 100);
    
    // Assertion helpers
    static bool assertTrue(bool condition, const std::string& message = "");
    static bool assertFalse(bool condition, const std::string& message = "");
    static bool assertEqual(const std::string& expected, const std::string& actual, 
                           const std::string& message = "");
    static bool assertNotEqual(const std::string& expected, const std::string& actual, 
                              const std::string& message = "");
    static bool assertNotNull(const void* ptr, const std::string& message = "");
    static bool assertNull(const void* ptr, const std::string& message = "");
    static bool assertThrows(std::function<void()> func, const std::string& message = "");
    
    // Test utilities
    static std::string generateRandomString(size_t length);
    static std::vector<uint8_t> generateRandomBytes(size_t length);
    static bool fileExists(const std::string& filename);
    static bool createTestFile(const std::string& filename, const std::string& content);
    static bool deleteTestFile(const std::string& filename);

private:
    std::vector<std::unique_ptr<TestSuite>> m_testSuites;
    std::vector<TestCase> m_testCases;
    std::unique_ptr<Logger> m_logger;
    std::map<std::string, std::vector<TestResult>> m_testResults;
    
    void logTestResult(const TestResult& result);
    std::string formatDuration(std::chrono::milliseconds duration);
};

// Test macros for easier test writing
#define TEST_CASE(name, category) \
    TestFramework::registerTestCase(name, [this]() -> bool

#define ASSERT_TRUE(condition) \
    if (!TestFramework::assertTrue(condition, #condition)) return false;

#define ASSERT_FALSE(condition) \
    if (!TestFramework::assertFalse(condition, #condition)) return false;

#define ASSERT_EQUAL(expected, actual) \
    if (!TestFramework::assertEqual(expected, actual, #expected " == " #actual)) return false;

#define ASSERT_NOT_EQUAL(expected, actual) \
    if (!TestFramework::assertNotEqual(expected, actual, #expected " != " #actual)) return false;

#define ASSERT_NOT_NULL(ptr) \
    if (!TestFramework::assertNotNull(ptr, #ptr " is not null")) return false;

#define ASSERT_NULL(ptr) \
    if (!TestFramework::assertNull(ptr, #ptr " is null")) return false;

#define ASSERT_THROWS(func) \
    if (!TestFramework::assertThrows(func, #func " should throw")) return false;

// Specific test suites for MiningSoft components
class RandomXTestSuite : public TestSuite {
public:
    std::string getName() const override { return "RandomX Algorithm Tests"; }
    std::vector<TestResult> runTests() override;
};

class MinerTestSuite : public TestSuite {
public:
    std::string getName() const override { return "Miner Core Tests"; }
    std::vector<TestResult> runTests() override;
};

class PoolConnectionTestSuite : public TestSuite {
public:
    std::string getName() const override { return "Pool Connection Tests"; }
    std::vector<TestResult> runTests() override;
};

class WalletTestSuite : public TestSuite {
public:
    std::string getName() const override { return "Wallet Management Tests"; }
    std::vector<TestResult> runTests() override;
};

class CLITestSuite : public TestSuite {
public:
    std::string getName() const override { return "CLI Interface Tests"; }
    std::vector<TestResult> runTests() override;
};

class PerformanceTestSuite : public TestSuite {
public:
    std::string getName() const override { return "Performance Tests"; }
    std::vector<TestResult> runTests() override;
};

class IntegrationTestSuite : public TestSuite {
public:
    std::string getName() const override { return "Integration Tests"; }
    std::vector<TestResult> runTests() override;
};
