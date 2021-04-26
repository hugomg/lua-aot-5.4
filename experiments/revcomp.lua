-- Reverse Complement benchmark from benchmarks game
-- https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/revcomp.html
--
-- Original LUa code:
-- https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/revcomp-lua-4.html
--
-- Adapted by Hugo Gualandi
--  * Removed the eval-based metaprogramming
--  * Don't do complicated logic with buffers
--  * Use string.gsub

-- 
-- DNA complement table
--
local R = {}
do
    local codes = "ACGTUMRWSYKVHDBN"
    local cmpls = "TGCAAKYWSRMBDHVN"
    for i = 1, #codes do
        local s = string.sub(codes, i, i)
        local t = string.sub(cmpls, i, i)
        R[s] = t
        R[string.lower(s)] = t
    end
end


--
-- Print a long DNA sequence in multiple lines,
-- with 60 columns per line
--
local function wrap(str)
    local n = #str
    local r = n
    local i = 1
    while r >= 60 do
        io.write(string.sub(str, i, i+59), "\n")
        i = i + 60
        r = r - 60
    end
    if r > 0 then
        io.write(string.sub(str, i, n), "\n")
    end
end

--
-- Print the reverse-complement of the input sequences
--
local function revcomp(infile)
    local n = 0
    local names  = {}
    local liness = {}
    
    local lines
    for line in infile:lines() do
        if string.sub(line, 1, 1) == ">" then
            n = n + 1
            lines = {}
            names[n] = line
            liness[n] = lines
        else
            lines[#lines+1] = line
        end
    end
    
    for i = 1, n do
        local seq = table.concat(liness[i])
        local rev = string.reverse(seq)
        local cpl = string.gsub(rev, '.', R)
        io.write(names[i], "\n")
        wrap(cpl)
    end
end

return function()
    revcomp(io.stdin)
end
