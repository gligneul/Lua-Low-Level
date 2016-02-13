#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_setlist.lua

local compare = require 'tests/compare'

function testsetlist(l)
    local f = load('return {' .. l .. '}')
    local correctlist = f()
    assert(lll.compile(f))
    local jitlist = f()
    assert(compare(correctlist, jitlist))
end

function f() return 'b', 2 end
testsetlist('f()')
testsetlist('"a", 1, 0.8')
testsetlist('["a"] = 3, [-1] = "a", [0] = "b", [2] = "c"')

