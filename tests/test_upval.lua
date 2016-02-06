#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_upval.lua

local a
local get = lll.compile(function() return a end)
local set = lll.compile(function(v) a = v end)

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
