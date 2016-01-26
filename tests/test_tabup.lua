#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_tabup.lua

local function _get() return a end
local function _set(val) a = val end
local get = lll.compile(_get)
local set = lll.compile(_set)

assert(get() == nil)

a = 10
assert(get() == 10)

set('abc')
assert(get() == 'abc' and a == 'abc')

set(nil)
assert(get() == nil and a == nil)

