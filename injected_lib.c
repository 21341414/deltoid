#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <sys/mman.h>

// internal func types
typedef void (*r_lua_pushcclosure)(void* L, int (*fn)(void*), int n);
typedef void (*r_lua_setfield)(void* L, int idx, const char* k);
typedef void (*r_lua_pushstring)(void* L, const char* s);
typedef void* (*r_lua_getmetatable)(void* L, int idx);
typedef void (*r_lua_setreadonly)(void* L, int idx, int enabled);

// function Pointers
r_lua_pushcclosure r_pushcclosure;
r_lua_setfield r_setfield;
r_lua_pushstring r_pushstring;
r_lua_setreadonly r_setreadonly;

// unc func
int identifyexecutor(void* L) {
    r_pushstring(L, "Deltoid KX");
    return 1; 
}

int getgenv(void* L) {
    return 1; // returns globals
}

// heavyweight signature scanner 
uintptr_t scan_memory(uintptr_t start, uintptr_t end, const char* pattern) {
    const char* pat = pattern;
    uintptr_t first_match = 0;
    for (uintptr_t cur = start; cur < end; cur++) {
        if (!*pat) return first_match;
        if (*(uint8_t*)pat == '?' || *(uint8_t*)cur == (uint8_t)strtoul(pat, (char**)&pat, 16)) {
            if (!first_match) first_match = cur;
            if (!*pat) return first_match;
        } else {
            pat = pattern;
            first_match = 0;
        }
    }
    return 0;
}

// entry
void __attribute__((constructor)) deltoid_init() {
    struct link_map* map;
    void* handle = dlopen("libSoberRuntime.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) return;
    dlinfo(handle, RTLD_DI_LINKMAP, &map);
    uintptr_t base = (uintptr_t)map->l_addr;

    // scan for roblox functions
    r_pushstring = (r_lua_pushstring)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B F2");
    r_pushcclosure = (r_lua_pushcclosure)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 4C 8B D1");
    r_setreadonly = (r_lua_setreadonly)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B DA");

    if (r_pushstring && r_pushcclosure) {
        printf("[ Deltoid ] module injected. unc.\n");
    }
}
