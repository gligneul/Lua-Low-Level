#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_binop.lua

local executetests = require 'tests/executetests' 
local generateargs = require 'tests/generateargs'

local ops = {'+', '-', '*', '/', '&', '|', '~', '<<', '>>', '%', '//', '^',
         '..'}
local values = generateargs(2)

local fs = {}
for _, op in ipairs(ops) do
    for _, v in ipairs(values) do
        table.insert(fs, 'function() local a, b = ' .. v[1] .. ', ' .. v[2] ..
                ' return a ' .. op .. ' b end')
        table.insert(fs, 'function() local a = ' .. v[1] ..
                ' return a ' .. op .. ' ' .. v[2] .. ' end')
        table.insert(fs, 'function() local b = ' .. v[2] .. 
                ' return ' .. v[1] .. ' ' .. op .. ' b end')
        table.insert(fs, 'function()' ..
                ' return ' .. v[1] .. ' ' .. op .. ' ' .. v[2] .. ' end')
    end
end

executetests(fs, {{}})

