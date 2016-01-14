#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- run_tests.lua
-- Executes each test module

-- Configuration constants
local TEST_PREFIX = 'tests/test_'
local TEST_SUFFIX = '.lua'

-- Declaraion of test modules
local modules = {
    'basic',
    'binop',
    'optest',
    'tabup',
    'unop',
}

-- Execute the tests
for _, m in pairs(modules) do
    local file = TEST_PREFIX .. m .. TEST_SUFFIX
    print('testing: ' .. file .. '... ')
    dofile(file)
    print('        passed!')
end

