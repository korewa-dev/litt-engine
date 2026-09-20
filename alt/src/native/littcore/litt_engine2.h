// Litt Engine 2.0 — JSON-Driven Runtime
// AI writes JSON. Engine renders. No compilation needed.
// Single header, ~150 lines. No cubes-and-cylinders problem.

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <limits>

namespace litt2 {

struct Vec3 { float x=0,y=0,z=0; Vec3()=default; Vec3(float a,float b,float c):x(a),y(b),z(c){} };
struct Color { uint8_t r=255,g=255,b=255; };
struct Object {
    std::string type, name;
    Vec3 pos, rot, scale{1,1,1};
    Color color{255,255,255};
};
struct World {
    std::string name="untitled";
    std::vector<Object> objects;
    Color sky{135,206,235};
    Vec3 camPos{0,5,10}, camTarget{0,0,0};
    float fov=60;
};

struct Json {
    enum T { NUL, NUM, STR, ARR, OBJ, BOOL } type=NUL;
    double num=0; std::string str; std::vector<Json> arr;
    std::unordered_map<std::string,Json> obj; bool bval=false;
    static Json parse(const std::string& s);
    double d(double v=0) const { return type==NUM?num:v; }
    const Json& operator[](const std::string& k) const { static const Json n; auto i=obj.find(k); return i!=obj.end()?i->second:n; }
    const Json& operator[](size_t i) const { static const Json n; return i<arr.size()?arr[i]:n; }
    size_t size() const { return arr.size(); }
};

Json Json::parse(const std::string& s) {
    struct Parser {
        const std::string& text;
        size_t pos = 0;
        unsigned depth = 0;
        bool ok = true;

        void skip_ws() {
            while (pos < text.size()) {
                const char ch = text[pos];
                if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') break;
                ++pos;
            }
        }

        bool consume(char expected) {
            skip_ws();
            if (pos >= text.size() || text[pos] != expected) {
                ok = false;
                return false;
            }
            ++pos;
            return true;
        }

        bool parse_string(std::string& out) {
            skip_ws();
            if (pos >= text.size() || text[pos] != '"') {
                ok = false;
                return false;
            }
            ++pos;
            out.clear();
            while (pos < text.size()) {
                const unsigned char ch = static_cast<unsigned char>(text[pos++]);
                if (ch == '"') return true;
                if (ch < 0x20) {
                    ok = false;
                    return false;
                }
                if (ch != '\\') {
                    out.push_back(static_cast<char>(ch));
                    continue;
                }
                if (pos >= text.size()) {
                    ok = false;
                    return false;
                }
                const char esc = text[pos++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    default:
                        // Unicode escapes are intentionally unsupported in this
                        // legacy compatibility parser. Reject rather than corrupt.
                        ok = false;
                        return false;
                }
            }
            ok = false;
            return false;
        }

        bool parse_number(Json& out) {
            skip_ws();
            const size_t begin = pos;
            if (pos < text.size() && text[pos] == '-') ++pos;
            if (pos >= text.size()) { ok = false; return false; }

            if (text[pos] == '0') {
                ++pos;
                if (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
                    ok = false;
                    return false;
                }
            } else {
                if (!std::isdigit(static_cast<unsigned char>(text[pos]))) {
                    ok = false;
                    return false;
                }
                while (pos < text.size() &&
                       std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
            }

            if (pos < text.size() && text[pos] == '.') {
                ++pos;
                const size_t frac = pos;
                while (pos < text.size() &&
                       std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
                if (pos == frac) { ok = false; return false; }
            }

            if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
                ++pos;
                if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) ++pos;
                const size_t exp = pos;
                while (pos < text.size() &&
                       std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
                if (pos == exp) { ok = false; return false; }
            }

            const std::string token = text.substr(begin, pos - begin);
            errno = 0;
            char* parsed_end = nullptr;
            const double value = std::strtod(token.c_str(), &parsed_end);
            if (errno == ERANGE || !parsed_end || *parsed_end != '\0' || !std::isfinite(value)) {
                ok = false;
                return false;
            }
            out = Json();
            out.type = Json::NUM;
            out.num = value;
            return true;
        }

        bool parse_value(Json& out) {
            skip_ws();
            if (!ok || pos >= text.size() || depth > 64) {
                ok = false;
                return false;
            }

            const char ch = text[pos];
            if (ch == '"') {
                out = Json();
                out.type = Json::STR;
                return parse_string(out.str);
            }

            if (ch == '{') {
                if (++depth > 64) { ok = false; return false; }
                out = Json();
                out.type = Json::OBJ;
                ++pos;
                skip_ws();
                if (pos < text.size() && text[pos] == '}') {
                    ++pos;
                    --depth;
                    return true;
                }
                while (ok) {
                    std::string key;
                    if (!parse_string(key) || !consume(':')) break;
                    Json value;
                    if (!parse_value(value)) break;
                    out.obj[std::move(key)] = std::move(value);
                    skip_ws();
                    if (pos < text.size() && text[pos] == '}') {
                        ++pos;
                        --depth;
                        return true;
                    }
                    if (!consume(',')) break;
                }
                --depth;
                ok = false;
                return false;
            }

            if (ch == '[') {
                if (++depth > 64) { ok = false; return false; }
                out = Json();
                out.type = Json::ARR;
                ++pos;
                skip_ws();
                if (pos < text.size() && text[pos] == ']') {
                    ++pos;
                    --depth;
                    return true;
                }
                while (ok) {
                    Json value;
                    if (!parse_value(value)) break;
                    out.arr.push_back(std::move(value));
                    skip_ws();
                    if (pos < text.size() && text[pos] == ']') {
                        ++pos;
                        --depth;
                        return true;
                    }
                    if (!consume(',')) break;
                }
                --depth;
                ok = false;
                return false;
            }

            auto literal = [&](const char* word, Json::T type, bool boolean) {
                const size_t len = std::strlen(word);
                if (text.compare(pos, len, word) != 0) {
                    ok = false;
                    return false;
                }
                pos += len;
                out = Json();
                out.type = type;
                out.bval = boolean;
                return true;
            };

            if (ch == 't') return literal("true", Json::BOOL, true);
            if (ch == 'f') return literal("false", Json::BOOL, false);
            if (ch == 'n') return literal("null", Json::NUL, false);
            return parse_number(out);
        }
    };

    Parser parser{s};
    Json result;
    if (!parser.parse_value(result)) return Json();
    parser.skip_ws();
    if (!parser.ok || parser.pos != s.size()) return Json();
    return result;
}

inline World buildWorld(const std::string& json) {
    World w; Json r=Json::parse(json);
    if (r.type != Json::OBJ) return w;
    w.name=r["name"].type==Json::STR?r["name"].str:"untitled";
    for(size_t i=0;i<r["objects"].size();i++){
        const Json& o=r["objects"][i]; Object ob;
        ob.type=o["type"].type==Json::STR?o["type"].str:"cube";
        ob.name=o["name"].type==Json::STR?o["name"].str:"";
        const Json& p=o["pos"]; if(p.type==Json::ARR&&p.size()>=3){ob.pos={(float)p[0].d(),(float)p[1].d(),(float)p[2].d()};}
        const Json& s=o["scale"]; if(s.type==Json::ARR&&s.size()>=3){ob.scale={(float)s[0].d(1),(float)s[1].d(1),(float)s[2].d(1)};}
        const Json& c=o["color"];
        if(c.type==Json::ARR&&c.size()>=3) {
            auto channel=[](double v)->uint8_t {
                if (!std::isfinite(v)) return 0;
                v=std::clamp(v,0.0,255.0);
                return static_cast<uint8_t>(v);
            };
            ob.color={channel(c[0].d()),channel(c[1].d()),channel(c[2].d())};
        }
        else if(c.type==Json::STR){ std::string col=c.str;
            if(col=="red")ob.color={255,0,0}; else if(col=="green")ob.color={0,255,0};
            else if(col=="blue")ob.color={0,0,255}; else if(col=="yellow")ob.color={255,255,0}; }
        w.objects.push_back(ob);
    }
    return w;
}

inline World buildWorldFromFile(const std::string& path) {
    std::ifstream f(path); if(!f.is_open()) return World();
    std::string s((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
    return buildWorld(s);
}

}
