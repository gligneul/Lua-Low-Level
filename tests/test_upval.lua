#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_upval.lua

local a

local get = function() return a end
assert(lll.compile(get))
local set = function(v) a = v end
assert(lll.compile(set))

local function testupval(v)
    set(v)
    assert(a == v and get() == v)
end

testupval(nil)
testupval(3.14)
testupval(nil)
testupval('abc')
testupval(nil)

local function foo() end
testupval(foo)
