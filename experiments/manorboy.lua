-- The man or boy test was proposed by computer scientist Donald Knuth as a
-- means of evaluating implementations of the ALGOL 60 programming language.
-- The aim of the test was to distinguish compilers that correctly implemented
-- "recursion and non-local references" from those that did not.
-- https://rosettacode.org/wiki/Man_or_boy_test#Lua

local function a(k,x1,x2,x3,x4,x5)
  local function b()
    k = k - 1
    return a(k,b,x1,x2,x3,x4)
  end
   if k <= 0 then return x4() + x5() else return b() end
end

local function K(n)
  return function()
    return n
  end
end

return function(N)
    N = N or 10
    print( a(N, K(1), K(-1), K(-1), K(1), K(0)) )
end
