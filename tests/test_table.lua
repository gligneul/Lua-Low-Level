#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- test_table.lua

local create = jit.compile(function() return {} end)
local get = jit.compile(function(t, k) return t[k] end)
local set = jit.compile(function(t, k, v) t[k] = v end)

local t = create();
assert(type(t) == 'table' and next(t) == nil)

set(t, 'a', 123)
assert(t.a == 123 and get(t, 'a') == 123)

set(t, 123, 'test')
assert(t[123] == 'test' and get(t, 123) == 'test')

set(t, 'a', nil)
assert(t.a == nil and get(t, 'a') == nil)

local setk = jit.compile(function(t) t.k = 357 end)
local getk = jit.compile(function(t) return t.k end)
setk(t)
assert(t.k == 357 and getk(t) == 357)

t.k = nil
assert(getk(t) == nil)

