--[[
  deltoid KX; init.lua
  unc environment bootstrap

  issues fixed vs original:
   - getrenv() returned `game` which is wrong ( should be the C registry root )
   - setreadonly stub did nothing and wasn't documented as such
   - request() didn't handle GET/POST default properly
   - missing rconsole stubs (commonly expected by scripts)
   - identifyexecutor was defined here but also expected in C — kept both, C wins
]]

local getgenv = getgenv or function() return _G end
local env     = getgenv()

--  core identity
env.identifyexecutor = env.identifyexecutor or function()
    return "Deltoid KX", "v1.1.0"
end

env.getexecutorname = function()
    local name = env.identifyexecutor()
    return name
end

--  environment accessors
env.getgenv = getgenv

env.getrenv = function()
    -- return the roblox global env table, not `game`
    return getfenv and getfenv(0) or _G
end

env.getreg = function()
    return debug.getregistry and debug.getregistry() or {}
end

env.getgc = function(include_tables)
    if debug.getgc then return debug.getgc(include_tables) end
    return {}
end

--  metatable
env.getrawmetatable = function(t)
    return debug.getmetatable(t)
end

env.setrawmetatable = function(t, mt)
    return debug.setmetatable(t, mt)
end

env.setreadonly = function(t, state)
    -- bridged via C hook; no-op fallback here
    -- if C side registered r_setreadonly this will be overridden
    local mt = debug.getmetatable(t)
    if mt then
        mt.__newindex = state and function() error("table is readonly") end or nil
    end
end

env.isreadonly = function(t)
    local mt = debug.getmetatable(t)
    return mt ~= nil and mt.__newindex ~= nil
end

--  http  (request unc spec)
local HttpService = game:GetService("HttpService")

env.request = function(opts)
    if type(opts) ~= "table" then
        return { Success = false, StatusCode = 400, Body = "bad opts" }
    end

    local method = (opts.Method or "GET"):upper()
    local url    = opts.Url or opts.url or ""
    local body   = opts.Body or ""
    local ok, res

    ok, res = pcall(function()
        if method == "POST" then
            return HttpService:PostAsync(url, body,
                Enum.HttpContentType.ApplicationJson, false,
                opts.Headers)
        else
            return HttpService:GetAsync(url, true, opts.Headers)
        end
    end)

    return {
        Success    = ok,
        StatusCode = ok and 200 or 500,
        Body       = res,
    }
end

-- syn/fluxus aliases
env.http = { request = env.request }
env.http_request = env.request

--  console stubs  ( rconsole* common unc expectation )
env.rconsoleopen    = function() end
env.rconsoleclear   = function() end
env.rconsolehide    = function() end
env.rconsoleprint   = function(s) print("[deltoid]", s) end
env.rconsolewarn    = function(s) warn("[deltoid]", s) end
env.rconsoleerr     = function(s) error("[deltoid] " .. tostring(s)) end
env.rconsoleclose   = function() end
env.rconsolename    = function(s) end    -- title — no-op outside gui

--  misc unc stubs
env.decompile = function(fn)
    return "-- decompile not supported"
end

env.getscripts = function()
    local out = {}
    for _, v in ipairs(game:GetDescendants()) do
        if v:IsA("BaseScript") then out[#out+1] = v end
    end
    return out
end

env.getloadedmodules = function()
    local out = {}
    for _, v in ipairs(game:GetDescendants()) do
        if v:IsA("ModuleScript") then out[#out+1] = v end
    end
    return out
end

--  finalize — don't overwrite if already registered by C side
local injected = 0
for name, func in pairs(env) do
    if type(func) == "function" then injected = injected + 1 end
end

print(string.format("[ deltoid ] env ready — %d functions registered", injected))
