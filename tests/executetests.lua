-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- executetests.lua
-- Barebone for creating tests for LLL

local compare = require 'tests/compare'

--- Verifies if results are the same
local function verity_result(ok_lua, r_lua, ok_lll, r_lll)
    if ok_lua or ok_lll then
        return ok_lua == ok_lll and compare(r_lua, r_lll)
    else
        return true
        -- TODO: fix error messages
        -- return string.match(r_lua, r_lll) == r_lll
    end
end

--- Creates the error message
local function error_msg(f, args, ok_lua, r_lua, ok_lll, r_lll)
    local msg = 
        'test failed\n' ..
        'function: ' ..  f .. '\n' ..
        'args:    '
    for _, arg in ipairs(args) do
        msg = msg .. ' ' .. arg
    end
    msg = msg .. '\n' ..
        'expected: ' .. tostring(r_lua) .. '\n' ..
        'result:   ' .. tostring(r_lll) .. '\n'
    return msg
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
    for _, f_str in ipairs(functions) do

        -- Create Lua and LLL functions
        local f_chunk, err = load(f_str)
        if not f_chunk then
            error('can\'t compile chunk: ' .. f_str .. '\nerror: ' .. err)
        end
        local f_lua = f_chunk()
        local f_lll = lll.compile(f_lua)

        -- Execute the functions
        for _, a in ipairs(arguments) do
            local ok_lua, r_lua = pcall(f_lua, table.unpack(a))
            local ok_lll, r_lll = pcall(f_lll, table.unpack(a))
            if not verity_result(ok_lua, r_lua, ok_lll, r_lll) then
                error(error_msg(f_str, a, ok_lua, r_lua, ok_lll, r_lll))
            end
        end
    end
end

