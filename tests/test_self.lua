#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_self.lua

local function get_(o) return o:getN() end
local get = lll.compile(get_)

local function set_(o, n) o:setN(n) end
local set = lll.compile(set_);

local obj = {
    n = 10,
    getN = function(o) return o.n end,
    setN = function(o, n) o.n = n end
}

assert(obj.n == 10 and get(obj) == 10)

set(obj, 'abc')
assert(obj.n == 'abc' and get(obj) == 'abc')

set(obj, nil)
assert(obj.n == nil and get(obj) == nil)

set(obj, 'abc')
assert(obj.n == 'abc' and get(obj) == 'abc')

