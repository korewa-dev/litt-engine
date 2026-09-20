// Phase 5: Production Systems - Scripting

#pragma once

#include "litt_math.h"
#include "litt_scripting_vm.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <fstream>
#include <iterator>
#include <utility>

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
    ScriptVM vm_;
    bool initialized_ = false;
};

// Script component
struct ScriptComponent {
    std::string script_path;
    ScriptContext context;
    bool enabled = true;
    bool loaded = false;
    std::string source;
    
    // Load script
    bool load(const std::string& path);
    
    // Execute
    bool execute();
    
    // Enable/disable
    void set_enabled(bool value) { enabled = value; }
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


inline void ScriptContext::set_variable(const std::string& name, float value) {
    float_vars_[name] = value;
    string_vars_.erase(name);
    bool_vars_.erase(name);
}

inline void ScriptContext::set_variable(const std::string& name, const std::string& value) {
    string_vars_[name] = value;
    float_vars_.erase(name);
    bool_vars_.erase(name);
}

inline void ScriptContext::set_variable(const std::string& name, bool value) {
    bool_vars_[name] = value;
    float_vars_.erase(name);
    string_vars_.erase(name);
}

inline float ScriptContext::get_float(const std::string& name) const {
    const auto it = float_vars_.find(name);
    return it == float_vars_.end() ? 0.0f : it->second;
}

inline std::string ScriptContext::get_string(const std::string& name) const {
    const auto it = string_vars_.find(name);
    return it == string_vars_.end() ? std::string{} : it->second;
}

inline bool ScriptContext::get_bool(const std::string& name) const {
    const auto it = bool_vars_.find(name);
    return it != bool_vars_.end() && it->second;
}

inline bool ScriptContext::has_variable(const std::string& name) const {
    return float_vars_.find(name) != float_vars_.end() ||
           string_vars_.find(name) != string_vars_.end() ||
           bool_vars_.find(name) != bool_vars_.end();
}

inline void ScriptContext::clear() {
    float_vars_.clear();
    string_vars_.clear();
    bool_vars_.clear();
}

inline bool ScriptEngine::initialize() {
    if (initialized_) return true;
    initialized_ = vm_.initialize();
    return initialized_;
}

inline void ScriptEngine::shutdown() {
    if (initialized_) vm_.shutdown();
    initialized_ = false;
    functions_.clear();
}

inline void ScriptEngine::register_function(const std::string& name, ScriptFunction func) {
    if (name.empty()) return;
    if (!func) {
        functions_.erase(name);
        return;
    }
    functions_[name] = std::move(func);
}

inline bool ScriptEngine::execute(const std::string& script) {
    if (!initialized_ && !initialize()) return false;
    if (script.empty()) return false;
    static const char* kInlineName = "__litt_inline_script__";
    if (!vm_.compile(kInlineName, script)) return false;
    return vm_.execute(kInlineName);
}

inline bool ScriptEngine::execute_function(const std::string& name, ScriptContext& context) {
    const auto it = functions_.find(name);
    if (it == functions_.end() || !it->second) return false;
    it->second(context);
    return true;
}

inline bool ScriptEngine::has_function(const std::string& name) const {
    return functions_.find(name) != functions_.end();
}

inline bool ScriptComponent::load(const std::string& path) {
    loaded = false;
    source.clear();
    script_path.clear();
    if (path.empty()) return false;

    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!in.good() && !in.eof()) return false;

    script_path = path;
    source = std::move(data);
    loaded = true;
    return true;
}

inline bool ScriptComponent::execute() {
    return enabled && loaded && !source.empty() &&
           ScriptEngine::get_instance().execute(source);
}

inline ScriptComponent* ScriptManager::load_script(const std::string& path) {
    if (path.empty()) return nullptr;
    auto existing = scripts_.find(path);
    if (existing != scripts_.end()) return existing->second.get();

    auto component = std::make_unique<ScriptComponent>();
    if (!component->load(path)) return nullptr;
    ScriptComponent* raw = component.get();
    scripts_[path] = std::move(component);
    return raw;
}

inline bool ScriptManager::execute_script(const std::string& path) {
    ScriptComponent* script = get_script(path);
    if (!script) script = load_script(path);
    return script && script->execute();
}

inline void ScriptManager::execute_all() {
    for (auto& pair : scripts_) {
        if (pair.second) (void)pair.second->execute();
    }
}

inline ScriptComponent* ScriptManager::get_script(const std::string& path) {
    const auto it = scripts_.find(path);
    return it == scripts_.end() ? nullptr : it->second.get();
}

inline void ScriptManager::remove_script(const std::string& path) {
    scripts_.erase(path);
}

inline void ScriptManager::clear() {
    scripts_.clear();
}

} // namespace litt
