#include "thermal_manager.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>

#ifdef __APPLE__
#include <IOKit/IOKitLib.h>
#include <sys/sysctl.h>
#endif

ThermalManager::ThermalManager() : m_running(false), m_shouldStop(false), m_throttling(false), m_throttleLevel(0.0) {
    LOG_DEBUG("ThermalManager constructor called");
}

ThermalManager::~ThermalManager() {
    stop();
    LOG_DEBUG("ThermalManager destructor called");
}

bool ThermalManager::initialize() {
    LOG_INFO("Initializing thermal management system");
    
    // Set up thermal callback
    m_thermalCallback = [this](double cpuTemp, double gpuTemp, double systemTemp) {
        LOG_DEBUG("Thermal event - CPU: {:.1f}°C, GPU: {:.1f}°C, System: {:.1f}°C", 
                 cpuTemp, gpuTemp, systemTemp);
    };
    
    LOG_INFO("Thermal management system initialized");
    return true;
}

void ThermalManager::start() {
    if (m_running) {
        LOG_WARNING("Thermal monitoring is already running");
        return;
    }
    
    LOG_INFO("Starting thermal monitoring");
    
    m_running = true;
    m_shouldStop = false;
    
    // Start thermal monitoring thread
    m_thermalThread = std::thread(&ThermalManager::thermalMonitoringLoop, this);
    
    LOG_INFO("Thermal monitoring started");
}

void ThermalManager::stop() {
    if (!m_running) {
        return;
    }
    
    LOG_INFO("Stopping thermal monitoring");
    
    m_shouldStop = true;
    m_running = false;
    
    // Wait for thermal thread to finish
    if (m_thermalThread.joinable()) {
        m_thermalThread.join();
    }
    
    // Reset throttling
    resetThrottling();
    
    LOG_INFO("Thermal monitoring stopped");
}

double ThermalManager::getCPUTemperature() const {
    return m_cpuTemperature.load();
}

double ThermalManager::getGPUTemperature() const {
    return m_gpuTemperature.load();
}

double ThermalManager::getSystemTemperature() const {
    return m_systemTemperature.load();
}

void ThermalManager::setThermalLimits(double cpuMaxTemp, double gpuMaxTemp, double systemMaxTemp) {
    m_cpuMaxTemp = cpuMaxTemp;
    m_gpuMaxTemp = gpuMaxTemp;
    m_systemMaxTemp = systemMaxTemp;
    
    LOG_INFO("Thermal limits set - CPU: {:.1f}°C, GPU: {:.1f}°C, System: {:.1f}°C",
             cpuMaxTemp, gpuMaxTemp, systemMaxTemp);
}

void ThermalManager::setThermalCallback(std::function<void(double, double, double)> callback) {
    m_thermalCallback = callback;
}

ThermalManager::ThermalStats ThermalManager::getStats() const {
    ThermalStats stats;
    stats.cpuTemperature = m_cpuTemperature.load();
    stats.gpuTemperature = m_gpuTemperature.load();
    stats.systemTemperature = m_systemTemperature.load();
    stats.throttling = m_throttling.load();
    stats.throttleLevel = m_throttleLevel.load();
    stats.lastUpdate = std::chrono::steady_clock::now();
    
    return stats;
}

void ThermalManager::thermalMonitoringLoop() {
    LOG_INFO("Thermal monitoring loop started");
    
    while (!m_shouldStop && m_running) {
        try {
            // Read temperatures
            double cpuTemp = readCPUTemperature();
            double gpuTemp = readGPUTemperature();
            double systemTemp = readSystemTemperature();
            
            // Update stored temperatures
            m_cpuTemperature = cpuTemp;
            m_gpuTemperature = gpuTemp;
            m_systemTemperature = systemTemp;
            
            // Calculate throttle level
            double throttleLevel = calculateThrottleLevel(cpuTemp, gpuTemp, systemTemp);
            m_throttleLevel = throttleLevel;
            
            // Apply throttling if necessary
            if (throttleLevel > 0.0) {
                if (!m_throttling) {
                    m_throttling = true;
                    LOG_WARNING("Thermal throttling activated at {:.1f}%", throttleLevel * 100);
                }
                applyThrottling(throttleLevel);
            } else {
                if (m_throttling) {
                    m_throttling = false;
                    LOG_INFO("Thermal throttling deactivated");
                }
                resetThrottling();
            }
            
            // Check for thermal emergency
            if (isThermalEmergency(cpuTemp, gpuTemp, systemTemp)) {
                LOG_CRITICAL("Thermal emergency detected! CPU: {:.1f}°C, GPU: {:.1f}°C, System: {:.1f}°C",
                            cpuTemp, gpuTemp, systemTemp);
                // In a real implementation, this would trigger emergency shutdown
            }
            
            // Call thermal callback
            if (m_thermalCallback) {
                m_thermalCallback(cpuTemp, gpuTemp, systemTemp);
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in thermal monitoring loop: {}", e.what());
        }
        
        // Wait before next check
        std::this_thread::sleep_for(m_monitoringInterval);
    }
    
    LOG_INFO("Thermal monitoring loop ended");
}

double ThermalManager::readCPUTemperature() {
#ifdef __APPLE__
    // Read CPU temperature from IOKit
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, 
                                                      IOServiceMatching("AppleARMIODevice"));
    if (service == 0) {
        return 0.0;
    }
    
    // Try to read temperature from thermal sensors
    // This is a simplified approach - in practice, you'd need to access specific thermal sensors
    
    // For now, return a simulated temperature based on system load
    // In a real implementation, you'd read from actual thermal sensors
    static double baseTemp = 40.0;
    static double loadFactor = 0.0;
    
    // Simulate temperature based on CPU usage
    loadFactor = std::min(1.0, loadFactor + 0.1);
    double temp = baseTemp + (loadFactor * 30.0);
    
    IOServiceClose(service);
    return temp;
#else
    // Fallback for non-macOS systems
    return 50.0;
#endif
}

double ThermalManager::readGPUTemperature() {
#ifdef __APPLE__
    // Read GPU temperature from IOKit
    // This is a simplified approach - in practice, you'd need to access GPU thermal sensors
    
    // For now, return a simulated temperature
    static double baseTemp = 45.0;
    static double loadFactor = 0.0;
    
    loadFactor = std::min(1.0, loadFactor + 0.05);
    double temp = baseTemp + (loadFactor * 25.0);
    
    return temp;
#else
    return 55.0;
#endif
}

double ThermalManager::readSystemTemperature() {
    // System temperature is typically the highest of CPU and GPU
    double cpuTemp = m_cpuTemperature.load();
    double gpuTemp = m_gpuTemperature.load();
    
    return std::max(cpuTemp, gpuTemp);
}

double ThermalManager::calculateThrottleLevel(double cpuTemp, double gpuTemp, double systemTemp) {
    const auto maxTemp = std::max({cpuTemp, gpuTemp, systemTemp});
    
    // Use C23 constexpr for compile-time constants
    constexpr double noThrottleThreshold = 0.8;
    constexpr double maxThrottleRatio = 0.5;
    constexpr double aggressiveThrottleBase = 0.5;
    constexpr double aggressiveThrottleRange = 0.2;
    
    if (maxTemp < m_cpuMaxTemp * noThrottleThreshold) {
        return 0.0; // No throttling needed
    } else if (maxTemp < m_cpuMaxTemp) {
        // Linear throttling from 80% to 100% of max temp
        const auto ratio = (maxTemp - m_cpuMaxTemp * noThrottleThreshold) / (m_cpuMaxTemp * aggressiveThrottleRange);
        return std::min(maxThrottleRatio, ratio); // Max 50% throttling
    } else {
        // Aggressive throttling above max temp
        const auto ratio = (maxTemp - m_cpuMaxTemp) / (m_cpuMaxTemp * aggressiveThrottleRange);
        return std::min(1.0, aggressiveThrottleBase + ratio); // 50% to 100% throttling
    }
}

void ThermalManager::applyThrottling(double throttleLevel) {
    // Apply thermal throttling
    // This could involve:
    // - Reducing CPU frequency
    // - Limiting GPU usage
    // - Adding sleep delays
    // - Reducing mining intensity
    
    LOG_DEBUG("Applying thermal throttling: {:.1f}%", throttleLevel * 100);
    
    // In a real implementation, this would interface with system power management
    // For now, we'll just log the throttling level
}

void ThermalManager::resetThrottling() {
    if (m_throttling) {
        m_throttling = false;
        m_throttleLevel = 0.0;
        LOG_INFO("Thermal throttling reset");
    }
}

bool ThermalManager::isThermalEmergency(double cpuTemp, double gpuTemp, double systemTemp) {
    double maxTemp = std::max({cpuTemp, gpuTemp, systemTemp});
    return maxTemp > m_emergencyThreshold;
}
