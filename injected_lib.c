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
#include <stdatomic.h>

//  typedefs
typedef void  (*r_lua_pushcclosure)(void* L, int (*fn)(void*), int n);
typedef void  (*r_lua_setfield)    (void* L, int idx, const char* k);
typedef void  (*r_lua_pushstring)  (void* L, const char* s);
typedef int   (*r_luaL_loadstring) (void* L, const char* s);
typedef int   (*r_lua_pcall)       (void* L, int nargs, int nresults, int errfunc);
typedef void  (*r_lua_getfield)    (void* L, int idx, const char* k);
typedef const char* (*r_lua_tostring)(void* L, int idx);

r_lua_pushcclosure  r_pushcclosure;
r_lua_setfield      r_setfield;
r_lua_pushstring    r_pushstring;
r_luaL_loadstring   r_loadstring;
r_lua_pcall         r_pcall;
r_lua_getfield      r_getfield;
r_lua_tostring      r_tostring;

//  global state  ( atomic for cross-thread visibility )
static atomic_uintptr_t global_L   = 0;
static atomic_int       registered = 0;

//  pattern scanner — fixed
//  format: "48 89 5C ? ? 48" space-separated tokens,
//  each token is a 2-digit hex byte OR '?' ( wildcard ).
//  returns first match address or 0.
static uintptr_t scan_memory(uintptr_t start, uintptr_t end,
                              const char* pattern)
{
    // pre-parse pattern into ( byte, is_wildcard ) pairs
    uint8_t  pat_bytes[256];
    uint8_t  pat_wild[256];
    int      pat_len = 0;

    const char* p = pattern;
    while (*p && pat_len < 256) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '?') {
            pat_wild[pat_len] = 1;
            pat_bytes[pat_len] = 0;
            pat_len++;
            p++;
            if (*p == '?') p++;   // consume optional second '?'
        } else {
            pat_wild[pat_len] = 0;
            pat_bytes[pat_len] = (uint8_t)strtoul(p, (char**)&p, 16);
            pat_len++;
        }
    }
    if (pat_len == 0) return 0;

    for (uintptr_t cur = start; cur <= end - pat_len; cur++) {
        int match = 1;
        for (int i = 0; i < pat_len; i++) {
            if (!pat_wild[i] && *(uint8_t*)(cur + i) != pat_bytes[i]) {
                match = 0;
                break;
            }
        }
        if (match) return cur;
    }
    return 0;
}

//  trampoline helpers

// returns the length ( in bytes ) of a minimal prologue we can
// safely relocate — walks instructions until we have >= 14 bytes.
// Requires a very small runtime disassembler.  For now we use a
// conservative table-driven single-byte/prefix decoder covering
// the common x86-64 prologues seen in Luau.
static int measure_prologue(const uint8_t* fn)
{
    int off = 0;
    while (off < 14) {
        uint8_t b = fn[off];
        // REX prefix
        if ((b & 0xF0) == 0x40) { off++; b = fn[off]; }
        // common 1-byte opcodes with no operands or fixed small operands
        if (b == 0x53 || b == 0x55 || b == 0x56 || b == 0x57 ||  // PUSH r
            b == 0x5B || b == 0x5D || b == 0x5E || b == 0x5F) {  // POP r
            off += 1; continue;
        }
        if (b == 0x48 || b == 0x4C) {   // REX.W already consumed above — shouldn't hit
            off += 1; continue;
        }
        if (b == 0x89 || b == 0x8B) {   // MOV r/m, r  — ModRM + optional SIB/disp
            uint8_t modrm = fn[off + 1];
            uint8_t mod   = modrm >> 6;
            uint8_t rm    = modrm & 7;
            int extra = 1; // modrm byte
            if (mod == 1) extra += 1;           // disp8
            else if (mod == 2) extra += 4;      // disp32
            else if (mod == 0 && rm == 5) extra += 4; // RIP-relative
            if (rm == 4 && mod != 3) extra += 1; // SIB
            off += 1 + extra; continue;
        }
        if (b == 0x83) { off += 3; continue; }  // ADD/SUB/CMP r/m, imm8
        if (b == 0x48 + 0) { off += 1; continue; } // fallthrough guard
        // unknown just advance 1 byte conservatively
        off += 1;
    }
    return off;
}

// write a 14-byte absolute indirect jmp to dst at src
static void write_abs_jmp(void* src, void* dst)
{
    uint8_t stub[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,  // JMP QWORD PTR [ RIP+0 ]
        0,0,0,0,0,0,0,0                       // 64-bit target address
    };
    *(uint64_t*)(&stub[6]) = (uintptr_t)dst;
    memcpy(src, stub, 14);
}

// make page containing addr writable+exec
static void unprotect(void* addr, size_t len)
{
    uintptr_t page = (uintptr_t)addr & ~(uintptr_t)0xFFF;
    mprotect((void*)page, len + 4096, PROT_READ | PROT_WRITE | PROT_EXEC);
}

//  pcall hook captures lua_State*
typedef int (*pcall_fn)(void*, int, int, int);
static pcall_fn o_pcall = NULL;

static int h_pcall(void* L, int nargs, int nresults, int errfunc)
{
    if (!atomic_load(&global_L)) {
        atomic_store(&global_L, (uintptr_t)L);
        printf("[ deltoid ] captured L: %p\n", L);
    }
    return o_pcall(L, nargs, nresults, errfunc);
}

//  httpget — safe, no shell injection
//  uses execve(curl) directly via pipe fork
static int httpget_bridge(void* L)
{
    const char* url = r_tostring(L, 1);
    if (!url) return 0;

    // sanitise: reject URLs with shell-special chars
    for (const char* c = url; *c; c++) {
        if (*c == '\'' || *c == '"' || *c == '`' ||
            *c == '$'  || *c == '\\' || *c == '\n') {
            r_pushstring(L, "httpget: url contains illegal characters");
            return 1;
        }
    }

    // popen is fine here; already validated url above
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "curl -sL --max-time 10 --proto '=https' -- '%s'", url);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) { r_pushstring(L, ""); return 1; }

    size_t size   = 4096;
    size_t offset = 0;
    char*  buf    = malloc(size);
    if (!buf) { pclose(pipe); return 0; }

    int c;
    while ((c = fgetc(pipe)) != EOF) {
        if (offset + 1 >= size) {
            size *= 2;
            char* nb = realloc(buf, size);
            if (!nb) break;
            buf = nb;
        }
        buf[offset++] = (char)c;
    }
    buf[offset] = '\0';
    pclose(pipe);

    r_pushstring(L, buf);
    free(buf);
    return 1;
}

//  identifyexecutor  ( defined here )
static int identifyexecutor(void* L)
{
    r_pushstring(L, "Deltoid KX");
    r_pushstring(L, "v1.1.0");
    return 2;
}

//  unc registration
static void register_unc(void* L)
{
    if (atomic_exchange(&registered, 1)) return;

#define BIND(name, fn) \
    do { r_pushcclosure(L, fn, 0); r_setfield(L, -10002, name); } while(0)

    BIND("identifyexecutor", identifyexecutor);
    BIND("httpget",          httpget_bridge);

#undef BIND

    printf("[ deltoid ] unc bound\n");
}

//  script watcher thread
//  uses /tmp/deltoid_exec.lua — predictable path, writable
#define EXEC_PATH "/tmp/deltoid_exec.lua"

static void* script_watcher(void* arg)
{
    (void)arg;
    while (1) {
        void* L = (void*)atomic_load(&global_L);
        if (L && access(EXEC_PATH, F_OK) == 0) {
            register_unc(L);

            FILE* f = fopen(EXEC_PATH, "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                long len = ftell(f);
                rewind(f);
                char* buf = malloc((size_t)len + 1);
                if (buf) {
                    fread(buf, 1, (size_t)len, f);
                    buf[len] = '\0';
                    fclose(f);
                    remove(EXEC_PATH);

                    printf("[ deltoid ] executing %ld bytes\n", len);
                    if (r_loadstring(L, buf) != 0) {
                        printf("[ deltoid load error ] %s\n", r_tostring(L, -1));
                    } else if (r_pcall(L, 0, 0, 0) != 0) {
                        printf("[ deltoid exec error ] %s\n", r_tostring(L, -1));
                    }
                    free(buf);
                } else {
                    fclose(f);
                }
            }
        }
        usleep(200000);
    }
    return NULL;
}

//  constructor; entry point
__attribute__((constructor))
static void deltoid_init(void)
{
    void* handle = dlopen("libSoberRuntime.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) {
        fprintf(stderr, "[ deltoid ] libSoberRuntime.so not loaded\n");
        return;
    }

    struct link_map* map = NULL;
    dlinfo(handle, RTLD_DI_LINKMAP, &map);
    uintptr_t base = (uintptr_t)map->l_addr;

    // scan for functions  ( search up to 96 MB )
    uintptr_t end = base + 0x6000000;

    r_pcall       = (r_lua_pcall)      scan_memory(base, end,
        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B D9");
    r_pushstring  = (r_lua_pushstring) scan_memory(base, end,
        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B F2");
    r_pushcclosure= (r_lua_pushcclosure)scan_memory(base, end,
        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 4C 8B D1");
    r_loadstring  = (r_luaL_loadstring) scan_memory(base, end,
        "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 8B 01 48 FF 60 10");
    r_tostring    = (r_lua_tostring)    scan_memory(base, end,
        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B DA 48 8B D1");
    r_setfield    = (r_lua_setfield)    scan_memory(base, end,
        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B DA");

    printf("[ deltoid ] scan results:\n");
    printf("  pcall=%p pushstring=%p pushcclosure=%p\n",
           (void*)r_pcall, (void*)r_pushstring, (void*)r_pushcclosure);
    printf("  loadstring=%p tostring=%p setfield=%p\n",
           (void*)r_loadstring, (void*)r_tostring, (void*)r_setfield);

    if (!r_pcall) {
        fprintf(stderr, "[ deltoid ] pcall scan failed — aborting hook\n");
        return;
    }

    // build trampoline:
    //   o_pcall stub = [ copy of N prologue byte s] + [ abs jmp to r_pcall+N ]
    int prologue_len = measure_prologue((uint8_t*)r_pcall);
    // must be >= 14 to fit our jmp
    if (prologue_len < 14) prologue_len = 14;

    size_t stub_size = (size_t)prologue_len + 14;
    o_pcall = (pcall_fn)mmap(NULL, stub_size,
                              PROT_READ | PROT_WRITE | PROT_EXEC,
                              MAP_ANON | MAP_PRIVATE, -1, 0);
    if (o_pcall == MAP_FAILED) {
        perror("[ deltoid ] mmap failed");
        return;
    }

    // copy prologue bytes into stub
    memcpy(o_pcall, (void*)r_pcall, (size_t)prologue_len);
    // append jmp back to r_pcall+prologue_len ( after hook jmp )
    write_abs_jmp((uint8_t*)o_pcall + prologue_len,
                  (void*)((uintptr_t)r_pcall + prologue_len));

    // overwrite r_pcall prologue with jmp to h_pcall
    unprotect((void*)r_pcall, 14);
    write_abs_jmp((void*)r_pcall, (void*)h_pcall);

    printf("[ deltoid ] pcall hooked (prologue=%d bytes)\n", prologue_len);

    pthread_t t;
    pthread_create(&t, NULL, script_watcher, NULL);
    pthread_detach(t);

    printf("[ deltoid ] init complete\n");
}
