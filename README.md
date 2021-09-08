# LuaAOT 5.4.3

This is a modified version of Lua 5.4.3 that allows Lua functions to be compiled ahead-of-time to more efficient code. It also includes a compiler called `luaot` that converts a Lua module to an equivalent one written in C.

The luaot compiler is a simplistic compiler that compiles Lua bytecode to C. One may see it as a partial evaluation of the reference Lua interpreter. The main optimizations are that Lua jumps are converted into C gotos, and that there is no overhead of interpreting bytecodes at run time. Additionally, the C compiler will be able to optimize some kinds of things, specially constant folding and dead-code elimination for the opcodes that include immediate parameters.

The performance gains depend on the kind of program being run. For numerical benchmarks it can be twice as fast as regular Lua. But overall, it is not as fast as LuaJIT or Pallene.

# Compilation and instalation

This is a modified version of the Lua interpreter so the process is the same you would use for compiling Lua. For full instructions, consult the doc/readme.html.

    make linux -j4

# Usage

Our compiler generates a `.c` file with a `luaopen_` function. You can compile that into a `.so` module and then require it from Lua. The compilation is the same as any other extension module, except that you need to pass the path to the LuaAOT headers.

    ./src/luaot test.lua -o testcompiled.c
    gcc -shared -fPIC -O2 -I./src testcompiled.c -o testcompiled.so
    ./src/lua -l testcompiled

Be careful about the name of the generated file. If it has the same name as the original Lua file, the `require` may prioritize the Lua file instead of the compiled on. We also recommend compiling with optimizations, at least -O2.

# Experiments

If you are interested in reproducing the experiments from our paper, please consult the documentation in the `experiments` and `scripts` directory. Note that you must be inside the experiments directory when you run the scripts:

    ../scripts/compile binarytrees.lua
    ../scripts/run binarytrees_fast 10

