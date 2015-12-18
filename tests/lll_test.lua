-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- lll_test.lua
-- Barebone for creating tests for LLL

--- Description
---   Compile the functions and run both Lua and LLL versions of it.
---   Then compare if the results are the same. If not, enter debug mode.
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
        local f_lll = jit.compile(f_lua)

        -- Execute the functions
        for _, a in ipairs(arguments) do
            local r_lua = f_lua(table.unpack(a))
            local r_lll = f_lll(table.unpack(a))
            if lua_result ~= lll_result then
                print('unmatched result: ', f_str)
                debug.debug()
                error('ending tests')
            end
        end
    end
end

