#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <pthread.h>

// internal func types
typedef void (*r_lua_pushcclosure)(void* L, int (*fn)(void*), int n);
typedef void (*r_lua_setfield)(void* L, int idx, const char* k);
typedef void (*r_lua_pushstring)(void* L, const char* s);
typedef void (*r_lua_setreadonly)(void* L, int idx, int enabled);
typedef void (*r_luaL_loadstring)(void* L, const char* s);
typedef int (*r_lua_pcall)(void* L, int nargs, int nresults, int errfunc);
typedef void (*r_lua_getregistry)(void* L);

// function Pointers
r_lua_pushcclosure r_pushcclosure;
r_lua_setfield r_setfield;
r_lua_pushstring r_pushstring;
r_lua_setreadonly r_setreadonly;
r_luaL_loadstring r_loadstring;
r_lua_pcall r_pcall;
r_lua_getregistry r_getregistry;

void* global_L = NULL;

// unc func bridge
int identifyexecutor(void* L) {
    r_pushstring(L, "Deltoid KX");
    r_pushstring(L, "v1.0.0");
    return 2; 
}

int getgenv(void* L) {
    r_pushstring(L, "_G");
    return 1;
}

int getreg_bridge(void* L) {
    r_getregistry(L);
    return 1;
}

int setreadonly_bridge(void* L) {
    r_setreadonly(L, 1, 0); 
    return 0;
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
        } else { pat = pattern; first_match = 0; }
    }
    return 0;
}

// execute loop for gui
void* script_watcher(void* arg) {
    while(1) {
        if (access("execute.lua", F_OK) == 0) {
            FILE* f = fopen("execute.lua", "r");
            if (f && global_L) {
                fseek(f, 0, SEEK_END);
                long len = ftell(f); rewind(f);
                char* buf = malloc(len + 1);
                fread(buf, 1, len, f); buf[len] = '\0';
                fclose(f);
                r_loadstring(global_L, buf);
                r_pcall(global_L, 0, 0, 0);
                free(buf);
            }
            remove("execute.lua");
        }
        usleep(100000);
    }
}

// entry
void __attribute__((constructor)) deltoid_init() {
    struct link_map* map;
    void* handle = dlopen("libSoberRuntime.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) return;
    dlinfo(handle, RTLD_DI_LINKMAP, &map);
    uintptr_t base = (uintptr_t)map->l_addr;

    // unc signatures
    r_pushstring = (r_lua_pushstring)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B F2");
    r_pushcclosure = (r_lua_pushcclosure)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 4C 8B D1");
    r_setreadonly = (r_lua_setreadonly)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B DA");
    r_getregistry = (r_lua_getregistry)scan_memory(base, base + 0x6000000, "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 8B 01 48 FF 60 18");
    r_loadstring = (r_luaL_loadstring)scan_memory(base, base + 0x6000000, "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 8B 01 48 FF 60 10");
    r_pcall = (r_lua_pcall)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B D9");

    if (r_pushstring && r_pushcclosure) {
        pthread_t t;
        pthread_create(&t, NULL, script_watcher, NULL);
        pthread_detach(t);
        printf("[ Deltoid KX ] unc module ready.\n");
    }
}
