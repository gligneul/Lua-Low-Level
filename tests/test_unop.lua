-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- test_unop.lua

local executetests = require 'tests/executetests' 
local generateargs = require 'tests/generateargs'

local ops = {'-', '~', '#', 'not '}

local functions = {}
for _, op in ipairs(ops) do
    f = 'return function(a) return ' .. op .. 'a end'
    table.insert(functions, f)
end

executetests(functions, generateargs(1))

