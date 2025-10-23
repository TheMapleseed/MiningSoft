#include "simple_json.h"
#include "logger.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>
#include <map>

SimpleJSON::SimpleJSON() {
    LOG_DEBUG("SimpleJSON constructor called");
}

SimpleJSON::~SimpleJSON() {
    LOG_DEBUG("SimpleJSON destructor called");
}

bool SimpleJSON::parse(const std::string& json) {
    LOG_DEBUG("Parsing JSON string");
    
    m_values.clear();
    
    size_t pos = 0;
    skipWhitespace(json, pos);
    
    if (pos >= json.length() || json[pos] != '{') {
        LOG_ERROR("Invalid JSON: expected '{' at position {}", pos);
        return false;
    }
    
    pos++; // Skip opening brace
    skipWhitespace(json, pos);
    
    while (pos < json.length() && json[pos] != '}') {
        // Parse key
        std::string key = parseString(json, pos);
        if (key.empty()) {
            LOG_ERROR("Invalid JSON: expected string key at position {}", pos);
            return false;
        }
        
        skipWhitespace(json, pos);
        if (pos >= json.length() || json[pos] != ':') {
            LOG_ERROR("Invalid JSON: expected ':' at position {}", pos);
            return false;
        }
        
        pos++; // Skip colon
        skipWhitespace(json, pos);
        
        // Parse value
        std::string value = parseValue(json, pos);
        if (value.empty() && !isBoolean(value)) {
            LOG_ERROR("Invalid JSON: expected value at position {}", pos);
            return false;
        }
        
        m_values[key] = value;
        
        skipWhitespace(json, pos);
        if (pos < json.length() && json[pos] == ',') {
            pos++; // Skip comma
            skipWhitespace(json, pos);
        }
    }
    
    if (pos >= json.length() || json[pos] != '}') {
        LOG_ERROR("Invalid JSON: expected '}' at position {}", pos);
        return false;
    }
    
    LOG_DEBUG("JSON parsed successfully with {} keys", m_values.size());
    return true;
}

std::string SimpleJSON::getString(const std::string& key, const std::string& defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        return unescapeString(it->second);
    }
    return defaultValue;
}

int SimpleJSON::getInt(const std::string& key, int defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            return std::stoi(it->second);
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to convert '{}' to int: {}", it->second, e.what());
        }
    }
    return defaultValue;
}

double SimpleJSON::getDouble(const std::string& key, double defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            return std::stod(it->second);
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to convert '{}' to double: {}", it->second, e.what());
        }
    }
    return defaultValue;
}

bool SimpleJSON::getBool(const std::string& key, bool defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        if (value == "true") return true;
        if (value == "false") return false;
    }
    return defaultValue;
}

bool SimpleJSON::hasKey(const std::string& key) const {
    return m_values.find(key) != m_values.end();
}

std::vector<std::string> SimpleJSON::getKeys() const {
    std::vector<std::string> keys;
    for (const auto& pair : m_values) {
        keys.push_back(pair.first);
    }
    return keys;
}

std::string SimpleJSON::toString() const {
    std::ostringstream oss;
    oss << "{\n";
    
    bool first = true;
    for (const auto& pair : m_values) {
        if (!first) {
            oss << ",\n";
        }
        first = false;
        
        oss << "  \"" << escapeString(pair.first) << "\": ";
        
        if (isNumber(pair.second) || isBoolean(pair.second)) {
            oss << pair.second;
        } else {
            oss << "\"" << escapeString(pair.second) << "\"";
        }
    }
    
    oss << "\n}";
    return oss.str();
}

std::string SimpleJSON::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string SimpleJSON::unescapeString(const std::string& str) const {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case '"': result += '"'; i++; break;
                case '\\': result += '\\'; i++; break;
                case '/': result += '/'; i++; break;
                case 'b': result += '\b'; i++; break;
                case 'f': result += '\f'; i++; break;
                case 'n': result += '\n'; i++; break;
                case 'r': result += '\r'; i++; break;
                case 't': result += '\t'; i++; break;
                default: result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    
    return result;
}

std::string SimpleJSON::escapeString(const std::string& str) const {
    std::string result;
    result.reserve(str.length() * 2);
    
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '/': result += "\\/"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    
    return result;
}

bool SimpleJSON::isNumber(const std::string& str) const {
    if (str.empty()) return false;
    
    size_t start = 0;
    if (str[0] == '-') start = 1;
    if (start >= str.length()) return false;
    
    bool hasDigit = false;
    bool hasDot = false;
    
    for (size_t i = start; i < str.length(); i++) {
        if (std::isdigit(str[i])) {
            hasDigit = true;
        } else if (str[i] == '.' && !hasDot) {
            hasDot = true;
        } else {
            return false;
        }
    }
    
    return hasDigit;
}

bool SimpleJSON::isBoolean(const std::string& str) const {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "false";
}

std::string SimpleJSON::parseValue(const std::string& json, size_t& pos) {
    skipWhitespace(json, pos);
    
    if (pos >= json.length()) {
        return "";
    }
    
    if (json[pos] == '"') {
        return parseString(json, pos);
    } else if (std::isdigit(json[pos]) || json[pos] == '-') {
        return parseNumber(json, pos);
    } else if (json[pos] == 't' || json[pos] == 'f') {
        return parseBoolean(json, pos);
    }
    
    return "";
}

std::string SimpleJSON::parseString(const std::string& json, size_t& pos) {
    if (pos >= json.length() || json[pos] != '"') {
        return "";
    }
    
    pos++; // Skip opening quote
    std::string result;
    
    while (pos < json.length() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.length()) {
            pos++; // Skip backslash
            switch (json[pos]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        pos++;
    }
    
    if (pos < json.length() && json[pos] == '"') {
        pos++; // Skip closing quote
    }
    
    return result;
}

std::string SimpleJSON::parseNumber(const std::string& json, size_t& pos) {
    size_t start = pos;
    
    if (json[pos] == '-') {
        pos++;
    }
    
    while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '.')) {
        pos++;
    }
    
    return json.substr(start, pos - start);
}

std::string SimpleJSON::parseBoolean(const std::string& json, size_t& pos) {
    if (pos + 4 < json.length() && json.substr(pos, 4) == "true") {
        pos += 4;
        return "true";
    } else if (pos + 5 < json.length() && json.substr(pos, 5) == "false") {
        pos += 5;
        return "false";
    }
    
    return "";
}

void SimpleJSON::skipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.length() && std::isspace(json[pos])) {
        pos++;
    }
}
