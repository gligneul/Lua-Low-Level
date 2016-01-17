#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- test_lll.lua

local function _add(a, b)
    return a + b
end

local add = jit.compile(_add)

for i = 1, 10e6 do
    add(i, i)
end

