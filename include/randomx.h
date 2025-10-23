#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <random>
#include <chrono>

// RandomX constants
constexpr size_t RANDOMX_CACHE_SIZE = 2097152; // 2MB
constexpr size_t RANDOMX_DATASET_SIZE = 1073741824; // 1GB
constexpr size_t RANDOMX_PROGRAM_SIZE = 256;
constexpr size_t RANDOMX_PROGRAM_COUNT = 8;
constexpr size_t RANDOMX_SCRATCHPAD_SIZE = 2097152; // 2MB
constexpr size_t RANDOMX_HASH_SIZE = 32;

// RandomX instruction types
enum class RandomXInstructionType {
    IADD_RS = 0,
    IADD_M = 1,
    ISUB_R = 2,
    ISUB_M = 3,
    IMUL_R = 4,
    IMUL_M = 5,
    IMULH_R = 6,
    IMULH_M = 7,
    ISMULH_R = 8,
    ISMULH_M = 9,
    IMUL_RCP = 10,
    INEG_R = 11,
    IXOR_R = 12,
    IXOR_M = 13,
    IROR_R = 14,
    IROL_R = 15,
    ISWAP_R = 16,
    FSWAP_R = 17,
    FADD_R = 18,
    FADD_M = 19,
    FSUB_R = 20,
    FSUB_M = 21,
    FSCAL_R = 22,
    FMUL_R = 23,
    FDIV_M = 24,
    FSQRT_R = 25,
    CBRANCH = 26,
    CFROUND = 27,
    ISTORE = 28,
    NOP = 29
};

// RandomX instruction
struct RandomXInstruction {
    RandomXInstructionType type;
    uint8_t dst;
    uint8_t src;
    uint32_t imm32;
    uint64_t imm64;
    uint8_t mod;
    uint8_t modShift;
    uint8_t modMask;
};

// RandomX dataset item
struct RandomXDatasetItem {
    uint64_t data[8];
};

// RandomX cache
class RandomXCache {
public:
    RandomXCache();
    ~RandomXCache();
    
    bool initialize(const uint8_t* key, size_t keySize);
    void destroy();
    const RandomXDatasetItem* getDatasetItem(uint32_t index) const;
    
private:
    void* m_cache;
    void* m_dataset;
    bool m_initialized;
    
    void generateCache(const uint8_t* key, size_t keySize);
    void generateDataset();
};

// RandomX VM
class RandomXVM {
public:
    RandomXVM();
    ~RandomXVM();
    
    bool initialize(RandomXCache* cache, bool lightMode = false);
    void destroy();
    
    void reset();
    void loadProgram(const uint8_t* seed, size_t seedSize);
    void execute();
    
    // Register access
    uint64_t getRegister(int index) const;
    void setRegister(int index, uint64_t value);
    
    // Scratchpad access
    uint64_t getScratchpad(int index) const;
    void setScratchpad(int index, uint64_t value);
    
    // Program access
    const RandomXInstruction& getInstruction(int index) const;
    void setInstruction(int index, const RandomXInstruction& instruction);
    
    // Execution statistics
    uint64_t getInstructionCount() const { return m_instructionCount; }
    uint64_t getCycleCount() const { return m_cycleCount; }
    
private:
    // Registers
    std::array<uint64_t, 8> m_registers;
    std::array<double, 8> m_fregisters;
    
    // Scratchpad
    std::array<uint64_t, 8> m_scratchpad;
    
    // Program
    std::array<RandomXInstruction, RANDOMX_PROGRAM_SIZE> m_program;
    int m_programCounter;
    
    // Execution state
    uint64_t m_instructionCount;
    uint64_t m_cycleCount;
    uint32_t m_branchRegister;
    uint32_t m_branchTarget;
    
    // Cache reference
    RandomXCache* m_cache;
    bool m_lightMode;
    bool m_initialized;
    
    // RNG
    std::mt19937_64 m_rng;
    
    // Instruction execution
    void executeInstruction(const RandomXInstruction& instruction);
    void executeIADD_RS(const RandomXInstruction& instruction);
    void executeIADD_M(const RandomXInstruction& instruction);
    void executeISUB_R(const RandomXInstruction& instruction);
    void executeISUB_M(const RandomXInstruction& instruction);
    void executeIMUL_R(const RandomXInstruction& instruction);
    void executeIMUL_M(const RandomXInstruction& instruction);
    void executeIMULH_R(const RandomXInstruction& instruction);
    void executeIMULH_M(const RandomXInstruction& instruction);
    void executeISMULH_R(const RandomXInstruction& instruction);
    void executeISMULH_M(const RandomXInstruction& instruction);
    void executeIMUL_RCP(const RandomXInstruction& instruction);
    void executeINEG_R(const RandomXInstruction& instruction);
    void executeIXOR_R(const RandomXInstruction& instruction);
    void executeIXOR_M(const RandomXInstruction& instruction);
    void executeIROR_R(const RandomXInstruction& instruction);
    void executeIROL_R(const RandomXInstruction& instruction);
    void executeISWAP_R(const RandomXInstruction& instruction);
    void executeFSWAP_R(const RandomXInstruction& instruction);
    void executeFADD_R(const RandomXInstruction& instruction);
    void executeFADD_M(const RandomXInstruction& instruction);
    void executeFSUB_R(const RandomXInstruction& instruction);
    void executeFSUB_M(const RandomXInstruction& instruction);
    void executeFSCAL_R(const RandomXInstruction& instruction);
    void executeFMUL_R(const RandomXInstruction& instruction);
    void executeFDIV_M(const RandomXInstruction& instruction);
    void executeFSQRT_R(const RandomXInstruction& instruction);
    void executeCBRANCH(const RandomXInstruction& instruction);
    void executeCFROUND(const RandomXInstruction& instruction);
    void executeISTORE(const RandomXInstruction& instruction);
    void executeNOP(const RandomXInstruction& instruction);
    
    // Utility functions
    uint64_t rotateRight64(uint64_t x, int n);
    uint64_t rotateLeft64(uint64_t x, int n);
    uint64_t mulh(uint64_t a, uint64_t b);
    int64_t smulh(int64_t a, int64_t b);
    double int64ToDouble(uint64_t x);
    uint64_t doubleToInt64(double x);
    
    // Program generation
    void generateProgram(const uint8_t* seed, size_t seedSize);
    RandomXInstruction generateInstruction(uint32_t pc, const uint8_t* seed, size_t seedSize);
    uint32_t getRegisterMask(const RandomXInstruction& instruction);
    uint32_t getMemoryAddress(const RandomXInstruction& instruction);
};

// Main RandomX class
class RandomX {
public:
    RandomX();
    ~RandomX();
    
    // Initialize RandomX algorithm
    bool initialize(const uint8_t* key, size_t keySize, bool lightMode = false);
    void destroy();
    
    // Calculate hash for given input
    void calculateHash(const uint8_t* input, size_t inputSize, uint8_t* output);
    
    // Check if hash meets target
    bool isValidHash(const uint8_t* hash, const uint8_t* target);
    
    // Performance monitoring
    double getHashRate() const;
    uint64_t getTotalHashes() const { return m_totalHashes; }
    uint64_t getValidHashes() const { return m_validHashes; }
    double getAcceptanceRate() const;
    
    // Utility functions
    static std::vector<uint8_t> hexToBytes(const std::string& hex);
    static std::string bytesToHex(const uint8_t* bytes, size_t length);
    
    // Benchmarking
    double benchmark(uint32_t iterations = 1000);
    
private:
    RandomXCache* m_cache;
    std::vector<std::unique_ptr<RandomXVM>> m_vms;
    bool m_initialized;
    bool m_lightMode;
    
    // Performance tracking
    uint64_t m_totalHashes;
    uint64_t m_validHashes;
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_lastHashTime;
    
    // Threading
    int m_threadCount;
    
    // Internal methods
    void calculateHashInternal(const uint8_t* input, size_t inputSize, uint8_t* output);
    void generateProgram(const uint8_t* seed, size_t seedSize, RandomXVM* vm);
    void executeProgram(RandomXVM* vm);
    void finalizeHash(const uint8_t* input, size_t inputSize, uint8_t* output);
};
