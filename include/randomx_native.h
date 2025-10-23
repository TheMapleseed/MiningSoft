#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <atomic>

/**
 * Native RandomX implementation for Apple Silicon
 * No external dependencies - everything implemented locally
 */
class RandomXNative {
public:
    RandomXNative();
    ~RandomXNative();

    // Initialize RandomX with seed
    bool initialize(const std::vector<uint8_t>& seed);
    
    // Hash input data using RandomX algorithm
    std::vector<uint8_t> hash(const std::vector<uint8_t>& input);
    
    // Verify a hash against target difficulty
    bool verifyHash(const std::vector<uint8_t>& hash, uint64_t target);
    
    // Get optimal thread count for current hardware
    int getOptimalThreadCount() const;
    
    // Get memory usage statistics
    struct MemoryStats {
        size_t allocatedMemory;
        size_t usedMemory;
        size_t availableMemory;
    };
    
    MemoryStats getMemoryStats() const;

private:
    // RandomX constants
    static constexpr size_t RANDOMX_DATASET_SIZE = 2ULL * 1024 * 1024 * 1024; // 2GB
    static constexpr size_t RANDOMX_CACHE_SIZE = 64 * 1024 * 1024; // 64MB
    static constexpr size_t RANDOMX_PROGRAM_SIZE = 256;
    static constexpr size_t RANDOMX_PROGRAM_COUNT = 8;
    static constexpr size_t RANDOMX_SCRATCHPAD_SIZE = 2097152; // 2MB
    
    // RandomX dataset and cache
    std::vector<uint8_t> m_dataset;
    std::vector<uint8_t> m_cache;
    
    // RandomX programs
    struct RandomXProgram {
        std::vector<uint8_t> code;
        std::vector<uint8_t> scratchpad;
        uint64_t registers[8];
        uint64_t flags;
    };
    
    std::vector<RandomXProgram> m_programs;
    
    // RandomX instruction set
    enum class RandomXInstruction {
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
    
    // RandomX instruction structure
    struct RandomXInstructionData {
        RandomXInstruction opcode;
        uint8_t dst;
        uint8_t src;
        uint32_t imm32;
        uint64_t target;
    };
    
    // RandomX execution functions
    void executeProgram(RandomXProgram& program, const std::vector<uint8_t>& input);
    void executeInstruction(RandomXProgram& program, const RandomXInstructionData& instruction);
    
    // RandomX dataset initialization
    void initializeDataset(const std::vector<uint8_t>& seed);
    void initializeCache(const std::vector<uint8_t>& seed);
    
    // RandomX program generation
    void generateProgram(RandomXProgram& program, const std::vector<uint8_t>& input);
    void generateInstruction(RandomXInstructionData& instruction, uint64_t& pc);
    
    // RandomX hash functions
    void blake2bHash(const uint8_t* input, size_t inputSize, uint8_t* output);
    void argon2Hash(const uint8_t* input, size_t inputSize, uint8_t* output);
    void keccakHash(const uint8_t* input, size_t inputSize, uint8_t* output);
    
    // RandomX utility functions
    uint64_t readUint64(const uint8_t* data);
    void writeUint64(uint8_t* data, uint64_t value);
    uint32_t readUint32(const uint8_t* data);
    void writeUint32(uint8_t* data, uint32_t value);
    
    // RandomX memory management
    void* allocateAlignedMemory(size_t size, size_t alignment = 64);
    void deallocateAlignedMemory(void* ptr, size_t size);
    
    // RandomX ARM64 optimizations
    void optimizeForAppleSilicon();
    void vectorizedHash(const uint8_t* input, size_t inputSize, uint8_t* output);
    
    // RandomX state
    bool m_initialized{false};
    int m_threadCount{0};
    
    // Performance counters
    mutable std::atomic<uint64_t> m_hashCount{0};
    mutable std::atomic<uint64_t> m_cycleCount{0};
};
