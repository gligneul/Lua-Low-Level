#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_api.lua

-- Auto compilation (ac) is enable by default
assert(lll.isAutoCompileEnable() == true)

-- Tests ac
local function f() return 123 end
assert(lll.isCompiled(f) == false)
assert(lll.getCallsToCompile() == 50)
for i = 1, lll.getCallsToCompile() do f() end
assert(lll.isCompiled(f) == true)

-- Disables ac
lll.setAutoCompileEnable(false)
assert(lll.isAutoCompileEnable() == false)

-- Checks if ac is disabled
local function g() return 'abc' end
assert(lll.isCompiled(g) == false)
for i = 1, lll.getCallsToCompile() do g() end
assert(lll.isCompiled(g) == false)

-- Tests manual compilation
local ok, err = lll.compile(f)
assert(ok == false and err == 'Function already compiled')
assert(lll.compile(g))
ok, err = lll.compile(g)
assert(ok == false and err == 'Function already compiled')

-- Change calls to compile
lll.setAutoCompileEnable(true)
local function h() return 'qwe' end
assert(lll.isCompiled(h) == false)
lll.setCallsToCompile(10)
for i = 1, 2 do h() end
assert(lll.isCompiled(h) == false)
for i = 3, 10 do h() end
assert(lll.isCompiled(h) == true)

-- Execute compiled functions
assert(f() == 123 and g() == 'abc' and h() == 'qwe')

