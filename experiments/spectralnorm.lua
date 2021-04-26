-- Spectral norm benchmark from benchmarks game
-- https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/spectralnorm.html
--
-- Original C# code:
-- https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/spectralnorm-csharpcore-1.html
--
-- Code by Hugo Gualandi. It is translated directly from the C# specification
-- with the following main differences:
--   - The A() function uses 1-based indexing instead of 0-based
--   - The A() function multiplies by 0.5 instead of doing an integer division.
--     According to my measurements, this is not a significant difference, and
--     the multiplication by 0.5 has the advantage that it works on LuaJIT too.
--   - Some of the "out" tables not initialized with zeroes.
--
-- Expected output (N = 5500):
--    1.274224153


-- Return A[i][j], for the infinite matrix A
--
--  A = 1/1  1/2  1/4 ...
--      1/3  1/5  ... ...
--      1/6  ...  ... ...
--      ...  ...  ... ...
local function A(i, j)
    local ij = i + j
    return 1.0 / ((ij-1) * (ij-2) * 0.5 + i)
end

-- Multiply vector v by matrix A
local function MultiplyAv(N, v, out)
    for i = 1, N do
        local s = 0.0
        for j = 1, N do
            s = s + A(i,j) * v[j]
        end
        out[i] = s
    end
end

-- Multiply vector v by matrix A transposed
local function MultiplyAtv(N, v, out)
    for i=1, N do
        local s = 0.0
        for j = 1, N do
            s = s + A(j,i) * v[j]
        end
        out[i] = s
    end
end

-- Multiply vector v by matrix A and then by matrix A transposed
local function MultiplyAtAv(N, v, out)
    local u = {}
    MultiplyAv(N, v, u)
    MultiplyAtv(N, u, out)
end

local function Approximate(N)
    -- Create unit vector
    local u = {}
    for i = 1, N do
        u[i] = 1.0
    end

    -- 20 steps of the power method
    local v = {}
    for _ = 1, 10 do
        MultiplyAtAv(N, u, v)
        MultiplyAtAv(N, v, u)
    end

    local vBv = 0.0
    local vv  = 0.0
    for i = 1, N do
        local ui = u[i]
        local vi = v[i]
        vBv = vBv + ui*vi
        vv  = vv  + vi*vi
    end

    return math.sqrt(vBv/vv)
end

return function(N)
    N = N or 5500
    local res = Approximate(N)
    print(string.format("%0.9f", res))
end
