// Litt Engine - Simple Bytecode Scripting VM
// Embedded scripting without external dependencies

#pragma once
#include <cstdint>
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
    
    // Compile the supported line-oriented subset to bytecode. Unsupported
    // control-flow is rejected rather than emitting unpatched jumps.
    bool compile(const std::string& script_name, const std::string& source) {
        if (script_name.empty()) return false;
        VMFunction func(script_name);

        std::istringstream stream(source);
        std::string line;
        while (std::getline(stream, line)) {
            std::istringstream probe(line);
            std::string first;
            probe >> first;
            if (first.empty() || first[0] == '#') continue;
            if (!compileLine(line, func)) return false;
            if (func.constants.size() > 255 || func.local_names.size() > 255 ||
                func.bytecode.size() > 65535) {
                return false;
            }
        }

        func.bytecode.push_back(static_cast<uint8_t>(OpCode::HALT));
        functions_[script_name] = std::move(func);
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
        if (!func || stack_.size() < func->num_params) return false;

        const size_t base = stack_.size() - func->num_params;
        CallFrame frame;
        frame.function = func;
        frame.pc = 0;
        frame.base = static_cast<uint32_t>(base);
        frames_.push_back(frame);

        const bool ok = run();
        frames_.clear();
        if (stack_.size() > base) stack_.resize(base);
        return ok;
    }

    // Register builtin function
    void registerBuiltin(const std::string& name, std::function<Value(const std::vector<Value>&)> fn) {
        if (name.empty()) return;
        if (!fn) {
            builtins_.erase(name);
            return;
        }
        builtins_[name] = std::move(fn);
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

    size_t stack_size() const { return stack_.size(); }

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
    
    bool addConstant(VMFunction& func, const Value& value, uint8_t& index) {
        if (func.constants.size() >= 256) return false;
        func.constants.push_back(value);
        index = static_cast<uint8_t>(func.constants.size() - 1);
        return true;
    }

    bool getLocalIndex(const std::string& name, VMFunction& func, uint8_t& index,
                       bool create_if_missing = true) {
        for (size_t i = 0; i < func.local_names.size(); ++i) {
            if (func.local_names[i] == name) {
                index = static_cast<uint8_t>(i);
                return true;
            }
        }
        if (!create_if_missing || name.empty() || func.local_names.size() >= 256) return false;
        func.local_names.push_back(name);
        index = static_cast<uint8_t>(func.local_names.size() - 1);
        return true;
    }

    bool compileLine(const std::string& line, VMFunction& func) {
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        if (token.empty() || token[0] == '#') return true;

        if (token == "var") {
            std::string name;
            if (!(ss >> name) || name.empty()) return false;
            uint8_t local = 0;
            if (!getLocalIndex(name, func, local, true)) return false;

            std::string eq;
            if (ss >> eq) {
                if (eq != "=" || !compileExpression(ss, func)) return false;
            } else {
                uint8_t idx = 0;
                if (!addConstant(func, Value(), idx)) return false;
                func.bytecode.push_back(static_cast<uint8_t>(OpCode::PUSH_FLOAT));
                // NIL has no dedicated PUSH opcode in this VM. Use numeric zero
                // for uninitialized locals until NIL bytecode support exists.
                func.constants.back() = Value(0.0f);
                func.bytecode.push_back(idx);
            }
            func.bytecode.push_back(static_cast<uint8_t>(OpCode::STORE_VAR));
            func.bytecode.push_back(local);
            return true;
        }

        if (token == "print") {
            if (!compileExpression(ss, func)) return false;
            func.bytecode.push_back(static_cast<uint8_t>(OpCode::PRINT));
            return true;
        }

        if (token == "return") {
            std::string remainder;
            std::getline(ss, remainder);
            std::istringstream expr(remainder);
            std::string probe;
            expr >> probe;
            if (!probe.empty()) {
                std::istringstream actual(remainder);
                if (!compileExpression(actual, func)) return false;
            }
            func.bytecode.push_back(static_cast<uint8_t>(OpCode::RETURN));
            return true;
        }

        if (token == "if" || token == "while" || token == "function" || token == "end") {
            return false;
        }

        std::istringstream expr(line);
        return compileExpression(expr, func);
    }

    bool compileExpression(std::istringstream& ss, VMFunction& func) {
        std::string token;
        if (!(ss >> token) || token.empty()) return false;

        auto emit_constant = [&](OpCode op, const Value& value) {
            uint8_t idx = 0;
            if (!addConstant(func, value, idx)) return false;
            func.bytecode.push_back(static_cast<uint8_t>(op));
            func.bytecode.push_back(idx);
            return true;
        };

        const bool numeric_start =
            std::isdigit(static_cast<unsigned char>(token[0])) ||
            (token[0] == '-' && token.size() > 1 &&
             std::isdigit(static_cast<unsigned char>(token[1])));
        if (numeric_start) {
            char* end = nullptr;
            const float value = std::strtof(token.c_str(), &end);
            if (!end || end == token.c_str() || *end != '\0' || !std::isfinite(value)) return false;
            return emit_constant(OpCode::PUSH_FLOAT, Value(value));
        }

        if (token == "true") return emit_constant(OpCode::PUSH_BOOL, Value(true));
        if (token == "false") return emit_constant(OpCode::PUSH_BOOL, Value(false));
        if (token == "nil") return emit_constant(OpCode::PUSH_FLOAT, Value(0.0f));

        if (token[0] == '"') {
            std::string str = token.substr(1);
            while ((token.size() < 2 || token.back() != '"') && ss >> token) {
                str.push_back(' ');
                str += token;
            }
            if (token.empty() || token.back() != '"') return false;
            if (!str.empty() && str.back() == '"') str.pop_back();
            return emit_constant(OpCode::PUSH_STRING, Value(str));
        }

        if (token == "not") {
            if (!compileExpression(ss, func)) return false;
            func.bytecode.push_back(static_cast<uint8_t>(OpCode::NOT));
            return true;
        }

        uint8_t local = 0;
        if (!getLocalIndex(token, func, local, false)) return false;

        std::string op;
        if (!(ss >> op)) {
            func.bytecode.push_back(static_cast<uint8_t>(OpCode::LOAD_VAR));
            func.bytecode.push_back(local);
            return true;
        }

        const bool binary = op == "+" || op == "-" || op == "*" || op == "/" ||
                            op == "%" || op == "==" || op == "!=" || op == "<" ||
                            op == ">" || op == "<=" || op == ">=";
        if (!binary) return false;

        func.bytecode.push_back(static_cast<uint8_t>(OpCode::LOAD_VAR));
        func.bytecode.push_back(local);
        if (!compileExpression(ss, func)) return false;

        if (op == "+") func.bytecode.push_back(static_cast<uint8_t>(OpCode::ADD));
        else if (op == "-") func.bytecode.push_back(static_cast<uint8_t>(OpCode::SUB));
        else if (op == "*") func.bytecode.push_back(static_cast<uint8_t>(OpCode::MUL));
        else if (op == "/") func.bytecode.push_back(static_cast<uint8_t>(OpCode::DIV));
        else if (op == "%") func.bytecode.push_back(static_cast<uint8_t>(OpCode::MOD));
        else if (op == "==") func.bytecode.push_back(static_cast<uint8_t>(OpCode::EQ));
        else if (op == "!=") func.bytecode.push_back(static_cast<uint8_t>(OpCode::NEQ));
        else if (op == "<") func.bytecode.push_back(static_cast<uint8_t>(OpCode::LT));
        else if (op == ">") func.bytecode.push_back(static_cast<uint8_t>(OpCode::GT));
        else if (op == "<=") func.bytecode.push_back(static_cast<uint8_t>(OpCode::LTE));
        else func.bytecode.push_back(static_cast<uint8_t>(OpCode::GTE));
        return true;
    }

    bool readByte(CallFrame& frame, const VMFunction& func, uint8_t& out) {
        if (frame.pc >= func.bytecode.size()) return false;
        out = func.bytecode[frame.pc++];
        return true;
    }

    bool readU16(CallFrame& frame, const VMFunction& func, uint16_t& out) {
        uint8_t lo = 0, hi = 0;
        if (!readByte(frame, func, lo) || !readByte(frame, func, hi)) return false;
        out = static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
        return true;
    }

    bool run() {
        while (!frames_.empty()) {
            CallFrame& frame = frames_.back();
            VMFunction* func = frame.function;
            if (!func) return false;

            if (frame.pc >= func->bytecode.size()) {
                frames_.pop_back();
                continue;
            }

            uint8_t raw_op = 0;
            if (!readByte(frame, *func, raw_op)) return false;
            const OpCode op = static_cast<OpCode>(raw_op);

            switch (op) {
                case OpCode::NOP:
                    break;

                case OpCode::PUSH_FLOAT:
                case OpCode::PUSH_STRING:
                case OpCode::PUSH_BOOL: {
                    uint8_t idx = 0;
                    if (!readByte(frame, *func, idx) || idx >= func->constants.size()) return false;
                    push(func->constants[idx]);
                    break;
                }

                case OpCode::POP:
                    if (stack_.empty()) return false;
                    (void)pop();
                    break;

                case OpCode::ADD:
                case OpCode::SUB:
                case OpCode::MUL:
                case OpCode::DIV:
                case OpCode::MOD:
                case OpCode::EQ:
                case OpCode::NEQ:
                case OpCode::LT:
                case OpCode::GT:
                case OpCode::LTE:
                case OpCode::GTE:
                case OpCode::AND:
                case OpCode::OR: {
                    if (stack_.size() < 2) return false;
                    Value b = pop();
                    Value a = pop();
                    switch (op) {
                        case OpCode::ADD: push(Value(a.to_float() + b.to_float())); break;
                        case OpCode::SUB: push(Value(a.to_float() - b.to_float())); break;
                        case OpCode::MUL: push(Value(a.to_float() * b.to_float())); break;
                        case OpCode::DIV: {
                            const float divisor = b.to_float();
                            if (divisor == 0.0f) return false;
                            push(Value(a.to_float() / divisor));
                            break;
                        }
                        case OpCode::MOD: {
                            const float divisor = b.to_float();
                            if (divisor == 0.0f) return false;
                            push(Value(std::fmod(a.to_float(), divisor)));
                            break;
                        }
                        case OpCode::EQ: push(Value(a == b)); break;
                        case OpCode::NEQ: push(Value(!(a == b))); break;
                        case OpCode::LT: push(Value(a.to_float() < b.to_float())); break;
                        case OpCode::GT: push(Value(a.to_float() > b.to_float())); break;
                        case OpCode::LTE: push(Value(a.to_float() <= b.to_float())); break;
                        case OpCode::GTE: push(Value(a.to_float() >= b.to_float())); break;
                        case OpCode::AND: push(Value(a.is_truthy() && b.is_truthy())); break;
                        case OpCode::OR: push(Value(a.is_truthy() || b.is_truthy())); break;
                        default: return false;
                    }
                    break;
                }

                case OpCode::NEG:
                case OpCode::NOT: {
                    if (stack_.empty()) return false;
                    Value a = pop();
                    if (op == OpCode::NEG) push(Value(-a.to_float()));
                    else push(Value(!a.is_truthy()));
                    break;
                }

                case OpCode::JUMP:
                case OpCode::JUMP_IF_FALSE:
                case OpCode::JUMP_IF_TRUE: {
                    uint16_t target = 0;
                    if (!readU16(frame, *func, target) || target >= func->bytecode.size()) return false;
                    if (op == OpCode::JUMP) {
                        frame.pc = target;
                    } else {
                        if (stack_.empty()) return false;
                        const bool truthy = pop().is_truthy();
                        if ((op == OpCode::JUMP_IF_FALSE && !truthy) ||
                            (op == OpCode::JUMP_IF_TRUE && truthy)) {
                            frame.pc = target;
                        }
                    }
                    break;
                }

                case OpCode::LOAD_VAR: {
                    uint8_t idx = 0;
                    if (!readByte(frame, *func, idx)) return false;
                    const size_t slot = static_cast<size_t>(frame.base) + idx;
                    push(slot < stack_.size() ? stack_[slot] : Value());
                    break;
                }

                case OpCode::STORE_VAR: {
                    uint8_t idx = 0;
                    if (!readByte(frame, *func, idx) || stack_.empty()) return false;
                    Value val = pop();
                    const size_t slot = static_cast<size_t>(frame.base) + idx;
                    if (slot > 65535) return false;
                    while (stack_.size() <= slot) stack_.push_back(Value());
                    stack_[slot] = val;
                    break;
                }

                case OpCode::PRINT:
                    if (stack_.empty()) return false;
                    std::cout << "[Script] " << pop().to_string() << std::endl;
                    break;

                case OpCode::RETURN:
                case OpCode::HALT:
                    frames_.pop_back();
                    break;

                default:
                    // The compiler does not emit the remaining opcodes yet.
                    return false;
            }
        }
        return true;
    }

    std::unordered_map<std::string, Value> globals_;
    std::unordered_map<std::string, VMFunction> functions_;
    std::unordered_map<std::string, std::function<Value(const std::vector<Value>&)>> builtins_;
    std::vector<CallFrame> frames_;
    std::vector<Value> stack_;
};

} // namespace litt
