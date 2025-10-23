#pragma once

#include <string>
#include <map>
#include <vector>
#include <cctype>

/**
 * Simple JSON parser - no external dependencies
 * Handles basic JSON parsing for configuration files
 */
class SimpleJSON {
public:
    SimpleJSON();
    ~SimpleJSON();

    // Parse JSON string
    bool parse(const std::string& json);
    
    // Get string value
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    
    // Get integer value
    int getInt(const std::string& key, int defaultValue = 0) const;
    
    // Get double value
    double getDouble(const std::string& key, double defaultValue = 0.0) const;
    
    // Get boolean value
    bool getBool(const std::string& key, bool defaultValue = false) const;
    
    // Check if key exists
    bool hasKey(const std::string& key) const;
    
    // Get all keys
    std::vector<std::string> getKeys() const;
    
    // Generate JSON string
    std::string toString() const;

private:
    std::map<std::string, std::string> m_values;
    
    // JSON parsing helpers
    std::string trim(const std::string& str);
    std::string unescapeString(const std::string& str) const;
    std::string escapeString(const std::string& str) const;
    bool isNumber(const std::string& str) const;
    bool isBoolean(const std::string& str) const;
    std::string parseValue(const std::string& json, size_t& pos);
    std::string parseString(const std::string& json, size_t& pos);
    std::string parseNumber(const std::string& json, size_t& pos);
    std::string parseBoolean(const std::string& json, size_t& pos);
    void skipWhitespace(const std::string& json, size_t& pos);
};
