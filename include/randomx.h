#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <random>

class RandomX {
public:
    RandomX();
    ~RandomX();
    
    // Initialize RandomX algorithm
    bool initialize();
    
    // Calculate hash for given input
    void calculateHash(const uint8_t* input, size_t inputSize, uint8_t* output);
    
    // Check if hash meets target
    bool isValidHash(const uint8_t* hash, const uint8_t* target);
    
    // Utility functions
    static std::vector<uint8_t> hexToBytes(const std::string& hex);
    static std::string bytesToHex(const uint8_t* bytes, size_t length);

private:
    class RandomXVM {
    public:
        RandomXVM();
        void reset();
        void loadProgram(const uint8_t* seed, size_t seedSize);
        void execute();
        uint64_t getRegister(int index) const;
        void setRegister(int index, uint64_t value);
        
    private:
        std::array<uint64_t, 8> registers;
        std::array<uint64_t, 8> scratchpad;
        std::vector<uint8_t> program;
        std::mt19937_64 rng;
        
        uint64_t rotateRight64(uint64_t x, int n);
        uint64_t rotateLeft64(uint64_t x, int n);
    };
    
    std::unique_ptr<RandomXVM> m_vm;
};
