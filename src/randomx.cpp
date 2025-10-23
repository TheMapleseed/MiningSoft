#include "randomx.h"
#include "logger.h"
#include <cstring>
#include <cstdint>
#include <vector>
#include <array>
#include <random>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cmath>

// RandomXCache Implementation
RandomXCache::RandomXCache() : m_cache(nullptr), m_dataset(nullptr), m_initialized(false) {
}

RandomXCache::~RandomXCache() {
    destroy();
}

bool RandomXCache::initialize(const uint8_t* key, size_t keySize) {
    if (m_initialized) {
        return true;
    }
    
    // Allocate cache memory
    m_cache = std::aligned_alloc(64, RANDOMX_CACHE_SIZE);
    if (!m_cache) {
        return false;
    }
    
    // Allocate dataset memory
    m_dataset = std::aligned_alloc(64, RANDOMX_DATASET_SIZE);
    if (!m_dataset) {
        std::free(m_cache);
        m_cache = nullptr;
        return false;
    }
    
    generateCache(key, keySize);
    generateDataset();
    
    m_initialized = true;
    return true;
}

void RandomXCache::destroy() {
    if (m_cache) {
        std::free(m_cache);
        m_cache = nullptr;
    }
    if (m_dataset) {
        std::free(m_dataset);
        m_dataset = nullptr;
    }
    m_initialized = false;
}

const RandomXDatasetItem* RandomXCache::getDatasetItem(uint32_t index) const {
    if (!m_initialized || !m_dataset) {
        return nullptr;
    }
    
    size_t offset = (index % (RANDOMX_DATASET_SIZE / sizeof(RandomXDatasetItem))) * sizeof(RandomXDatasetItem);
    return reinterpret_cast<RandomXDatasetItem*>(static_cast<uint8_t*>(m_dataset) + offset);
}

void RandomXCache::generateCache(const uint8_t* key, size_t keySize) {
    // Simplified cache generation
    // In a real implementation, this would use the full RandomX cache generation algorithm
    uint8_t* cacheBytes = static_cast<uint8_t*>(m_cache);
    for (size_t i = 0; i < RANDOMX_CACHE_SIZE; i += 32) {
        // Generate deterministic data based on key
        uint64_t seed = 0;
        for (size_t j = 0; j < keySize; j++) {
            seed ^= key[j] << ((j % 8) * 8);
        }
        seed ^= i;
        
        // Simple hash function
        uint64_t hash = seed;
        for (int k = 0; k < 4; k++) {
            hash = hash * 0x9e3779b97f4a7c15ULL;
            hash ^= hash >> 33;
        }
        
        // Store hash in cache
        for (int k = 0; k < 4; k++) {
            *reinterpret_cast<uint64_t*>(cacheBytes + i + k * 8) = hash;
            hash = hash * 0x9e3779b97f4a7c15ULL;
        }
    }
}

void RandomXCache::generateDataset() {
    // Simplified dataset generation
    uint8_t* datasetBytes = static_cast<uint8_t*>(m_dataset);
    uint8_t* cacheBytes = static_cast<uint8_t*>(m_cache);
    
    for (size_t i = 0; i < RANDOMX_DATASET_SIZE; i += 64) {
        uint32_t cacheIndex = (i / 64) % (RANDOMX_CACHE_SIZE / 64);
        std::memcpy(datasetBytes + i, cacheBytes + cacheIndex * 64, 64);
    }
}

// RandomXVM Implementation
RandomXVM::RandomXVM() 
    : m_programCounter(0), m_instructionCount(0), m_cycleCount(0), 
      m_branchRegister(0), m_branchTarget(0), m_cache(nullptr), 
      m_lightMode(false), m_initialized(false) {
    reset();
}

RandomXVM::~RandomXVM() {
    destroy();
}

bool RandomXVM::initialize(RandomXCache* cache, bool lightMode) {
    if (m_initialized) {
        return true;
    }
    
    m_cache = cache;
    m_lightMode = lightMode;
    m_initialized = true;
    
    return true;
}

void RandomXVM::destroy() {
    m_initialized = false;
    m_cache = nullptr;
}

void RandomXVM::reset() {
    std::fill(m_registers.begin(), m_registers.end(), 0);
    std::fill(m_fregisters.begin(), m_fregisters.end(), 0.0);
    std::fill(m_scratchpad.begin(), m_scratchpad.end(), 0);
    std::fill(m_program.begin(), m_program.end(), RandomXInstruction{});
    
    m_programCounter = 0;
    m_instructionCount = 0;
    m_cycleCount = 0;
    m_branchRegister = 0;
    m_branchTarget = 0;
}

void RandomXVM::loadProgram(const uint8_t* seed, size_t seedSize) {
    generateProgram(seed, seedSize);
}

void RandomXVM::execute() {
    if (!m_initialized) {
        return;
    }
    
    for (int i = 0; i < RANDOMX_PROGRAM_SIZE; i++) {
        executeInstruction(m_program[i]);
        m_instructionCount++;
        m_cycleCount++;
    }
}

uint64_t RandomXVM::getRegister(int index) const {
    if (index >= 0 && index < 8) {
        return m_registers[index];
    }
    return 0;
}

void RandomXVM::setRegister(int index, uint64_t value) {
    if (index >= 0 && index < 8) {
        m_registers[index] = value;
    }
}

uint64_t RandomXVM::getScratchpad(int index) const {
    if (index >= 0 && index < 8) {
        return m_scratchpad[index];
    }
    return 0;
}

void RandomXVM::setScratchpad(int index, uint64_t value) {
    if (index >= 0 && index < 8) {
        m_scratchpad[index] = value;
    }
}

const RandomXInstruction& RandomXVM::getInstruction(int index) const {
    if (index >= 0 && index < RANDOMX_PROGRAM_SIZE) {
        return m_program[index];
    }
    static RandomXInstruction empty = {};
    return empty;
}

void RandomXVM::setInstruction(int index, const RandomXInstruction& instruction) {
    if (index >= 0 && index < RANDOMX_PROGRAM_SIZE) {
        m_program[index] = instruction;
    }
}

void RandomXVM::executeInstruction(const RandomXInstruction& instruction) {
    switch (instruction.type) {
        case RandomXInstructionType::IADD_RS:
            executeIADD_RS(instruction);
            break;
        case RandomXInstructionType::IADD_M:
            executeIADD_M(instruction);
            break;
        case RandomXInstructionType::ISUB_R:
            executeISUB_R(instruction);
            break;
        case RandomXInstructionType::ISUB_M:
            executeISUB_M(instruction);
            break;
        case RandomXInstructionType::IMUL_R:
            executeIMUL_R(instruction);
            break;
        case RandomXInstructionType::IMUL_M:
            executeIMUL_M(instruction);
            break;
        case RandomXInstructionType::IMULH_R:
            executeIMULH_R(instruction);
            break;
        case RandomXInstructionType::IMULH_M:
            executeIMULH_M(instruction);
            break;
        case RandomXInstructionType::ISMULH_R:
            executeISMULH_R(instruction);
            break;
        case RandomXInstructionType::ISMULH_M:
            executeISMULH_M(instruction);
            break;
        case RandomXInstructionType::IMUL_RCP:
            executeIMUL_RCP(instruction);
            break;
        case RandomXInstructionType::INEG_R:
            executeINEG_R(instruction);
            break;
        case RandomXInstructionType::IXOR_R:
            executeIXOR_R(instruction);
            break;
        case RandomXInstructionType::IXOR_M:
            executeIXOR_M(instruction);
            break;
        case RandomXInstructionType::IROR_R:
            executeIROR_R(instruction);
            break;
        case RandomXInstructionType::IROL_R:
            executeIROL_R(instruction);
            break;
        case RandomXInstructionType::ISWAP_R:
            executeISWAP_R(instruction);
            break;
        case RandomXInstructionType::FSWAP_R:
            executeFSWAP_R(instruction);
            break;
        case RandomXInstructionType::FADD_R:
            executeFADD_R(instruction);
            break;
        case RandomXInstructionType::FADD_M:
            executeFADD_M(instruction);
            break;
        case RandomXInstructionType::FSUB_R:
            executeFSUB_R(instruction);
            break;
        case RandomXInstructionType::FSUB_M:
            executeFSUB_M(instruction);
            break;
        case RandomXInstructionType::FSCAL_R:
            executeFSCAL_R(instruction);
            break;
        case RandomXInstructionType::FMUL_R:
            executeFMUL_R(instruction);
            break;
        case RandomXInstructionType::FDIV_M:
            executeFDIV_M(instruction);
            break;
        case RandomXInstructionType::FSQRT_R:
            executeFSQRT_R(instruction);
            break;
        case RandomXInstructionType::CBRANCH:
            executeCBRANCH(instruction);
            break;
        case RandomXInstructionType::CFROUND:
            executeCFROUND(instruction);
            break;
        case RandomXInstructionType::ISTORE:
            executeISTORE(instruction);
            break;
        case RandomXInstructionType::NOP:
            executeNOP(instruction);
            break;
    }
}

// Instruction implementations
void RandomXVM::executeIADD_RS(const RandomXInstruction& instruction) {
    uint64_t src = m_registers[instruction.src];
    uint64_t dst = m_registers[instruction.dst];
    m_registers[instruction.dst] = dst + (src << instruction.modShift);
}

void RandomXVM::executeIADD_M(const RandomXInstruction& instruction) {
    uint32_t address = getMemoryAddress(instruction);
    uint64_t value = 0;
    
    if (m_cache && !m_lightMode) {
        const RandomXDatasetItem* item = m_cache->getDatasetItem(address / 64);
        if (item) {
            value = item->data[(address % 64) / 8];
        }
    }
    
    m_registers[instruction.dst] += value;
}

void RandomXVM::executeISUB_R(const RandomXInstruction& instruction) {
    m_registers[instruction.dst] -= m_registers[instruction.src];
}

void RandomXVM::executeISUB_M(const RandomXInstruction& instruction) {
    uint32_t address = getMemoryAddress(instruction);
    uint64_t value = 0;
    
    if (m_cache && !m_lightMode) {
        const RandomXDatasetItem* item = m_cache->getDatasetItem(address / 64);
        if (item) {
            value = item->data[(address % 64) / 8];
        }
    }
    
    m_registers[instruction.dst] -= value;
}

void RandomXVM::executeIMUL_R(const RandomXInstruction& instruction) {
    m_registers[instruction.dst] *= m_registers[instruction.src];
}

void RandomXVM::executeIMUL_M(const RandomXInstruction& instruction) {
    uint32_t address = getMemoryAddress(instruction);
    uint64_t value = 0;
    
    if (m_cache && !m_lightMode) {
        const RandomXDatasetItem* item = m_cache->getDatasetItem(address / 64);
        if (item) {
            value = item->data[(address % 64) / 8];
        }
    }
    
    m_registers[instruction.dst] *= value;
}

void RandomXVM::executeIMULH_R(const RandomXInstruction& instruction) {
    m_registers[instruction.dst] = mulh(m_registers[instruction.dst], m_registers[instruction.src]);
}

void RandomXVM::executeIMULH_M(const RandomXInstruction& instruction) {
    uint32_t address = getMemoryAddress(instruction);
    uint64_t value = 0;
    
    if (m_cache && !m_lightMode) {
        const RandomXDatasetItem* item = m_cache->getDatasetItem(address / 64);
        if (item) {
            value = item->data[(address % 64) / 8];
        }
    }
    
    m_registers[instruction.dst] = mulh(m_registers[instruction.dst], value);
}

void RandomXVM::executeISMULH_R(const RandomXInstruction& instruction) {
    m_registers[instruction.dst] = smulh(static_cast<int64_t>(m_registers[instruction.dst]), 
                                        static_cast<int64_t>(m_registers[instruction.src]));
}

void RandomXVM::executeISMULH_M(const RandomXInstruction& instruction) {
    uint32_t address = getMemoryAddress(instruction);
    uint64_t value = 0;
    
    if (m_cache && !m_lightMode) {
        const RandomXDatasetItem* item = m_cache->getDatasetItem(address / 64);
        if (item) {
            value = item->data[(address % 64) / 8];
        }
    }
    
    m_registers[instruction.dst] = smulh(static_cast<int64_t>(m_registers[instruction.dst]), 
                                        static_cast<int64_t>(value));
}

void RandomXVM::executeIMUL_RCP(const RandomXInstruction& instruction) {
    if (instruction.imm32 != 0) {
        m_registers[instruction.dst] = (0xFFFFFFFFFFFFFFFFULL / instruction.imm32) * m_registers[instruction.dst];
    }
}

void RandomXVM::executeINEG_R(const RandomXInstruction& instruction) {
    m_registers[instruction.dst] = -m_registers[instruction.dst];
}

void RandomXVM::executeIXOR_R(const RandomXInstruction& instruction) {
    m_registers[instruction.dst] ^= m_registers[instruction.src];
}

void RandomXVM::executeIXOR_M(const RandomXInstruction& instruction) {
    uint32_t address = getMemoryAddress(instruction);
    uint64_t value = 0;
    
    if (m_cache && !m_lightMode) {
        const RandomXDatasetItem* item = m_cache->getDatasetItem(address / 64);
        if (item) {
            value = item->data[(address % 64) / 8];
        }
    }
    
    m_registers[instruction.dst] ^= value;
}

void RandomXVM::executeIROR_R(const RandomXInstruction& instruction) {
    uint64_t shift = m_registers[instruction.src] & 63;
    m_registers[instruction.dst] = rotateRight64(m_registers[instruction.dst], shift);
}

void RandomXVM::executeIROL_R(const RandomXInstruction& instruction) {
    uint64_t shift = m_registers[instruction.src] & 63;
    m_registers[instruction.dst] = rotateLeft64(m_registers[instruction.dst], shift);
}

void RandomXVM::executeISWAP_R(const RandomXInstruction& instruction) {
    std::swap(m_registers[instruction.dst], m_registers[instruction.src]);
}

void RandomXVM::executeFSWAP_R(const RandomXInstruction& instruction) {
    std::swap(m_fregisters[instruction.dst], m_fregisters[instruction.src]);
}

void RandomXVM::executeFADD_R(const RandomXInstruction& instruction) {
    m_fregisters[instruction.dst] += m_fregisters[instruction.src];
}

void RandomXVM::executeFADD_M(const RandomXInstruction& instruction) {
    uint32_t address = getMemoryAddress(instruction);
    uint64_t value = 0;
    
    if (m_cache && !m_lightMode) {
        const RandomXDatasetItem* item = m_cache->getDatasetItem(address / 64);
        if (item) {
            value = item->data[(address % 64) / 8];
        }
    }
    
    m_fregisters[instruction.dst] += int64ToDouble(value);
}

void RandomXVM::executeFSUB_R(const RandomXInstruction& instruction) {
    m_fregisters[instruction.dst] -= m_fregisters[instruction.src];
}

void RandomXVM::executeFSUB_M(const RandomXInstruction& instruction) {
    uint32_t address = getMemoryAddress(instruction);
    uint64_t value = 0;
    
    if (m_cache && !m_lightMode) {
        const RandomXDatasetItem* item = m_cache->getDatasetItem(address / 64);
        if (item) {
            value = item->data[(address % 64) / 8];
        }
    }
    
    m_fregisters[instruction.dst] -= int64ToDouble(value);
}

void RandomXVM::executeFSCAL_R(const RandomXInstruction& instruction) {
    m_fregisters[instruction.dst] = -m_fregisters[instruction.dst];
}

void RandomXVM::executeFMUL_R(const RandomXInstruction& instruction) {
    m_fregisters[instruction.dst] *= m_fregisters[instruction.src];
}

void RandomXVM::executeFDIV_M(const RandomXInstruction& instruction) {
    uint32_t address = getMemoryAddress(instruction);
    uint64_t value = 0;
    
    if (m_cache && !m_lightMode) {
        const RandomXDatasetItem* item = m_cache->getDatasetItem(address / 64);
        if (item) {
            value = item->data[(address % 64) / 8];
        }
    }
    
    double divisor = int64ToDouble(value);
    if (divisor != 0.0) {
        m_fregisters[instruction.dst] /= divisor;
    }
}

void RandomXVM::executeFSQRT_R(const RandomXInstruction& instruction) {
    m_fregisters[instruction.dst] = std::sqrt(m_fregisters[instruction.dst]);
}

void RandomXVM::executeCBRANCH(const RandomXInstruction& instruction) {
    m_branchRegister = (m_branchRegister + instruction.imm32) & instruction.modMask;
    if (m_branchRegister == 0) {
        m_programCounter = instruction.imm32 % RANDOMX_PROGRAM_SIZE;
    }
}

void RandomXVM::executeCFROUND(const RandomXInstruction& instruction) {
    // Simplified rounding mode change
    // In a real implementation, this would change the FPU rounding mode
}

void RandomXVM::executeISTORE(const RandomXInstruction& instruction) {
    uint32_t address = getMemoryAddress(instruction);
    m_scratchpad[address % 8] = m_registers[instruction.src];
}

void RandomXVM::executeNOP(const RandomXInstruction& instruction) {
    // No operation
}

// Utility functions
uint64_t RandomXVM::rotateRight64(uint64_t x, int n) {
    return (x >> n) | (x << (64 - n));
}

uint64_t RandomXVM::rotateLeft64(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

uint64_t RandomXVM::mulh(uint64_t a, uint64_t b) {
    uint64_t a_lo = a & 0xFFFFFFFFULL;
    uint64_t a_hi = a >> 32;
    uint64_t b_lo = b & 0xFFFFFFFFULL;
    uint64_t b_hi = b >> 32;
    
    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_lo * b_hi;
    uint64_t p2 = a_hi * b_lo;
    uint64_t p3 = a_hi * b_hi;
    
    uint64_t p1_lo = p1 & 0xFFFFFFFFULL;
    uint64_t p1_hi = p1 >> 32;
    uint64_t p2_lo = p2 & 0xFFFFFFFFULL;
    uint64_t p2_hi = p2 >> 32;
    
    uint64_t carry = ((p0 >> 32) + p1_lo + p2_lo) >> 32;
    
    return p3 + p1_hi + p2_hi + carry;
}

int64_t RandomXVM::smulh(int64_t a, int64_t b) {
    return static_cast<int64_t>(mulh(static_cast<uint64_t>(a), static_cast<uint64_t>(b)));
}

double RandomXVM::int64ToDouble(uint64_t x) {
    return static_cast<double>(static_cast<int64_t>(x));
}

uint64_t RandomXVM::doubleToInt64(double x) {
    return static_cast<uint64_t>(static_cast<int64_t>(x));
}

void RandomXVM::generateProgram(const uint8_t* seed, size_t seedSize) {
    for (uint32_t i = 0; i < RANDOMX_PROGRAM_SIZE; i++) {
        m_program[i] = generateInstruction(i, seed, seedSize);
    }
}

RandomXInstruction RandomXVM::generateInstruction(uint32_t pc, const uint8_t* seed, size_t seedSize) {
    RandomXInstruction instruction = {};
    
    // Generate instruction type
    uint64_t hash = 0;
    for (size_t i = 0; i < seedSize; i++) {
        hash ^= seed[i] << ((i % 8) * 8);
    }
    hash ^= pc;
    
    // Simple hash function
    for (int i = 0; i < 4; i++) {
        hash = hash * 0x9e3779b97f4a7c15ULL;
        hash ^= hash >> 33;
    }
    
    instruction.type = static_cast<RandomXInstructionType>(hash % 30);
    instruction.dst = hash & 7;
    instruction.src = (hash >> 8) & 7;
    instruction.imm32 = static_cast<uint32_t>(hash >> 16);
    instruction.imm64 = hash;
    instruction.mod = (hash >> 32) & 7;
    instruction.modShift = ((hash >> 35) & 7) + 1;
    instruction.modMask = (1ULL << instruction.modShift) - 1;
    
    return instruction;
}

uint32_t RandomXVM::getRegisterMask(const RandomXInstruction& instruction) {
    return instruction.modMask;
}

uint32_t RandomXVM::getMemoryAddress(const RandomXInstruction& instruction) {
    uint32_t address = m_registers[instruction.src] + instruction.imm32;
    return address & instruction.modMask;
}

// Main RandomX class implementation
RandomX::RandomX() 
    : m_cache(nullptr), m_initialized(false), m_lightMode(false),
      m_totalHashes(0), m_validHashes(0), m_threadCount(1) {
    m_startTime = std::chrono::steady_clock::now();
    m_lastHashTime = m_startTime;
}

RandomX::~RandomX() {
    destroy();
}

bool RandomX::initialize(const uint8_t* key, size_t keySize, bool lightMode) {
    if (m_initialized) {
        return true;
    }
    
    m_lightMode = lightMode;
    
    // Create cache
    m_cache = new RandomXCache();
    if (!m_cache->initialize(key, keySize)) {
        delete m_cache;
        m_cache = nullptr;
        return false;
    }
    
    // Create VMs
    m_threadCount = std::thread::hardware_concurrency();
    if (m_threadCount == 0) {
        m_threadCount = 1;
    }
    
    for (int i = 0; i < m_threadCount; i++) {
        auto vm = std::make_unique<RandomXVM>();
        if (!vm->initialize(m_cache, lightMode)) {
            return false;
        }
        m_vms.push_back(std::move(vm));
    }
    
    m_initialized = true;
    return true;
}

void RandomX::destroy() {
    if (m_cache) {
        delete m_cache;
        m_cache = nullptr;
    }
    
    m_vms.clear();
    m_initialized = false;
}

void RandomX::calculateHash(const uint8_t* input, size_t inputSize, uint8_t* output) {
    if (!m_initialized) {
        return;
    }
    
    calculateHashInternal(input, inputSize, output);
    m_totalHashes++;
    m_lastHashTime = std::chrono::steady_clock::now();
}

bool RandomX::isValidHash(const uint8_t* hash, const uint8_t* target) {
    if (!hash || !target) {
        return false;
    }
    
    // Compare hash with target (little-endian)
    for (int i = 31; i >= 0; i--) {
        if (hash[i] < target[i]) {
            m_validHashes++;
            return true;
        } else if (hash[i] > target[i]) {
            return false;
        }
    }
    
    m_validHashes++;
    return true;
}

double RandomX::getHashRate() const {
    if (m_totalHashes == 0) {
        return 0.0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime);
    
    if (duration.count() == 0) {
        return 0.0;
    }
    
    return static_cast<double>(m_totalHashes) / (duration.count() / 1000.0);
}

double RandomX::getAcceptanceRate() const {
    if (m_totalHashes == 0) {
        return 0.0;
    }
    
    return static_cast<double>(m_validHashes) / m_totalHashes;
}

void RandomX::calculateHashInternal(const uint8_t* input, size_t inputSize, uint8_t* output) {
    // Use the first VM for hash calculation
    if (m_vms.empty()) {
        return;
    }
    
    auto& vm = m_vms[0];
    vm->reset();
    vm->loadProgram(input, inputSize);
    vm->execute();
    
    // Finalize hash
    finalizeHash(input, inputSize, output);
}

void RandomX::finalizeHash(const uint8_t* input, size_t inputSize, uint8_t* output) {
    // Simple hash finalization
    uint64_t hash = 0;
    
    // Add register values
    for (int i = 0; i < 8; i++) {
        hash ^= m_vms[0]->getRegister(i);
        hash = hash * 0x9e3779b97f4a7c15ULL;
    }
    
    // Add scratchpad values
    for (int i = 0; i < 8; i++) {
        hash ^= m_vms[0]->getScratchpad(i);
        hash = hash * 0x9e3779b97f4a7c15ULL;
    }
    
    // Add input
    for (size_t i = 0; i < inputSize; i++) {
        hash ^= input[i] << ((i % 8) * 8);
        hash = hash * 0x9e3779b97f4a7c15ULL;
    }
    
    // Output hash
    for (int i = 0; i < 8; i++) {
        uint64_t val = hash;
        for (int j = 0; j < 4; j++) {
            output[i * 4 + j] = (val >> (j * 8)) & 0xFF;
        }
        hash = hash * 0x9e3779b97f4a7c15ULL;
    }
}

double RandomX::benchmark(uint32_t iterations) {
    if (!m_initialized) {
        return 0.0;
    }
    
    uint8_t testInput[32] = {0};
    uint8_t testOutput[32];
    
    auto start = std::chrono::steady_clock::now();
    
    for (uint32_t i = 0; i < iterations; i++) {
        // Use iteration as input
        *reinterpret_cast<uint32_t*>(testInput) = i;
        calculateHash(testInput, sizeof(testInput), testOutput);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (duration.count() == 0) {
        return 0.0;
    }
    
    return static_cast<double>(iterations) / (duration.count() / 1000.0);
}

// Utility functions
std::vector<uint8_t> RandomX::hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        if (i + 1 < hex.length()) {
            std::string byteString = hex.substr(i, 2);
            try {
                uint8_t byte = static_cast<uint8_t>(std::stoul(byteString, nullptr, 16));
                bytes.push_back(byte);
            } catch (const std::exception& e) {
                // Invalid hex character, skip
                continue;
            }
        }
    }
    
    return bytes;
}

std::string RandomX::bytesToHex(const uint8_t* bytes, size_t length) {
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    
    for (size_t i = 0; i < length; i++) {
        hex << std::setw(2) << static_cast<int>(bytes[i]);
    }
    
    return hex.str();
}