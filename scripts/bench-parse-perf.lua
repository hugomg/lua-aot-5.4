#!/usr/bin/lua

local pretty_names = {
    binarytrees  = "Binary Trees",
    fannkuch     = "Fannkuch",
    fasta        = "Fasta",
    knucleotide  = "K-Nucleotide",
    mandelbrot   = "Mandelbrot",
    nbody        = "N-Body",
    spectralnorm = "Spectral Norm",
}

local function pretty_name(module)
    local code, aot = string.match(module, "^(.*)(_cor)$")
    local name = assert(pretty_names[code or module])
    if aot then
        return name .. " (AOT)"
    else
        return name .. " (Lua)"
    end
end

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

    local bench = pretty_name(module)
    cycle = tonumber(cycle)
    instr = tonumber(instr)
    ipc   = tonumber(ipc)
    time  = tonumber(time)

    local sinstr = string.format("$%6.2f\\times10^9$", instr/1.0e9)
    local scycle = string.format("$%6.2f\\times10^9$", cycle/1.0e9)
    print(string.format("%-20s & %6.2f & %18s & %18s & %4.2f \\\\", bench, time, sinstr, scycle, ipc))
end
