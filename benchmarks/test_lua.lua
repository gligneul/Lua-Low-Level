#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- test_lua.lua

local function add(a, b)
    return a + b
end

for i = 1, 10e5 do
    add(i, i)
end

