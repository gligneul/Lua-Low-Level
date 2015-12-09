-- LLL - Lua Low-Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- lll_test.lua
-- Barebone for creating tests for LLL

local lll_test = {}

--- Description
---   Calls compile_and_test for each function passed.
---
--- Parameters
---   fs : List of Functions
---     List of lua functions that will be tested.
---
---   args : List of Lists
---     List of arguments that will be passed.
function lll_test.test_all (fs, args)
    for _, f in ipairs(fs) do
        lll_test.compile_and_test(f, args)
    end
end

--- Description
---   Compiles the lua function and test it for the passed arguments.
---
--- Parameters
---   f : Function (Lua)
---     Function that will be compiled and tested.
---
---   args : List of lists
---     List of lists that will be unpacked add passed to the function
function lll_test.compile_and_test (f, args)
    local lllf = jit.compile(f)
    for _, a in ipairs(args) do
        lll_test.test(f, lllf, table.unpack(a))
    end
end

--- Description
---   Executes both functions and compares the results.
---
--- Parameters
---   lua_function : Function (Lua)
---   lll_function : Function (LLL)
---     Functions that will be compared.
---
---   ... : Vararg
---     Arguments that will be passed to the functions.
function lll_test.test (lua_function, lll_function, ...)
    local lua_result = lua_function(...)
    local lll_result = lll_function(...)
    if lua_result ~= lll_result then
        print("Error in function: ", lua_function)
        debug.debug()
    end
end

return lll_test
