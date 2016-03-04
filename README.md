# Lua Low Level

## Brief
This is an alternative Lua (5.3.2) implementation that aims to archive better 
performance by generating native code with the help of LLVM toolchain.

For further information about Lua, see https://www.lua.org.

<b>ATENTION: This is still on development state.</b>

## Usage
The usage should be equal to the standard Lua interpreter.
The compilation is done automatically by default, but you can also pre-compile
a function by calling ```lll.compile(f)```. You can also disable the
auto compilation or change the number of calls required to auto compile a
function.

## TODO List
- Support coroutines;
- Archive better performance by proper implementing the SSA format;
- Support newer LLVM versions;
- Support gcc/g++ compilation;
- Rename the genarated binary to ```lll```.

## Requiriments
- LLVM (version 3.5)
- Clang (same version of llvm)

## Compilation
Run ```make``` at project root folder.
The compilation/installation is equal to Lua.

## Library
A library is provided to manually control the LLL compiler behavior.

```
lll.compile(f)
  Compiles $f and returns true if it succeeds. If not, returns false and the
  error message.

lll.setAutoCompileEnable(b)
  Enables or disables the auto compilation. $b will be converted to a boolean.
  (default = enable)

lll.isAutoCompileEnable()
  Returns whether the auto compilation is enable.

lll.setCallsToCompile(calls)
  Sets the number of $calls required to auto compile a function. (default = 2)

lll.getCallsToCompile()
  Obtains the number of calls required to auto-compile a function.

lll.isCompiled(f)
  Returns whether $f is compiled.

lll.debug(f)
  Writes in stderr the generated LLVM IR of $f. (DEBUG)

lll.write(f, path)
  Writes to $path (default = 'f') the generated IR and the respective assembly
  (.s) file. The asm file is created by llc command. (DEBUG)

```

