#include "miner.h"
#include "config_manager.h"
#include "logger.h"
#include <iostream>
#include <signal.h>
#include <memory>

// Global miner instance for signal handling
std::unique_ptr<Miner> g_miner;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    if (g_miner) {
        std::cout << "\nReceived signal " << signal << ", shutting down gracefully...\n";
        g_miner->stop();
    }
}

// Print usage information
void printUsage(const char* programName) {
    std::cout << "Monero Miner for Apple Silicon\n";
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -c, --config <file>    Configuration file (default: config.json)\n";
    std::cout << "  -p, --pool <url>       Mining pool URL\n";
    std::cout << "  -u, --user <username>  Pool username\n";
    std::cout << "  -w, --pass <password>  Pool password\n";
    std::cout << "  -t, --threads <num>    Number of threads (0 = auto)\n";
    std::cout << "  --gpu                  Enable GPU mining\n";
    std::cout << "  --no-gpu               Disable GPU mining\n";
    std::cout << "  --intensity <0-100>    Mining intensity (default: 100)\n";
    std::cout << "  --thermal-limit <temp> CPU thermal limit in Celsius (default: 85)\n";
    std::cout << "  --log-level <level>    Log level: debug, info, warn, error (default: info)\n";
    std::cout << "  --log-file <file>      Log file path (default: console only)\n";
    std::cout << "  --help                 Show this help message\n";
    std::cout << "  --version              Show version information\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " -p stratum+tcp://pool.monero.hashvault.pro:4444 -u wallet -w x\n";
    std::cout << "  " << programName << " -c myconfig.json\n";
    std::cout << "  " << programName << " --pool stratum+tcp://pool.supportxmr.com:443 --user wallet --pass x --threads 8\n";
}

// Print version information
void printVersion() {
    std::cout << "Monero Miner for Apple Silicon v1.0.0\n";
    std::cout << "Compatible with all Apple Silicon: M1, M2, M3, M4, M5\n";
    std::cout << "Built with ARM64 optimizations and Metal GPU support\n";
    std::cout << "Features: GPU mining, Vector Processor support, CPU demand throttling\n";
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // Initialize logger first
        g_logger = std::make_unique<Logger>();
        
        // Check for help or version first
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h" || arg == "help") {
                printUsage(argv[0]);
                return 0;
            }
            if (arg == "--version" || arg == "-v" || arg == "version") {
                printVersion();
                return 0;
            }
        }
        
        // Parse command line arguments
        ConfigManager configManager;
        if (!configManager.loadFromArgs(argc, argv)) {
            std::cerr << "Failed to parse command line arguments\n";
            return 1;
        }
        
        // Initialize logger with configuration
        const auto logLevel = configManager.getValue<std::string>("log-level", "info");
        const auto logFile = configManager.getValue<std::string>("log-file", "");
        const auto console = configManager.getValue<bool>("console", true);
        
        auto level = Logger::Level::Info;
        if (logLevel == "debug") level = Logger::Level::Debug;
        else if (logLevel == "warn") level = Logger::Level::Warning;
        else if (logLevel == "error") level = Logger::Level::Error;
        
        LOG_INFO("Setting log level to: {}", logLevel);
        
        if (!g_logger->initialize(level, logFile, console)) {
            std::cerr << "Failed to initialize logger\n";
            return 1;
        }
        
        LOG_INFO("Starting Monero Miner for Apple Silicon v1.0.0");
        LOG_INFO("Compatible with all Apple Silicon: M1, M2, M3, M4, M5");
        
        // Create and initialize miner
        g_miner = std::make_unique<Miner>();
        
        // Initialize miner with command line configuration
        if (!g_miner->initialize(configManager)) {
            LOG_ERROR("Failed to initialize miner");
            return 1;
        }
        
        LOG_INFO("Miner initialized successfully");
        
        // Start mining
        g_miner->start();
        LOG_INFO("Mining started");
        
        // Main loop - wait for miner to finish
        while (g_miner->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        LOG_INFO("Mining stopped");
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        if (g_logger) {
            LOG_CRITICAL("Fatal error: {}", e.what());
        }
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        if (g_logger) {
            LOG_CRITICAL("Unknown fatal error occurred");
        }
        return 1;
    }
    
    return 0;
}
