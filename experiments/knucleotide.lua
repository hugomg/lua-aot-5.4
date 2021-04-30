-- k-nucleotide norm benchmark from benchmarks game
-- https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/knucleotide.html#knucleotide
--
-- Original Lua code by Mike Pall:
-- https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/knucleotide-lua-2.html
--
-- Original Ruby code:
-- https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/knucleotide-mri-2.html
--
-- Modifications by Hugo Gualandi:
--  * reindented the code
--  * rewrote in more idiomatic lua
--
-- Expected output (N = 5500):
--    1.274224153


--
-- Reads the last sequence generated by the FASTA benchmark
--
local function read_input(f)
    for line in f:lines() do
        if string.sub(line, 1, 6) == ">THREE" then
            break
        end
    end
    local lines = {}
    for line in f:lines() do
        table.insert(lines, line)
    end
    return string.upper(table.concat(lines))
end

--
-- Compute the frequency table for substrings of size `length`.
-- The particular iteration order, using a moving reading frame, is mandatory.
--
local function compute_freqs(seq, length)
    local freqs = setmetatable({}, { __index = function() return 0 end })
    local n = #seq - length + 1
    for f = 1, length do
        for i = f, n, length do
            local s = string.sub(seq, i, i+length-1)
            freqs[s] = freqs[s] + 1
        end
    end
    return n, freqs
end

--
-- Print the most common subsequences, in decreasing order
--
local function sort_by_freq(seq, length)
    local n, freq = compute_freqs(seq, length)

    local seqs = {}
    for k, _ in pairs(freq) do
        seqs[#seqs+1] = k
    end

    table.sort(seqs, function(a, b)
        local fa, fb = freq[a], freq[b]
        return (fa == fb) and (a < b) or (fa > fb)
    end)

    for _, c in ipairs(seqs) do
        io.write(string.format("%s %0.3f\n", c, freq[c]*100/n))
    end
    io.write("\n")
end

local function single_freq(seq, s)
    local _, freq = compute_freqs(seq, #s)
    io.write(freq[s], "\t", s, "\n")
end

return function(N)
    local f, err = io.open("fasta-output-"..tostring(N)..".txt", "r")
    if not f then error(err) end

    local seq = read_input(f)

    sort_by_freq(seq, 1)
    sort_by_freq(seq, 2)

    single_freq(seq, "GGT")
    single_freq(seq, "GGTA")
    single_freq(seq, "GGTATT")
    single_freq(seq, "GGTATTTTAATT")
    single_freq(seq, "GGTATTTTAATTTATAGT")
end
