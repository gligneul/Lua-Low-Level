-- LLL - Lua Low-Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- test_basic.lua

local lll_test = require 'tests/lll_test' 

local functions = {
function ()
end,

function (a)
end,

function (a, b)
    return 10
end,

function (a, b)
    return 10
end,

function (a, b, c)
    return a
end,

function (a, b, c)
    return b
end,

function (a, b, c)
    return c
end,
}

local args = {
    {},
    {10},
    {12.3},
    {3, 5.7, 0},
    {'abc', function() end},
}

lll_test.test_all(functions, args)

