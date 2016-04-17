#!/bin/bash
# LLL - Lua Low Level
# September, 2015
# Author: Gabriel de Quadros Ligneul
# Copyright Notice for LLL: see lllvmjit.h
#
# runbenchmarks.lua
# Execute each benchmark modules with and prints the Lua Interpreter and 
# the LLL time

n_tests=20

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

function statistics {
    sum=0
    i=1
    xs=()
    while read -e x; do
        sum=`echo "$sum + $x" | bc -l`
        xs[$i]=$x
        i=$(($i + 1))
    done

    avg=`echo "$sum / $n_tests" | bc -l`
    dev=0
    for x in "${xs[@]}"; do
        dev=`echo "$dev + ( $x - $avg ) ^ 2" | bc -l`
    done

    stddev=`echo "sqrt( $dev / ( $n_tests - 1 ) )" | bc -l`
    ratio=`echo "100 * $stddev / $avg" | bc -l`
    printf "%.5f\t%.5f\t(%.5f%%)" "$avg" "$stddev" "$ratio"
}

function measure {
    echo $( { TIMEFORMAT='%3R'; time $* > /dev/null; } 2>&1 )
}

function benchmark {
    program=$1
    input=$2
    flag=$3
    compflag=$4
    for i in `seq 1 $n_tests`; do
        runtime=`measure $program $input $flag`
        if [ "$compflag" != "" ]; then
            comptime=`measure $program $input $compflag`
            echo "$runtime - $comptime" | bc -l
        else
            echo "$runtime"
        fi
    done | statistics
}

function printratio {
    printf "$1 / $3:\t%02.8f\n" `echo "$2 / $4" | bc -l`
}

function first {
    echo "$1"
}

echo "Distro: "`cat /etc/*-release | head -1`
echo "Kernel: "`uname -r`
echo "CPU:    "`cat /proc/cpuinfo | grep 'model name' | tail -1 | \
                sed 's/model name.*:.//'`
for m in "${modules[@]}"; do
    path="$modules_prefix""$m""$modules_suffix"

    luatime=`benchmark ./src/lua $path`
    llltime=`benchmark ./src/lua $path --lll --lll-compile-only`
    luajittime=`benchmark luajit $path`
    lllcompiletime=`benchmark ./src/lua $path --lll-compile-only`

    echo "
Module: $path
Benchmarks:
             avg        stddev
Lua:         $luatime
LLL:         $llltime
LuaJit:      $luajittime
Compile:     $lllcompiletime

Ratios:"

    tlua=`first $luatime`
    tlll=`first $llltime`
    tcomp=`first $lllcompiletime`
    printratio 'Lua' $tlua 'LLL' $tlll
    printratio 'LLL' $tlll 'Lua' $tlua
    printratio 'comp' $tcomp 'LLL' $tlll

    echo "
--------------------"
done

