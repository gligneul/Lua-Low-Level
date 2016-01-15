#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- test_lll.lua

local function _hypot(a, b)
    return math.sqrt(a * a + b * b)
end

local hypot = jit.compile(_hypot)

for i = 1, 1000 do
    for j = 1, 100 do
        hypot(i, j)
    end
end

