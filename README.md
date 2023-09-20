# LuaAOT 5.4.3

This is a modified version of Lua 5.4.3 that allows Lua functions to be compiled ahead-of-time to more efficient code. It also includes a compiler called `luaot` that converts a Lua module to an equivalent one written in C.

The luaot compiler is a simplistic compiler that compiles Lua bytecode to C. One may see it as a partial evaluation of the reference Lua interpreter. The main optimizations are that Lua jumps are converted into C gotos, and that there is no overhead of interpreting bytecodes at run time. Additionally, the C compiler will be able to optimize some kinds of things, specially constant folding and dead-code elimination for the opcodes that include immediate parameters.

The performance gains depend on the kind of program being run. For numerical benchmarks it can be twice as fast as regular Lua. But overall, it is not as fast as LuaJIT or Pallene.

# Compilation and instalation

This is a modified version of the Lua interpreter so the process is the same you would use for compiling Lua. For full instructions, consult the doc/readme.html.
```bash
make linux -j4
```
# Usage

Our compiler generates a `.c` file with a `luaopen_` function. You can compile that into a `.so` module and then require it from Lua. The compilation is the same as any other extension module, except that you need to pass the path to the LuaAOT headers.
```bash
./src/luaot test.lua -o testcompiled.c
gcc -shared -fPIC -O2 -I./src testcompiled.c -o testcompiled.so
./src/lua -l testcompiled
```
Be careful about the name of the generated file. If it has the same name as the original Lua file, the `require` may prioritize the Lua file instead of the compiled on. We also recommend compiling with optimizations, at least -O2.

## Options
### `-o`
`-o` is for the output file like:
```bash
./src/luaot test.lua -o testcompiled.c # Compile test.lua to testcompiled.c
```
### `-m`
`-m` is for what the name of luaopen function should be in the compiled file, by default it is `luaopen_<FILENAME>`.

Example:
```bash
./src/luaot test.lua -o testcompiled.c -m luaopen_testfile # Compile test.lua to testcompiled.c with `luaopen_testfile` as the luaopen function
```
### `-e` 
`-e` just tells the compiler to add a `main` symbol/function so the file can be quickly compiled to an executable like:
```bash
./src/luaot test.lua -o testcompiled.c -e # Compile test.lua to testcompiled.c and add a main func for compiling to executables
gcc -o testexec testcompiled.c -llua5.4 -I/src # Compile testcompiled to an executable that will run the lua code
```
# Experiments

If you are interested in reproducing the experiments from our paper, please consult the documentation in the `experiments` and `scripts` directory. Note that you must be inside the experiments directory when you run the scripts:

    ../scripts/compile binarytrees.lua
    ../scripts/run binarytrees_fast 10

