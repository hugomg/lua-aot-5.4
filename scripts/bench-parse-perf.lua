#!/usr/bin/lua

local pretty_names = {
    binarytrees = "Binary Trees",
    fannkuch = "Fannkuch",
    fasta = "Fasta",
    knucleotide = "K-Nucleotide",
    mandelbrot = "Mandelbrot",
    nbody = "N-Body",
    spectralnorm = "Spectral Norm",
    cd = "CD",
    deltablue = "Deltablue",
    havlak = "Havlak",
    json = "Json",
    list = "List",
    permute = "Permute",
    richards = "Richards",
    towers = "Hanoi Towers",
}

local function parse_name(module)
    local code, is_aot = string.match(module, "^(.*)(_aot)$")
    if is_aot then
        return code, "aot"
    else
        return module, "lua"
    end
end

local function pretty_name(module)
    local code, aot = string.match(module, "^(.*)(_aot)$")
    local name = assert(pretty_names[code or module])
    if aot then
        return name .. " (AOT)"
    else
        return name .. " (Lua)"
    end
end

local rows = {}
while true do
    local blank = io.read("l");
    if not blank then break end
    assert(blank == "")

    local lines = {}
    for i = 1, 18 do
        lines[i] = io.read("l")
    end

    local module = assert(string.match(lines[1], "stats for .* (%S+) %d+ >"))
    local cycle  = assert(string.match(lines[7], "(%S+) *cycles"))
    local instr  = assert(string.match(lines[8], "(%S+) *instructions"))
    local ipc    = assert(string.match(lines[8], "(%S+) *insn per cycle"))
    local time   = assert(string.match(lines[12], "(%S+) *seconds time elapsed"))

    local r = {}
    r.code, r.is_aot = parse_name(module)
    r.cycle = tonumber(cycle)
    r.instr = tonumber(instr)
    r.ipc   = tonumber(ipc)
    r.time  = tonumber(time)

    table.insert(rows, r)
end

for i = 1, #rows, 2 do
    local lua = rows[i]
    local aot = rows[i+1]

    assert(lua.code == aot.code)
    assert(lua.is_aot == "lua")
    assert(aot.is_aot == "aot")

    local name = pretty_names[lua.code]
    local instr = (aot.instr / lua.instr) * 100.0
    local cycle = (aot.cycle / lua.cycle) * 100.0
    print(string.format("%-14s & %3.1f & %3.1f \\\\", name, instr, cycle))
end
