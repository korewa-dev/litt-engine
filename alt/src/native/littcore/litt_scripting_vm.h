// Litt Engine - Simple Bytecode Scripting VM
// Embedded scripting without external dependencies

#pragma once
#include <cstdint>
#include "litt_scripting.h"
#include <stack>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <cstring>
#include <iostream>
#include <cmath>
#include <cctype>
#include <cstdlib>

namespace litt {

// Simple bytecode opcodes
enum class OpCode : uint8_t {
    NOP = 0,
    PUSH_FLOAT,
    PUSH_STRING,
    PUSH_BOOL,
    POP,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    NEG,
    EQ,
    NEQ,
    LT,
    GT,
    LTE,
    GTE,
    AND,
    OR,
    NOT,
    JUMP,
    JUMP_IF_FALSE,
    JUMP_IF_TRUE,
    LOAD_VAR,
    STORE_VAR,
    LOAD_GLOBAL,
    STORE_GLOBAL,
    CALL,
    CALL_BUILTIN,
    RETURN,
    PRINT,
    TO_FLOAT,
    TO_STRING,
    TO_BOOL,
    CONCAT,
    GET_PROP,
    SET_PROP,
    NEW_TABLE,
    GET_INDEX,
    SET_INDEX,
    LENGTH,
    HALT
};

// Value types
enum class ValueType : uint8_t {
    NIL,
    FLOAT,
    STRING,
    BOOL,
    TABLE,
    FUNCTION
};

struct Value {
    ValueType type = ValueType::NIL;
    union {
        float float_val;
        bool bool_val;
    };
    std::string string_val;
    
    Value() : float_val(0) {}
    Value(float v) : type(ValueType::FLOAT), float_val(v) {}
    Value(const std::string& v) : type(ValueType::STRING), string_val(v) {}
    Value(bool v) : type(ValueType::BOOL), bool_val(v) {}
    
    float to_float() const {
        switch (type) {
            case ValueType::FLOAT: return float_val;
            case ValueType::BOOL: return bool_val ? 1.0f : 0.0f;
            case ValueType::STRING: {
                char* end = nullptr;
                const float v = std::strtof(string_val.c_str(), &end);
                return (end && end != string_val.c_str() && *end == '\0') ? v : 0.0f;
            }
            default: return 0.0f;
        }
    }
    
    std::string to_string() const {
        switch (type) {
            case ValueType::FLOAT: return std::to_string(float_val);
            case ValueType::BOOL: return bool_val ? "true" : "false";
            case ValueType::STRING: return string_val;
            case ValueType::NIL: return "nil";
            case ValueType::TABLE: return "table";
            case ValueType::FUNCTION: return "function";
            default: return "?";
        }
    }
    
    bool is_truthy() const {
        switch (type) {
            case ValueType::BOOL: return bool_val;
            case ValueType::FLOAT: return float_val != 0;
            case ValueType::STRING: return !string_val.empty();
            case ValueType::NIL: return false;
            default: return true;
        }
    }
    
    bool operator==(const Value& other) const {
        if (type != other.type) return false;
        switch (type) {
            case ValueType::FLOAT: return float_val == other.float_val;
            case ValueType::BOOL: return bool_val == other.bool_val;
            case ValueType::STRING: return string_val == other.string_val;
            case ValueType::NIL: return true;
            default: return false;
        }
    }
};

// Script function
struct VMFunction {
    std::string name;
    std::vector<uint8_t> bytecode;
    std::vector<Value> constants;
    std::vector<std::string> local_names;
    uint32_t num_params = 0;
    
    VMFunction() = default;
    VMFunction(const std::string& n) : name(n) {}
};

// Call frame
struct CallFrame {
    VMFunction* function = nullptr;
    uint32_t pc = 0;
    uint32_t base = 0;
};

// Script VM
class ScriptVM {
public:
    static constexpr size_t DEFAULT_MAX_SOURCE_BYTES = 64 * 1024;
    static constexpr size_t DEFAULT_MAX_INSTRUCTIONS = 100000;
    static constexpr size_t DEFAULT_MAX_STACK_VALUES = 4096;

    ScriptVM() = default;

    void set_limits(size_t max_source_bytes, size_t max_instructions, size_t max_stack_values) {
        max_source_bytes_ = max_source_bytes ? max_source_bytes : 1;
        max_instructions_ = max_instructions ? max_instructions : 1;
        max_stack_values_ = max_stack_values ? max_stack_values : 1;
    }

    const std::string& last_error() const { return last_error_; }
    
    // Initialize VM
    bool initialize() {
        globals_["print"] = Value("print");
        globals_["math"] = Value("math");
        globals_["game"] = Value("game");
        globals_["input"] = Value("input");
        
        registerBuiltins();
        return true;
    }
    
    void shutdown() {
        globals_.clear();
        functions_.clear();
        frames_.clear();
        stack_.clear();
    }
    
    // Compile script text to bytecode
    bool compile(const std::string& script_name, const std::string& source) {
        last_error_.clear();
        if (script_name.empty()) {
            last_error_ = "script name is empty";
            return false;
        }
        if (source.size() > max_source_bytes_) {
            last_error_ = "script source exceeds configured byte limit";
            return false;
        }

        VMFunction func(script_name);
        try {
            std::istringstream stream(source);
            std::string line;
            while (std::getline(stream, line)) {
                if (line.empty() || line[0] == '#') continue;
                compileLine(line, func);
                if (func.bytecode.size() > 65535 || func.constants.size() > 255 ||
                    func.local_names.size() > 255) {
                    last_error_ = "script exceeds VM bytecode/index limits";
                    return false;
                }
            }
        } catch (const std::exception& e) {
            last_error_ = std::string("compile error: ") + e.what();
            return false;
        }

        func.bytecode.push_back((uint8_t)OpCode::HALT);
        functions_[script_name] = std::move(func);
        return true;
    }
    
    // Execute script
    bool execute(const std::string& script_name) {
        last_error_.clear();
        auto it = functions_.find(script_name);
        if (it == functions_.end()) {
            last_error_ = "script not found";
            return false;
        }
        stack_.clear();
        frames_.clear();
        return executeFunction(&it->second);
    }
    
    // Execute function
    bool executeFunction(VMFunction* func) {
        if (!func) return false;
        
        CallFrame frame;
        frame.function = func;
        frame.pc = 0;
        if (stack_.size() < func->num_params) return false;
        frame.base = static_cast<uint32_t>(stack_.size() - func->num_params);
        frames_.push_back(frame);
        
        return run();
    }
    
    // Register builtin function
    void registerBuiltin(const std::string& name, std::function<Value(const std::vector<Value>&)> fn) {
        builtins_[name] = fn;
    }
    
    // Set global variable
    void setGlobal(const std::string& name, const Value& val) {
        globals_[name] = val;
    }
    
    // Get global variable
    Value getGlobal(const std::string& name) const {
        auto it = globals_.find(name);
        return it != globals_.end() ? it->second : Value();
    }
    
    // Get stack top
    Value pop() {
        if (stack_.empty()) return Value();
        Value v = stack_.back();
        stack_.pop_back();
        return v;
    }
    
    // Push to stack
    void push(const Value& val) {
        stack_.push_back(val);
    }
    
private:
    void registerBuiltins() {
        registerBuiltin("print", [this](const std::vector<Value>& args) {
            std::string output;
            for (const auto& arg : args) {
                output += arg.to_string();
            }
            std::cout << "[Script] " << output << std::endl;
            return Value();
        });
        
        registerBuiltin("type", [](const std::vector<Value>& args) {
            if (args.empty()) return Value("nil");
            return Value(args[0].to_string());
        });
        
        registerBuiltin("tostring", [](const std::vector<Value>& args) {
            if (args.empty()) return Value("");
            return Value(args[0].to_string());
        });
        
        registerBuiltin("tonumber", [](const std::vector<Value>& args) {
            if (args.empty()) return Value(0.0f);
            return Value(args[0].to_float());
        });
    }
    
    void compileLine(const std::string& line, VMFunction& func) {
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        
        if (token == "var") {
            std::string name;
            ss >> name;
            func.local_names.push_back(name);
            
            // Check for initializer
            std::string eq;
            ss >> eq;
            if (eq == "=") {
                compileExpression(ss, func);
            }
            
            func.bytecode.push_back((uint8_t)OpCode::STORE_VAR);
            func.bytecode.push_back((uint8_t)func.local_names.size() - 1);
        } else if (token == "function") {
            std::string name;
            ss >> name;
            func.bytecode.push_back((uint8_t)OpCode::PUSH_STRING);
            func.constants.push_back(Value(name));
            func.bytecode.push_back((uint8_t)func.constants.size() - 1);
        } else if (token == "print") {
            compileExpression(ss, func);
            func.bytecode.push_back((uint8_t)OpCode::PRINT);
        } else if (token == "if") {
            compileExpression(ss, func);
            func.bytecode.push_back((uint8_t)OpCode::JUMP_IF_FALSE);
            func.bytecode.push_back(0); // placeholder
            func.bytecode.push_back(0); // placeholder
        } else if (token == "while") {
            compileExpression(ss, func);
            func.bytecode.push_back((uint8_t)OpCode::JUMP_IF_FALSE);
            func.bytecode.push_back(0); // placeholder
            func.bytecode.push_back(0); // placeholder
        } else if (token == "return") {
            compileExpression(ss, func);
            func.bytecode.push_back((uint8_t)OpCode::RETURN);
        } else if (token == "end") {
            // End of block
        } else {
            // Treat as expression statement
            std::string rest = line;
            std::istringstream rest_ss(rest);
            compileExpression(rest_ss, func);
        }
    }
    
    void compileExpression(std::istringstream& ss, VMFunction& func) {
        std::string token;
        ss >> token;
        
        if (token.empty()) return;
        
        // Number literal
        if (isdigit(token[0]) || (token[0] == '-' && token.size() > 1 && isdigit(token[1]))) {
            float val = std::stof(token);
            func.bytecode.push_back((uint8_t)OpCode::PUSH_FLOAT);
            func.constants.push_back(Value(val));
            func.bytecode.push_back((uint8_t)func.constants.size() - 1);
        } else if (token == "true") {
            func.bytecode.push_back((uint8_t)OpCode::PUSH_BOOL);
            func.constants.push_back(Value(true));
            func.bytecode.push_back((uint8_t)func.constants.size() - 1);
        } else if (token == "false") {
            func.bytecode.push_back((uint8_t)OpCode::PUSH_BOOL);
            func.constants.push_back(Value(false));
            func.bytecode.push_back((uint8_t)func.constants.size() - 1);
        } else if (token == "nil") {
            func.bytecode.push_back((uint8_t)OpCode::PUSH_FLOAT);
            func.constants.push_back(Value(0.0f));
            func.bytecode.push_back((uint8_t)func.constants.size() - 1);
        } else if (token[0] == '"') {
            // String literal
            std::string str = token.substr(1);
            if (str.back() == '"') str.pop_back();
            func.bytecode.push_back((uint8_t)OpCode::PUSH_STRING);
            func.constants.push_back(Value(str));
            func.bytecode.push_back((uint8_t)func.constants.size() - 1);
        } else if (token == "not") {
            compileExpression(ss, func);
            func.bytecode.push_back((uint8_t)OpCode::NOT);
        } else if (token == "and") {
            compileExpression(ss, func);
            func.bytecode.push_back((uint8_t)OpCode::AND);
        } else if (token == "or") {
            compileExpression(ss, func);
            func.bytecode.push_back((uint8_t)OpCode::OR);
        } else {
            // Variable or function call
            std::string name = token;
            
            // Check for binary operators
            std::string op;
            ss >> op;
            
            if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" || 
                op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
                // Binary operators are emitted left-to-right so non-commutative
                // operations (sub/div/mod/comparisons) preserve source order.
                func.bytecode.push_back((uint8_t)OpCode::LOAD_VAR);
                func.bytecode.push_back((uint8_t)getLocalIndex(name, func));
                compileExpression(ss, func);
                
                if (op == "+") func.bytecode.push_back((uint8_t)OpCode::ADD);
                else if (op == "-") func.bytecode.push_back((uint8_t)OpCode::SUB);
                else if (op == "*") func.bytecode.push_back((uint8_t)OpCode::MUL);
                else if (op == "/") func.bytecode.push_back((uint8_t)OpCode::DIV);
                else if (op == "%") func.bytecode.push_back((uint8_t)OpCode::MOD);
                else if (op == "==") func.bytecode.push_back((uint8_t)OpCode::EQ);
                else if (op == "!=") func.bytecode.push_back((uint8_t)OpCode::NEQ);
                else if (op == "<") func.bytecode.push_back((uint8_t)OpCode::LT);
                else if (op == ">") func.bytecode.push_back((uint8_t)OpCode::GT);
                else if (op == "<=") func.bytecode.push_back((uint8_t)OpCode::LTE);
                else if (op == ">=") func.bytecode.push_back((uint8_t)OpCode::GTE);
            } else {
                // Simple variable load
                func.bytecode.push_back((uint8_t)OpCode::LOAD_VAR);
                func.bytecode.push_back((uint8_t)getLocalIndex(name, func));
            }
        }
    }
    
    uint32_t getLocalIndex(const std::string& name, VMFunction& func) {
        for (uint32_t i = 0; i < func.local_names.size(); i++) {
            if (func.local_names[i] == name) return i;
        }
        func.local_names.push_back(name);
        return func.local_names.size() - 1;
    }
    
    bool run() {
        size_t instructions = 0;
        while (!frames_.empty()) {
            if (++instructions > max_instructions_) {
                last_error_ = "instruction limit exceeded";
                frames_.clear();
                stack_.clear();
                return false;
            }
            CallFrame& frame = frames_.back();
            VMFunction* func = frame.function;
            
            if (frame.pc >= func->bytecode.size()) {
                frames_.pop_back();
                continue;
            }
            
            uint8_t op = func->bytecode[frame.pc++];
            switch ((OpCode)op) {
                case OpCode::NOP: break;
                case OpCode::PUSH_FLOAT:
                case OpCode::PUSH_STRING:
                case OpCode::PUSH_BOOL: {
                    uint8_t idx = func->bytecode[frame.pc++];
                    if (idx < func->constants.size()) {
                        push(func->constants[idx]);
                    }
                    break;
                }
                case OpCode::POP: pop(); break;
                case OpCode::ADD: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a.to_float() + b.to_float()));
                    break;
                }
                case OpCode::SUB: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a.to_float() - b.to_float()));
                    break;
                }
                case OpCode::MUL: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a.to_float() * b.to_float()));
                    break;
                }
                case OpCode::DIV: {
                    Value b = pop();
                    Value a = pop();
                    if (b.to_float() != 0) push(Value(a.to_float() / b.to_float()));
                    else push(Value(0.0f));
                    break;
                }
                case OpCode::MOD: {
                    Value b = pop();
                    Value a = pop();
                    const float divisor = b.to_float();
                    push(Value(divisor != 0.0f ? std::fmod(a.to_float(), divisor) : 0.0f));
                    break;
                }
                case OpCode::NEG: {
                    Value a = pop();
                    push(Value(-a.to_float()));
                    break;
                }
                case OpCode::EQ: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a == b));
                    break;
                }
                case OpCode::NEQ: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(!(a == b)));
                    break;
                }
                case OpCode::LT: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a.to_float() < b.to_float()));
                    break;
                }
                case OpCode::GT: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a.to_float() > b.to_float()));
                    break;
                }
                case OpCode::LTE: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a.to_float() <= b.to_float()));
                    break;
                }
                case OpCode::GTE: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a.to_float() >= b.to_float()));
                    break;
                }
                case OpCode::AND: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a.is_truthy() && b.is_truthy()));
                    break;
                }
                case OpCode::OR: {
                    Value b = pop();
                    Value a = pop();
                    push(Value(a.is_truthy() || b.is_truthy()));
                    break;
                }
                case OpCode::NOT: {
                    Value a = pop();
                    push(Value(!a.is_truthy()));
                    break;
                }
                case OpCode::JUMP: {
                    uint8_t lo = func->bytecode[frame.pc++];
                    uint8_t hi = func->bytecode[frame.pc++];
                    frame.pc = (hi << 8) | lo;
                    break;
                }
                case OpCode::JUMP_IF_FALSE: {
                    uint8_t lo = func->bytecode[frame.pc++];
                    uint8_t hi = func->bytecode[frame.pc++];
                    Value cond = pop();
                    if (!cond.is_truthy()) {
                        frame.pc = (hi << 8) | lo;
                    }
                    break;
                }
                case OpCode::JUMP_IF_TRUE: {
                    uint8_t lo = func->bytecode[frame.pc++];
                    uint8_t hi = func->bytecode[frame.pc++];
                    Value cond = pop();
                    if (cond.is_truthy()) {
                        frame.pc = (hi << 8) | lo;
                    }
                    break;
                }
                case OpCode::LOAD_VAR: {
                    uint8_t idx = func->bytecode[frame.pc++];
                    const size_t slot = static_cast<size_t>(frame.base) + idx;
                    if (slot < stack_.size()) {
                        push(stack_[slot]);
                    } else {
                        push(Value());
                    }
                    break;
                }
                case OpCode::STORE_VAR: {
                    uint8_t idx = func->bytecode[frame.pc++];
                    Value val = pop();
                    while (stack_.size() <= frame.base + idx) {
                        stack_.push_back(Value());
                    }
                    stack_[frame.base + idx] = val;
                    break;
                }
                case OpCode::PRINT: {
                    Value val = pop();
                    std::cout << "[Script] " << val.to_string() << std::endl;
                    break;
                }
                case OpCode::RETURN:
                case OpCode::HALT:
                    frames_.pop_back();
                    break;
                default:
                    last_error_ = "invalid opcode";
                    frames_.clear();
                    stack_.clear();
                    return false;
            }
            if (stack_.size() > max_stack_values_) {
                last_error_ = "stack value limit exceeded";
                frames_.clear();
                stack_.clear();
                return false;
            }
        }
        return true;
    }

    size_t max_source_bytes_ = DEFAULT_MAX_SOURCE_BYTES;
    size_t max_instructions_ = DEFAULT_MAX_INSTRUCTIONS;
    size_t max_stack_values_ = DEFAULT_MAX_STACK_VALUES;
    std::string last_error_;
    
    std::unordered_map<std::string, Value> globals_;
    std::unordered_map<std::string, VMFunction> functions_;
    std::unordered_map<std::string, std::function<Value(const std::vector<Value>&)>> builtins_;
    std::vector<CallFrame> frames_;
    std::vector<Value> stack_;
};

} // namespace litt
