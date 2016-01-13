-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- test_optest.lua

local executetests = require 'tests/executetests' 
local generateargs = require 'tests/generateargs'

do
    local prefix = 'return function(a) if '
    local suffix = ' then return true else return false end end'
    local f1 = prefix .. 'a' .. suffix
    local f2 = prefix .. 'not a' .. suffix
    executetests({f1, f2}, generateargs(1))
end

do
    local prefix = 'return function(a, b) return '
    local suffix = ' end'
    local f1 = prefix .. 'a or b' .. suffix
    local f2 = prefix .. 'a and b' .. suffix
    executetests({f1, f2}, generateargs(2))
end

