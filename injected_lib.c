#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

// fix: proper pointer typedefs + void* L
typedef void (*r_lua_pushcclosure)(void* L, int (*fn)(void*), int n);
typedef void (*r_lua_setfield)(void* L, int idx, const char* k);
typedef void (*r_lua_pushstring)(void* L, const char* s);
typedef void (*r_lua_setreadonly)(void* L, int idx, int enabled);
typedef void (*r_luaL_loadstring)(void* L, const char* s);
typedef int (*r_lua_pcall)(void* L, int nargs, int nresults, int errfunc);
typedef void (*r_lua_getfield)(void* L, int idx, const char* k);
typedef const char* (*r_lua_tostring)(void* L, int idx);

r_lua_pushcclosure r_pushcclosure;
r_lua_setfield r_setfield;
r_lua_pushstring r_pushstring;
r_lua_setreadonly r_setreadonly;
r_luaL_loadstring r_loadstring;
r_lua_pcall r_pcall;
r_lua_getfield r_getfield;
r_lua_tostring r_tostring;

void* global_L = NULL;
int registered = 0;

// fix: scanner pointer logic + wildcards
uintptr_t scan_memory(uintptr_t start, uintptr_t end, const char* pattern) {
    const char* pat = pattern;
    uintptr_t first_match = 0;
    for (uintptr_t cur = start; cur < end; cur++) {
        if (!*pat) return first_match;
        if (*pat == ' ') { pat++; cur--; continue; }
        if (*pat == '?' || *(uint8_t*)cur == (uint8_t)strtoul(pat, (char**)&pat, 16)) {
            if (!first_match) first_match = cur;
            if (*pat == '?') pat++;
        } else {
            pat = pattern;
            first_match = 0;
        }
    }
    return 0;
}

// fix: actual memory write into stub
void place_jmp(void* src, void* dst) {
    uintptr_t page = (uintptr_t)src & ~0xFFF;
    mprotect((void*)page, 4096, PROT_READ | PROT_WRITE | PROT_EXEC);
    uint8_t stub[] = { 
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp qword ptr [rip+0]
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // 64-bit addr
    };
    *(uint64_t*)(&stub[6]) = (uintptr_t)dst;
    memcpy(src, stub, sizeof(stub));
}

typedef int (*pcall_fn)(void* L, int na, int nr, int ef);
pcall_fn o_pcall;

int h_pcall(void* L, int nargs, int nresults, int errfunc) {
    if (!global_L) {
        global_L = L;
        printf("[ Deltoid KX ] captured state: %p\n", L);
    }
    return o_pcall(L, nargs, nresults, errfunc);
}

int httpget_bridge(void* L) {
    const char* url = r_tostring(L, 1);
    if (!url) return 0;
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -sL --max-time 10 '%s'", url);
    FILE* p = popen(cmd, "r");
    if (!p) return 0;

    size_t size = 1024;
    char* buf = malloc(size);
    size_t offset = 0;
    
    while (fgets(buf + offset, size - offset, p)) {
        size_t len = strlen(buf + offset);
        offset += len;
        if (size - offset < 128) {
            size *= 2;
            buf = realloc(buf, size);
        }
    }
    pclose(p);
    r_pushstring(L, buf);
    free(buf);
    return 1;
}

// fix: multiline macro + proper casting
void register_unc(void* L) {
    if (registered) return;
    #define BIND(n, f) { r_pushcclosure(L, f, 0); r_setfield(L, -10002, n); }
    BIND("identifyexecutor", identifyexecutor);
    BIND("httpget", httpget_bridge);
    registered = 1;
    printf("[ Deltoid KX ] unc bound\n");
}

void* script_watcher(void* arg) {
    while(1) {
        if (global_L && access("execute.lua", F_OK) == 0) {
            register_unc(global_L);
            FILE* f = fopen("execute.lua", "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                long len = ftell(f);
                rewind(f);
                char* buf = malloc(len + 1);
                fread(buf, 1, len, f);
                buf[len] = '\0';
                fclose(f);
                
                printf("[ Deltoid LOG ] executing...\n");
                r_loadstring(global_L, buf);
                if (r_pcall(global_L, 0, 0, 0) != 0) {
                    printf("[ Deltoid ERROR ] %s\n", r_tostring(global_L, -1));
                }
                free(buf);
            }
            remove("execute.lua");
        }
        usleep(200000);
    }
}

// fix: correct constructor syntax
void __attribute__((constructor)) deltoid_init() {
    struct link_map* map;
    void* handle = dlopen("libSoberRuntime.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) return;
    dlinfo(handle, RTLD_DI_LINKMAP, &map);
    uintptr_t base = (uintptr_t)map->l_addr;

    r_pcall = (r_lua_pcall)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B D9");
    r_pushstring = (r_lua_pushstring)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B F2");
    r_pushcclosure = (r_lua_pushcclosure)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 4C 8B D1");
    r_loadstring = (r_luaL_loadstring)scan_memory(base, base + 0x6000000, "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 8B 01 48 FF 60 10");
    r_tostring = (r_lua_tostring)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B DA 48 8B D1");
    r_setfield = (r_lua_setfield)scan_memory(base, base + 0x6000000, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B DA");

    if (r_pcall) {
        o_pcall = mmap(NULL, 4096, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_PRIVATE, -1, 0);
        // fix: 14 byte copy for absolute jump boundary
        memcpy(o_pcall, r_pcall, 14); 
        place_jmp((void*)((uintptr_t)o_pcall + 14), (void*)((uintptr_t)r_pcall + 14));
        place_jmp(r_pcall, h_pcall);
    }

    pthread_t t;
    pthread_create(&t, NULL, script_watcher, NULL);
    pthread_detach(t);
}
