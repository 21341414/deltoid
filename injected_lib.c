#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <link.h>
#include <sys/mman.h>

// roblox lua types 
typedef void (*r_lua_pushstring)(void* L, const char* s);
typedef void (*r_lua_setglobal)(void* L, const char* name);
typedef void (*r_lua_pushcclosure)(void* L, int (*fn)(void*), int n);

// unc func implementations 
int identifyexecutor(void* L) {
    // ts is how we find the internal pushstring function
    // for this example, we assume we've mapped it to a global pointer
    printf("[Deltoid] identifyexecutor called!\n");
    return 1; // num of results returned to lua
}

int getgenv(void* L) {
    // returns the global environment table
    return 1;
}

//  mem scanner 
uintptr_t find_pattern(uintptr_t start, uintptr_t end, const char* pattern) {
    const char* pat = pattern;
    uintptr_t first_match = 0;
    for (uintptr_t cur = start; cur < end; cur++) {
        if (!*pat) return first_match;
        if (*(uint8_t*)pat == '\?' || *(uint8_t*)cur == (uint8_t)strtoul(pat, (char**)&pat, 16)) {
            if (!first_match) first_match = cur;
            if (!*pat) return first_match;
        } else {
            pat = pattern;
            first_match = 0;
        }
    }
    return 0;
}

// constructor ( runs on load ) 
void __attribute__((constructor)) deltoid_main() {
    printf("\n[Deltoid] Hooking Sober Runtime...\n");

    // get the base address of libSoberRuntime.so
    struct link_map* map;
    void* handle = dlopen("libSoberRuntime.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) {
        printf("[ Deltoid ] failed to find sober lib.\n");
        return;
    }
    dlinfo(handle, RTLD_DI_LINKMAP, &map);
    uintptr_t base = (uintptr_t)map->l_addr;

    // scan for roblox's lua funcs ( using AOBs )
    // noted : these AOBs are example patterns for the x86_64 android build
    uintptr_t pushstring_addr = find_pattern(base, base + 0x5000000, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B F2");
    
    if (pushstring_addr) {
        printf("[ Deltoid ] found pushstring at %p\n", (void*)pushstring_addr);
        // you would cast pushstring_addr to r_lua_pushstring and use it
    }

    printf("[Deltoid] UNC Bridge Initialized.\n");
}
