// Phase 5: Production Systems - Scripting

#pragma once

#include "litt_math.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace litt {

// Script context
class ScriptContext {
public:
    ScriptContext() = default;
    
    // Set variable
    void set_variable(const std::string& name, float value);
    void set_variable(const std::string& name, const std::string& value);
    void set_variable(const std::string& name, bool value);
    
    // Get variable
    float get_float(const std::string& name) const;
    std::string get_string(const std::string& name) const;
    bool get_bool(const std::string& name) const;
    
    // Check if variable exists
    bool has_variable(const std::string& name) const;
    
    // Clear all variables
    void clear();

private:
    std::unordered_map<std::string, float> float_vars_;
    std::unordered_map<std::string, std::string> string_vars_;
    std::unordered_map<std::string, bool> bool_vars_;
};

// Script function
using ScriptFunction = std::function<void(ScriptContext&)>;

// Script engine
class ScriptEngine {
public:
    static ScriptEngine& get_instance() {
        static ScriptEngine instance;
        return instance;
    }
    
    // Initialize scripting
    bool initialize();
    
    // Shutdown scripting
    void shutdown();
    
    // Register function
    void register_function(const std::string& name, ScriptFunction func);
    
    // Execute script
    bool execute(const std::string& script);
    
    // Execute function
    bool execute_function(const std::string& name, ScriptContext& context);
    
    // Check if function exists
    bool has_function(const std::string& name) const;
    
    // Get function count
    size_t get_function_count() const { return functions_.size(); }

private:
    ScriptEngine() = default;
    std::unordered_map<std::string, ScriptFunction> functions_;
};

// Script component
struct ScriptComponent {
    std::string script_path;
    ScriptContext context;
    bool enabled = true;
    bool loaded = false;
    
    // Load script
    bool load(const std::string& path);
    
    // Execute
    bool execute();
    
    // Enable/disable
    void set_enabled(bool enabled) { enabled = enabled; }
    bool is_enabled() const { return enabled; }
};

// Script manager
class ScriptManager {
public:
    static ScriptManager& get_instance() {
        static ScriptManager instance;
        return instance;
    }
    
    // Load script
    ScriptComponent* load_script(const std::string& path);
    
    // Execute script
    bool execute_script(const std::string& path);
    
    // Execute all scripts
    void execute_all();
    
    // Get script
    ScriptComponent* get_script(const std::string& path);
    
    // Remove script
    void remove_script(const std::string& path);
    
    // Clear all scripts
    void clear();
    
    // Get script count
    size_t get_script_count() const { return scripts_.size(); }

private:
    ScriptManager() = default;
    std::unordered_map<std::string, std::unique_ptr<ScriptComponent>> scripts_;
};

} // namespace litt
