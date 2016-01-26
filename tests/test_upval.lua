#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_table.lua

local a
local get = lll.compile(function() return a end)
local set = lll.compile(function(v) a = v end)

assert(a == nil and get() == nil)

a = 10
assert(a == 10 and get() == 10)

set('abc')
assert(a == 'abc' and get() == 'abc')

set(nil)
assert(a == nil and get() == nil)

