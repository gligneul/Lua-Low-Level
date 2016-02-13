#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- test_api.lua

-- Auto compilation (ac) is enable by default
assert(lll.getautocompile() == true)

-- Tests ac
local function f() return 123 end
assert(lll.iscompiled(f) == false)
for i = 1, 51 do f() end
assert(lll.iscompiled(f) == true)

-- Disables ac
lll.setautocompile(false)
assert(lll.getautocompile() == false)

-- Checks if ac is disabled
local function g() return 'abc' end
assert(lll.iscompiled(g) == false)
for i = 1, 51 do g() end
assert(lll.iscompiled(g) == false)

-- Tests manual compilation
local ok, err = lll.compile(f)
assert(ok == false and err == 'Function already compiled')
assert(lll.compile(g))
ok, err = lll.compile(g)
assert(ok == false and err == 'Function already compiled')

-- Execute compiled functions
assert(f() == 123 and g() == 'abc')

