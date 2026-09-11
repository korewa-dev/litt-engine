#include "litt_engine2.h"
#include <cstdio>

int main() {
    litt2::World w = litt2::buildWorldFromFile("alt/Project/json-demo/world.json");
    
    printf("=== %s ===\n", w.name.c_str());
    printf("Objects: %zu\n", w.objects.size());
    for (const auto& obj : w.objects) {
        printf("  %-10s '%s' at (%5.1f, %5.1f, %5.1f) scale (%.1f, %.1f, %.1f)\n", 
               obj.type.c_str(), obj.name.c_str(), 
               obj.pos.x, obj.pos.y, obj.pos.z,
               obj.scale.x, obj.scale.y, obj.scale.z);
    }
    return 0;
}
