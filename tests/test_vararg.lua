#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_vararg.lua

local executetests = require 'tests/executetests' 
local generateargs = require 'tests/generateargs'

local functions = {
'function(...) return {...} end',
'function(...) local a, b = ...; return {a, b} end',
'function(a, b, ...) return {...} end'
}

for i = 0, 4 do
    executetests(functions, generateargs(i, {'nil', '"a"'}))
end

