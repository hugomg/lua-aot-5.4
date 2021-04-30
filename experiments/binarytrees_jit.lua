-- Binary trees benchmark from benchmarks game
-- https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/binarytrees.html
--
-- This is a small GC-focused benchmark, with not much room for clever tricks.
-- The code used here is based on the one contributed by Mike Pall:
--  * The leaf nodes are now store `false` instead of being empty tables.
--    This is more consistent with what is done in other languages.
--  * Use print instead of io.write
--  * Use bitshift operator in Lua 5.4 and Pallene
--
-- Expected output (N = 21)
--   stretch tree of depth 22	      check: 8388607
--   2097152  trees of depth 4	      check: 65011712
--   524288	  trees of depth 6	      check: 66584576
--   131072	  trees of depth 8	      check: 66977792
--   32768	  trees of depth 10	      check: 67076096
--   8192	  trees of depth 12	      check: 67100672
--   2048	  trees of depth 14	      check: 67106816
--   512      trees of depth 16	      check: 67108352
--   128      trees of depth 18	      check: 67108736
--   32       trees of depth 20       check: 67108832
--   long lived tree of depth 21      check: 4194303

local function BottomUpTree(depth)
    if depth > 0 then
        depth = depth - 1
        local left  = BottomUpTree(depth)
        local right = BottomUpTree(depth)
        return { left, right }
    else
        return { false, false }
    end
end

local function ItemCheck(tree)
    if tree[1] then
        return 1 + ItemCheck(tree[1]) + ItemCheck(tree[2])
    else
        return 1
    end
end

local function Stress(mindepth, maxdepth, depth)
    local iterations = 2 ^ (maxdepth - depth + mindepth)
    local check = 0
    for _ = 1, iterations do
        local t = BottomUpTree(depth)
        check = check + ItemCheck(t)
    end
    return { iterations, check }
end

return function(N)
    N = N or 10

    local mindepth = 4
    local maxdepth = math.max(mindepth + 2, N)

    do
        local stretchdepth = maxdepth + 1
        local stretchtree = BottomUpTree(stretchdepth)
        print(string.format("stretch tree of depth %d\t check: %d",
            stretchdepth, ItemCheck(stretchtree)))
    end

    local longlivedtree = BottomUpTree(maxdepth)

    for depth = mindepth, maxdepth, 2 do
        local r = Stress(mindepth, maxdepth, depth)
        local iterations = r[1]
        local check      = r[2]
        print(string.format("%d\t trees of depth %d\t check: %d",
            iterations, depth, check))
    end

    print(string.format("long lived tree of depth %d\t check: %d",
        maxdepth, ItemCheck(longlivedtree)))
end

