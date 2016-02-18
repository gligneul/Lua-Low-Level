#!/bin/bash
# LLL - Lua Low Level
# September, 2015
# Author: Gabriel de Quadros Ligneul
# Copyright Notice for LLL: see lllvmjit.h
#
# runbenchmarks.lua
# Execute each benchmark modules with and prints the Lua Interpreter and 
# the LLL time

n_tests=25

modules_prefix='benchmarks/'
modules=(
    'ack.lua 8 2'
    'fixpoint_fact.lua 90'
    'heapsort.lua'
    'loopsum.lua'
    'mandelbrot.lua'
    'matmul.lua'
    'sudoku.lua'
)

function benchmark {
    sum=0
    for i in `seq 1 $n_tests`; do
        t=$( { TIMEFORMAT='%3R'; time $*; } 2>&1 )
        sum=`echo "$sum + $t" | bc -l`
    done
    echo "$sum / $n_tests" | bc -l
}

for m in "${modules[@]}"; do
    path="$modules_prefix""$m""$modules_suffix"
    luatime=`benchmark ./src/lua $path`
    llltime=`benchmark ./src/lua $path --lll-compile`
    echo "Module: $path"
    echo "Lua time: $luatime"
    echo "LLL time: $llltime"
    echo ""
done

