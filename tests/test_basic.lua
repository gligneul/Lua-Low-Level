#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_basic.lua

local executetests = require 'tests/executetests' 
local generateargs = require 'tests/generateargs'

local rets = {'a', 'b', 'c'}
for _, v in ipairs(generateargs(1)) do
    table.insert(rets, v[0])
end

local functions = {}
for _, r in ipairs(rets) do
    local f = 'function(a, b, c) return ' .. r .. ' end'
    table.insert(functions, f)
end

executetests(functions, generateargs(3))

