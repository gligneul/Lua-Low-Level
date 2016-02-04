#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_setlist.lua

local compare = require 'tests/compare'

local a
assert(a == nil)

lll.compile(function() a = {'a', 1, 0.8} end)()
assert(compare(a, {'a', 1, 0.8}))

local function f() return 'b', 2 end
lll.compile(function() a = {f()} end)()
assert(compare(a, {'b', 2}))


