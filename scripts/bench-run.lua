#!/usr/bin/lua

local nkey = "slow"
do
    local i = 1
    while i <= #arg do
        if     arg[i] == "--fast"   then nkey = "fast"
        elseif arg[i] == "--medium" then nkey = "medium"
        elseif arg[i] == "--slow"   then nkey = "slow"
        end
        i = i + 1
    end
end

local repetitions = 20

-- fast   : runs on a blink of an eye (for testing / debugging)
-- medium : the aot version takes more than 1 second
-- slow   : the jit version takes more than 1 second

local benchs = {
    { name = "binarytrees",  fast =   5, medium =      16, slow =      16 },
    { name = "fannkuch",     fast =   5, medium =      10, slow =      11 },
    { name = "fasta",        fast = 100, medium = 1000000, slow = 2500000 },
    { name = "knucleotide",  fast = 100, medium = 1000000, slow = 1000000 },
    { name = "mandelbrot",   fast =  20, medium =    2000, slow =    4000 },
    { name = "nbody",        fast = 100, medium = 1000000, slow = 5000000 },
    { name = "spectralnorm", fast = 100, medium =    1000, slow =    4000 },
}

local impls = {
    { name = "jit", suffix = "",     interpreter = "luajit",     compile = false                    },
    { name = "lua", suffix = "",     interpreter = "../src/lua", compile = false,                   },
    { name = "aot", suffix = "_aot", interpreter = "../src/lua", compile = "../src/luaot"           },
    { name = "cor", suffix = "_cor", interpreter = "../src/lua", compile = "../src/luaot -C"        },
    { name = "trm", suffix = "_trm", interpreter = "../src/lua", compile = "../src/luaot-trampoline"},
}

--
-- Shell
--

local function quote(s)
    if string.find(s, '^[A-Za-z0-9_./]*$') then
        return s
    else
        return "'" .. s:gsub("'", "'\\''") .. "'"
    end
end

local function prepare(cmd_fmt, ...)
    local params = table.pack(...)
    return (string.gsub(cmd_fmt, '%$(%d+)', function(i)
        return quote(params[tonumber(i)])
    end))
end

local function run(cmd_fmt, ...)
    return (os.execute(prepare(cmd_fmt, ...)))
end

local function exists(filename)
    return run("test -f $1", filename)
end

--
-- Recompile
--

print("Recompiling the compiler...")
assert(run("cd .. && make guess --quiet"))

for _, b in ipairs(benchs) do
    for _, s in ipairs(impls) do
        local mod = b.name .. s.suffix
        if s.compile and not exists(mod .. ".so") then
            assert(run(s.compile .. " $1.lua -o $2.c", b.name, mod))
            assert(run("../scripts/compile $2.c",      b.name, mod))
        end
    end
end

--
-- Execute
--

print("---START---")

for _, b in ipairs(benchs) do
    for _, s in ipairs(impls) do

        local mod
        if s.name == "jit" and exists(b.name.."_jit.lua") then
            mod = b.name .. "_jit"
        else
            mod = b.name .. s.suffix
        end

        local n = assert(b[nkey])
        local cmd = prepare("$1 main.lua $2 $3 > /dev/null", s.interpreter, mod, n)

        for rep = 1, repetitions do
            print(string.format("RUN %s %s %s", b.name, s.name, rep))
            assert(run("/usr/bin/time -p sh -c $1 2>&1", cmd))
            print()
        end
    end
end

