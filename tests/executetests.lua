-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- executetests.lua
-- Barebone for creating tests for LLL

local compare = require 'tests/compare'

--- Verifies if results are the same
local function verifyresult(oklua, retlua, oklll, retlll)
    if oklua ~= oklll then
        return false
    elseif oklua then
        return compare(retlua, retlll)
    else
        return true
        -- return string.match(retlua, retlll) == retlll
    end
end

--- Creates the error message
local function createerrmsg(f, args, retlua, retlll)
    local msg = 
        'test failed\n' ..
        'function: ' ..  f .. '\n' ..
        'args:    '
    for _, arg in ipairs(args) do
        msg = msg .. ' ' .. tostring(arg)
    end
    msg = msg .. '\n' ..
        'expected: ' .. tostring(retlua) .. '\n' ..
        'result:   ' .. tostring(retlll) .. '\n'
    return msg
end

--- Convert a list of string to a list of values
local function tovalues(l)
    local lstr = '{'
    for _, v in ipairs(l) do
        lstr = lstr .. v .. ','
    end
    lstr = lstr .. '}'
    return load('return ' .. lstr)()
end

--- Description
---   Compile the functions and run both Lua and LLL versions of it.
---   Then compare if the results are the same.
---
--- Parameters
---   functions
---     List of strings that will be compiled into Lua and LLL functions
---     
---   args
---     List of arguments that will be passed to the functions
return function(functions, arguments)
    lll.setautocompile(false)
    local nf = 0
    local nc = 0
    for _, fstr in ipairs(functions) do
        local chunk, err = load('return ' .. fstr)
        if not chunk then
            error('can\'t compile chunk: ' .. fstr .. '\nerror: ' .. err)
        end
        local flua = chunk()
        local flll = chunk()
        lll.compile(flll)
        for _, astr in ipairs(arguments) do
            local a = tovalues(astr)
            local oklua, retlua = pcall(flua, table.unpack(a))
            local oklll, retlll = pcall(flll, table.unpack(a))
            if not verifyresult(oklua, retlua, oklll, retlll) then
                error(createerrmsg(fstr, a, retlua, retlll))
            end
            nc = nc + 1
        end
        nf = nf + 1
    end
    print("\tfunctions: " .. nf .. " --- calls: " .. nc)
end

