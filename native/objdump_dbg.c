/* objdump_dbg.c - temporary loader audit */
#include <stdio.h>
#include "littcore/litt_obj.h"

int main(int argc, char **argv) {
    LvModel m;
    if (lv_obj_load(argv[1], &m)) {
        printf("LOAD FAILED\n");
        return 1;
    }
    printf("meshes=%d\n", m.count);
    for (int i = 0; i < m.count && i < 12; i++) {
        printf("  [%d] %-20s in=%-4d bmin=(%.1f %.1f %.1f) bmax=(%.1f %.1f %.1f)\n",
               i, m.meshes[i].name, m.meshes[i].in,
               m.meshes[i].bmin[0], m.meshes[i].bmin[1], m.meshes[i].bmin[2],
               m.meshes[i].bmax[0], m.meshes[i].bmax[1], m.meshes[i].bmax[2]);
    }
    return 0;
}
