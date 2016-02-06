#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_closure.lua

local function createbox_()
    local a = nil
    return function() return a end, function(v) a = v end
end
local createbox = lll.compile(createbox_)
local get, set = createbox()

local function testclosure(v)
    set(v)
    assert(get() == v)
end

testclosure(nil)
testclosure(3.14)
testclosure(nil)
testclosure('abc')
testclosure(nil)

local function foo() end
testclosure(foo)

