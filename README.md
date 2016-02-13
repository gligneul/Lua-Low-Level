# Lua Low Level

## Brief
This is an alternative Lua (5.3.2) implementation that aims to archive better performace by generating native code with the help of LLVM toolchain.

For further information about Lua, see https://www.lua.org.

<b>ATENTION: This is still on development state.</b>

## Requiriments
- LLVM (version 3.5 or latter)
- Clang (recommended)

## Compilation
Run ```make``` at project root folder.

## Library
A library is provided to manually control the LLL compiler behavior.

<b>```lll.compile(f)```</b>  
Compiles ```f``` and returns true if it succeeds. If not, returns false and the error message.

<b>```lll.setautocompile(b)```</b>  
Enables or disables the auto compilation.

<b>```lll.getautocompile()```</b>  
Returns whether the auto compilation is enable.

<b>```lll.iscompiled(f)```</b>  
Returns whether ```f``` is compiled.

<b>```lll.dump(f)```</b>  
Dumps LLVM intermediate representation of ```f``` (DEBUG).

