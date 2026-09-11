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
    const Json& operator[](const std::string& k) const { static Json n; auto i=obj.find(k); return i!=obj.end()?i->second:n; }
    const Json& operator[](size_t i) const { static Json n; return i<arr.size()?arr[i]:n; }
    size_t size() const { return arr.size(); }
};

Json Json::parse(const std::string& s) {
    Json r; size_t i=0;
    auto skip=[&]{ while(i<s.size()&&(s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r'))i++; };
    auto parseStr=[&](){
        std::string r; i++;
        while(i<s.size()&&s[i]!='\"'){ if(s[i]=='\\'){r+=s[i+1];i++;} else r+=s[i]; i++; }
        i++; return r;
    };
    std::function<Json()> parseVal;
    parseVal=[&](){
        skip(); if(i>=s.size()) return Json();
        if(s[i]=='\"'){ auto j=Json(); j.type=STR; j.str=parseStr(); return j; }
        if(s[i]=='{'){ auto j=Json(); j.type=OBJ; i++; skip();
            while(i<s.size()&&s[i]!='}'){ auto k=parseStr(); skip(); i++; skip();
                j.obj[k]=parseVal(); skip(); if(s[i]==',')i++; skip(); } i++; return j; }
        if(s[i]=='['){ auto j=Json(); j.type=ARR; i++; skip();
            while(i<s.size()&&s[i]!=']'){ j.arr.push_back(parseVal()); skip(); if(s[i]==',')i++; skip(); } i++; return j; }
        if(s[i]=='t'||s[i]=='f'){ auto j=Json(); j.type=BOOL; j.bval=(s[i]=='t'); i+=j.bval?4:5; return j; }
        if(s[i]=='n'){ i+=4; return Json(); }
        auto j=Json(); j.type=NUM; size_t pos; j.num=std::stod(s.substr(i),&pos); i+=pos;
        return j;
    };
    return parseVal();
}

inline World buildWorld(const std::string& json) {
    World w; Json r=Json::parse(json);
    w.name=r["name"].type==Json::STR?r["name"].str:"untitled";
    for(size_t i=0;i<r["objects"].size();i++){
        const Json& o=r["objects"][i]; Object ob;
        ob.type=o["type"].type==Json::STR?o["type"].str:"cube";
        ob.name=o["name"].type==Json::STR?o["name"].str:"";
        const Json& p=o["pos"]; if(p.type==Json::ARR&&p.size()>=3){ob.pos={(float)p[0].d(),(float)p[1].d(),(float)p[2].d()};}
        const Json& s=o["scale"]; if(s.type==Json::ARR&&s.size()>=3){ob.scale={(float)s[0].d(1),(float)s[1].d(1),(float)s[2].d(1)};}
        const Json& c=o["color"];
        if(c.type==Json::ARR&&c.size()>=3) ob.color={(uint8_t)c[0].d(),(uint8_t)c[1].d(),(uint8_t)c[2].d()};
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
