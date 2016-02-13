#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_for.lua

local executetests = require 'tests/executetests' 
local generateargs = require 'tests/generateargs'

local values = {'nil', 'true', 'false', '-4', '-0.1', '3', '5.5', '"20"',
        '"-3.5"', '"a"', 'function() end'}

local f = [[
function(a, b, c)
    local sum = 0
    for i = a, b, c do
        sum = sum + i
    end
    return sum
end
]]

executetests({f}, generateargs(3, values))

