--[[
    [ Deltoid ] KX init.lua
--]]

local getgenv = getgenv or function() return _G end
local env = getgenv()

local HttpService = game:GetService("HttpService")
local CoreGui = game:GetService("CoreGui")

local function protect(fn)
    return (newcclosure or function(f) return f end)(fn)
end

-- core unc api
local api = {
    identifyexecutor = function() return "Deltoid KX", "v1.0.0" end,
    getgenv = getgenv,
    getrenv = getrenv or function() return game end,
    getreg  = getreg  or function() return debug.getregistry() end,
    getgc   = getgc   or function() return debug.getgc() end,
    getsenv = getsenv or function(_) return nil end,
    getfenv = getfenv or function(_) return nil end,
    setfenv = setfenv or function(_, _) end,
    getcallingscript = getcallingscript or function() return nil end,
    
    getrawmetatable = getrawmetatable or function(t) return debug.getmetatable(t) end,
    setreadonly = setreadonly or function(t, v) end, -- built-in via c
    
    hookfunction   = hookfunction   or function(_, new) return new end,
    newcclosure    = newcclosure    or function(fn) return fn end,
    replaceclosure = replaceclosure or function(_, new) return new end,
    checkcaller    = checkcaller    or function() return false end,
}

-- req bridge
env.request = request or http_request or function(opts)
    if type(opts) ~= "table" or not opts.Url then
        return { StatusCode = 400, Body = "invalid reqs" }
    end
    local ok, result = pcall(function()
        if opts.Method == "POST" then
            return HttpService:PostAsync(opts.Url, opts.Body or "")
        else
            return HttpService:GetAsync(opts.Url)
        end
    end)
    return { StatusCode = ok and 200 or 500, Body = ok and result or "req failed" }
end

-- map everything to global env
for name, func in pairs(api) do
    if not env[name] then
        env[name] = func
    end
end

print("[ Deltoid ] functions mapped to global env.")
