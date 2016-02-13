#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_table.lua

local create = function() return {} end
assert(lll.compile(create))
local get = function(t, k) return t[k] end
assert(lll.compile(get))
local set = function(t, k, v) t[k] = v end
assert(lll.compile(set))

local t = create();
assert(type(t) == 'table' and next(t) == nil)

set(t, 'a', 123)
assert(t.a == 123 and get(t, 'a') == 123)

set(t, 123, 'test')
assert(t[123] == 'test' and get(t, 123) == 'test')

set(t, 'a', nil)
assert(t.a == nil and get(t, 'a') == nil)

local setk = function(t) t.k = 357 end
assert(lll.compile(setk))
local getk = function(t) return t.k end
assert(lll.compile(getk))

setk(t)
assert(t.k == 357 and getk(t) == 357)

t.k = nil
assert(getk(t) == nil)

