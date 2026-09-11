#include "litt_engine2.h"
#include <cstdio>

int main() {
    // Test: parse JSON and build world
    const char* json = R"({
        "name": "Test World",
        "objects": [
            {"type":"cube", "name":"ground", "pos":[0,0,0], "scale":[10,0.5,10], "color":"green"},
            {"type":"sphere", "name":"ball", "pos":[0,3,0], "scale":[1,1,1], "color":"red"},
            {"type":"cylinder", "name":"pillar", "pos":[2,1,2], "scale":[0.5,2,0.5], "color":"blue"}
        ]
    })";
    
    litt2::World w = litt2::buildWorld(json);
    
    printf("World: %s\n", w.name.c_str());
    printf("Objects: %zu\n", w.objects.size());
    for (const auto& obj : w.objects) {
        printf("  %s '%s' at (%.1f, %.1f, %.1f)\n", 
               obj.type.c_str(), obj.name.c_str(), obj.pos.x, obj.pos.y, obj.pos.z);
    }
    
    return 0;
}
