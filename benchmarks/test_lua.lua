#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- test_lua.lua

local function hypot(a, b)
    return math.sqrt(a * a + b * b)
end

for i = 1, 10000 do
    for j = 1, 10000 do
        hypot(i, j)
    end
end

