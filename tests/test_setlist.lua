#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_setlist.lua

local compare = require 'tests/compare'

function testsetlist(l)
    local correctlist = load('return {' .. l .. '}')()
    local jitlist = load('return \
            lll.compile(function() return {' .. l .. '} end)()')()
    assert(compare(correctlist, jitlist))
end

function f() return 'b', 2 end
testsetlist('f()')
testsetlist('"a", 1, 0.8')
testsetlist('["a"] = 3, [-1] = "a", [0] = "b", [2] = "c"')

