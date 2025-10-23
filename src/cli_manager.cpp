#include "cli_manager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

CLIManager::CLIManager() : m_running(false), m_interactive(false) {
    m_miner = std::make_unique<Miner>();
    m_config = std::make_unique<ConfigManager>();
    m_logger = std::make_unique<Logger>();
}

CLIManager::~CLIManager() {
    if (m_running) {
        stopMining();
    }
    if (m_statsThread.joinable()) {
        m_statsThread.join();
    }
}

bool CLIManager::initialize() {
    // Initialize logger
    if (!m_logger->initialize(Logger::Level::Info, "", true)) {
        std::cerr << "Failed to initialize logger" << std::endl;
        return false;
    }
    
    // Register commands
    registerCommands();
    
    // Load default config
    if (!loadConfig("config.json")) {
        std::cout << "Warning: Could not load config.json, using defaults" << std::endl;
    }
    
    return true;
}

void CLIManager::registerCommands() {
    m_commands["start"] = [this](const std::vector<std::string>& args) { handleStart(args); };
    m_commands["stop"] = [this](const std::vector<std::string>& args) { handleStop(args); };
    m_commands["status"] = [this](const std::vector<std::string>& args) { handleStatus(args); };
    m_commands["config"] = [this](const std::vector<std::string>& args) { handleConfig(args); };
    m_commands["help"] = [this](const std::vector<std::string>& args) { handleHelp(args); };
    m_commands["exit"] = [this](const std::vector<std::string>& args) { handleExit(args); };
    m_commands["quit"] = [this](const std::vector<std::string>& args) { handleExit(args); };
    m_commands["stats"] = [this](const std::vector<std::string>& args) { handleStats(args); };
    m_commands["connect"] = [this](const std::vector<std::string>& args) { handleConnect(args); };
    m_commands["disconnect"] = [this](const std::vector<std::string>& args) { handleDisconnect(args); };
    m_commands["set"] = [this](const std::vector<std::string>& args) { handleSet(args); };
    m_commands["show"] = [this](const std::vector<std::string>& args) { handleShow(args); };
    m_commands["clear"] = [this](const std::vector<std::string>& args) { clearScreen(); };
}

void CLIManager::run() {
    printBanner();
    printHelp();
    
    m_running = true;
    m_interactive = true;
    
    // Start stats monitoring thread
    m_statsThread = std::thread([this]() {
        while (m_running) {
            updateStats();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    
    runInteractive();
}

void CLIManager::runInteractive() {
    std::string input;
    
    while (m_running && m_interactive) {
        printPrompt();
        std::getline(std::cin, input);
        
        if (input.empty()) continue;
        
        auto args = parseCommand(input);
        if (args.empty()) continue;
        
        std::string command = args[0];
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);
        
        auto it = m_commands.find(command);
        if (it != m_commands.end()) {
            try {
                it->second(args);
            } catch (const std::exception& e) {
                std::cout << "Error executing command: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Unknown command: " << command << std::endl;
            std::cout << "Type 'help' for available commands" << std::endl;
        }
    }
}

void CLIManager::printPrompt() {
    std::cout << "\nMiningSoft> ";
    std::cout.flush();
}

std::vector<std::string> CLIManager::parseCommand(const std::string& input) {
    std::vector<std::string> args;
    std::istringstream iss(input);
    std::string arg;
    
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    return args;
}

void CLIManager::handleStart(const std::vector<std::string>& args) {
    if (m_miner->isRunning()) {
        std::cout << "Miner is already running" << std::endl;
        return;
    }
    
    std::cout << "Starting miner..." << std::endl;
    startMining();
}

void CLIManager::handleStop(const std::vector<std::string>& args) {
    if (!m_miner->isRunning()) {
        std::cout << "Miner is not running" << std::endl;
        return;
    }
    
    std::cout << "Stopping miner..." << std::endl;
    stopMining();
}

void CLIManager::handleStatus(const std::vector<std::string>& args) {
    printStatus();
}

void CLIManager::handleConfig(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        printConfig();
        return;
    }
    
    std::string subcommand = args[1];
    std::transform(subcommand.begin(), subcommand.end(), subcommand.begin(), ::tolower);
    
    if (subcommand == "show" || subcommand == "list") {
        printConfig();
    } else if (subcommand == "load" && args.size() > 2) {
        if (loadConfig(args[2])) {
            std::cout << "Configuration loaded from " << args[2] << std::endl;
        } else {
            std::cout << "Failed to load configuration from " << args[2] << std::endl;
        }
    } else if (subcommand == "save" && args.size() > 2) {
        if (saveConfig(args[2])) {
            std::cout << "Configuration saved to " << args[2] << std::endl;
        } else {
            std::cout << "Failed to save configuration to " << args[2] << std::endl;
        }
    } else {
        std::cout << "Usage: config [show|load <file>|save <file>]" << std::endl;
    }
}

void CLIManager::handleHelp(const std::vector<std::string>& args) {
    printHelp();
}

void CLIManager::handleExit(const std::vector<std::string>& args) {
    std::cout << "Exiting..." << std::endl;
    m_running = false;
    m_interactive = false;
}

void CLIManager::handleStats(const std::vector<std::string>& args) {
    printStats();
}

void CLIManager::handleConnect(const std::vector<std::string>& args) {
    if (m_miner->isConnected()) {
        std::cout << "Already connected to pool" << std::endl;
        return;
    }
    
    std::cout << "Connecting to pool..." << std::endl;
    connectToPool();
}

void CLIManager::handleDisconnect(const std::vector<std::string>& args) {
    if (!m_miner->isConnected()) {
        std::cout << "Not connected to pool" << std::endl;
        return;
    }
    
    std::cout << "Disconnecting from pool..." << std::endl;
    disconnectFromPool();
}

void CLIManager::handleSet(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cout << "Usage: set <key> <value>" << std::endl;
        return;
    }
    
    std::string key = args[1];
    std::string value = args[2];
    
    if (setConfigValue(key, value)) {
        std::cout << "Set " << key << " = " << value << std::endl;
    } else {
        std::cout << "Failed to set " << key << " = " << value << std::endl;
    }
}

void CLIManager::handleShow(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Usage: show <item>" << std::endl;
        std::cout << "Available items: stats, status, config, version" << std::endl;
        return;
    }
    
    std::string item = args[1];
    std::transform(item.begin(), item.end(), item.begin(), ::tolower);
    
    if (item == "stats") {
        printStats();
    } else if (item == "status") {
        printStatus();
    } else if (item == "config") {
        printConfig();
    } else if (item == "version") {
        printVersion();
    } else {
        std::cout << "Unknown item: " << item << std::endl;
    }
}

void CLIManager::updateStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    // Update basic stats
    m_stats.isMining = m_miner->isRunning();
    m_stats.isConnected = m_miner->isConnected();
    
    // Simulate some stats for now (in real implementation, get from miner)
    if (m_stats.isMining) {
        m_stats.hashrate += (rand() % 100) - 50; // Simulate hashrate variation
        if (m_stats.hashrate < 0) m_stats.hashrate = 0;
        m_stats.totalHashes += 1000;
        m_stats.nonceCount++;
    }
}

void CLIManager::printStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    clearScreen();
    printBanner();
    printSeparator();
    
    std::cout << "MINING STATISTICS" << std::endl;
    printSeparator();
    
    std::cout << "Status: " << (m_stats.isMining ? "MINING" : "IDLE") << std::endl;
    std::cout << "Pool: " << (m_stats.isConnected ? "CONNECTED" : "DISCONNECTED") << std::endl;
    std::cout << "Hashrate: " << formatHashrate(m_stats.hashrate) << std::endl;
    std::cout << "Total Hashes: " << m_stats.totalHashes << std::endl;
    std::cout << "Accepted Shares: " << m_stats.acceptedShares << std::endl;
    std::cout << "Rejected Shares: " << m_stats.rejectedShares << std::endl;
    std::cout << "Temperature: " << std::fixed << std::setprecision(1) << m_stats.temperature << "°C" << std::endl;
    
    if (m_stats.isMining && m_stats.startTime.time_since_epoch().count() > 0) {
        std::cout << "Runtime: " << formatTime(m_stats.startTime) << std::endl;
    }
    
    printSeparator();
    std::cout << "Press 'q' to return to main menu" << std::endl;
}

void CLIManager::printStatus() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    std::cout << "\n=== MINER STATUS ===" << std::endl;
    std::cout << "Mining: " << (m_stats.isMining ? "YES" : "NO") << std::endl;
    std::cout << "Pool Connected: " << (m_stats.isConnected ? "YES" : "NO") << std::endl;
    std::cout << "Hashrate: " << formatHashrate(m_stats.hashrate) << std::endl;
    std::cout << "Total Hashes: " << m_stats.totalHashes << std::endl;
    std::cout << "Shares: " << m_stats.acceptedShares << " accepted, " << m_stats.rejectedShares << " rejected" << std::endl;
    std::cout << "===================" << std::endl;
}

void CLIManager::printHelp() {
    std::cout << "\n=== MININGSOFT CLI COMMANDS ===" << std::endl;
    std::cout << "start                    - Start mining" << std::endl;
    std::cout << "stop                     - Stop mining" << std::endl;
    std::cout << "status                   - Show current status" << std::endl;
    std::cout << "stats                    - Show detailed statistics" << std::endl;
    std::cout << "connect                  - Connect to mining pool" << std::endl;
    std::cout << "disconnect               - Disconnect from pool" << std::endl;
    std::cout << "config [show|load|save]  - Configuration management" << std::endl;
    std::cout << "set <key> <value>        - Set configuration value" << std::endl;
    std::cout << "show <item>              - Show specific information" << std::endl;
    std::cout << "clear                    - Clear screen" << std::endl;
    std::cout << "help                     - Show this help" << std::endl;
    std::cout << "exit/quit                - Exit program" << std::endl;
    std::cout << "=================================" << std::endl;
}

void CLIManager::printConfig() {
    std::cout << "\n=== CURRENT CONFIGURATION ===" << std::endl;
    std::cout << "Pool URL: " << m_config->getPoolConfig().url << std::endl;
    std::cout << "Username: " << m_config->getPoolConfig().username << std::endl;
    std::cout << "Password: " << std::string(m_config->getPoolConfig().password.length(), '*') << std::endl;
    std::cout << "Worker ID: " << m_config->getPoolConfig().workerId << std::endl;
    std::cout << "Threads: " << m_config->getMiningConfig().threads << std::endl;
    std::cout << "Use GPU: " << (m_config->getMiningConfig().useGPU ? "YES" : "NO") << std::endl;
    std::cout << "Intensity: " << m_config->getMiningConfig().intensity << std::endl;
    std::cout << "=============================" << std::endl;
}

bool CLIManager::loadConfig(const std::string& filename) {
    return m_config->loadFromFile(filename);
}

bool CLIManager::saveConfig(const std::string& filename) {
    return m_config->saveToFile(filename);
}

bool CLIManager::setConfigValue(const std::string& key, const std::string& value) {
    // This would need to be implemented in ConfigManager
    // For now, just return true
    return true;
}

void CLIManager::startMining() {
    if (!m_miner->isInitialized()) {
        if (!m_miner->initialize(*m_config)) {
            std::cout << "Failed to initialize miner" << std::endl;
            return;
        }
    }
    
    m_miner->start();
    m_stats.startTime = std::chrono::steady_clock::now();
    std::cout << "Miner started successfully" << std::endl;
}

void CLIManager::stopMining() {
    m_miner->stop();
    std::cout << "Miner stopped" << std::endl;
}

void CLIManager::connectToPool() {
    if (!m_miner->isInitialized()) {
        if (!m_miner->initialize(*m_config)) {
            std::cout << "Failed to initialize miner" << std::endl;
            return;
        }
    }
    
    std::cout << "Connected to pool" << std::endl;
}

void CLIManager::disconnectFromPool() {
    m_miner->stop();
    std::cout << "Disconnected from pool" << std::endl;
}

void CLIManager::printBanner() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    MININGSOFT v1.0.0                        ║" << std::endl;
    std::cout << "║              Monero Miner for Apple Silicon                 ║" << std::endl;
    std::cout << "║              M1, M2, M3, M4, M5 Compatible                  ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
}

void CLIManager::printVersion() {
    std::cout << "MiningSoft v1.0.0" << std::endl;
    std::cout << "Monero Miner for Apple Silicon" << std::endl;
    std::cout << "Compatible with M1, M2, M3, M4, M5 processors" << std::endl;
}

std::string CLIManager::formatHashrate(double hashrate) {
    if (hashrate >= 1e9) {
        return std::to_string(hashrate / 1e9) + " GH/s";
    } else if (hashrate >= 1e6) {
        return std::to_string(hashrate / 1e6) + " MH/s";
    } else if (hashrate >= 1e3) {
        return std::to_string(hashrate / 1e3) + " KH/s";
    } else {
        return std::to_string(hashrate) + " H/s";
    }
}

std::string CLIManager::formatTime(std::chrono::steady_clock::time_point start) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start);
    
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration % std::chrono::hours(1));
    auto seconds = duration % std::chrono::minutes(1);
    
    std::ostringstream oss;
    oss << hours.count() << "h " << minutes.count() << "m " << seconds.count() << "s";
    return oss.str();
}

void CLIManager::clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void CLIManager::printSeparator() {
    std::cout << "──────────────────────────────────────────────────────────────" << std::endl;
}
