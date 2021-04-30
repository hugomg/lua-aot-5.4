-- Fasta benchmark from benchmarks game
-- https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/fasta.html
--
-- Translated from the Java and Python versions found at
-- https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/fasta-java-2.html
-- https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/fasta-python3-1.html
--
-- This differs from the Lua versions of the benchmark in some aspects
--  * Don't use load/eval metaprogramming
--  * Repeat fasta now works correctly when #alu < 60
--  * Use linear search (actually faster than binary search in the tests)
--  * Use // integer division

local IM   = 139968
local IA   = 3877
local IC   = 29573

local seed = 42
local function random(max)
    seed = (seed * IA + IC) % IM
    return max * seed / IM;
end

local WIDTH = 60

local function print_fasta_header(id, desc)
    io.write(">" .. id .. " " .. desc .. "\n")
end

local function repeat_fasta(id, desc, alu, n)
    print_fasta_header(id, desc)

    local alusize = #alu

    local aluwrap = alu .. alu
    while #aluwrap < alusize + WIDTH do
        aluwrap = aluwrap .. alu
    end

    local lines     = math.floor(n / WIDTH)
    local last_line = n % WIDTH
    local start = 0 -- (This index is 0-based bacause of the % operator)
    for _ = 1, lines do
        local stop = start + WIDTH
        io.write(string.sub(aluwrap, start+1, stop))
        io.write("\n")
        start = stop % alusize
    end
    if last_line > 0 then
        io.write(string.sub(aluwrap, start+1, start + last_line))
        io.write("\n")
    end
end

local function linear_search(ps, p)
    for i = 1, #ps do
        if ps[i]>= p then
            return i
        end
    end
    return 1
end

local function random_fasta(id, desc, frequencies, n)
    print_fasta_header(id, desc)

    -- Prepare the cummulative probability table
    local nitems  = #frequencies
    local letters = {}
    local probs = {}
    do
        local total = 0.0
        for i = 1, nitems do
            local o = frequencies[i]
            local c = o[1]
            local p = o[2]
            total = total + p
            letters[i] = c
            probs[i]   = total
        end
        probs[nitems] = 1.0
    end

    -- Generate the output
    local col = 0
    for _ = 1, n do
        local ix = linear_search(probs, random(1.0))
        local c = letters[ix]

        io.write(c)
        col = col + 1
        if col >= WIDTH then
            io.write("\n")
            col = 0
        end
    end
    if col > 0 then
        io.write("\n")
    end
end

local HUMAN_ALU =
    "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG" ..
    "GAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGA" ..
    "CCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAAT" ..
    "ACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCA" ..
    "GCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGG" ..
    "AGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCC" ..
    "AGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA"

local IUB = {
    { 'a', 0.27 },
    { 'c', 0.12 },
    { 'g', 0.12 },
    { 't', 0.27 },
    { 'B', 0.02 },
    { 'D', 0.02 },
    { 'H', 0.02 },
    { 'K', 0.02 },
    { 'M', 0.02 },
    { 'N', 0.02 },
    { 'R', 0.02 },
    { 'S', 0.02 },
    { 'V', 0.02 },
    { 'W', 0.02 },
    { 'Y', 0.02 },
}

local HOMO_SAPIENS = {
    { 'a', 0.3029549426680 },
    { 'c', 0.1979883004921 },
    { 'g', 0.1975473066391 },
    { 't', 0.3015094502008 },
}

return function(N)
    N = N or 100
    repeat_fasta("ONE", "Homo sapiens alu", HUMAN_ALU, N*2)
    random_fasta('TWO', 'IUB ambiguity codes', IUB, N*3)
    random_fasta('THREE', 'Homo sapiens frequency', HOMO_SAPIENS, N*5)
end
