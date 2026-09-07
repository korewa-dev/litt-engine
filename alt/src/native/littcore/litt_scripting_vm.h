// Litt Engine - Simple Bytecode Scripting VM
// Embedded scripting without external dependencies

#pragma once
#include "litt_scripting.h"
#include <stack>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <cstring>

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
            case ValueType::STRING: return (float)std::stod(string_val);
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
    ScriptVM() = default;
    
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
        VMFunction func(script_name);
        
        // Simple line-based compiler
        std::istringstream stream(source);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty() || line[0] == '#') continue;
            compileLine(line, func);
        }
        
        func.bytecode.push_back((uint8_t)OpCode::HALT);
        functions_[script_name] = func;
        return true;
    }
    
    // Execute script
    bool execute(const std::string& script_name) {
        auto it = functions_.find(script_name);
        if (it == functions_.end()) return false;
        return executeFunction(&it->second);
    }
    
    // Execute function
    bool executeFunction(VMFunction* func) {
        if (!func) return false;
        
        CallFrame frame;
        frame.function = func;
        frame.pc = 0;
        frame.base = stack_.size() - func->num_params;
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
    
    void compileExpression(std::istringstream& ss, ScriptFunction& func) {
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
                compileExpression(ss, func);
                func.bytecode.push_back((uint8_t)OpCode::LOAD_VAR);
                func.bytecode.push_back((uint8_t)getLocalIndex(name, func));
                
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
    
    uint32_t getLocalIndex(const std::string& name, ScriptFunction& func) {
        for (uint32_t i = 0; i < func.local_names.size(); i++) {
            if (func.local_names[i] == name) return i;
        }
        func.local_names.push_back(name);
        return func.local_names.size() - 1;
    }
    
    bool run() {
        while (!frames_.empty()) {
            CallFrame& frame = frames_.back();
            ScriptFunction* func = frame.function;
            
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
                    push(Value(fmod(a.to_float(), b.to_float())));
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
                    if (idx < stack_.size()) {
                        push(stack_[frame.base + idx]);
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
                case OpCode::HALT:
                    frames_.pop_back();
                    break;
                default:
                    break;
            }
        }
        return true;
    }
    
    std::unordered_map<std::string, Value> globals_;
    std::unordered_map<std::string, ScriptFunction> functions_;
    std::unordered_map<std::string, std::function<Value(const std::vector<Value>&)>> builtins_;
    std::vector<CallFrame> frames_;
    std::vector<Value> stack_;
};

} // namespace litt
