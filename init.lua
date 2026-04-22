--[[
    [ Deltoid KX ] init.lua
--]]

local getgenv = getgenv or function() return _G end
local env = getgenv()

local HttpService = game:GetService("HttpService")

-- core api
local api = {
    identifyexecutor = function() return "Deltoid KX", "v1.0.0" end,
    getgenv = getgenv,
    getrenv = function() return game end,
    getreg  = function() return debug.getregistry() end,
    getgc   = function() return debug.getgc() end,
    getrawmetatable = function(t) return debug.getmetatable(t) end,
    setreadonly = function(t, v) end, -- bridged via C
}

-- request handler
env.request = function(opts)
    local ok, res = pcall(function()
        if opts.Method == "POST" then
            return HttpService:PostAsync(opts.Url, opts.Body or "")
        end
        return HttpService:GetAsync(opts.Url)
    end)
    return { StatusCode = ok and 200 or 500, Body = res }
end

-- finalize env
for name, func in pairs(api) do
    if not env[name] then env[name] = func end
end

print("[ Deltoid KX ] env ready.")
