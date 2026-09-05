// LittConfig - Configuration system for Litt Engine

#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace litt {

// Key/value settings store (named Settings to avoid clashing with
// litt_world.h's game-state Config in the same namespace).
class Settings {
public:
    Settings() = default;
    
    bool load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << path << std::endl;
            return false;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            
            // Parse key=value
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            values_[key] = value;
        }
        
        return true;
    }
    
    bool save(const std::string& path) const {
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file for writing: " << path << std::endl;
            return false;
        }
        
        for (const auto& [key, value] : values_) {
            file << key << " = " << value << "\n";
        }
        
        return true;
    }
    
    std::string get(const std::string& key, const std::string& defaultVal = "") const {
        auto it = values_.find(key);
        return it != values_.end() ? it->second : defaultVal;
    }
    
    int getInt(const std::string& key, int defaultVal = 0) const {
        auto it = values_.find(key);
        if (it == values_.end()) return defaultVal;
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultVal;
        }
    }
    
    float getFloat(const std::string& key, float defaultVal = 0.0f) const {
        auto it = values_.find(key);
        if (it == values_.end()) return defaultVal;
        try {
            return std::stof(it->second);
        } catch (...) {
            return defaultVal;
        }
    }
    
    bool getBool(const std::string& key, bool defaultVal = false) const {
        auto it = values_.find(key);
        if (it == values_.end()) return defaultVal;
        const std::string& val = it->second;
        return val == "true" || val == "1" || val == "yes" || val == "on";
    }
    
    void set(const std::string& key, const std::string& value) {
        values_[key] = value;
    }
    
    void setInt(const std::string& key, int value) {
        values_[key] = std::to_string(value);
    }
    
    void setFloat(const std::string& key, float value) {
        values_[key] = std::to_string(value);
    }
    
    void setBool(const std::string& key, bool value) {
        values_[key] = value ? "true" : "false";
    }
    
    bool has(const std::string& key) const {
        return values_.find(key) != values_.end();
    }
    
    void remove(const std::string& key) {
        values_.erase(key);
    }
    
    void clear() {
        values_.clear();
    }
    
    size_t size() const {
        return values_.size();
    }
    
    const std::unordered_map<std::string, std::string>& getAll() const {
        return values_;
    }
    
private:
    std::unordered_map<std::string, std::string> values_;
};

// Preset configurations
class Presets {
public:
    static Settings loadPreset(const std::string& preset) {
        Settings config;
        
        if (preset == "low") {
            config.setInt("texture_quality", 1);
            config.setInt("shadow_quality", 1);
            config.setInt("anti_aliasing", 0);
            config.setInt("draw_distance", 500);
            config.setInt("volumetric_fog", 0);
        } else if (preset == "medium") {
            config.setInt("texture_quality", 2);
            config.setInt("shadow_quality", 2);
            config.setInt("anti_aliasing", 1);
            config.setInt("draw_distance", 800);
            config.setInt("volumetric_fog", 1);
        } else if (preset == "high") {
            config.setInt("texture_quality", 3);
            config.setInt("shadow_quality", 3);
            config.setInt("anti_aliasing", 2);
            config.setInt("draw_distance", 1200);
            config.setInt("volumetric_fog", 2);
        } else if (preset == "ultra") {
            config.setInt("texture_quality", 4);
            config.setInt("shadow_quality", 4);
            config.setInt("anti_aliasing", 3);
            config.setInt("draw_distance", 2000);
            config.setInt("volumetric_fog", 3);
        }
        
        return config;
    }
};

} // namespace litt
