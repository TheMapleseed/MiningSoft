#include "cpu_throttle_manager.h"
#include "logger.h"
#include <algorithm>
#include <numeric>

#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

CPUThrottleManager::CPUThrottleManager() : m_running(false), m_shouldStop(false), m_throttling(false), m_throttleLevel(0.0) {
    LOG_DEBUG("CPUThrottleManager constructor called");
}

CPUThrottleManager::~CPUThrottleManager() {
    stop();
    LOG_DEBUG("CPUThrottleManager destructor called");
}

bool CPUThrottleManager::initialize() {
    LOG_INFO("Initializing CPU demand-based throttling system");
    
    // Initialize usage history
    std::fill(m_usageHistory, m_usageHistory + MAX_HISTORY_SIZE, 0.0);
    
    // Set up CPU callback
    m_cpuCallback = [this](double usage, double throttleLevel) {
        LOG_DEBUG("CPU usage: {:.1f}%, Throttle: {:.1f}%", usage, throttleLevel * 100);
    };
    
    LOG_INFO("CPU throttling system initialized");
    return true;
}

void CPUThrottleManager::start() {
    if (m_running) {
        LOG_WARNING("CPU monitoring is already running");
        return;
    }
    
    LOG_INFO("Starting CPU demand monitoring");
    
    m_running = true;
    m_shouldStop = false;
    
    // Start CPU monitoring thread
    m_cpuThread = std::thread(&CPUThrottleManager::cpuMonitoringLoop, this);
    
    LOG_INFO("CPU monitoring started");
}

void CPUThrottleManager::stop() {
    if (!m_running) {
        return;
    }
    
    LOG_INFO("Stopping CPU monitoring");
    
    m_shouldStop = true;
    m_running = false;
    
    // Wait for CPU thread to finish
    if (m_cpuThread.joinable()) {
        m_cpuThread.join();
    }
    
    // Reset throttling
    resetThrottling();
    
    LOG_INFO("CPU monitoring stopped");
}

double CPUThrottleManager::getCPUUsage() const {
    return m_currentUsage.load();
}

void CPUThrottleManager::setThresholds(double lowThreshold, double highThreshold, double maxThreshold) {
    m_lowThreshold = lowThreshold;
    m_highThreshold = highThreshold;
    m_maxThreshold = maxThreshold;
    
    LOG_INFO("CPU thresholds set - Low: {:.1f}%, High: {:.1f}%, Max: {:.1f}%",
             lowThreshold, highThreshold, maxThreshold);
}

void CPUThrottleManager::setCPUCallback(std::function<void(double, double)> callback) {
    m_cpuCallback = callback;
}

CPUThrottleManager::CPUStats CPUThrottleManager::getStats() const {
    CPUStats stats;
    stats.currentUsage = m_currentUsage.load();
    stats.averageUsage = m_averageUsage.load();
    stats.peakUsage = m_peakUsage.load();
    stats.throttling = m_throttling.load();
    stats.throttleLevel = m_throttleLevel.load();
    stats.lastUpdate = std::chrono::steady_clock::now();
    
    return stats;
}

void CPUThrottleManager::cpuMonitoringLoop() {
    LOG_INFO("CPU monitoring loop started");
    
    while (!m_shouldStop && m_running) {
        try {
            // Read current CPU usage
            double cpuUsage = readCPUUsage();
            m_currentUsage = cpuUsage;
            
            // Update usage history for averaging
            m_usageHistory[m_historyIndex] = cpuUsage;
            m_historyIndex = (m_historyIndex + 1) % MAX_HISTORY_SIZE;
            if (m_historyCount < MAX_HISTORY_SIZE) {
                m_historyCount++;
            }
            
            // Calculate average usage
            double sum = std::accumulate(m_usageHistory, m_usageHistory + m_historyCount, 0.0);
            double averageUsage = sum / m_historyCount;
            m_averageUsage = averageUsage;
            
            // Update peak usage
            if (cpuUsage > m_peakUsage) {
                m_peakUsage = cpuUsage;
            }
            
            // Calculate throttle level based on CPU usage
            double throttleLevel = calculateThrottleLevel(cpuUsage);
            m_throttleLevel = throttleLevel;
            
            // Apply throttling if necessary
            if (throttleLevel > 0.0) {
                if (!m_throttling) {
                    m_throttling = true;
                    LOG_INFO("CPU throttling activated - Usage: {:.1f}%, Throttle: {:.1f}%", 
                            cpuUsage, throttleLevel * 100);
                }
                applyThrottling(throttleLevel);
            } else {
                if (m_throttling) {
                    m_throttling = false;
                    LOG_INFO("CPU throttling deactivated - Usage: {:.1f}%", cpuUsage);
                }
                resetThrottling();
            }
            
            // Call CPU callback
            if (m_cpuCallback) {
                m_cpuCallback(cpuUsage, throttleLevel);
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in CPU monitoring loop: {}", e.what());
        }
        
        // Wait before next check
        std::this_thread::sleep_for(m_monitoringInterval);
    }
    
    LOG_INFO("CPU monitoring loop ended");
}

double CPUThrottleManager::readCPUUsage() {
#ifdef __APPLE__
    // Read CPU usage from macOS system
    host_cpu_load_info_data_t cpuInfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, 
                       (host_info_t)&cpuInfo, &count) != KERN_SUCCESS) {
        return 0.0;
    }
    
    // Calculate CPU usage percentage
    unsigned long totalTicks = cpuInfo.cpu_ticks[CPU_STATE_USER] + 
                              cpuInfo.cpu_ticks[CPU_STATE_SYSTEM] + 
                              cpuInfo.cpu_ticks[CPU_STATE_IDLE] + 
                              cpuInfo.cpu_ticks[CPU_STATE_NICE];
    
    if (totalTicks == 0) {
        return 0.0;
    }
    
    unsigned long idleTicks = cpuInfo.cpu_ticks[CPU_STATE_IDLE];
    double cpuUsage = 100.0 * (1.0 - (double)idleTicks / totalTicks);
    
    return std::min(100.0, std::max(0.0, cpuUsage));
#else
    // Fallback for non-macOS systems
    return 50.0;
#endif
}

double CPUThrottleManager::calculateThrottleLevel(double cpuUsage) {
    if (cpuUsage < m_lowThreshold) {
        return 0.0; // No throttling needed
    } else if (cpuUsage < m_highThreshold) {
        // Linear throttling from low to high threshold
        double ratio = (cpuUsage - m_lowThreshold) / (m_highThreshold - m_lowThreshold);
        return ratio * 0.3; // Max 30% throttling
    } else if (cpuUsage < m_maxThreshold) {
        // More aggressive throttling from high to max threshold
        double ratio = (cpuUsage - m_highThreshold) / (m_maxThreshold - m_highThreshold);
        return 0.3 + (ratio * 0.5); // 30% to 80% throttling
    } else {
        // Maximum throttling above max threshold
        return 1.0; // 100% throttling
    }
}

void CPUThrottleManager::applyThrottling(double throttleLevel) {
    // Apply CPU-based throttling
    // This could involve:
    // - Reducing mining thread count
    // - Adding sleep delays
    // - Reducing GPU workload
    // - Limiting Vector Processor usage
    
    LOG_DEBUG("Applying CPU throttling: {:.1f}%", throttleLevel * 100);
    
    // In a real implementation, this would interface with the mining system
    // to reduce resource usage based on the throttle level
}

void CPUThrottleManager::resetThrottling() {
    if (m_throttling) {
        m_throttling = false;
        m_throttleLevel = 0.0;
        LOG_INFO("CPU throttling reset");
    }
}
