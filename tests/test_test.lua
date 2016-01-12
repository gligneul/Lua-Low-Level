-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- test_arith.lua

local lll_test = require 'tests/lll_test' 

local params = {{'nil'}, {'true'}, {'false'}, {'-5000'}, {'0'}, {'123'},
        {'0.00001'}, {'1.23'}, {'10e999'}, {'"0"'}, {'"15.4"'}, {'"a"'}}

local f = 'return function(a) if a then return true else return false end end'

lll_test({f}, params)

