#!/bin/bash
# LLL - Lua Low Level
# September, 2015
# Author: Gabriel de Quadros Ligneul
# Copyright Notice for LLL: see lllvmjit.h
#
# runbenchmarks.lua
# Execute each benchmark modules with and prints the Lua Interpreter and 
# the LLL time

n_tests=10

modules_prefix='benchmarks/'
modules=(
    'floatarith.lua'
    'heapsort.lua'
    'increment.lua'
    'loopsum.lua'
    'mandelbrot.lua'
    'matmul.lua'
    'qt.lua'
    'queen.lua'
    'sieve.lua'
    'sudoku.lua'
)

function benchmark {
    sum=0
    ts=()
    for i in `seq 1 $n_tests`; do
        t=$( { TIMEFORMAT='%3R'; time $* > /dev/null; } 2>&1 )
        ts[$i]=$t
        sum=`echo "$sum + $t" | bc -l`
    done
    avg=`echo "$sum / $n_tests" | bc -l`
    s=0
    for i in `seq 1 $n_tests`; do
        s=`echo "$s + ( ${ts[$i]} - $avg ) ^ 2" | bc -l`
    done
    s=`echo "sqrt( $s / ( $n_tests - 1 ) )" | bc -l`
    sp=`echo "100 * $s / $avg" | bc -l`
    printf "%.5f\t%.5f\t(%.5f%%)" "$avg" "$s" "$sp"
}

echo "Distro: "`cat /etc/*-release | head -1`
echo "Kernel: "`uname -r`
echo "CPU:    "`cat /proc/cpuinfo | grep 'model name' | tail -1 | \
                sed 's/model name.*:.//'`
echo ""

for m in "${modules[@]}"; do
    path="$modules_prefix""$m""$modules_suffix"
    luatime=`benchmark ./src/lua $path`
    llltime=`benchmark ./src/lua $path --lll`
    echo "Module: $path"
    echo "     avg        stddev"
    echo "Lua: $luatime"
    echo "LLL: $llltime"

    luatime=`echo $luatime | sed 's/ .*//'`
    llltime=`echo $llltime | sed 's/ .*//'`
    printf "     %.3f time\n\n" `echo "$llltime / $luatime" | bc -l`
done

