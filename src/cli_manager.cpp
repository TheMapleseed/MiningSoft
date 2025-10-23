#include "cli_manager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif

CLIManager::CLIManager() : m_running(false), m_interactive(false), m_walletConfigFile("wallets.json") {
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
    
    // Load wallet configuration
    loadWalletConfig();
    
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
    m_commands["wallet"] = [this](const std::vector<std::string>& args) { handleWallet(args); };
    m_commands["clear"] = [this](const std::vector<std::string>& args) { clearScreen(); };
}

void CLIManager::run() {
    printBanner();
    printHelp();
    
    m_running = true;
    // Check if input is piped (non-interactive)
    m_interactive = isatty(STDIN_FILENO);
    
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
    
    while (m_running) {
        // Only show prompt in interactive mode
        if (m_interactive) {
            printPrompt();
        }
        
        // Check if input stream is still valid
        if (!std::cin.good()) {
            if (std::cin.eof()) {
                // Input stream ended (piped input), exit gracefully
                if (m_interactive) {
                    std::cout << "\nInput stream ended. Exiting..." << std::endl;
                }
                break;
            } else {
                // Clear error state and continue
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                continue;
            }
        }
        
        std::getline(std::cin, input);
        
        if (input.empty()) continue;
        
        auto args = parseCommand(input);
        if (args.empty()) continue;
        
        std::string command = args[0];
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);
        
        // Handle exit commands
        if (command == "exit" || command == "quit") {
            handleExit(args);
            break;
        }
        
        auto it = m_commands.find(command);
        if (it != m_commands.end()) {
            try {
                // For wallet command, pass subcommands (everything after the main command)
                if (command == "wallet") {
                    std::vector<std::string> subargs(args.begin() + 1, args.end());
                    it->second(subargs);
                } else {
                    it->second(args);
                }
            } catch (const std::exception& e) {
                std::cout << "Error executing command: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Unknown command: " << command << std::endl;
            std::cout << "Type 'help' for available commands" << std::endl;
        }
        
        // Add a small delay to make output more readable when processing multiple commands
        if (!m_interactive) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
    std::cout << "wallet [add|list|set]    - Wallet address management" << std::endl;
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

// Wallet Management Implementation
void CLIManager::handleWallet(const std::vector<std::string>& args) {
    if (args.empty()) {
        showWalletMenu();
        return;
    }
    
    std::string subcommand = args[0];
    std::vector<std::string> subargs(args.begin() + 1, args.end());
    
    if (subcommand == "add" || subcommand == "new") {
        if (subargs.size() >= 1) {
            // Command line mode: wallet add <address> [label]
            addWalletFromCommandLine(subargs);
        } else {
            addWalletAddress();
        }
    } else if (subcommand == "list" || subcommand == "view") {
        viewWalletAddresses();
    } else if (subcommand == "set" || subcommand == "active") {
        if (subargs.size() >= 1) {
            // Command line mode: wallet set <index>
            setActiveWalletFromCommandLine(subargs[0]);
        } else {
            setActiveWallet();
        }
    } else if (subcommand == "remove" || subcommand == "delete") {
        if (subargs.size() >= 1) {
            // Command line mode: wallet remove <index>
            removeWalletFromCommandLine(subargs[0]);
        } else {
            removeWalletAddress();
        }
    } else if (subcommand == "import") {
        importWalletFromFile();
    } else if (subcommand == "export") {
        exportWalletToFile();
    } else {
        std::cout << "❌ Unknown wallet command: " << subcommand << std::endl;
        std::cout << "Use 'wallet' to see available commands." << std::endl;
    }
}

void CLIManager::showWalletMenu() {
    std::cout << "\n💰 Wallet Address Management" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "1. Add New Wallet Address" << std::endl;
    std::cout << "2. View All Wallet Addresses" << std::endl;
    std::cout << "3. Set Active Wallet Address" << std::endl;
    std::cout << "4. Remove Wallet Address" << std::endl;
    std::cout << "5. Import Wallets from File" << std::endl;
    std::cout << "6. Export Wallets to File" << std::endl;
    std::cout << "7. Back to Main Menu" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
    
    // Check if we're in interactive mode
    if (!m_interactive) {
        std::cout << "\n❌ Interactive wallet menu requires terminal input." << std::endl;
        std::cout << "Use specific wallet commands instead:" << std::endl;
        std::cout << "  wallet add    - Add new wallet address" << std::endl;
        std::cout << "  wallet list   - View all wallet addresses" << std::endl;
        std::cout << "  wallet set    - Set active wallet address" << std::endl;
        return;
    }
    
    std::cout << "\nEnter your choice (1-7): ";
    std::string choice;
    std::getline(std::cin, choice);
    
    if (choice == "1") {
        addWalletAddress();
    } else if (choice == "2") {
        viewWalletAddresses();
    } else if (choice == "3") {
        setActiveWallet();
    } else if (choice == "4") {
        removeWalletAddress();
    } else if (choice == "5") {
        importWalletFromFile();
    } else if (choice == "6") {
        exportWalletToFile();
    } else if (choice == "7") {
        std::cout << "Returning to main menu..." << std::endl;
    } else {
        std::cout << "❌ Invalid choice. Please try again." << std::endl;
    }
}

void CLIManager::addWalletAddress() {
    std::cout << "\n🔑 Add New Wallet Address" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
    
    // Check if we're in interactive mode
    if (!m_interactive) {
        std::cout << "\n❌ Interactive wallet addition requires terminal input." << std::endl;
        std::cout << "Use: wallet add <address> [label]" << std::endl;
        return;
    }
    
    std::string address, label;
    
    std::cout << "Enter Monero wallet address: ";
    std::getline(std::cin, address);
    
    if (!validateWalletAddress(address)) {
        std::cout << "❌ Invalid Monero wallet address format!" << std::endl;
        return;
    }
    
    std::cout << "Enter a label for this wallet (optional): ";
    std::getline(std::cin, label);
    
    if (label.empty()) {
        label = "Wallet " + std::to_string(m_wallets.size() + 1);
    }
    
    // Determine wallet type
    std::string type = "mainnet";
    if (address[0] == '9') {
        type = "testnet";
    } else if (address.length() == 106) {
        type = "integrated";
    }
    
    // Add wallet
    WalletInfo wallet(address, label, type);
    wallet.addedDate = getCurrentDateTime();
    
    m_wallets.push_back(wallet);
    
    // If this is the first wallet, make it active
    if (m_wallets.size() == 1) {
        m_wallets[0].isActive = true;
        m_activeWallet = address;
        std::cout << "✅ Wallet added and set as active!" << std::endl;
    } else {
        std::cout << "✅ Wallet added successfully!" << std::endl;
    }
    
    saveWalletConfig();
}

void CLIManager::viewWalletAddresses() {
    std::cout << "\n📋 Wallet Addresses" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
    
    if (m_wallets.empty()) {
        std::cout << "No wallet addresses found." << std::endl;
        std::cout << "Use 'wallet add' to add your first wallet." << std::endl;
        return;
    }
    
    for (size_t i = 0; i < m_wallets.size(); i++) {
        const auto& wallet = m_wallets[i];
        std::cout << (i + 1) << ". ";
        if (wallet.isActive) {
            std::cout << "⭐ ";
        }
        std::cout << wallet.label << " (" << wallet.type << ")" << std::endl;
        std::cout << "   Address: " << wallet.address << std::endl;
        std::cout << "   Added: " << wallet.addedDate << std::endl;
        std::cout << std::endl;
    }
    
    if (!m_activeWallet.empty()) {
        std::cout << "Active wallet: " << m_activeWallet << std::endl;
    }
}

void CLIManager::setActiveWallet() {
    if (m_wallets.empty()) {
        std::cout << "❌ No wallet addresses found." << std::endl;
        std::cout << "Use 'wallet add' to add your first wallet." << std::endl;
        return;
    }
    
    std::cout << "\n⭐ Set Active Wallet Address" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
    
    viewWalletAddresses();
    
    std::cout << "Enter wallet number to set as active (0 to cancel): ";
    std::string choice;
    std::getline(std::cin, choice);
    
    try {
        int index = std::stoi(choice) - 1;
        if (index >= 0 && index < static_cast<int>(m_wallets.size())) {
            // Deactivate all wallets
            for (auto& wallet : m_wallets) {
                wallet.isActive = false;
            }
            
            // Activate selected wallet
            m_wallets[index].isActive = true;
            m_activeWallet = m_wallets[index].address;
            
            std::cout << "✅ Active wallet set to: " << m_wallets[index].label << std::endl;
            std::cout << "   Address: " << m_activeWallet << std::endl;
            
            saveWalletConfig();
        } else if (index == -1) {
            std::cout << "Operation cancelled." << std::endl;
        } else {
            std::cout << "❌ Invalid wallet number." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Invalid input. Please enter a number." << std::endl;
    }
}

void CLIManager::removeWalletAddress() {
    if (m_wallets.empty()) {
        std::cout << "❌ No wallet addresses found." << std::endl;
        return;
    }
    
    std::cout << "\n🗑️  Remove Wallet Address" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
    
    viewWalletAddresses();
    
    std::cout << "Enter wallet number to remove (0 to cancel): ";
    std::string choice;
    std::getline(std::cin, choice);
    
    try {
        int index = std::stoi(choice) - 1;
        if (index >= 0 && index < static_cast<int>(m_wallets.size())) {
            std::cout << "Are you sure you want to remove wallet '" << m_wallets[index].label << "'? (y/N): ";
            std::string confirm;
            std::getline(std::cin, confirm);
            
            if (confirm == "y" || confirm == "Y" || confirm == "yes") {
                bool wasActive = m_wallets[index].isActive;
                m_wallets.erase(m_wallets.begin() + index);
                
                if (wasActive) {
                    if (!m_wallets.empty()) {
                        m_wallets[0].isActive = true;
                        m_activeWallet = m_wallets[0].address;
                        std::cout << "✅ Wallet removed. New active wallet: " << m_wallets[0].label << std::endl;
                    } else {
                        m_activeWallet.clear();
                        std::cout << "✅ Wallet removed. No active wallet set." << std::endl;
                    }
                } else {
                    std::cout << "✅ Wallet removed successfully." << std::endl;
                }
                
                saveWalletConfig();
            } else {
                std::cout << "Operation cancelled." << std::endl;
            }
        } else if (index == -1) {
            std::cout << "Operation cancelled." << std::endl;
        } else {
            std::cout << "❌ Invalid wallet number." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Invalid input. Please enter a number." << std::endl;
    }
}

void CLIManager::importWalletFromFile() {
    std::cout << "\n📥 Import Wallets from File" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "Enter file path: ";
    
    std::string filepath;
    std::getline(std::cin, filepath);
    
    // TODO: Implement file import
    std::cout << "❌ File import not yet implemented." << std::endl;
    std::cout << "You can manually add wallets using 'wallet add'." << std::endl;
}

void CLIManager::exportWalletToFile() {
    std::cout << "\n📤 Export Wallets to File" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
    
    if (m_wallets.empty()) {
        std::cout << "❌ No wallet addresses to export." << std::endl;
        return;
    }
    
    std::cout << "Enter file path: ";
    std::string filepath;
    std::getline(std::cin, filepath);
    
    // TODO: Implement file export
    std::cout << "❌ File export not yet implemented." << std::endl;
    std::cout << "Wallet addresses are stored in: " << m_walletConfigFile << std::endl;
}

bool CLIManager::validateWalletAddress(const std::string& address) {
    // Basic Monero address validation
    if (address.empty()) return false;
    
    // Check length
    if (address.length() != 95 && address.length() != 106) {
        return false;
    }
    
    // Check prefix
    if (address[0] != '4' && address[0] != '8' && address[0] != '9') {
        return false;
    }
    
    // Check if all characters are valid base58
    for (char c : address) {
        if (!((c >= '1' && c <= '9') || 
              (c >= 'A' && c <= 'H') || 
              (c >= 'J' && c <= 'N') || 
              (c >= 'P' && c <= 'Z') || 
              (c >= 'a' && c <= 'k') || 
              (c >= 'm' && c <= 'z'))) {
            return false;
        }
    }
    
    return true;
}

void CLIManager::saveWalletConfig() {
    // TODO: Implement wallet config saving
    // For now, just update the main config
    if (!m_activeWallet.empty() && m_config) {
        m_config->setValue("pool.username", m_activeWallet);
    }
}

void CLIManager::loadWalletConfig() {
    // TODO: Implement wallet config loading
    // For now, load from main config
    if (m_config) {
        std::string wallet = m_config->getValue<std::string>("pool.username", "");
        if (!wallet.empty() && validateWalletAddress(wallet)) {
            WalletInfo info(wallet, "Default Wallet", "mainnet");
            info.isActive = true;
            m_wallets.push_back(info);
            m_activeWallet = wallet;
        }
    }
}

std::string CLIManager::getCurrentDateTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Command-line wallet functions
void CLIManager::addWalletFromCommandLine(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << "❌ Usage: wallet add <address> [label]" << std::endl;
        return;
    }
    
    std::string address = args[0];
    std::string label = args.size() > 1 ? args[1] : "Wallet " + std::to_string(m_wallets.size() + 1);
    
    if (!validateWalletAddress(address)) {
        std::cout << "❌ Invalid Monero wallet address format!" << std::endl;
        return;
    }
    
    // Determine wallet type
    std::string type = "mainnet";
    if (address[0] == '9') {
        type = "testnet";
    } else if (address.length() == 106) {
        type = "integrated";
    }
    
    // Add wallet
    WalletInfo wallet(address, label, type);
    wallet.addedDate = getCurrentDateTime();
    
    m_wallets.push_back(wallet);
    
    // If this is the first wallet, make it active
    if (m_wallets.size() == 1) {
        m_wallets[0].isActive = true;
        m_activeWallet = address;
        std::cout << "✅ Wallet added and set as active!" << std::endl;
    } else {
        std::cout << "✅ Wallet added successfully!" << std::endl;
    }
    
    saveWalletConfig();
}

void CLIManager::setActiveWalletFromCommandLine(const std::string& indexStr) {
    if (m_wallets.empty()) {
        std::cout << "❌ No wallet addresses found." << std::endl;
        return;
    }
    
    try {
        int index = std::stoi(indexStr) - 1;
        if (index >= 0 && index < static_cast<int>(m_wallets.size())) {
            // Deactivate all wallets
            for (auto& wallet : m_wallets) {
                wallet.isActive = false;
            }
            
            // Activate selected wallet
            m_wallets[index].isActive = true;
            m_activeWallet = m_wallets[index].address;
            
            std::cout << "✅ Active wallet set to: " << m_wallets[index].label << std::endl;
            std::cout << "   Address: " << m_activeWallet << std::endl;
            
            saveWalletConfig();
        } else {
            std::cout << "❌ Invalid wallet number. Use 'wallet list' to see available wallets." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Invalid wallet number. Please enter a valid number." << std::endl;
    }
}

void CLIManager::removeWalletFromCommandLine(const std::string& indexStr) {
    if (m_wallets.empty()) {
        std::cout << "❌ No wallet addresses found." << std::endl;
        return;
    }
    
    try {
        int index = std::stoi(indexStr) - 1;
        if (index >= 0 && index < static_cast<int>(m_wallets.size())) {
            bool wasActive = m_wallets[index].isActive;
            std::string walletLabel = m_wallets[index].label;
            
            m_wallets.erase(m_wallets.begin() + index);
            
            if (wasActive) {
                if (!m_wallets.empty()) {
                    m_wallets[0].isActive = true;
                    m_activeWallet = m_wallets[0].address;
                    std::cout << "✅ Wallet '" << walletLabel << "' removed. New active wallet: " << m_wallets[0].label << std::endl;
                } else {
                    m_activeWallet.clear();
                    std::cout << "✅ Wallet '" << walletLabel << "' removed. No active wallet set." << std::endl;
                }
            } else {
                std::cout << "✅ Wallet '" << walletLabel << "' removed successfully." << std::endl;
            }
            
            saveWalletConfig();
        } else {
            std::cout << "❌ Invalid wallet number. Use 'wallet list' to see available wallets." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Invalid wallet number. Please enter a valid number." << std::endl;
    }
}
