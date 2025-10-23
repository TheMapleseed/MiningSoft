#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <functional>

/**
 * Thermal management system for Apple Silicon
 * Monitors temperature and implements throttling to prevent overheating
 */
class ThermalManager {
public:
    ThermalManager();
    ~ThermalManager();

    // Initialize thermal monitoring
    bool initialize();
    
    // Start thermal monitoring
    void start();
    
    // Stop thermal monitoring
    void stop();
    
    // Get current CPU temperature
    double getCPUTemperature() const;
    
    // Get current GPU temperature
    double getGPUTemperature() const;
    
    // Get current system temperature
    double getSystemTemperature() const;
    
    // Check if thermal throttling is active
    bool isThrottling() const { return m_throttling; }
    
    // Get current throttle level (0.0 = no throttle, 1.0 = max throttle)
    double getThrottleLevel() const { return m_throttleLevel; }
    
    // Set thermal limits
    void setThermalLimits(double cpuMaxTemp, double gpuMaxTemp, double systemMaxTemp);
    
    // Set callback for thermal events
    void setThermalCallback(std::function<void(double, double, double)> callback);
    
    // Get thermal statistics
    struct ThermalStats {
        double cpuTemperature;
        double gpuTemperature;
        double systemTemperature;
        bool throttling;
        double throttleLevel;
        std::chrono::steady_clock::time_point lastUpdate;
    };
    
    ThermalStats getStats() const;

private:
    // Main thermal monitoring loop
    void thermalMonitoringLoop();
    
    // Read temperature from system sensors
    double readCPUTemperature();
    double readGPUTemperature();
    double readSystemTemperature();
    
    // Calculate throttle level based on temperatures
    double calculateThrottleLevel(double cpuTemp, double gpuTemp, double systemTemp);
    
    // Apply thermal throttling
    void applyThrottling(double throttleLevel);
    
    // Reset thermal throttling
    void resetThrottling();
    
    // Check for thermal emergency
    bool isThermalEmergency(double cpuTemp, double gpuTemp, double systemTemp);

private:
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_shouldStop{false};
    std::atomic<bool> m_throttling{false};
    std::atomic<double> m_throttleLevel{0.0};
    
    std::thread m_thermalThread;
    
    // Temperature limits
    double m_cpuMaxTemp{85.0};      // Celsius
    double m_gpuMaxTemp{90.0};      // Celsius
    double m_systemMaxTemp{80.0};   // Celsius
    
    // Current temperatures
    std::atomic<double> m_cpuTemperature{0.0};
    std::atomic<double> m_gpuTemperature{0.0};
    std::atomic<double> m_systemTemperature{0.0};
    
    // Callback for thermal events
    std::function<void(double, double, double)> m_thermalCallback;
    
    // Monitoring interval
    std::chrono::milliseconds m_monitoringInterval{1000};
    
    // Thermal emergency threshold
    double m_emergencyThreshold{95.0}; // Celsius
};
