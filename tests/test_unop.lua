-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- test_arith.lua

local lll_test = require 'tests/lll_test' 

-- Create all possible combinations of operations and values
local ops = {'-', '~', '#', 'not '}
local values = {'nil', 'true', 'false', '-5000', '0', '123', '0.00001', '1.23',
        '10e999', '"0"', '"15.4"', '"a"'}

local functions = {}
for _, op in ipairs(ops) do
    for _, v in ipairs(values) do
        f = 'return function() local a = ' .. v  ..
            ' return ' .. op .. 'a end'
        table.insert(functions, f)
    end
end

lll_test(functions, {{}})

