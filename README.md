# deltoid

> external ui + internal executor for sober ( roblox )  
> built for chromeos ( linux env )  
> injects lua via `.so` runtime hooking

---

## overview

deltoid is a hybrid executor consisting of:

- **external ui** > sends scripts
- **internal module ( .so )** > injected into target runtime
- **lua bridge layer** > executes code inside the engine

the internal component:
- hooks lua `pcall` to capture `lua_State*`
- builds a trampoline with instruction-aware patching
- registers custom functions into the lua environment
- executes scripts from an external source

---

## features

- ✅ internal lua execution (loadstring + pcall)
- ✅ runtime lua state capture via hook
- ✅ instruction-aware trampoline (not fixed-size patching)
- ✅ unc-style bridge functions:
  - `identifyexecutor`
  - `httpget`
- ✅ external > internal execution pipeline
- ✅ bounded http requests + basic sanitization

---

## comparison

### atingle executor ( upstream )

- primarily frontend / concept-stage implementation
- no fully integrated internal execution core
- no confirmed lua state capture mechanism
- no completed exploit or hook layer
- largely ui-driven architecture with placeholder backend references
- design focus is on interface and structure rather than runtime execution

---

### deltoid ( this fork )

- includes internal injection approach within a shared library context
- implements a functional lua execution path ( `loadstring + pcall` )
- contains hook-based state capture via runtime interception
- bridge system exposing native functions into the lua environment
- execution model supports runtime-driven scripting flow
- focuses on backend execution behavior in addition to ui components

---

## architecture

external ui  
⬇️  
writes script > /tmp/deltoid_exec.lua  
⬇️  
internal watcher thread  
⬇️  
lua loadstring + pcall  
⬇️  
executed inside sober runtime  

---

## limitations

- partial instruction decoder ( not fully relocation safe )
- rip-relative instructions not handled in trampoline
- assumes single lua state
- _G binding index may vary across runtimes
- signature scanning depends on binary stability

---

## comparison

compared to similar projects:

- many public executors are ui-only or placeholders
- deltoid includes a real internal execution core
- focuses on runtime integration, not just interface

| area | atingle | deltoid |
|------|--------|--------|
| execution core | none | real |
| lua state handling | none | hooked |
| internal hooking | none | yes |
| completeness | low | high |

---

## educational use

this project is intended for:

- learning about:
  - dynamic linking ( dlopen, dlinfo )
  - memory scanning / pattern matching
  - function hooking / trampolines
  - lua vm interaction
- reverse engineering practice
- runtime modification techniques

not intended for misuse or violating platform rules

---

## status

NOT actively developed, core execution works, stability improvements ongoing

## notice

i work when i want to
