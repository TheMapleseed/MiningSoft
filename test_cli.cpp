#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

std::vector<std::string> parseCommand(const std::string& input) {
    std::vector<std::string> args;
    std::istringstream iss(input);
    std::string arg;
    
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    return args;
}

int main() {
    std::cout << "Testing CLI input processing..." << std::endl;
    
    std::string input;
    while (std::getline(std::cin, input)) {
        std::cout << "Received input: '" << input << "'" << std::endl;
        
        if (input.empty()) {
            std::cout << "Empty input, continuing..." << std::endl;
            continue;
        }
        
        auto args = parseCommand(input);
        std::cout << "Parsed " << args.size() << " arguments:" << std::endl;
        for (size_t i = 0; i < args.size(); i++) {
            std::cout << "  [" << i << "] = '" << args[i] << "'" << std::endl;
        }
        
        if (args[0] == "wallet") {
            std::cout << "WALLET COMMAND DETECTED!" << std::endl;
        } else if (args[0] == "help") {
            std::cout << "HELP COMMAND DETECTED!" << std::endl;
        } else if (args[0] == "exit") {
            std::cout << "EXIT COMMAND DETECTED!" << std::endl;
            break;
        } else {
            std::cout << "Unknown command: " << args[0] << std::endl;
        }
        
        std::cout << "MiningSoft> ";
        std::cout.flush();
    }
    
    std::cout << "CLI test completed." << std::endl;
    return 0;
}
