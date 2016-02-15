#!src/lua
-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- runbenchmarks.lua
-- Execute each benchmark modules with and prints the Lua Interpreter and 
-- the LLL time

-- Configuration constants
local MODULES_PREFIX = 'benchmarks/'
local MODULES_SUFFIX = '.lua'
local NUMBER_OF_RUNS = 100

-- Declaraion of benchmarks modules
local modules = {
    'mandelbrot',
    'matmul',
}

-- Returns the average execution time of $f
local function benchmark(f)
    for i = 1, NUMBER_OF_RUNS / 2 do
        f()
    end
    local begin = os.clock()
    for i = 1, NUMBER_OF_RUNS do
        f()
    end
    return (os.clock() - begin) / NUMBER_OF_RUNS
end

-- Benchmark each module
lll.setautocompile(false)
for _, m in ipairs(modules) do
    local path = MODULES_PREFIX .. m .. MODULES_SUFFIX
    local f = dofile(path)
    local luatime = benchmark(f)
    assert(lll.compile(f))
    local llltime = benchmark(f)
    print('File: ' .. path)
    print('Lua time: ' .. luatime .. 's')
    print('LLL time: ' .. llltime .. 's')
    print()
end

